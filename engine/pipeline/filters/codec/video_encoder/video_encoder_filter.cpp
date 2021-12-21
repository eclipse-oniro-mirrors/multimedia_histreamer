/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(RECORDER_SUPPORT) && defined(VIDEO_SUPPORT)

#define HST_LOG_TAG "VideoEncoderFIlter"

#include "video_encoder_filter.h"
#include "foundation/log.h"
#include "osal/utils/util.h"
#include "utils/constants.h"
#include "utils/memory_helper.h"
#include "utils/steady_clock.h"
#include "factory/filter_factory.h"
#include "plugin/common/plugin_buffer.h"
#include "plugin/common/plugin_video_tags.h"

namespace OHOS {
namespace Media {
namespace Pipeline {
const uint32_t DEFAULT_IN_BUFFER_POOL_SIZE = 200;
const uint32_t DEFAULT_OUT_BUFFER_POOL_SIZE = 8;
const float VIDEO_PIX_DEPTH = 1.5;
static uint32_t VIDEO_ALIGN_SIZE = 16;
static uint32_t DEFAULT_TRY_DECODE_TIME = 10;

static AutoRegisterFilter<VideoEncoderFIlter> g_registerFilterHelper("builtin.recorder.videoEncoder");

class VideoEncoderFIlter::DataCallbackImpl : public Plugin::DataCallbackHelper {
public:
    explicit DataCallbackImpl(VideoEncoderFIlter& filter) : decFilter_(filter)
    {
    }

    ~DataCallbackImpl() override = default;

    void OnInputBufferDone(const std::shared_ptr<Plugin::Buffer>& input) override
    {
        decFilter_.OnInputBufferDone(input);
    }

    void OnOutputBufferDone(const std::shared_ptr<Plugin::Buffer>& output) override
    {
        decFilter_.OnOutputBufferDone(output);
    }

private:
    VideoEncoderFIlter& decFilter_;
};

VideoEncoderFIlter::VideoEncoderFIlter(const std::string& name)
    : DecoderFilterBase(name), dataCallback_(std::make_shared<DataCallbackImpl>(*this))
{
    MEDIA_LOG_I("video encoder ctor called");
    vdecFormat_.width = 0;
    vdecFormat_.height = 0;
    vdecFormat_.bitRate = -1;
}

VideoEncoderFIlter::~VideoEncoderFIlter()
{
    MEDIA_LOG_I("video encoder dtor called");
    if (plugin_) {
        plugin_->Stop();
        plugin_->Deinit();
    }
    if (handleFrameTask_) {
        handleFrameTask_->Stop();
        handleFrameTask_.reset();
    }
    if (inBufQue_) {
        inBufQue_->SetActive(false);
        inBufQue_.reset();
    }
    if (pushTask_) {
        pushTask_->Stop();
        pushTask_.reset();
    }
    if (outBufQue_) {
        outBufQue_->SetActive(false);
        outBufQue_.reset();
    }
}

ErrorCode VideoEncoderFIlter::Start()
{
    MEDIA_LOG_D("video encoder start called");
    if (state_ != FilterState::READY && state_ != FilterState::PAUSED) {
        MEDIA_LOG_W("call encoder start() when state_ is not ready or working");
        return ErrorCode::ERROR_INVALID_OPERATION;
    }
    return FilterBase::Start();
}

ErrorCode VideoEncoderFIlter::Prepare()
{
    MEDIA_LOG_D("video encoder prepare called");
    if (state_ != FilterState::INITIALIZED) {
        MEDIA_LOG_W("encoder filter is not in init state_");
        return ErrorCode::ERROR_INVALID_OPERATION;
    }
    if (!outBufQue_) {
        outBufQue_ = std::make_shared<BlockingQueue<AVBufferPtr>>("vdecFilterOutBufQue", DEFAULT_OUT_BUFFER_POOL_SIZE);
    } else {
        outBufQue_->SetActive(true);
    }
    if (!pushTask_) {
        pushTask_ = std::make_shared<OHOS::Media::OSAL::Task>("vdecPushThread");
        pushTask_->RegisterHandler([this] { FinishFrame(); });
    }
    if (!inBufQue_) {
        inBufQue_ = std::make_shared<BlockingQueue<AVBufferPtr>>("vdecFilterInBufQue", DEFAULT_IN_BUFFER_POOL_SIZE);
    } else {
        inBufQue_->SetActive(true);
    }
    if (!handleFrameTask_) {
        handleFrameTask_ = std::make_shared<OHOS::Media::OSAL::Task>("decHandleFrameThread");
        handleFrameTask_->RegisterHandler([this] { HandleFrame(); });
    }
    return FilterBase::Prepare();
}

ErrorCode VideoEncoderFIlter::SetVideoEncoder(int32_t sourceId, OHOS::Media::Plugin::VideoFormat encoder)
{
    return ErrorCode::SUCCESS;
}

bool VideoEncoderFIlter::Negotiate(const std::string& inPort,
                                   const std::shared_ptr<const Plugin::Capability>& upstreamCap,
                                   Capability& upstreamNegotiatedCap)
{
    PROFILE_BEGIN("video encoder negotiate start");
    if (state_ != FilterState::PREPARING) {
        MEDIA_LOG_W("encoder filter is not in preparing when negotiate");
        return false;
    }
    auto targetOutPort = GetRouteOutPort(inPort);
    if (targetOutPort == nullptr) {
        MEDIA_LOG_E("encoder out port is not found");
        return false;
    }
    std::shared_ptr<Plugin::PluginInfo> selectedPluginInfo = nullptr;
    bool atLeastOutCapMatched = false;
    auto candidatePlugins = FindAvailablePlugins(*upstreamCap, Plugin::PluginType::CODEC);
    for (const auto& candidate : candidatePlugins) {
        if (candidate.first->outCaps.empty()) {
            MEDIA_LOG_E("encoder plugin must have out caps");
        }
        for (const auto& outCap : candidate.first->outCaps) { // each codec plugin should have at least one out cap
            auto thisOut = std::make_shared<Plugin::Capability>();
            if (!MergeCapabilityKeys(*upstreamCap, outCap, *thisOut)) {
                MEDIA_LOG_W("one of out cap of plugin %s does not match with upstream capability",
                            candidate.first->name.c_str());
                continue;
            }
            atLeastOutCapMatched = true;
            thisOut->mime = outCap.mime;
            if (targetOutPort->Negotiate(thisOut, capNegWithDownstream_)) {
                capNegWithUpstream_ = candidate.second;
                selectedPluginInfo = candidate.first;
                MEDIA_LOG_I("choose plugin %s as working parameter", candidate.first->name.c_str());
                break;
            }
        }
        if (selectedPluginInfo != nullptr) { // select the first one
            break;
        }
    }
    if (!atLeastOutCapMatched) {
        MEDIA_LOG_W("cannot find available encoder plugin");
        return false;
    }
    if (selectedPluginInfo == nullptr) {
        MEDIA_LOG_W("cannot find available downstream plugin");
        return false;
    }
    PROFILE_END("video encoder negotiate end");
    return UpdateAndInitPluginByInfo(selectedPluginInfo);
}

bool VideoEncoderFIlter::Configure(const std::string& inPort, const std::shared_ptr<const Plugin::Meta>& upstreamMeta)
{
    PROFILE_BEGIN("video encoder configure start");
    if (plugin_ == nullptr || targetPluginInfo_ == nullptr) {
        MEDIA_LOG_E("cannot configure encoder when no plugin available");
        return false;
    }
    if (upstreamMeta->GetString(Plugin::MetaID::MIME, vdecFormat_.mime)) {
        MEDIA_LOG_D("mime: %s", vdecFormat_.mime.c_str());
    }
    auto thisMeta = std::make_shared<Plugin::Meta>();
    if (!MergeMetaWithCapability(*upstreamMeta, capNegWithDownstream_, *thisMeta)) {
        MEDIA_LOG_E("cannot configure encoder plugin since meta is not compatible with negotiated caps");
    }
    auto targetOutPort = GetRouteOutPort(inPort);
    if (targetOutPort == nullptr) {
        MEDIA_LOG_E("encoder out port is not found");
        return false;
    }
    if (!targetOutPort->Configure(thisMeta)) {
        MEDIA_LOG_E("encoder filter downstream Configure failed");
        return false;
    }
    auto err = ConfigureNoLocked(thisMeta);
    if (err != ErrorCode::SUCCESS) {
        MEDIA_LOG_E("encoder configure error");
        Event event{
            .type = EVENT_ERROR,
            .param = err,
        };
        OnEvent(event);
        return false;
    }
    state_ = FilterState::READY;
    Event event{
            .type = EVENT_READY,
    };
    OnEvent(event);
    MEDIA_LOG_I("video encoder send EVENT_READY");
    PROFILE_END("video encoder configure end");
    return true;
}

ErrorCode VideoEncoderFIlter::AllocateOutputBuffers()
{
    uint32_t bufferCnt = 0;
    if (GetPluginParameterLocked(Tag::REQUIRED_OUT_BUFFER_CNT, bufferCnt) != ErrorCode::SUCCESS) {
        bufferCnt = DEFAULT_OUT_BUFFER_POOL_SIZE;
    }
    outBufPool_ = std::make_shared<BufferPool<AVBuffer>>(bufferCnt);
    // YUV420: size = stride * height * 1.5
    uint32_t bufferSize = 0;
    uint32_t stride = Plugin::AlignUp(vdecFormat_.width, VIDEO_ALIGN_SIZE);
    if (vdecFormat_.format == Plugin::VideoPixelFormat::YUV420P ||
        vdecFormat_.format == Plugin::VideoPixelFormat::NV21 ||
        vdecFormat_.format == Plugin::VideoPixelFormat::NV12) {
        bufferSize = static_cast<uint32_t>(Plugin::AlignUp(stride, VIDEO_ALIGN_SIZE) *
                                           Plugin::AlignUp(vdecFormat_.height, VIDEO_ALIGN_SIZE) * VIDEO_PIX_DEPTH);
        MEDIA_LOG_D("Output buffer size: %u", bufferSize);
    } else {
        // need to check video sink support and calc buffer size
        MEDIA_LOG_E("Unsupported video pixel format: %u", vdecFormat_.format);
        return ErrorCode::ERROR_UNIMPLEMENTED;
    }
    auto outAllocator = plugin_->GetAllocator(); // zero copy need change to use sink allocator
    if (outAllocator == nullptr) {
        MEDIA_LOG_I("plugin doest not support out allocator, using framework allocator");
        outBufPool_->Init(bufferSize, Plugin::BufferMetaType::VIDEO);
    } else {
        MEDIA_LOG_I("using plugin output allocator");
        for (size_t cnt = 0; cnt < bufferCnt; cnt++) {
            auto buf = MemoryHelper::make_unique<AVBuffer>(Plugin::BufferMetaType::VIDEO);
            buf->AllocMemory(outAllocator, bufferSize);
            outBufPool_->Append(std::move(buf));
        }
    }
    return ErrorCode::SUCCESS;
}

ErrorCode VideoEncoderFIlter::SetVideoDecoderFormat(const std::shared_ptr<const Plugin::Meta>& meta)
{
    if (!meta->GetData<Plugin::VideoPixelFormat>(Plugin::MetaID::VIDEO_PIXEL_FORMAT, vdecFormat_.format)) {
        MEDIA_LOG_E("Get video pixel format fail");
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    if (!meta->GetUint32(Plugin::MetaID::VIDEO_WIDTH, vdecFormat_.width)) {
        MEDIA_LOG_E("Get video width fail");
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    if (!meta->GetUint32(Plugin::MetaID::VIDEO_HEIGHT, vdecFormat_.height)) {
        MEDIA_LOG_E("Get video width height");
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    if (!meta->GetInt64(Plugin::MetaID::MEDIA_BITRATE, vdecFormat_.bitRate)) {
        MEDIA_LOG_D("Do not have codec bit rate");
    }
    // Optional: codec extra data
    if (!meta->GetData<std::vector<uint8_t>>(Plugin::MetaID::MEDIA_CODEC_CONFIG, vdecFormat_.codecConfig)) {
        MEDIA_LOG_D("Do not have codec extra data");
    }
    return ErrorCode::SUCCESS;
}

ErrorCode VideoEncoderFIlter::ConfigurePluginParams()
{
    if (SetPluginParameterLocked(Tag::VIDEO_WIDTH, vdecFormat_.width) != ErrorCode::SUCCESS) {
        MEDIA_LOG_W("Set width to plugin fail");
        return ErrorCode::ERROR_UNKNOWN;
    }
    if (SetPluginParameterLocked(Tag::VIDEO_HEIGHT, vdecFormat_.height) != ErrorCode::SUCCESS) {
        MEDIA_LOG_W("Set height to plugin fail");
        return ErrorCode::ERROR_UNKNOWN;
    }
    if (SetPluginParameterLocked(Tag::VIDEO_PIXEL_FORMAT, vdecFormat_.format) != ErrorCode::SUCCESS) {
        MEDIA_LOG_W("Set pixel format to plugin fail");
        return ErrorCode::ERROR_UNKNOWN;
    }
    if (vdecFormat_.bitRate != -1) {
        if (SetPluginParameterLocked(Tag::MEDIA_BITRATE, vdecFormat_.bitRate) != ErrorCode::SUCCESS) {
            MEDIA_LOG_W("Set bitrate to plugin fail");
        }
    }
    // Optional: codec extra data
    if (vdecFormat_.codecConfig.size() > 0) {
        if (SetPluginParameterLocked(Tag::MEDIA_CODEC_CONFIG, std::move(vdecFormat_.codecConfig)) !=
            ErrorCode::SUCCESS) {
            MEDIA_LOG_W("Set bitrate to plugin fail");
        }
    }
    MEDIA_LOG_D("ConfigurePluginParams success, mime: %s, width: %u, height: %u, format: %u, bitRate: %u",
                vdecFormat_.mime.c_str(), vdecFormat_.width, vdecFormat_.height, vdecFormat_.format,
                vdecFormat_.bitRate);
    return ErrorCode::SUCCESS;
}

ErrorCode VideoEncoderFIlter::ConfigurePluginOutputBuffers()
{
    ErrorCode err = ErrorCode::SUCCESS;
    while (!outBufPool_->Empty()) {
        auto ptr = outBufPool_->AllocateBuffer();
        if (ptr == nullptr) {
            MEDIA_LOG_W("cannot allocate buffer in buffer pool");
            continue;
        }
        err = TranslatePluginStatus(plugin_->QueueOutputBuffer(ptr, -1));
        if (err != ErrorCode::SUCCESS) {
            MEDIA_LOG_E("queue output buffer error");
        }
    }
    return err;
}

ErrorCode VideoEncoderFIlter::ConfigurePlugin()
{
    RETURN_ERR_MESSAGE_LOG_IF_FAIL(TranslatePluginStatus(plugin_->SetDataCallback(dataCallback_)),
                                   "Set plugin callback fail");
    RETURN_ERR_MESSAGE_LOG_IF_FAIL(ConfigurePluginParams(), "Configure plugin params error");
    RETURN_ERR_MESSAGE_LOG_IF_FAIL(ConfigurePluginOutputBuffers(), "Configure plugin output buffers error");
    RETURN_ERR_MESSAGE_LOG_IF_FAIL(TranslatePluginStatus(plugin_->Prepare()), "Prepare plugin fail");
    return TranslatePluginStatus(plugin_->Start());
}

ErrorCode VideoEncoderFIlter::ConfigureNoLocked(const std::shared_ptr<const Plugin::Meta>& meta)
{
    MEDIA_LOG_D("video encoder configure called");
    RETURN_ERR_MESSAGE_LOG_IF_FAIL(SetVideoDecoderFormat(meta), "Set video encoder format fail");
    RETURN_ERR_MESSAGE_LOG_IF_FAIL(AllocateOutputBuffers(), "Alloc output buffers fail");
    RETURN_ERR_MESSAGE_LOG_IF_FAIL(ConfigurePlugin(), "Config plugin fail");
    if (handleFrameTask_) {
        handleFrameTask_->Start();
    }
    if (pushTask_) {
        pushTask_->Start();
    }
    return ErrorCode::SUCCESS;
}

ErrorCode VideoEncoderFIlter::PushData(const std::string& inPort, AVBufferPtr buffer, int64_t offset)
{
    if (state_ != FilterState::READY && state_ != FilterState::PAUSED && state_ != FilterState::RUNNING) {
        MEDIA_LOG_W("pushing data to encoder when state_ is %d", static_cast<int>(state_.load()));
        return ErrorCode::ERROR_INVALID_OPERATION;
    }
    if (isFlushing_) {
        MEDIA_LOG_I("encoder is flushing, discarding this data from port %s", inPort.c_str());
        return ErrorCode::SUCCESS;
    }
    inBufQue_->Push(buffer);
    return ErrorCode::SUCCESS;
}

void VideoEncoderFIlter::FlushStart()
{
    MEDIA_LOG_I("FlushStart entered");
    isFlushing_ = true;
    if (inBufQue_) {
        inBufQue_->SetActive(false);
    }
    if (handleFrameTask_) {
        handleFrameTask_->PauseAsync();
    }
    if (outBufQue_) {
        outBufQue_->SetActive(false);
    }
    if (pushTask_) {
        pushTask_->PauseAsync();
    }
    if (plugin_ != nullptr) {
        auto err = TranslatePluginStatus(plugin_->Flush());
        if (err != ErrorCode::SUCCESS) {
            MEDIA_LOG_E("encoder plugin flush error");
        }
    }
}

void VideoEncoderFIlter::FlushEnd()
{
    MEDIA_LOG_I("FlushEnd entered");
    isFlushing_ = false;
    if (inBufQue_) {
        inBufQue_->SetActive(true);
    }
    if (handleFrameTask_) {
        handleFrameTask_->Start();
    }
    if (outBufQue_) {
        outBufQue_->SetActive(true);
    }
    if (pushTask_) {
        pushTask_->Start();
    }
    if (plugin_) {
        ConfigurePluginOutputBuffers();
    }
}

ErrorCode VideoEncoderFIlter::Stop()
{
    RETURN_ERR_MESSAGE_LOG_IF_FAIL(TranslatePluginStatus(plugin_->Flush()), "Flush plugin fail");
    RETURN_ERR_MESSAGE_LOG_IF_FAIL(TranslatePluginStatus(plugin_->Stop()), "Stop plugin fail");
    outBufQue_->SetActive(false);
    pushTask_->Pause();
    inBufQue_->SetActive(false);
    if (handleFrameTask_) {
        handleFrameTask_->Pause();
    }
    outBufPool_.reset();
    MEDIA_LOG_I("Stop success");
    return FilterBase::Stop();
}

void VideoEncoderFIlter::HandleFrame()
{
    MEDIA_LOG_D("HandleFrame called");
    auto oneBuffer = inBufQue_->Pop();
    if (oneBuffer == nullptr) {
        MEDIA_LOG_W("encoder find nullptr in esBufferQ");
        return;
    }
    HandleOneFrame(oneBuffer);
}

void VideoEncoderFIlter::HandleOneFrame(const std::shared_ptr<AVBuffer>& data)
{
    MEDIA_LOG_D("HandleOneFrame called");
    Plugin::Status ret;
    do {
        ret = plugin_->QueueInputBuffer(data, -1);
        if (ret == Plugin::Status::OK) {
            break;
        }
        MEDIA_LOG_D("Send data to plugin error: %d", ret);
        OSAL::SleepFor(DEFAULT_TRY_DECODE_TIME);
    } while (1);
}

void VideoEncoderFIlter::FinishFrame()
{
    MEDIA_LOG_D("begin finish frame");
    auto ptr = outBufQue_->Pop();
    if (ptr) {
        auto oPort = outPorts_[0];
        if (oPort->GetWorkMode() == WorkMode::PUSH) {
            oPort->PushData(ptr, -1);
        } else {
            MEDIA_LOG_W("encoder out port works in pull mode");
        }
        ptr.reset();
        auto oPtr = outBufPool_->AllocateBuffer();
        if (oPtr != nullptr) {
            oPtr->Reset();
            plugin_->QueueOutputBuffer(oPtr, 0);
        }
    }
    MEDIA_LOG_D("end finish frame");
}

void VideoEncoderFIlter::OnInputBufferDone(const std::shared_ptr<AVBuffer>& buffer)
{
    // do nothing since we has no input buffer pool
}

void VideoEncoderFIlter::OnOutputBufferDone(const std::shared_ptr<AVBuffer>& buffer)
{
    outBufQue_->Push(buffer);
}
} // namespace Pipeline
} // namespace Media
} // namespace OHOS
#endif
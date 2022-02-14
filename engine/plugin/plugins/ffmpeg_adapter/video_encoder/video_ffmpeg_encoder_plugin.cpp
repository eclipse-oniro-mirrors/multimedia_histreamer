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

#define HST_LOG_TAG "Ffmpeg_Video_Encoder"

#include "video_ffmpeg_encoder_plugin.h"
#include <cstring>
#include <map>
#include <set>
#include "utils/memory_helper.h"
#include "plugin/common/plugin_time.h"
#include "plugins/ffmpeg_adapter/utils/ffmpeg_utils.h"
#include "ffmpeg_vid_enc_config.h"

namespace {
// register plugins
using namespace OHOS::Media::Plugin;
using namespace Ffmpeg;
void UpdatePluginDefinition(const AVCodec* codec, CodecPluginDef& definition);

std::map<std::string, std::shared_ptr<const AVCodec>> codecMap;

const size_t BUFFER_QUEUE_SIZE = 6;
const size_t DEFAULT_ALIGN = 16;

std::set<AVCodecID> supportedCodec = {AV_CODEC_ID_H264};

std::shared_ptr<CodecPlugin> VideoFfmpegEncoderCreator(const std::string& name)
{
    return std::make_shared<VideoFfmpegEncoderPlugin>(name);
}

Status RegisterVideoEncoderPlugins(const std::shared_ptr<Register>& reg)
{
    const AVCodec* codec = nullptr;
    void* iter = nullptr;
    MEDIA_LOG_I("registering video encoders");
    while ((codec = av_codec_iterate(&iter))) {
        if (!av_codec_is_encoder(codec) || codec->type != AVMEDIA_TYPE_VIDEO) {
            continue;
        }
        if (supportedCodec.find(codec->id) == supportedCodec.end()) {
            MEDIA_LOG_D("codec %s(%s) is not supported right now", codec->name, codec->long_name);
            continue;
        }
        CodecPluginDef definition;
        definition.name = "video_encoder_" + std::string(codec->name);
        definition.codecType = CodecType::VIDEO_ENCODER;
        definition.rank = 100; // 100
        definition.creator = VideoFfmpegEncoderCreator;
        UpdatePluginDefinition(codec, definition);
        // do not delete the codec in the deleter
        codecMap[definition.name] = std::shared_ptr<AVCodec>(const_cast<AVCodec*>(codec), [](void* ptr) {});
        if (reg->AddPlugin(definition) != Status::OK) {
            MEDIA_LOG_W("register plugin %s(%s) failed", codec->name, codec->long_name);
        }
    }
    return Status::OK;
}

void UnRegisterVideoEncoderPlugins()
{
    codecMap.clear();
}

void UpdatePluginDefinition(const AVCodec* codec, CodecPluginDef& definition)
{
    Capability inputCaps(OHOS::Media::MEDIA_MIME_VIDEO_RAW);
    if (codec->pix_fmts != nullptr) {
        DiscreteCapability<uint32_t> values;
        size_t index = 0;
        for (index = 0; codec->pix_fmts[index] != 0; ++index) {
            values.push_back(codec->pix_fmts[index]);
        }
        if (index) {
            inputCaps.AppendDiscreteKeys(Capability::Key::VIDEO_PIXEL_FORMAT, values);
        }
    } else {
        inputCaps.AppendDiscreteKeys<VideoPixelFormat>(
                Capability::Key::VIDEO_PIXEL_FORMAT, {VideoPixelFormat::NV21});
    }
    definition.inCaps.push_back(inputCaps);

    Capability outputCaps("video/unknown");
    switch (codec->id) {
        case AV_CODEC_ID_H264:
            inputCaps.SetMime(OHOS::Media::MEDIA_MIME_VIDEO_AVC);
            break;
        default:
            MEDIA_LOG_I("codec is not supported right now");
            break;
    }
    definition.outCaps.push_back(outputCaps);
}
} // namespace

PLUGIN_DEFINITION(FFmpegVideoEncoders, LicenseType::LGPL, RegisterVideoEncoderPlugins, UnRegisterVideoEncoderPlugins);

namespace OHOS {
namespace Media {
namespace Plugin {
namespace Ffmpeg {
VideoFfmpegEncoderPlugin::VideoFfmpegEncoderPlugin(std::string name)
    : CodecPlugin(std::move(name)), outBufferQ_("vencPluginQueue", BUFFER_QUEUE_SIZE)
{
}

Status VideoFfmpegEncoderPlugin::Init()
{
    OSAL::ScopedLock l(avMutex_);
    auto iter = codecMap.find(pluginName_);
    if (iter == codecMap.end()) {
        MEDIA_LOG_W("cannot find codec with name %s", pluginName_.c_str());
        return Status::ERROR_UNSUPPORTED_FORMAT;
    }
    OSAL::ScopedLock lock(avMutex_);
    avCodec_ = iter->second;
    cachedFrame_ = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame* fp) { av_frame_free(&fp); });
    cachedPacket_ = std::make_shared<AVPacket>();
    vencParams_[Tag::REQUIRED_OUT_BUFFER_CNT] = (uint32_t)BUFFER_QUEUE_SIZE;
    if (!encodeTask_) {
        encodeTask_ = std::make_shared<OHOS::Media::OSAL::Task>("videoFfmpegEncThread");
        encodeTask_->RegisterHandler([this] { ReceiveBuffer(); });
    }
    state_ = State::INITIALIZED;
    MEDIA_LOG_I("Init success");
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::Deinit()
{
    OSAL::ScopedLock l(avMutex_);
    avCodec_.reset();
    cachedFrame_.reset();
    cachedPacket_.reset();
    ResetLocked();
    if (encodeTask_) {
        encodeTask_->Stop();
        encodeTask_.reset();
    }
    state_ = State::DESTROYED;
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::SetParameter(Tag tag, const ValueType& value)
{
    OSAL::ScopedLock l(parameterMutex_);
    vencParams_.insert(std::make_pair(tag, value));
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::GetParameter(Tag tag, ValueType& value)
{
    {
        OSAL::ScopedLock l(parameterMutex_);
        auto res = vencParams_.find(tag);
        if (res != vencParams_.end()) {
            value = res->second;
            return Status::OK;
        }
    }
    OSAL::ScopedLock lock(avMutex_);
    if (avCodecContext_ == nullptr) {
        MEDIA_LOG_E("GetParameter is NULL");
        return Status::ERROR_WRONG_STATE;
    }
    return GetVideoEncoderParameters(*avCodecContext_, tag, value);
}

template <typename T>
void VideoFfmpegEncoderPlugin::FindInParameterMapThenAssignLocked(Tag tag, T& assign)
{
    auto iter = vencParams_.find(tag);
    if (iter != vencParams_.end() && typeid(T) == iter->second.Type()) {
        assign = Plugin::AnyCast<T>(iter->second);
    } else {
        MEDIA_LOG_W("parameter %d is not found or type mismatch", static_cast<int32_t>(tag));
    }
}

Status VideoFfmpegEncoderPlugin::CreateCodecContext()
{
    auto context = avcodec_alloc_context3(avCodec_.get());
    if (context == nullptr) {
        MEDIA_LOG_E("cannot allocate codec context");
        return Status::ERROR_UNKNOWN;
    }
    avCodecContext_ = std::shared_ptr<AVCodecContext>(context, [](AVCodecContext* ptr) {
        if (ptr != nullptr) {
            if (ptr->extradata) {
                av_free(ptr->extradata);
                ptr->extradata = nullptr;
            }
            avcodec_free_context(&ptr);
        }
    });
    MEDIA_LOG_I("Create ffmpeg codec context success");
    return Status::OK;
}

void VideoFfmpegEncoderPlugin::InitCodecContext()
{
    avCodecContext_->codec_type = AVMEDIA_TYPE_VIDEO;
    FindInParameterMapThenAssignLocked<std::uint32_t>(Tag::VIDEO_WIDTH, width_);
    FindInParameterMapThenAssignLocked<std::uint32_t>(Tag::VIDEO_HEIGHT, height_);
    FindInParameterMapThenAssignLocked<uint64_t>(Tag::VIDEO_FRAME_RATE, frameRate_);
    FindInParameterMapThenAssignLocked<Plugin::VideoPixelFormat>(Tag::VIDEO_PIXEL_FORMAT, pixelFormat_);
    MEDIA_LOG_D("width: %u, height: %u, pixelFormat: %u, frameRate_: %" PUBLIC_LOG_U64,
                width_, height_, pixelFormat_, frameRate_);
    ConfigVideoEncoder(*avCodecContext_, vencParams_);
}

void VideoFfmpegEncoderPlugin::DeinitCodecContext()
{
    if (avCodecContext_ == nullptr) {
        return;
    }
    if (avCodecContext_->extradata) {
        av_free(avCodecContext_->extradata);
        avCodecContext_->extradata = nullptr;
    }
    avCodecContext_->extradata_size = 0;
    avCodecContext_->opaque = nullptr;
    avCodecContext_->width = 0;
    avCodecContext_->height = 0;
    avCodecContext_->time_base.den = 0;
    avCodecContext_->time_base.num = 0;
    avCodecContext_->ticks_per_frame = 0;
    avCodecContext_->sample_aspect_ratio.num = 0;
    avCodecContext_->sample_aspect_ratio.den = 0;
    avCodecContext_->get_buffer2 = nullptr;
}

Status VideoFfmpegEncoderPlugin::OpenCodecContext()
{
    AVCodec* venc = avcodec_find_encoder(avCodecContext_->codec_id);
    if (venc == nullptr) {
        MEDIA_LOG_E("Codec: %d is not found", static_cast<int32_t>(avCodecContext_->codec_id));
        DeinitCodecContext();
        return Status::ERROR_INVALID_PARAMETER;
    }
    auto res = avcodec_open2(avCodecContext_.get(), avCodec_.get(), nullptr);
    if (res != 0) {
        MEDIA_LOG_E("avcodec open error %s when start encoder ", AVStrError(res).c_str());
        DeinitCodecContext();
        return Status::ERROR_UNKNOWN;
    }
    MEDIA_LOG_I("Open ffmpeg codec context success");
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::CloseCodecContext()
{
    Status ret = Status::OK;
    if (avCodecContext_ != nullptr) {
        auto res = avcodec_close(avCodecContext_.get());
        if (res != 0) {
            DeinitCodecContext();
            MEDIA_LOG_E("avcodec close error %s when stop encoder", AVStrError(res).c_str());
            ret = Status::ERROR_UNKNOWN;
        }
        avCodecContext_.reset();
    }
    return ret;
}

Status VideoFfmpegEncoderPlugin::Prepare()
{
    {
        OSAL::ScopedLock l(avMutex_);
        if (state_ != State::INITIALIZED && state_ != State::PREPARED) {
            return Status::ERROR_WRONG_STATE;
        }
        if (CreateCodecContext() != Status::OK) {
            MEDIA_LOG_E("Create codec context fail");
            return Status::ERROR_UNKNOWN;
        }
        {
            OSAL::ScopedLock lock(parameterMutex_);
            InitCodecContext();
        }
#ifdef DUMP_RAW_DATA
        dumpData_.open("./enc_out.dat", std::ios::out | std::ios::binary);
#endif
        state_ = State::PREPARED;
    }
    outBufferQ_.SetActive(true);
    MEDIA_LOG_I("Prepare success");
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::ResetLocked()
{
    {
        OSAL::ScopedLock lock(parameterMutex_);
        vencParams_.clear();
    }
    avCodecContext_.reset();
    outBufferQ_.Clear();
#ifdef DUMP_RAW_DATA
    dumpData_.close();
#endif
    state_ = State::INITIALIZED;
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::Reset()
{
    OSAL::ScopedLock l(avMutex_);
    return ResetLocked();
}

Status VideoFfmpegEncoderPlugin::Start()
{
    {
        OSAL::ScopedLock l(avMutex_);
        if (state_ != State::PREPARED) {
            return Status::ERROR_WRONG_STATE;
        }
        if (OpenCodecContext() != Status::OK) {
            MEDIA_LOG_E("Open codec context fail");
            return Status::ERROR_UNKNOWN;
        }
        state_ = State::RUNNING;
    }
    outBufferQ_.SetActive(true);
    encodeTask_->Start();
    MEDIA_LOG_I("Start success");
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::Stop()
{
    Status ret = Status::OK;
    {
        OSAL::ScopedLock l(avMutex_);
        ret = CloseCodecContext();
#ifdef DUMP_RAW_DATA
        dumpData_.close();
#endif
        state_ = State::INITIALIZED;
    }
    outBufferQ_.SetActive(false);
    encodeTask_->Stop();
    MEDIA_LOG_I("Stop success");
    return ret;
}

Status VideoFfmpegEncoderPlugin::QueueOutputBuffer(const std::shared_ptr<Buffer>& outputBuffer, int32_t timeoutMs)
{
    MEDIA_LOG_D("queue output buffer");
    if (outputBuffer) {
        outBufferQ_.Push(outputBuffer);
        return Status::OK;
    }
    return Status::ERROR_INVALID_PARAMETER;
}

Status VideoFfmpegEncoderPlugin::DequeueOutputBuffer(std::shared_ptr<Buffer>& outputBuffers, int32_t timeoutMs)
{
    (void)timeoutMs;
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::Flush()
{
    OSAL::ScopedLock l(avMutex_);
    if (avCodecContext_ != nullptr) {
        // flush avcodec buffers
    }
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::QueueInputBuffer(const std::shared_ptr<Buffer>& inputBuffer, int32_t timeoutMs)
{
    MEDIA_LOG_D("queue input buffer");
    if (inputBuffer->IsEmpty() && !(inputBuffer->flag & BUFFER_FLAG_EOS)) {
        MEDIA_LOG_E("encoder does not support fd buffer");
        return Status::ERROR_INVALID_DATA;
    }
    Status ret = Status::OK;
    {
        OSAL::ScopedLock l(avMutex_);
        ret = SendBufferLocked(inputBuffer);
    }
    NotifyInputBufferDone(inputBuffer);
    return ret;
}

Status VideoFfmpegEncoderPlugin::DequeueInputBuffer(std::shared_ptr<Buffer>& inputBuffer, int32_t timeoutMs)
{
    (void)timeoutMs;
    return Status::OK;
}

#ifdef DUMP_RAW_DATA
void VideoFfmpegEncoderPlugin::DumpVideoRawOutData()
{
    if (cachedFrame_->format == AV_PIX_FMT_YUV420P) {
        if (cachedFrame_->data[0] != nullptr && cachedFrame_->linesize[0] != 0) {
            dumpData_.write((char*)cachedFrame_->data[0], cachedFrame_->linesize[0] * cachedFrame_->height);
        }
        if (cachedFrame_->data[1] != nullptr && cachedFrame_->linesize[1] != 0) {
            dumpData_.write((char*)cachedFrame_->data[1], cachedFrame_->linesize[1] * cachedFrame_->height / 2); // 2
        }
        if (cachedFrame_->data[2] != nullptr && cachedFrame_->linesize[2] != 0) {                                // 2
            dumpData_.write((char*)cachedFrame_->data[2], cachedFrame_->linesize[2] * cachedFrame_->height / 2); // 2
        }
    } else if (cachedFrame_->format == AV_PIX_FMT_NV12 || cachedFrame_->format == AV_PIX_FMT_NV21) {
        if (cachedFrame_->data[0] != nullptr && cachedFrame_->linesize[0] != 0) {
            dumpData_.write((char*)cachedFrame_->data[0], cachedFrame_->linesize[0] * cachedFrame_->height);
        }
        if (cachedFrame_->data[1] != nullptr && cachedFrame_->linesize[1] != 0) {
            dumpData_.write((char*)cachedFrame_->data[1], cachedFrame_->linesize[1] * cachedFrame_->height / 2); // 2
        }
    }
}
#endif

Status VideoFfmpegEncoderPlugin::SendBufferLocked(const std::shared_ptr<Buffer>& inputBuffer)
{
    if (state_ != State::RUNNING) {
        MEDIA_LOG_W("queue input buffer in wrong state");
        return Status::ERROR_WRONG_STATE;
    }
    bool isEos = false;
    if (inputBuffer == nullptr || (inputBuffer->flag & BUFFER_FLAG_EOS) != 0) {
        isEos = true;
    } else {
        auto inputMemory = inputBuffer->GetMemory();
        const uint8_t *data = inputMemory->GetReadOnlyData();
        auto bufferMeta = inputBuffer->GetBufferMeta();
        if (bufferMeta != nullptr && bufferMeta->GetType() == BufferMetaType::VIDEO) {
            std::shared_ptr<VideoBufferMeta> videoMeta = std::dynamic_pointer_cast<VideoBufferMeta>(bufferMeta);
            if (pixelFormat_ != videoMeta->videoPixelFormat) {
                MEDIA_LOG_E("pixel format change");
            }
            // FIXME: if width/height of input frame have change after start, need to re-configure encoder
            cachedFrame_->format = ConvertPixelFormatToFFmpeg(videoMeta->videoPixelFormat);
            cachedFrame_->width = videoMeta->width;
            cachedFrame_->height = videoMeta->height;
            if (!videoMeta->stride.empty()) {
                for (auto i = 0; i < videoMeta->planes; i++) {
                    cachedFrame_->linesize[i] = videoMeta->stride[i];
                }
            }
            int32_t ySize = cachedFrame_->linesize[0] * AlignUp(cachedFrame_->height, DEFAULT_ALIGN);
            // AV_PIX_FMT_YUV420P: linesize[0] = linesize[1] * 2, AV_PIX_FMT_NV12: linesize[0] = linesize[1]
            int32_t uvSize = cachedFrame_->linesize[1] * AlignUp(cachedFrame_->height, DEFAULT_ALIGN) / 2; // 2
            if (cachedFrame_->format == AV_PIX_FMT_YUV420P) {
                cachedFrame_->data[0] = const_cast<uint8_t *>(data);
                cachedFrame_->data[1] = cachedFrame_->data[0] + ySize;
                cachedFrame_->data[2] = cachedFrame_->data[1] + uvSize;
            } else if ((cachedFrame_->format == AV_PIX_FMT_NV12) || (cachedFrame_->format == AV_PIX_FMT_NV21)) {
                cachedFrame_->data[0] = const_cast<uint8_t *>(data);
                cachedFrame_->data[1] = cachedFrame_->data[0] + ySize;
            } else {
                MEDIA_LOG_E("Unsupported pixel format: %d", cachedFrame_->format);
                return Status::ERROR_UNSUPPORTED_FORMAT;
            }
#ifdef DUMP_RAW_DATA
            DumpVideoRawOutData();
#endif
            AVRational bq = {1, HST_SECOND};
            cachedFrame_->pts = ConvertTimeToFFmpeg(
                    static_cast<uint64_t>(inputBuffer->pts) / avCodecContext_->ticks_per_frame,
                    avCodecContext_->time_base);
        }
    }
    AVFrame *frame = nullptr;
    if (!isEos) {
        frame = cachedFrame_.get();
    }
    auto ret = avcodec_send_frame(avCodecContext_.get(), frame);
    if (ret < 0) {
        MEDIA_LOG_D("send buffer error %s", AVStrError(ret).c_str());
        if (ret == AVERROR_EOF) {
            return Status::END_OF_STREAM;
        }
        return Status::ERROR_NO_MEMORY;
    }
    if (frame) {
        av_frame_unref(cachedFrame_.get());
    }
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::FillFrameBuffer(const std::shared_ptr<Buffer>& packetBuffer)
{
    if (cachedPacket_->data == nullptr) {
        MEDIA_LOG_E("avcodec_receive_packet() packet data is empty");
        return Status::ERROR_UNKNOWN;
    }
    auto frameBufferMem = packetBuffer->GetMemory();
    if (frameBufferMem->Write(cachedPacket_->data, cachedPacket_->size, 0) != cachedPacket_->size) {
        MEDIA_LOG_E("copy packet data to buffer fail");
        return Status::ERROR_UNKNOWN;
    }
    if (cachedPacket_->flags & AV_PKT_FLAG_KEY) {
        MEDIA_LOG_D("It is key frame");
    }
    packetBuffer->pts =
            static_cast<uint64_t>(ConvertTimeFromFFmpeg(cachedPacket_->pts, avCodecContext_->time_base));
    packetBuffer->dts =
            static_cast<uint64_t>(ConvertTimeFromFFmpeg(cachedPacket_->dts, avCodecContext_->time_base));
    return Status::OK;
}

Status VideoFfmpegEncoderPlugin::ReceiveBufferLocked(const std::shared_ptr<Buffer>& packetBuffer)
{
    if (state_ != State::RUNNING) {
        MEDIA_LOG_W("queue input buffer in wrong state");
        return Status::ERROR_WRONG_STATE;
    }
    Status status;
    auto ret = avcodec_receive_packet(avCodecContext_.get(), cachedPacket_.get());
    if (ret >= 0) {
        status = FillFrameBuffer(packetBuffer);
    } else if (ret == AVERROR_EOF) {
        MEDIA_LOG_I("eos received");
        packetBuffer->GetMemory()->Reset();
        avcodec_flush_buffers(avCodecContext_.get());
        status = Status::END_OF_STREAM;
    } else {
        MEDIA_LOG_D("video encoder receive error: %s", AVStrError(ret).c_str());
        status = Status::ERROR_TIMED_OUT;
    }
    av_frame_unref(cachedFrame_.get());
    return status;
}

void VideoFfmpegEncoderPlugin::ReceiveBuffer()
{
    std::shared_ptr<Buffer> packetBuffer = outBufferQ_.Pop();
    if (packetBuffer == nullptr || packetBuffer->IsEmpty() ||
            packetBuffer->GetBufferMeta()->GetType() != BufferMetaType::VIDEO) {
        MEDIA_LOG_W("cannot fetch valid buffer to output");
        return;
    }
    Status status;
    {
        OSAL::ScopedLock l(avMutex_);
        status = ReceiveBufferLocked(packetBuffer);
    }
    if (status == Status::OK || status == Status::END_OF_STREAM) {
        NotifyOutputBufferDone(packetBuffer);
    } else {
        outBufferQ_.Push(packetBuffer);
    }
}

void VideoFfmpegEncoderPlugin::NotifyInputBufferDone(const std::shared_ptr<Buffer>& input)
{
    if (dataCb_ != nullptr) {
        dataCb_->OnInputBufferDone(const_cast<std::shared_ptr<Buffer>&>(input));
    }
}

void VideoFfmpegEncoderPlugin::NotifyOutputBufferDone(const std::shared_ptr<Buffer>& output)
{
    if (dataCb_ != nullptr) {
        dataCb_->OnOutputBufferDone(const_cast<std::shared_ptr<Buffer>&>(output));
    }
}

std::shared_ptr<Allocator> VideoFfmpegEncoderPlugin::GetAllocator()
{
    return nullptr;
}
} // namespace Ffmpeg
} // namespace Plugin
} // namespace Media
} // namespace OHOS
#endif

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

#define HST_LOG_TAG "HiPlayerImpl"

#include "hiplayer_impl.h"
#include "foundation/log.h"
#include "pipeline/factory/filter_factory.h"
#include "plugin/common/plugin_time.h"
#include "plugin/core/plugin_meta.h"
#include "utils/steady_clock.h"
#include "utils/utils.h"

namespace {
const float MAX_MEDIA_VOLUME = 100.0f;
}

namespace OHOS {
namespace Media {
using namespace Pipeline;

HiPlayerImpl::HiPlayerImpl()
    : fsm_(*this),
      curFsmState_(StateId::INIT),
      pipelineStates_(PlayerStates::PLAYER_IDLE),
      volume_(-1.0f),
      errorCode_(ErrorCode::SUCCESS),
      mediaStats_()
{
    MEDIA_LOG_I("hiPlayerImpl ctor");
    FilterFactory::Instance().Init();

    audioSource_ =
        FilterFactory::Instance().CreateFilterWithType<MediaSourceFilter>("builtin.player.mediasource", "mediaSource");

#ifdef UNIT_TEST
    demuxer_ = FilterFactory::Instance().CreateFilterWithType<DemuxerFilter>("builtin.player.demuxer", "demuxer");
    audioDecoder_ = FilterFactory::Instance().CreateFilterWithType<AudioDecoderFilter>("builtin.player.audiodecoder",
                                                                                       "audiodecoder");
    audioSink_ =
        FilterFactory::Instance().CreateFilterWithType<AudioSinkFilter>("builtin.player.audiosink", "audiosink");
#else
    demuxer_ = FilterFactory::Instance().CreateFilterWithType<DemuxerFilter>("builtin.player.demuxer", "demuxer");
    audioSink_ =
        FilterFactory::Instance().CreateFilterWithType<AudioSinkFilter>("builtin.player.audiosink", "audioSink");
#ifdef VIDEO_SUPPORT
    videoSink =
        FilterFactory::Instance().CreateFilterWithType<VideoSinkFilter>("builtin.player.videosink", "videoSink");
    FALSE_RETURN(videoSink != nullptr);
#endif
#endif
    FALSE_RETURN(audioSource_ != nullptr);
    FALSE_RETURN(demuxer_ != nullptr);
    FALSE_RETURN(audioSink_ != nullptr);

    pipeline_ = std::make_shared<PipelineCore>();
}

HiPlayerImpl::~HiPlayerImpl()
{
    fsm_.Stop();
    MEDIA_LOG_D("dtor called.");
}

std::shared_ptr<HiPlayerImpl> HiPlayerImpl::CreateHiPlayerImpl()
{
    return std::shared_ptr<HiPlayerImpl>(new (std::nothrow) HiPlayerImpl());
}

int32_t HiPlayerImpl::Init()
{
    errorCode_ = ErrorCode::SUCCESS;
    mediaStats_.Reset();
    if (initialized_.load()) {
        return to_underlying(ErrorCode::SUCCESS);
    }
    pipeline_->Init(this, this);
    ErrorCode ret = pipeline_->AddFilters({audioSource_.get(), demuxer_.get()});
    if (ret == ErrorCode::SUCCESS) {
        ret = pipeline_->LinkFilters({audioSource_.get(), demuxer_.get()});
    }

    if (ret == ErrorCode::SUCCESS) {
        pipelineStates_ = PlayerStates::PLAYER_INITIALIZED;
        fsm_.SetStateCallback(this);
        fsm_.Start();
        initialized_ = true;
    } else {
        pipeline_->UnlinkPrevFilters();
        pipeline_->RemoveFilterChain(audioSource_.get());
        pipelineStates_ = PLAYER_STATE_ERROR;
    }
    return to_underlying(ret);
}
int32_t HiPlayerImpl::SetSource(const Source& source)
{
    PROFILE_BEGIN("SetSource begin");
    auto ret = Init();
    if (ret != to_underlying(ErrorCode::SUCCESS)) {
        return ret;
    }
    ret = to_underlying(fsm_.SendEvent(Intent::SET_SOURCE, std::make_shared<MediaSource>(source.GetSourceUri())));
    PROFILE_END("SetSource end.");
    return ret;
}

int32_t HiPlayerImpl::Prepare()
{
    MEDIA_LOG_D("Prepare entered, current fsm state: %" PUBLIC_LOG "s.", fsm_.GetCurrentState().c_str());
    OSAL::ScopedLock lock(stateMutex_);
    PROFILE_BEGIN();
    if (curFsmState_ == StateId::PREPARING) {
        errorCode_ = ErrorCode::SUCCESS;
        cond_.Wait(lock, [this] { return curFsmState_ == StateId::READY || curFsmState_ == StateId::INIT; });
    }
    MEDIA_LOG_D("Prepare finished, current fsm state: %" PUBLIC_LOG "s.", fsm_.GetCurrentState().c_str());
    if (curFsmState_ == StateId::READY) {
        PROFILE_END("Prepare successfully,");
        return to_underlying(ErrorCode::SUCCESS);
    } else if (curFsmState_ == StateId::INIT) {
        PROFILE_END("Prepare failed,");
        if (errorCode_ == ErrorCode::SUCCESS) {
            errorCode_ = ErrorCode::ERROR_INVALID_STATE;
        }
        return to_underlying(errorCode_.load());
    } else {
        return to_underlying(ErrorCode::ERROR_INVALID_OPERATION);
    }
}

PFilter HiPlayerImpl::CreateAudioDecoder(const std::string& desc)
{
    if (!audioDecoderMap_[desc]) {
        audioDecoderMap_[desc] = FilterFactory::Instance().CreateFilterWithType<AudioDecoderFilter>(
            "builtin.player.audiodecoder", "audiodecoder-" + desc);
        // set parameters to decoder.
    }
    return audioDecoderMap_[desc];
}

int32_t HiPlayerImpl::Play()
{
    PROFILE_BEGIN();
    ErrorCode ret;
    if (pipelineStates_ == PlayerStates::PLAYER_PAUSED) {
        ret = fsm_.SendEvent(Intent::RESUME);
    } else {
        ret = fsm_.SendEvent(Intent::PLAY);
    }
    PROFILE_END("Play ret = %" PUBLIC_LOG "d", to_underlying(ret));
    return to_underlying(ret);
}

bool HiPlayerImpl::IsPlaying()
{
    return pipelineStates_ == PlayerStates::PLAYER_STARTED;
}

int32_t HiPlayerImpl::Pause()
{
    PROFILE_BEGIN();
    auto ret = to_underlying(fsm_.SendEvent(Intent::PAUSE));
    PROFILE_END("Pause ret = %" PUBLIC_LOG "d", ret);
    return ret;
}

ErrorCode HiPlayerImpl::Resume()
{
    PROFILE_BEGIN();
    auto ret = fsm_.SendEvent(Intent::RESUME);
    PROFILE_END("Resume ret = %" PUBLIC_LOG "d", ret);
    return ret;
}

int32_t HiPlayerImpl::Stop()
{
    PROFILE_BEGIN();
    auto ret = to_underlying(fsm_.SendEvent(Intent::STOP));
    PROFILE_END("Stop ret = %" PUBLIC_LOG "d", ret);
    return ret;
}

void HiPlayerImpl::MediaStats::Reset()
{
    mediaStats.clear();
}

void HiPlayerImpl::MediaStats::Append(MediaType mediaType)
{
    for (auto& stat : mediaStats) {
        if (stat.mediaType == mediaType) {
            return;
        }
    }
    mediaStats.emplace_back(mediaType);
}

void HiPlayerImpl::MediaStats::ReceiveEvent(EventType eventType, int64_t param)
{
    switch (eventType) {
        case EventType::EVENT_COMPLETE:
            for (auto& stat : mediaStats) {
                if (stat.mediaType == MediaType::AUDIO) {
                    stat.completeEventReceived = true;
                    break;
                }
            }
            break;
        case EventType::EVENT_AUDIO_PROGRESS:
            for (auto& stat : mediaStats) {
                if (stat.mediaType == MediaType::AUDIO) {
                    stat.currentPositionMs = param;
                    break;
                }
            }
            break;
        default:
            MEDIA_LOG_W("MediaStats::ReceiveEvent receive unexpected event %" PUBLIC_LOG "d",
                        static_cast<int>(eventType));
            break;
    }
}

int64_t HiPlayerImpl::MediaStats::GetCurrentPosition()
{
    int64_t currentPosition = 0;
    for (const auto& stat : mediaStats) {
        currentPosition = std::max(currentPosition, stat.currentPositionMs.load());
    }
    return currentPosition;
}

bool HiPlayerImpl::MediaStats::IsEventCompleteAllReceived()
{
    return std::all_of(mediaStats.begin(), mediaStats.end(),
                       [](const MediaStat& stat) { return stat.completeEventReceived.load(); });
}

ErrorCode HiPlayerImpl::StopAsync()
{
    return fsm_.SendEventAsync(Intent::STOP);
}

int32_t HiPlayerImpl::Rewind(int64_t mSeconds, int32_t mode)
{
    int64_t hstTime = 0;
    if (!Plugin::Ms2HstTime(mSeconds, hstTime)) {
        return to_underlying(ErrorCode::ERROR_INVALID_PARAMETER_VALUE);
    }
    return to_underlying(fsm_.SendEventAsync(Intent::SEEK, hstTime));
}

int32_t HiPlayerImpl::SetVolume(float leftVolume, float rightVolume)
{
    if (leftVolume < 0 || leftVolume > MAX_MEDIA_VOLUME || rightVolume < 0 || rightVolume > MAX_MEDIA_VOLUME) {
        MEDIA_LOG_E("volume not valid, should be in range [0,100]");
        return to_underlying(ErrorCode::ERROR_INVALID_PARAMETER_VALUE);
    }
    if (leftVolume < 1e-6 && rightVolume >= 1e-6) { // 1e-6
        volume_ = rightVolume;
    } else if (rightVolume < 1e-6 && leftVolume >= 1e-6) { // 1e-6
        volume_ = leftVolume;
    } else {
        volume_ = (leftVolume + rightVolume) / 2; // 2
    }
    volume_ /= MAX_MEDIA_VOLUME; // normalize to 0~1
    PlayerStates states = pipelineStates_.load();
    if (states != Media::PlayerStates::PLAYER_STARTED) {
        return to_underlying(ErrorCode::SUCCESS);
    }
    return to_underlying(SetVolume(volume_));
}

#ifndef SURFACE_DISABLED
int32_t HiPlayerImpl::SetSurface(Surface* surface)
{
    return to_underlying(ErrorCode::ERROR_UNIMPLEMENTED);
}
#endif
ErrorCode HiPlayerImpl::SetBufferSize(size_t size)
{
    return audioSource_->SetBufferSize(size);
}

void HiPlayerImpl::OnEvent(const Event& event)
{
    MEDIA_LOG_D("[HiStreamer] OnEvent (%" PUBLIC_LOG "d)", event.type);
    switch (event.type) {
        case EventType::EVENT_ERROR: {
            fsm_.SendEventAsync(Intent::NOTIFY_ERROR, event.param);
            break;
        }
        case EventType::EVENT_READY:
            fsm_.SendEventAsync(Intent::NOTIFY_READY);
            break;
        case EventType::EVENT_COMPLETE:
            mediaStats_.ReceiveEvent(EventType::EVENT_COMPLETE, 0);
            if (mediaStats_.IsEventCompleteAllReceived()) {
                fsm_.SendEventAsync(Intent::NOTIFY_COMPLETE);
            }
            break;
        case EventType::EVENT_AUDIO_PROGRESS:
            mediaStats_.ReceiveEvent(EventType::EVENT_AUDIO_PROGRESS, Plugin::AnyCast<int64_t>(event.param));
            break;
        case EventType::EVENT_PLUGIN_ERROR: {
            Plugin::PluginEvent pluginEvent = Plugin::AnyCast<Plugin::PluginEvent>(event.param);
            MEDIA_LOG_I("Receive PLUGIN_ERROR, type:  %" PUBLIC_LOG_D32, to_underlying(pluginEvent.type));
            int32_t errorCode {-1};
            if (pluginEvent.type == Plugin::PluginEventType::CLIENT_ERROR &&
                pluginEvent.param.SameTypeWith(typeid(Plugin::NetworkClientErrorCode))&&
                Plugin::AnyCast<Plugin::NetworkClientErrorCode>(pluginEvent.param)
                == Plugin::NetworkClientErrorCode::ERROR_TIME_OUT) {
                errorCode = to_underlying(Plugin::NetworkClientErrorCode::ERROR_TIME_OUT);
            }
            auto ptr = callback_.lock();
            if (ptr != nullptr) {
                ptr->OnError(PlayerCallback::PlayerErrorType::PLAYER_ERROR_UNKNOWN, errorCode);
            }
            break;
        }
        case EventType::EVENT_PLUGIN_EVENT: {
            Plugin::PluginEvent pluginEvent = Plugin::AnyCast<Plugin::PluginEvent>(event.param);
            if (pluginEvent.type == Plugin::PluginEventType::BELOW_LOW_WATERLINE ||
                pluginEvent.type == Plugin::PluginEventType::ABOVE_LOW_WATERLINE) {
                MEDIA_LOG_I("Receive PLUGIN_EVENT, type:  %" PUBLIC_LOG_D32, to_underlying(pluginEvent.type));
            }
            break;
        }
        default:
            MEDIA_LOG_E("Unknown event(%" PUBLIC_LOG "d)", event.type);
    }
}

ErrorCode HiPlayerImpl::DoSetSource(const std::shared_ptr<MediaSource>& source) const
{
    return audioSource_->SetSource(source);
}

ErrorCode HiPlayerImpl::PrepareFilters()
{
    auto ret = pipeline_->Prepare();
    if (ret == ErrorCode::SUCCESS) {
        pipelineStates_ = PlayerStates::PLAYER_PREPARED;
    }
    return ret;
}

ErrorCode HiPlayerImpl::DoPlay()
{
    (void)SetVolume(volume_);
    auto ret = pipeline_->Start();
    if (ret == ErrorCode::SUCCESS) {
        pipelineStates_ = PlayerStates::PLAYER_STARTED;
    }
    return ret;
}

ErrorCode HiPlayerImpl::DoPause()
{
    auto ret = pipeline_->Pause();
    if (ret == ErrorCode::SUCCESS) {
        pipelineStates_ = PlayerStates::PLAYER_PAUSED;
    }
    return ret;
}

ErrorCode HiPlayerImpl::DoResume()
{
    (void)SetVolume(volume_);
    auto ret = pipeline_->Resume();
    if (ret == ErrorCode::SUCCESS) {
        pipelineStates_ = PlayerStates::PLAYER_STARTED;
    }
    return ret;
}

ErrorCode HiPlayerImpl::DoStop()
{
    mediaStats_.Reset();
    auto ret = pipeline_->Stop();
    if (ret == ErrorCode::SUCCESS) {
        pipelineStates_ = PlayerStates::PLAYER_STOPPED;
    }
    return ret;
}

ErrorCode HiPlayerImpl::DoSeek(bool allowed, int64_t hstTime)
{
    PROFILE_BEGIN();
    auto rtv = allowed && hstTime >= 0 ? ErrorCode::SUCCESS : ErrorCode::ERROR_INVALID_OPERATION;
    if (rtv == ErrorCode::SUCCESS) {
        pipeline_->FlushStart();
        PROFILE_END("Flush start");
        PROFILE_RESET();
        pipeline_->FlushEnd();
        PROFILE_END("Flush end");
        PROFILE_RESET();
        rtv = demuxer_->SeekTo(hstTime);
        if (rtv == ErrorCode::SUCCESS) {
            mediaStats_.ReceiveEvent(EventType::EVENT_AUDIO_PROGRESS, hstTime);
            mediaStats_.ReceiveEvent(EventType::EVENT_VIDEO_PROGRESS, hstTime);
        }
        PROFILE_END("SeekTo");
    }
    auto ptr = callback_.lock();
    if (ptr != nullptr) {
        if (rtv != ErrorCode::SUCCESS) {
            ptr->OnError(to_underlying(PlayerErrorTypeExt::SEEK_ERROR), to_underlying(rtv));
        } else {
            ptr->OnRewindToComplete();
        }
    }
    return rtv;
}

ErrorCode HiPlayerImpl::DoOnReady()
{
    pipelineStates_ = PlayerStates::PLAYER_PREPARED;
    sourceMeta_ = demuxer_->GetGlobalMetaInfo();
    streamMeta_.clear();
    for (auto& streamMeta : demuxer_->GetStreamMetaInfo()) {
        streamMeta_.push_back(streamMeta);
    }
    return ErrorCode::SUCCESS;
}

ErrorCode HiPlayerImpl::DoOnComplete()
{
    MEDIA_LOG_W("OnComplete looping: %" PUBLIC_LOG "d.", singleLoop_.load());
    if (!singleLoop_) {
        StopAsync();
    } else {
        fsm_.SendEventAsync(Intent::SEEK, static_cast<int64_t>(0));
    }
    auto ptr = callback_.lock();
    if (ptr != nullptr) {
        ptr->OnPlaybackComplete();
    }
    return ErrorCode::SUCCESS;
}

ErrorCode HiPlayerImpl::DoOnError(ErrorCode errorCode)
{
    errorCode_ = errorCode;
    auto ptr = callback_.lock();
    if (ptr != nullptr) {
        ptr->OnError(PlayerCallback::PLAYER_ERROR_UNKNOWN, static_cast<int32_t>(errorCode));
    }
    pipelineStates_ = PlayerStates::PLAYER_STATE_ERROR;
    return ErrorCode::SUCCESS;
}

bool HiPlayerImpl::IsSingleLooping()
{
    return singleLoop_.load();
}

int32_t HiPlayerImpl::SetLoop(bool loop)
{
    singleLoop_ = loop;
    return to_underlying(ErrorCode::SUCCESS);
}

void HiPlayerImpl::SetPlayerCallback(const std::shared_ptr<PlayerCallback>& cb)
{
    callback_ = cb;
}

int32_t HiPlayerImpl::Reset()
{
    Stop();
    pipelineStates_ = PlayerStates::PLAYER_IDLE;
    singleLoop_ = false;
    mediaStats_.Reset();
    return to_underlying(ErrorCode::SUCCESS);
}

int32_t HiPlayerImpl::Release()
{
    PROFILE_BEGIN();
    auto ret = Reset();
    fsm_.Stop();
    pipeline_.reset();
    audioSource_.reset();
    demuxer_.reset();
    audioDecoderMap_.clear();
    audioSink_.reset();
    PROFILE_END("Release ret = %" PUBLIC_LOG "d", ret);
    return ret;
}

int32_t HiPlayerImpl::DeInit()
{
    return Reset();
}

int32_t HiPlayerImpl::GetPlayerState(int32_t& state)
{
    state = static_cast<int32_t>(pipelineStates_.load());
    return to_underlying(ErrorCode::SUCCESS);
}

int32_t HiPlayerImpl::GetCurrentPosition(int64_t& currentPositionMs)
{
    currentPositionMs = Plugin::HstTime2Ms(mediaStats_.GetCurrentPosition());
    return to_underlying(ErrorCode::SUCCESS);
}

int32_t HiPlayerImpl::GetDuration(int64_t& outDurationMs)
{
    if (audioSource_ && !audioSource_->IsSeekable()) {
        outDurationMs = -1;
        return to_underlying(ErrorCode::SUCCESS);
    }
    uint64_t duration = 0;
    auto sourceMeta = sourceMeta_.lock();
    if (sourceMeta == nullptr) {
        outDurationMs = 0;
        return to_underlying(ErrorCode::ERROR_AGAIN);
    }
    if (sourceMeta->GetUint64(Media::Plugin::MetaID::MEDIA_DURATION, duration)) {
        outDurationMs = Plugin::HstTime2Ms(duration);
        return to_underlying(ErrorCode::SUCCESS);
    }
    // use max stream duration as whole source duration if source meta does not contains the duration meta
    uint64_t tmp = 0;
    bool found = false;
    for (const auto& streamMeta : streamMeta_) {
        auto ptr = streamMeta.lock();
        if (ptr != nullptr && ptr->GetUint64(Media::Plugin::MetaID::MEDIA_DURATION, tmp)) {
            found = true;
            duration = std::max(duration, tmp);
        }
    }
    if (found) {
        outDurationMs = Plugin::HstTime2Ms(duration);
        return to_underlying(ErrorCode::SUCCESS);
    }
    return to_underlying(ErrorCode::ERROR_AGAIN);
}

ErrorCode HiPlayerImpl::SetVolume(float volume)
{
    if (audioSink_ == nullptr) {
        MEDIA_LOG_W("cannot set volume while audio sink filter is null");
        return ErrorCode::ERROR_AGAIN;
    }
    ErrorCode ret = ErrorCode::SUCCESS;
    if (volume > 0) {
        MEDIA_LOG_I("set volume %" PUBLIC_LOG ".3f", volume);
        ret = audioSink_->SetVolume(volume);
    }
    if (ret != ErrorCode::SUCCESS) {
        MEDIA_LOG_E("SetVolume failed with error %" PUBLIC_LOG "d", static_cast<int>(ret));
    }
    return ret;
}

void HiPlayerImpl::OnStateChanged(StateId state)
{
    OSAL::ScopedLock lock(stateMutex_);
    curFsmState_ = state;
    cond_.NotifyOne();
}

ErrorCode HiPlayerImpl::OnCallback(const FilterCallbackType& type, Filter* filter, const Plugin::Any& parameter)
{
    ErrorCode ret = ErrorCode::SUCCESS;
    switch (type) {
        case FilterCallbackType::PORT_ADDED:
            ret = NewAudioPortFound(filter, parameter);
#ifdef VIDEO_SUPPORT
            ret = NewVideoPortFound(filter, parameter);
#endif
            break;
        case FilterCallbackType::PORT_REMOVE:
            ret = RemoveFilterChains(filter, parameter);
            break;
        default:
            break;
    }
    return ret;
}

ErrorCode HiPlayerImpl::GetTrackCnt(size_t& cnt) const
{
    cnt = streamMeta_.size();
    return ErrorCode::SUCCESS;
}

ErrorCode HiPlayerImpl::GetSourceMeta(shared_ptr<const Plugin::Meta>& meta) const
{
    meta = sourceMeta_.lock();
    return meta ? ErrorCode::SUCCESS : ErrorCode::ERROR_AGAIN;
}

ErrorCode HiPlayerImpl::GetTrackMeta(size_t id, shared_ptr<const Plugin::Meta>& meta) const
{
    if (id > streamMeta_.size() || id < 0) {
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    meta = streamMeta_[id].lock();
    if (meta == nullptr) {
        return ErrorCode::ERROR_AGAIN;
    }
    return ErrorCode::SUCCESS;
}

ErrorCode HiPlayerImpl::NewAudioPortFound(Filter* filter, const Plugin::Any& parameter)
{
    if (!parameter.SameTypeWith(typeid(PortInfo))) {
        return ErrorCode::ERROR_INVALID_PARAMETER_TYPE;
    }
    ErrorCode rtv = ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    auto param = Plugin::AnyCast<PortInfo>(parameter);
    if (filter == demuxer_.get() && param.type == PortType::OUT) {
        MEDIA_LOG_I("new port found on demuxer %" PUBLIC_LOG "zu", param.ports.size());
        for (const auto& portDesc : param.ports) {
            if (!StringStartsWith(portDesc.name, "audio")) {
                continue;
            }
            MEDIA_LOG_I("port name %" PUBLIC_LOG "s", portDesc.name.c_str());
            auto fromPort = filter->GetOutPort(portDesc.name);
            if (portDesc.isPcm) {
                pipeline_->AddFilters({audioSink_.get()});
                FAIL_LOG(pipeline_->LinkPorts(fromPort, audioSink_->GetInPort(PORT_NAME_DEFAULT)));
                ActiveFilters({audioSink_.get()});
            } else {
                auto newAudioDecoder = CreateAudioDecoder(portDesc.name);
                pipeline_->AddFilters({newAudioDecoder.get(), audioSink_.get()});
                FAIL_LOG(pipeline_->LinkPorts(fromPort, newAudioDecoder->GetInPort(PORT_NAME_DEFAULT)));
                FAIL_LOG(pipeline_->LinkPorts(newAudioDecoder->GetOutPort(PORT_NAME_DEFAULT),
                                              audioSink_->GetInPort(PORT_NAME_DEFAULT)));
                ActiveFilters({newAudioDecoder.get(), audioSink_.get()});
            }
            mediaStats_.Append(MediaType::AUDIO);
            rtv = ErrorCode::SUCCESS;
            break;
        }
    }
    return rtv;
}

#ifdef VIDEO_SUPPORT
ErrorCode HiPlayerImpl::NewVideoPortFound(Filter* filter, const Plugin::Any& parameter)
{
    if (!parameter.SameTypeWith(typeid(PortInfo))) {
        return ErrorCode::ERROR_INVALID_PARAMETER_TYPE;
    }
    auto param = Plugin::AnyCast<PortInfo>(parameter);
    if (filter != demuxer_.get() || param.type != PortType::OUT) {
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    std::vector<Filter*> newFilters;
    for (const auto& portDesc : param.ports) {
        if (StringStartsWith(portDesc.name, "video")) {
            MEDIA_LOG_I("port name %" PUBLIC_LOG "s", portDesc.name.c_str());
            videoDecoder = FilterFactory::Instance().CreateFilterWithType<VideoDecoderFilter>(
                "builtin.player.videodecoder", "videodecoder-" + portDesc.name);
            if (pipeline_->AddFilters({videoDecoder.get()}) == ErrorCode::SUCCESS) {
                // link demuxer and video decoder
                auto fromPort = filter->GetOutPort(portDesc.name);
                auto toPort = videoDecoder->GetInPort(PORT_NAME_DEFAULT);
                FAIL_LOG(pipeline_->LinkPorts(fromPort, toPort)); // link ports
                newFilters.emplace_back(videoDecoder.get());

                // link video decoder and video sink
                if (pipeline_->AddFilters({videoSink.get()}) == ErrorCode::SUCCESS) {
                    fromPort = videoDecoder->GetOutPort(PORT_NAME_DEFAULT);
                    toPort = videoSink->GetInPort(PORT_NAME_DEFAULT);
                    FAIL_LOG(pipeline_->LinkPorts(fromPort, toPort)); // link ports
                    newFilters.push_back(videoSink.get());
                }
            }
        }
        break;
    }
    if (!newFilters.empty()) {
        ActiveFilters(newFilters);
    }
    return ErrorCode::SUCCESS;
}
#endif

ErrorCode HiPlayerImpl::RemoveFilterChains(Filter* filter, const Plugin::Any& parameter)
{
    ErrorCode ret = ErrorCode::SUCCESS;
    auto param = Plugin::AnyCast<PortInfo>(parameter);
    if (filter != demuxer_.get() || param.type != PortType::OUT) {
        return ret;
    }
    for (const auto& portDesc : param.ports) {
        MEDIA_LOG_I("remove filter chain for port: %" PUBLIC_LOG "s", portDesc.name.c_str());
        auto peerPort = filter->GetOutPort(portDesc.name)->GetPeerPort();
        if (peerPort) {
            auto nextFilter = const_cast<Filter*>(dynamic_cast<const Filter*>(peerPort->GetOwnerFilter()));
            if (nextFilter) {
                pipeline_->RemoveFilterChain(nextFilter);
            }
        }
    }
    return ret;
}

void HiPlayerImpl::ActiveFilters(const std::vector<Filter*>& filters)
{
    for (auto it = filters.rbegin(); it != filters.rend(); ++it) {
        (*it)->Prepare();
    }
}
} // namespace Media
} // namespace OHOS

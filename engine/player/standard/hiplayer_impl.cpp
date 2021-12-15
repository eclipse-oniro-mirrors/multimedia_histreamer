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
#include "player/standard/media_utils.h"
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
    pipeline_.reset();
    audioSource_.reset();
    demuxer_.reset();
    audioDecoderMap_.clear();
    audioSink_.reset();
    fsm_.Stop();
    MEDIA_LOG_D("dtor called.");
}

ErrorCode HiPlayerImpl::Init()
{
    errorCode_ = ErrorCode::SUCCESS;
    mediaStats_.Reset();
    if (initialized_.load()) {
        return ErrorCode::SUCCESS;
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
    return ret;
}

int32_t HiPlayerImpl::SetSource(const std::string& uri)
{
    PROFILE_BEGIN("SetSource begin");
    auto ret = Init();
    if (ret == ErrorCode::SUCCESS) {
        ret = fsm_.SendEvent(Intent::SET_SOURCE, std::make_shared<Media::Source>(uri));
    }
    PROFILE_END("SetSource end.");
    return TransErrorCode(ret);
}

int32_t HiPlayerImpl::SetSource(const std::shared_ptr<IMediaDataSource>& dataSrc)
{
    MEDIA_LOG_W("SetSource only support url format source!");
    return TransErrorCode(ErrorCode::ERROR_UNIMPLEMENTED);
}

int32_t HiPlayerImpl::Prepare()
{
    MEDIA_LOG_D("Prepare entered, current fsm state: %s.", fsm_.GetCurrentState().c_str());
    OSAL::ScopedLock lock(stateMutex_);
    PROFILE_BEGIN();
    if (curFsmState_ == StateId::PREPARING) {
        errorCode_ = ErrorCode::SUCCESS;
        cond_.Wait(lock, [this] { return curFsmState_ == StateId::READY || curFsmState_ == StateId::INIT; });
    }
    MEDIA_LOG_D("Prepare finished, current fsm state: %s.", fsm_.GetCurrentState().c_str());
    if (curFsmState_ == StateId::READY) {
        PROFILE_END("Prepare successfully,");
        return TransErrorCode(ErrorCode::SUCCESS);
    } else if (curFsmState_ == StateId::INIT) {
        PROFILE_END("Prepare failed,");
        if (errorCode_ == ErrorCode::SUCCESS) {
            errorCode_ = ErrorCode::ERROR_INVALID_STATE;
        }
        return TransErrorCode(errorCode_.load());
    } else {
        return TransErrorCode(ErrorCode::ERROR_INVALID_OPERATION);
    }
}

int HiPlayerImpl::PrepareAsync()
{
    return TransErrorCode(ErrorCode::SUCCESS);
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
    PROFILE_END("Play ret = %d", TransErrorCode(ret));
    return TransErrorCode(ret);
}

int32_t HiPlayerImpl::Pause()
{
    PROFILE_BEGIN();
    auto ret = TransErrorCode(fsm_.SendEvent(Intent::PAUSE));
    PROFILE_END("Pause ret = %d", ret);
    return ret;
}

int32_t HiPlayerImpl::Stop()
{
    PROFILE_BEGIN();
    auto ret = TransErrorCode(fsm_.SendEvent(Intent::STOP));
    PROFILE_END("Stop ret = %d", ret);
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
        case EVENT_AUDIO_COMPLETE:
            for (auto& stat : mediaStats) {
                if (stat.mediaType == MediaType::AUDIO) {
                    stat.completeEventReceived = true;
                    break;
                }
            }
            break;
        case EVENT_AUDIO_PROGRESS:
            for (auto& stat : mediaStats) {
                if (stat.mediaType == MediaType::AUDIO) {
                    stat.currentPositionMs = param;
                    break;
                }
            }
            break;
        default:
            MEDIA_LOG_W("MediaStats::ReceiveEvent receive unexpected event %d", static_cast<int>(eventType));
            break;
    }
}

int32_t HiPlayerImpl::MediaStats::GetCurrentTime()
{
    int32_t currentPosition = 0;
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

int32_t HiPlayerImpl::Seek(int32_t mSeconds, PlayerSeekMode mode)
{
    return TransErrorCode(fsm_.SendEventAsync(Intent::SEEK, mSeconds));
}

int32_t HiPlayerImpl::SetVolume(float leftVolume, float rightVolume)
{
    if (leftVolume < 0 || leftVolume > MAX_MEDIA_VOLUME || rightVolume < 0 || rightVolume > MAX_MEDIA_VOLUME) {
        MEDIA_LOG_E("volume not valid, should be in range [0,100]");
        return TransErrorCode(ErrorCode::ERROR_INVALID_PARAMETER_VALUE);
    }
    if (leftVolume < 1e-6 && rightVolume >= 1e-6) {  // 1e-6
        volume_ = rightVolume;
    } else if (rightVolume < 1e-6 && leftVolume >= 1e-6) {  // 1e-6
        volume_ = leftVolume;
    } else {
        volume_ = (leftVolume + rightVolume) / 2;  // 2
    }
    volume_ /= MAX_MEDIA_VOLUME;  // normalize to 0~1
    PlayerStates states = pipelineStates_.load();
    if (states != Media::PlayerStates::PLAYER_STARTED) {
        return TransErrorCode(ErrorCode::SUCCESS);
    }
    return TransErrorCode(SetVolume(volume_));
}

#ifndef SURFACE_DISABLED
int32_t HiPlayerImpl::SetVideoSurface(sptr<Surface> surface)
{
    return TransErrorCode(ErrorCode::ERROR_UNIMPLEMENTED);
}
#endif

void HiPlayerImpl::OnEvent(Event event)
{
    MEDIA_LOG_D("[HiStreamer] OnEvent (%d)", event.type);
    switch (event.type) {
        case EVENT_ERROR: {
            fsm_.SendEventAsync(Intent::NOTIFY_ERROR, event.param);
            break;
        }
        case EVENT_READY:
            fsm_.SendEventAsync(Intent::NOTIFY_READY);
            break;
        case EVENT_AUDIO_COMPLETE:
            mediaStats_.ReceiveEvent(EVENT_AUDIO_COMPLETE, 0);
            if (mediaStats_.IsEventCompleteAllReceived()) {
                fsm_.SendEventAsync(Intent::NOTIFY_COMPLETE);
            }
            break;
        case EVENT_AUDIO_PROGRESS:
            mediaStats_.ReceiveEvent(EVENT_AUDIO_PROGRESS, Plugin::AnyCast<int64_t>(event.param));
            break;
        default:
            MEDIA_LOG_E("Unknown event(%d)", event.type);
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

ErrorCode HiPlayerImpl::DoSeek(bool allowed, int32_t msec)
{
    PROFILE_BEGIN();
    auto rtv = allowed && msec >= 0 ? ErrorCode::SUCCESS : ErrorCode::ERROR_INVALID_OPERATION;
    if (rtv == ErrorCode::SUCCESS) {
        pipeline_->FlushStart();
        PROFILE_END("Flush start");
        PROFILE_RESET();
        pipeline_->FlushEnd();
        PROFILE_END("Flush end");
        PROFILE_RESET();
        rtv = demuxer_->SeekTo(msec);
        if (rtv == ErrorCode::SUCCESS) {
            mediaStats_.ReceiveEvent(EVENT_AUDIO_PROGRESS, msec);
            mediaStats_.ReceiveEvent(EVENT_VIDEO_PROGRESS, msec);
        }
        PROFILE_END("SeekTo");
    }
    auto ptr = obs_.lock();
    if (ptr != nullptr) {
        if (rtv != ErrorCode::SUCCESS) {
            ptr->OnError(PLAYER_ERROR, TransErrorCode(rtv));
        } else {
            Format format;
            ptr->OnInfo(INFO_TYPE_SEEKDONE, mediaStats_.GetCurrentTime(), format);
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
    MEDIA_LOG_W("OnComplete looping: %d.", singleLoop_.load());
    if (!singleLoop_) {
        StopAsync();
    } else {
        fsm_.SendEventAsync(Intent::SEEK, static_cast<int32_t>(0));
    }
    auto ptr = obs_.lock();
    if (ptr != nullptr) {
        Format format;
        ptr->OnInfo(INFO_TYPE_EOS, static_cast<int32_t>(singleLoop_.load()), format);
    }
    return ErrorCode::SUCCESS;
}

ErrorCode HiPlayerImpl::DoOnError(ErrorCode errorCode)
{
    errorCode_ = errorCode;
    auto ptr = obs_.lock();
    if (ptr != nullptr) {
        ptr->OnError(PLAYER_ERROR, TransErrorCode(errorCode));
    }
    pipelineStates_ = PlayerStates::PLAYER_STATE_ERROR;
    return ErrorCode::SUCCESS;
}

ErrorCode HiPlayerImpl::SetVolume(float volume)
{
    if (audioSink_ == nullptr) {
        MEDIA_LOG_W("cannot set volume while audio sink filter is null");
        return ErrorCode::ERROR_AGAIN;
    }
    ErrorCode ret = ErrorCode::SUCCESS;
    if (volume > 0) {
        MEDIA_LOG_I("set volume %.3f", volume);
        ret = audioSink_->SetVolume(volume);
    }
    if (ret != ErrorCode::SUCCESS) {
        MEDIA_LOG_E("SetVolume failed with error %d", static_cast<int>(ret));
    }
    return ret;
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

int32_t HiPlayerImpl::SetLooping(bool loop)
{
    singleLoop_ = loop;
    return TransErrorCode(ErrorCode::SUCCESS);
}

int32_t HiPlayerImpl::SetObs(const std::weak_ptr<IPlayerEngineObs>& obs)
{
    obs_ = obs;
    return TransErrorCode(ErrorCode::SUCCESS);
}

int32_t HiPlayerImpl::Reset()
{
    Stop();
    pipelineStates_ = PlayerStates::PLAYER_IDLE;
    singleLoop_ = false;
    mediaStats_.Reset();
    return TransErrorCode(ErrorCode::SUCCESS);
}

int32_t HiPlayerImpl::GetCurrentTime(int32_t& currentTime)
{
    currentTime = mediaStats_.GetCurrentTime();
    return TransErrorCode(ErrorCode::SUCCESS);
}

int32_t HiPlayerImpl::GetDuration(int32_t& durationMs)
{
    if (audioSource_ && !audioSource_->IsSeekable()) {
        durationMs = -1;
        return TransErrorCode(ErrorCode::SUCCESS);
    }
    uint64_t duration = 0;
    auto sourceMeta = sourceMeta_.lock();
    if (sourceMeta == nullptr) {
        durationMs = 0;
        return TransErrorCode(ErrorCode::ERROR_AGAIN);
    }
    if (sourceMeta->GetUint64(Media::Plugin::MetaID::MEDIA_DURATION, duration)) {
        durationMs = duration;
        return TransErrorCode(ErrorCode::SUCCESS);
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
        durationMs = duration;
        return TransErrorCode(ErrorCode::SUCCESS);
    }
    return TransErrorCode(ErrorCode::ERROR_AGAIN);
}

int32_t HiPlayerImpl::SetPlaybackSpeed(PlaybackRateMode mode)
{
    (void)mode;
    return TransErrorCode(ErrorCode::ERROR_UNIMPLEMENTED);
}
int32_t HiPlayerImpl::GetPlaybackSpeed(PlaybackRateMode& mode)
{
    (void)mode;
    return TransErrorCode(ErrorCode::ERROR_UNIMPLEMENTED);
}

void HiPlayerImpl::OnStateChanged(StateId state)
{
    MEDIA_LOG_I("OnStateChanged from %d to %d", curFsmState_.load(), state);
    {
        OSAL::ScopedLock lock(stateMutex_);
        curFsmState_ = state;
        cond_.NotifyOne();
    }
    auto ptr = obs_.lock();
    if (ptr != nullptr) {
        Format format;
        ptr->OnInfo(INFO_TYPE_STATE_CHANGE, TransStateId2PlayerState(state), format);
    }
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

ErrorCode HiPlayerImpl::NewAudioPortFound(Filter* filter, const Plugin::Any& parameter)
{
    if (parameter.Type() != typeid(PortInfo)) {
        return ErrorCode::ERROR_INVALID_PARAMETER_TYPE;
    }
    ErrorCode rtv = ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    auto param = Plugin::AnyCast<PortInfo>(parameter);
    if (filter == demuxer_.get() && param.type == PortType::OUT) {
        MEDIA_LOG_I("new port found on demuxer %zu", param.ports.size());
        for (const auto& portDesc : param.ports) {
            if (!StringStartsWith(portDesc.name, "audio")) {
                continue;
            }
            MEDIA_LOG_I("port name %s", portDesc.name.c_str());
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
    if (parameter.Type() != typeid(PortInfo)) {
        return ErrorCode::ERROR_INVALID_PARAMETER_TYPE;
    }
    auto param = Plugin::AnyCast<PortInfo>(parameter);
    if (filter != demuxer_.get() || param.type != PortType::OUT) {
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    std::vector<Filter*> newFilters;
    for (const auto& portDesc : param.ports) {
        if (StringStartsWith(portDesc.name, "video")) {
            MEDIA_LOG_I("port name %s", portDesc.name.c_str());
            videoDecoder = FilterFactory::Instance().CreateFilterWithType<VideoDecoderFilter>(
                "builtin.player.videodecoder", "videodecoder-" + portDesc.name);
            if (pipeline_->AddFilters({videoDecoder.get()}) == ErrorCode::SUCCESS) {
                // link demuxer and video decoder
                auto fromPort = filter->GetOutPort(portDesc.name);
                auto toPort = videoDecoder->GetInPort(PORT_NAME_DEFAULT);
                FAIL_LOG(pipeline_->LinkPorts(fromPort, toPort));  // link ports
                newFilters.emplace_back(videoDecoder.get());

                // link video decoder and video sink
                if (pipeline_->AddFilters({videoSink.get()}) == ErrorCode::SUCCESS) {
                    fromPort = videoDecoder->GetOutPort(PORT_NAME_DEFAULT);
                    toPort = videoSink->GetInPort(PORT_NAME_DEFAULT);
                    FAIL_LOG(pipeline_->LinkPorts(fromPort, toPort));  // link ports
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
        MEDIA_LOG_I("remove filter chain for port: %s", portDesc.name.c_str());
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
}  // namespace Media
}  // namespace OHOS

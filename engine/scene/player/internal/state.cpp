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

#define HST_LOG_TAG "State"

#include "state.h"
#include "pipeline/core/event.h"
#include "foundation/log.h"

namespace OHOS {
namespace Media {
State::State(StateId stateId, std::string name, PlayExecutor& executor)
    : stateId_(stateId), name_(std::move(name)), executor_(executor)
{
}
std::tuple<ErrorCode, Action> State::Enter(Intent intent)
{
    (void)intent;
    MEDIA_LOG_D("Enter state: " PUBLIC_LOG "s", name_.c_str());
    return {ErrorCode::SUCCESS, Action::ACTION_BUTT};
}
void State::Exit()
{
    MEDIA_LOG_D("Exit state: " PUBLIC_LOG "s", name_.c_str());
}
std::tuple<ErrorCode, Action> State::Execute(Intent intent, const Plugin::Any& param)
{
    return DispatchIntent(intent, param);
}
const std::string& State::GetName()
{
    return name_;
}
StateId State::GetStateId()
{
    return stateId_;
}
std::tuple<ErrorCode, Action> State::SetSource(const Plugin::Any& source)
{
    (void)source;
    return {ErrorCode::ERROR_INVALID_OPERATION, Action::ACTION_BUTT};
}
std::tuple<ErrorCode, Action> State::Play()
{
    return {ErrorCode::ERROR_INVALID_OPERATION, Action::ACTION_BUTT};
}
std::tuple<ErrorCode, Action> State::Stop()
{
    return {ErrorCode::ERROR_INVALID_OPERATION, Action::ACTION_BUTT};
}
std::tuple<ErrorCode, Action> State::Pause()
{
    return {ErrorCode::ERROR_INVALID_OPERATION, Action::ACTION_BUTT};
}
std::tuple<ErrorCode, Action> State::Resume()
{
    return {ErrorCode::ERROR_INVALID_OPERATION, Action::ACTION_BUTT};
}
std::tuple<ErrorCode, Action> State::Seek(const Plugin::Any& param)
{
    (void)param;
    executor_.DoSeek(false, 0, Plugin::SeekMode::SEEK_PREVIOUS_SYNC);
    return {ErrorCode::ERROR_INVALID_OPERATION, Action::ACTION_BUTT};
}
std::tuple<ErrorCode, Action> State::SetAttribute()
{
    return {ErrorCode::ERROR_INVALID_OPERATION, Action::ACTION_BUTT};
}
std::tuple<ErrorCode, Action> State::OnReady()
{
    return {ErrorCode::ERROR_INVALID_OPERATION, Action::ACTION_BUTT};
}
std::tuple<ErrorCode, Action> State::OnError(const Plugin::Any& param)
{
    ErrorCode errorCode = ErrorCode::ERROR_UNKNOWN;
    if (param.SameTypeWith(typeid(ErrorCode))) {
        errorCode = Plugin::AnyCast<ErrorCode>(param);
    }
    executor_.DoOnError(errorCode);
    return {ErrorCode::SUCCESS, Action::TRANS_TO_INIT};
}
std::tuple<ErrorCode, Action> State::OnComplete()
{
    return {ErrorCode::SUCCESS, Action::ACTION_BUTT};
}
std::tuple<ErrorCode, Action> State::DispatchIntent(Intent intent, const Plugin::Any& param)
{
    ErrorCode rtv = ErrorCode::SUCCESS;
    Action nextAction = Action::ACTION_BUTT;
    switch (intent) {
        case Intent::SET_SOURCE:
            std::tie(rtv, nextAction) = SetSource(param);
            break;
        case Intent::SEEK:
            std::tie(rtv, nextAction) = Seek(param);
            break;
        case Intent::PLAY:
            std::tie(rtv, nextAction) = Play();
            break;
        case Intent::PAUSE:
            std::tie(rtv, nextAction) = Pause();
            break;
        case Intent::RESUME:
            std::tie(rtv, nextAction) = Resume();
            break;
        case Intent::STOP:
            std::tie(rtv, nextAction) = Stop();
            break;
        case Intent::SET_ATTRIBUTE:
            std::tie(rtv, nextAction) = SetAttribute();
            break;
        case Intent::NOTIFY_READY:
            std::tie(rtv, nextAction) = OnReady();
            break;
        case Intent::NOTIFY_COMPLETE:
            std::tie(rtv, nextAction) = OnComplete();
            break;
        case Intent::NOTIFY_ERROR:
            std::tie(rtv, nextAction) = OnError(param);
            break;
        default:
            break;
    }
    MEDIA_LOG_D("DispatchIntent " PUBLIC_LOG "s, curState: " PUBLIC_LOG "s, nextState: " PUBLIC_LOG "s",
                intentDesc_.at(intent).c_str(), name_.c_str(), actionDesc_.at(nextAction).c_str());
    return {rtv, nextAction};
}
} // namespace Media
} // namespace OHOS
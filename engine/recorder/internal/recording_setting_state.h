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

#ifndef HISTREAMER_HIRECORDER_RECORDING_SETTING_STATE_H
#define HISTREAMER_HIRECORDER_RECORDING_SETTING_STATE_H

#include <memory>
#include "foundation/error_code.h"
#include "foundation/log.h"
#include "osal/thread/mutex.h"
#include "recorder_executor.h"
#include "state.h"

namespace OHOS {
namespace Media {
namespace Record {
class RecordingSettingState : public State {
public:
    explicit RecordingSettingState(StateId stateId, RecorderExecutor& executor)
        : State(stateId, "RecordingSettingState", executor)
    {
    }

    ~RecordingSettingState() override = default;

    std::tuple<ErrorCode, Action> SetParameter(const Plugin::Any& param) override
    {
        OSAL::ScopedLock lock(mutex_);
        auto ret = executor_.DoSetParameter(param);
        Action action = (ret == ErrorCode::SUCCESS) ? Action::TRANS_TO_RECORDING_SETTING : Action::ACTION_BUTT;
        return {ret, action};
    }

    std::tuple<ErrorCode, Action> Prepare() override
    {
        return {ErrorCode::SUCCESS, Action::TRANS_TO_READY};
    }

    std::tuple<ErrorCode, Action> Start() override
    {
        return {ErrorCode::SUCCESS, Action::ACTION_PENDING};
    }

    std::tuple<ErrorCode, Action> Stop() override
    {
        return {ErrorCode::SUCCESS, Action::TRANS_TO_INIT};
    }
private:
    OSAL::Mutex mutex_{};
};
} // namespace Record
} // namespace Media
} // namespace OHOS
#endif

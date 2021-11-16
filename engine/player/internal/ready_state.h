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

#ifndef HISTREAMER_PLAYER_READY_STATE_H
#define HISTREAMER_PLAYER_READY_STATE_H

#include <memory>
#include "foundation/error_code.h"
#include "foundation/log.h"
#include "play_executor.h"
#include "state.h"

namespace OHOS {
namespace Media {
class ReadyState : public State {
public:
    explicit ReadyState(StateId stateId, PlayExecutor& executor) : State(stateId, "ReadyState", executor)
    {
    }

    ~ReadyState() override = default;

    std::tuple<ErrorCode, Action> Enter(Intent intent) override
    {
        (void)intent;
        auto rtv = executor_.DoOnReady();
        if (rtv == ErrorCode::SUCCESS) {
            return {rtv, Action::ACTION_BUTT};
        } else {
            return {rtv, Action::TRANS_TO_INIT};
        }
    }

    std::tuple<ErrorCode, Action> Seek(const Plugin::Any& param) override
    {
        MEDIA_LOG_D("Seek in ready state.");
        if (param.Type() != typeid(int64_t)) {
            return {ErrorCode::ERROR_INVALID_PARAMETER_TYPE, Action::ACTION_BUTT};
        }
        auto timeMs = Plugin::AnyCast<int64_t>(param);
        auto ret = executor_.DoSeek(true, timeMs);
        return {ret, Action::ACTION_BUTT};
    }

    std::tuple<ErrorCode, Action> Play() override
    {
        MEDIA_LOG_D("Play in ready state.");
        return {ErrorCode::SUCCESS, Action::TRANS_TO_PLAYING};
    }

    std::tuple<ErrorCode, Action> Stop() override
    {
        MEDIA_LOG_D("Stop in ready state.");
        return {ErrorCode::SUCCESS, Action::TRANS_TO_INIT};
    }
};
} // namespace Media
} // namespace OHOS
#endif

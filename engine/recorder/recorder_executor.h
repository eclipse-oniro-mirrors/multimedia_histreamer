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

#ifndef HISTREAMER_RECORDER_EXECUTOR_H
#define HISTREAMER_RECORDER_EXECUTOR_H

#include <memory>
#include "foundation/error_code.h"
#include "recorder.h"

namespace OHOS {
namespace Media {
class RecorderExecutor {
public:
    virtual ~RecorderExecutor()
    {
    }

    virtual ErrorCode DoSetVideoSource(VideoSourceType sourceType, int32_t sourceId) const
    {
        (void)sourceType;
        (void)sourceId;
        return ErrorCode::SUCCESS;
    }

    virtual ErrorCode DoSetAudioSource(AudioSourceType sourceType, int32_t sourceId) const
    {
        (void)sourceType;
        (void)sourceId;
        return ErrorCode::SUCCESS;
    }

    virtual ErrorCode DoSetParameter(const Plugin::Any& param) const
    {
        return ErrorCode::SUCCESS;
    }

    virtual ErrorCode DoPrepare()
    {
        return ErrorCode::SUCCESS;
    }

    virtual ErrorCode DoStart()
    {
        return ErrorCode::SUCCESS;
    }

    virtual ErrorCode DoPause()
    {
        return ErrorCode::SUCCESS;
    }

    virtual ErrorCode DoResume()
    {
        return ErrorCode::SUCCESS;
    }

    virtual ErrorCode DoStop()
    {
        return ErrorCode::SUCCESS;
    }

    virtual ErrorCode DoOnComplete()
    {
        return ErrorCode::SUCCESS;
    }

    virtual ErrorCode DoOnError(RecorderErrorType infoType, ErrorCode errorCode)
    {
        (void)infoType;
        (void)errorCode;
        return ErrorCode::SUCCESS;
    }
};
} // namespace Media
} // namespace OHOS

#endif //HISTREAMER_RECORDER_EXECUTOR_H

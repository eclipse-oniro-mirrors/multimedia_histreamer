/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd.
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
#include "hitrace_utils.h"

namespace OHOS {
namespace Media {
    SyncScopedTracer::SyncScopedTracer(uint64_t label, const std::string &value, float limit) : label_(label)
    {
        StartTrace(label, value, limit);
    }

    SyncScopedTracer::~SyncScopedTracer()
    {
        FinishTrace(label_);
    }

    AsyncScopedTracer::AsyncScopedTracer(uint64_t label, const std::string &value, int32_t taskId, float limit)
        :label_(label), value_(value), taskId_(taskId)
    {
        StartAsyncTrace(label, value, taskId, limit);
    }

    AsyncScopedTracer::~AsyncScopedTracer()
    {
        FinishAsyncTrace(label_, value_, taskId_);
    }

}
}

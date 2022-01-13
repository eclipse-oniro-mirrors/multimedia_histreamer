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

#include "recorder/standard/media_utils.h"
#include "media_errors.h"
namespace OHOS {
namespace Media {
namespace {
struct ErrorCodePair {
    ErrorCode errorCode;
    int serviceErrCode;
};

const ErrorCodePair g_errorCodePair[] = {
    {ErrorCode::SUCCESS, MSERR_OK},
    {ErrorCode::ERROR_UNKNOWN, MSERR_UNKNOWN},
    {ErrorCode::ERROR_AGAIN, MSERR_UNKNOWN},
    {ErrorCode::ERROR_UNIMPLEMENTED, MSERR_UNSUPPORT},
    {ErrorCode::ERROR_INVALID_PARAMETER_VALUE, MSERR_INVALID_VAL},
    {ErrorCode::ERROR_INVALID_PARAMETER_TYPE, MSERR_INVALID_VAL},
    {ErrorCode::ERROR_INVALID_OPERATION, MSERR_INVALID_OPERATION},
    {ErrorCode::ERROR_UNSUPPORTED_FORMAT, MSERR_UNSUPPORT_CONTAINER_TYPE},
    {ErrorCode::ERROR_NOT_EXISTED, MSERR_OPEN_FILE_FAILED},
    {ErrorCode::ERROR_TIMED_OUT, MSERR_EXT_TIMEOUT},
    {ErrorCode::ERROR_NO_MEMORY, MSERR_EXT_NO_MEMORY},
    {ErrorCode::ERROR_INVALID_STATE, MSERR_INVALID_STATE},
};
}  // namespace
namespace Record {
int TransErrorCode(ErrorCode errorCode)
{
    for (const auto& errPair : g_errorCodePair) {
        if (errPair.errorCode == errorCode) {
            return errPair.serviceErrCode;
        }
    }
    return MSERR_UNKNOWN;
}
}  // namespace Record
}  // namespace Media
}  // namespace OHOS
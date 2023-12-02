/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

#include "avbuffer_capi_mock.h"
#include <gtest/gtest.h>
#include "avformat_capi_mock.h"
#include "native_avbuffer.h"
#include "native_averrors.h"
#include "surface_buffer.h"
#include "unittest_log.h"

namespace OHOS {
namespace MediaAVCodec {
uint8_t *AVBufferCapiMock::GetAddr()
{
    UNITTEST_CHECK_AND_RETURN_RET_LOG(buffer_ != nullptr, nullptr, "buffer_ is nullptr!");
    return OH_AVBuffer_GetAddr(buffer_);
}

int32_t AVBufferCapiMock::GetCapacity()
{
    UNITTEST_CHECK_AND_RETURN_RET_LOG(buffer_ != nullptr, 0, "buffer_ is nullptr!");
    return OH_AVBuffer_GetCapacity(buffer_);
}

OH_AVBufferAttr AVBufferCapiMock::GetBufferAttr()
{
    OH_AVBufferAttr attr;
    UNITTEST_CHECK_AND_RETURN_RET_LOG(buffer_ != nullptr, attr, "buffer_ is nullptr!");
    return OH_AVBuffer_GetBufferAttr(buffer_);
}
int32_t AVBufferCapiMock::SetBufferAttr(OH_AVBufferAttr &attr)
{
    UNITTEST_CHECK_AND_RETURN_RET_LOG(buffer_ != nullptr, static_cast<int32_t>(Status::ERROR_UNKNOWN),
                                      "buffer_ is nullptr!");
    return OH_AVBuffer_SetBufferAttr(buffer_, &attr);
}

std::shared_ptr<FormatMock> AVBufferCapiMock::GetParameter()
{
    UNITTEST_CHECK_AND_RETURN_RET_LOG(buffer_ != nullptr, nullptr, "buffer_ is nullptr!");
    OH_AVFormat *format = OH_AVBuffer_GetParameter(buffer_);
    EXPECT_NE(format, nullptr);
    auto formatMock = std::make_shared<AVFormatCapiMock>(format);
    return formatMock;
}

int32_t AVBufferCapiMock::SetParameter(const std::shared_ptr<FormatMock> &format)
{
    UNITTEST_CHECK_AND_RETURN_RET_LOG(buffer_ != nullptr, static_cast<int32_t>(Status::ERROR_UNKNOWN),
                                      "buffer_ is nullptr!");
    return OH_AVBuffer_SetParameter(buffer_, std::static_pointer_cast<AVFormatCapiMock>(format)->GetFormat());
}

int32_t AVBufferCapiMock::Destroy()
{
    int32_t ret = OH_AVBuffer_Destroy(buffer_);
    UNITTEST_CHECK_AND_RETURN_RET_LOG(ret == static_cast<int32_t>(Status::OK), ret, "OH_AVBuffer_Destroy failed!");
    buffer_ = nullptr;
    return static_cast<int32_t>(Status::OK);
}

OH_AVBuffer *AVBufferCapiMock::GetAVBuffer()
{
    return buffer_;
}
} // namespace MediaAVCodec
} // namespace OHOS
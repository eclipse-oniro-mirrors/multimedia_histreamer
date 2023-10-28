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

#ifndef NATIVE_AVMAGIC_H
#define NATIVE_AVMAGIC_H

#include <refbase.h>
#include "interface/inner_api/buffer/avbuffer.h"

#define AV_MAGIC(a, b, c, d) (((a) << 24) + ((b) << 16) + ((c) << 8) + ((d) << 0))

enum class AVMagic {
    AVCODEC_MAGIC_AVBUFFER = AV_MAGIC('B', 'B', 'U', 'F'),
};

struct AVObjectMagic : public OHOS::RefBase {
    explicit AVObjectMagic(enum AVMagic m) : magic_(m) {}
    virtual ~AVObjectMagic() = default;
    enum AVMagic magic_;
};

struct OH_AVBuffer : public AVObjectMagic {
    explicit OH_AVBuffer(const std::shared_ptr<OHOS::Media::AVBuffer> &buf);
    virtual ~OH_AVBuffer() = default;
    bool IsEqualBuffer(const std::shared_ptr<OHOS::Media::AVBuffer> &buf);
    std::shared_ptr<OHOS::Media::AVBuffer> buffer_;
    bool isUserCreated = false;
};
#endif // NATIVE_AVMAGIC_H
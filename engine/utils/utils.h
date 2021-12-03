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

#ifndef HISTREAMER_FOUNDATION_UTILS_H
#define HISTREAMER_FOUNDATION_UTILS_H

#include <cstdint>
#include <string>
#include "securec.h"

#define CALL_PTR_FUNC(ptr, func, param)                                                                                \
    if ((ptr)) {                                                                                                       \
        (ptr)->func(param);                                                                                            \
    } else {                                                                                                           \
        MEDIA_LOG_E("Call weakPtr " #func " error.");                                                                  \
    }

#define UNUSED_VARIABLE(v) ((void)(v))

#if (defined(__GNUC__) || defined(__clang__)) && (!defined(WIN32))
#define MEDIA_UNUSED __attribute__((unused))
#else
#define MEDIA_UNUSED
#endif

namespace OHOS {
namespace Media {
inline bool StringStartsWith(const std::string& input, const std::string& prefix)
{
    return input.rfind(prefix, 0) == 0;
}

template <typename ...Args>
static inline std::string FormatString(const char* format, Args... args)
{
    char buffer[BUFSIZ] = {0};

    int length = snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, format, args...);
    if (length <= 0) {
        return "Unknown error";
    }

    return buffer;
}

template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept
{
    return static_cast<typename std::underlying_type<E>::type>(e);
}

size_t GetFileSize(const char* fileName);
} // namespace Media
} // namespace OHOS
#endif // HISTREAMER_FOUNDATION_UTILS_H

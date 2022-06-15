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

#ifndef HISTREAMER_PLUGIN_CORE_HDI_UTILS_H
#define HISTREAMER_PLUGIN_CORE_HDI_UTILS_H

#include <iostream>
#include "hdi_adapter.h"

namespace OHOS {
namespace Media {
namespace Plugin {
std::string TransHdfStatus2String(int32_t status);

std::string TransPortIndex2String(PortIndex portIndex);

template<typename T, typename U>
bool TranslatesByMap(const T& t, U& u, const std::pair<T, U>* transMap, size_t mapSize);

Status TranslateRets(const int32_t& ret);

/**
 * translate type T into type U
 *
 * @tparam T
 * @tparam U
 * @return success to translate
 */
template<typename T,typename U,
         typename std::enable_if<!std::is_same<typename std::decay<T>::type,
                typename std::decay<U>::type>::value, bool>::type = true>
bool Translates(const T&, U&);

uint64_t Translate2PluginFlagSet(uint32_t omxBufFlag);

uint32_t Translate2omxFlagSet(uint64_t pluginFlags);
} // namespace Plugin
} // namespace Media
} // namespace OHOS
#endif // HISTREAMER_PLUGIN_CORE_HDI_UTILS_H

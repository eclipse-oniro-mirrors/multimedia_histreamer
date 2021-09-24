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

#ifndef HISTREAMER_PIPELINE_FILTER_PLUGIN_UTILS_H
#define HISTREAMER_PIPELINE_FILTER_PLUGIN_UTILS_H

#include <functional>
#include <memory>

#include "foundation/meta.h"
#include "foundation/type_define.h"
#include "foundation/error_code.h"
#include "pipeline/core/compatible_check.h"
#include "plugin/common/plugin_types.h"
#include "plugin/common/plugin_tags.h"
#include "plugin/common/plugin_buffer.h"
#include "plugin/core/plugin_info.h"
#include "plugin/core/plugin_manager.h"

namespace OHOS {
namespace Media {
namespace Pipeline {
#define RETURN_PLUGIN_NOT_FOUND_IF_NULL(plugin)                                                                        \
    if ((plugin) == nullptr) {                                                                                         \
        return PLUGIN_NOT_FOUND;                                                                                       \
    }

/**
 * translate plugin error into pipeline error code
 * @param pluginError
 * @return
 */
ErrorCode TranslatePluginStatus(Plugin::Status pluginError);

bool TranslateIntoParameter(const int &key, OHOS::Media::Plugin::Tag &tag);

template <typename T>
inline ErrorCode FindPluginAndUpdate(const std::shared_ptr<const OHOS::Media::Meta> &inMeta,
    Plugin::PluginType pluginType, std::shared_ptr<T>& plugin, std::shared_ptr<Plugin::PluginInfo>& pluginInfo,
    std::function<std::shared_ptr<T>(const std::string&)> pluginCreator)
{
    uint32_t maxRank = 0;
    std::shared_ptr<Plugin::PluginInfo> info;
    auto pluginNames = Plugin::PluginManager::Instance().ListPlugins(pluginType);
    for (const auto &name:pluginNames) {
        auto tmpInfo = Plugin::PluginManager::Instance().GetPluginInfo(pluginType, name);
        if (CompatibleWith(tmpInfo->inCaps, *inMeta) && tmpInfo->rank > maxRank) {
            info = tmpInfo;
        }
    }
    if (info == nullptr) {
        return PLUGIN_NOT_FOUND;
    }

    // try to reuse the plugin if their name are the same
    if (plugin != nullptr && pluginInfo != nullptr) {
        if (info->name == pluginInfo->name) {
            if (TranslatePluginStatus(plugin->Reset()) == SUCCESS) {
                return SUCCESS;
            }
        }
        plugin->Deinit();
    }
    plugin = pluginCreator(info->name);
    if (plugin == nullptr) {
        return PLUGIN_NOT_FOUND;
    }
    pluginInfo = info;
    return SUCCESS;
}
}
}
}
#endif // HISTREAMER_PIPELINE_FILTER_PLUGIN_UTILS_H

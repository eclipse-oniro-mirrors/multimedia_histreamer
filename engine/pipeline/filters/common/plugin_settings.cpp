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

#define HST_LOG_TAG "PluginSettings"

#include "plugin_settings.h"

#include <vector>
#include "foundation/log.h"
#include "plugin/common/plugin_audio_tags.h"
#include "pipeline/core/plugin_attr_desc.h"
#include "utils/utils.h"

namespace OHOS {
namespace Media {
namespace Pipeline {
bool CommonParameterChecker (Plugin::Tag tag, const Plugin::ValueType& val)
{
    // return true if not in tag in case of specially key used by plugin
    FALSE_RETURN_MSG(g_tagInfoMap.count(tag) != 0, true,
                     "%" PUBLIC_LOG_D32 "is not found in map, may be update it?", tag);
    const auto& tuple = g_tagInfoMap.at(tag);
    return std::get<1>(tuple).SameTypeWith(val);
}

// TypedParameterType is used for Surface etc. which has g_unknown default value in g_tagInfoMap
template <typename T>
MEDIA_UNUSED static bool TypedParameterType(const Plugin::ValueType& value)
{
    return value.SameTypeWith(typeid(T));
}

const PluginParaAllowedMap& PluginParameterTable::FindAllowedParameterMap(FilterType category)
{
    static const PluginParaAllowedMap emptyMap;
    auto ite = table_.find(category);
    if (ite == table_.end()) {
        return emptyMap;
    }
    return ite->second;
}

using namespace OHOS::Media::Plugin;
const std::map<FilterType, PluginParaAllowedMap> PluginParameterTable::table_ = {
    {FilterType::AUDIO_DECODER, {
        {Tag::AUDIO_CHANNELS, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_RATE, {CommonParameterChecker, PARAM_SET}},
        {Tag::MEDIA_BITRATE, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_FORMAT, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_PER_FRAME, {CommonParameterChecker, PARAM_SET}},
        {Tag::MEDIA_CODEC_CONFIG, {CommonParameterChecker, PARAM_SET}},
    }},
    {FilterType::AUDIO_SINK, {
        {Tag::AUDIO_CHANNELS, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_RATE, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_FORMAT, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_CHANNEL_LAYOUT, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_PER_FRAME, {CommonParameterChecker, PARAM_SET}},
    }},
    {FilterType::MUXER, {
        {Tag::MIME, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_CHANNELS, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_RATE, {CommonParameterChecker, PARAM_SET}},
        {Tag::MEDIA_BITRATE, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_FORMAT, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_PER_FRAME, {CommonParameterChecker, PARAM_SET}},
        {Tag::MEDIA_CODEC_CONFIG, {CommonParameterChecker, PARAM_SET}},
    }},
    {FilterType::AUDIO_ENCODER, {
        {Tag::AUDIO_CHANNELS, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_RATE, {CommonParameterChecker, PARAM_SET}},
        {Tag::MEDIA_BITRATE, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_FORMAT, {CommonParameterChecker, PARAM_SET}},
        {Tag::AUDIO_SAMPLE_PER_FRAME, {CommonParameterChecker, PARAM_SET | PARAM_GET}},
        {Tag::AUDIO_CHANNEL_LAYOUT, {CommonParameterChecker, PARAM_SET}},
        {Tag::MEDIA_CODEC_CONFIG, {CommonParameterChecker, PARAM_SET | PARAM_GET}},
        {Tag::AUDIO_AAC_PROFILE, {CommonParameterChecker, PARAM_SET | PARAM_GET}},
        {Tag::AUDIO_AAC_LEVEL, {CommonParameterChecker, PARAM_SET | PARAM_GET}},
    }},
};
}
}
}
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

#ifndef HISTREAMER_PIPELINE_PLUGIN_CAP_DESC_H
#define HISTREAMER_PIPELINE_PLUGIN_CAP_DESC_H
#include <tuple>
#include "plugin/common/plugin_source_tags.h"
#include "plugin/common/plugin_video_tags.h"

namespace OHOS {
namespace Media {
namespace Pipeline {
const Plugin::ValueType g_emptyString = std::string();
const Plugin::ValueType g_u32Def = (uint32_t)0;
const Plugin::ValueType g_d64Def = (int64_t)0;
const Plugin::ValueType g_u64Def = (uint64_t)0;
const Plugin::ValueType g_srcInputTypedef = Plugin::SrcInputType::UNKNOWN;
const Plugin::ValueType g_unknown = nullptr;
const Plugin::ValueType g_vecBufDef = std::vector<uint8_t>();
const Plugin::ValueType g_channelLayoutDef = Plugin::AudioChannelLayout::MONO;
const Plugin::ValueType g_auSampleFmtDef = Plugin::AudioSampleFormat::U8;
const Plugin::ValueType g_aacProfileDef = Plugin::AudioAacProfile::LC;
const Plugin::ValueType g_aacStFmtDef = Plugin::AudioAacStreamFormat::RAW;
const Plugin::ValueType g_vdPixelFmtDef = Plugin::VideoPixelFormat::UNKNOWN;

// tuple is <tagName, default_val, typeName> default_val is used for type compare
const std::map<Plugin::Tag, std::tuple<const char*, const Plugin::ValueType&, const char*>> g_tagInfoMap = {
    {Plugin::Tag::MIME, {"mime",                               g_emptyString,      "string"}},
    {Plugin::Tag::TRACK_ID, {"track_id",                       g_u32Def,           "uin32_t"}},
    {Plugin::Tag::REQUIRED_OUT_BUFFER_CNT, {"req_out_buf_cnt", g_u32Def,           "uin32_t"}},
    {Plugin::Tag::BUFFER_ALLOCATOR, {"buf_allocator",          g_unknown,          "shared_ptr<Allocator>"}},
    {Plugin::Tag::BUFFERING_SIZE, {"bufing_size",              g_u32Def,           "uin32_t"}},
    {Plugin::Tag::WATERLINE_HIGH, {"waterline_h",              g_u32Def,           "uint32_t"}},
    {Plugin::Tag::WATERLINE_LOW, {"waterline_l",               g_u32Def,           "uint32_t"}},
    {Plugin::Tag::SRC_INPUT_TYPE, {"src_input_typ",            g_srcInputTypedef,  "SrcInputType"}},
    {Plugin::Tag::MEDIA_TITLE, {"title",                       g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_ARTIST, {"artist",                     g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_LYRICIST, {"lyricist",                 g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_ALBUM, {"album",                       g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_ALBUM_ARTIST, {"album_artist",         g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_DATE, {"date",                         g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_COMMENT, {"comment",                   g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_GENRE, {"genre",                       g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_COPYRIGHT, {"copyright",               g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_LANGUAGE, {"lang",                     g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_DESCRIPTION, {"media_desc",            g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_LYRICS, {"lyrics",                     g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_DURATION, {"duration",                 g_d64Def,           "int64_t"}},
    {Plugin::Tag::MEDIA_FILE_SIZE, {"file_size",               g_u64Def,           "uint64_t"}},
    {Plugin::Tag::MEDIA_BITRATE, {"bit_rate",                  g_d64Def,           "int64_t"}},
    {Plugin::Tag::MEDIA_FILE_EXTENSION, {"file_ext",           g_emptyString,      "string"}},
    {Plugin::Tag::MEDIA_CODEC_CONFIG, {"codec_config",         g_vecBufDef,        "std::vector<uint8_t>"}},
    {Plugin::Tag::MEDIA_POSITION, {"position",                 g_u64Def,           "uint64_t"}},
    {Plugin::Tag::AUDIO_CHANNELS, {"channel",                  g_u32Def,           "uint32_t"}},
    {Plugin::Tag::AUDIO_CHANNEL_LAYOUT, {"channel_layout",     g_channelLayoutDef, "AudioChannelLayout"}},
    {Plugin::Tag::AUDIO_SAMPLE_RATE, {"sample_rate",           g_u32Def,           "uint32_t"}},
    {Plugin::Tag::AUDIO_SAMPLE_FORMAT, {"sample_fmt",          g_auSampleFmtDef,   "AudioSampleFormat"}},
    {Plugin::Tag::AUDIO_SAMPLE_PER_FRAME, {"sample_per_frame", g_u32Def,           "uin32_t"}},
    {Plugin::Tag::AUDIO_MPEG_VERSION, {"ad_mpeg_ver",          g_u32Def,           "uint32_t"}},
    {Plugin::Tag::AUDIO_MPEG_LAYER, {"ad_mpeg_layer",          g_u32Def,           "uint32_t"}},
    {Plugin::Tag::AUDIO_AAC_PROFILE, {"aac_profile",           g_aacProfileDef,    "AudioAacProfile"}},
    {Plugin::Tag::AUDIO_AAC_LEVEL, {"aac_level",               g_u32Def,           "uint32_t"}},
    {Plugin::Tag::AUDIO_AAC_STREAM_FORMAT, {"aac_stm_fmt",     g_aacStFmtDef,      "AudioAacStreamFormat"}},
    {Plugin::Tag::VIDEO_WIDTH, {"vd_w",                        g_u32Def,           "uin32_t"}},
    {Plugin::Tag::VIDEO_HEIGHT, {"vd_h",                       g_u32Def,           "uin32_t"}},
    {Plugin::Tag::VIDEO_PIXEL_FORMAT, {"pixel_fmt",            g_vdPixelFmtDef,    "VideoPixelFormat"}},
    {Plugin::Tag::VIDEO_FRAME_RATE, {"frm_rate",               g_u32Def,           "uint32_t"}},
    {Plugin::Tag::VIDEO_SURFACE, {"surface",                   g_unknown,          "Surface"}},
    {Plugin::Tag::VIDEO_MAX_SURFACE_NUM, {"surface_num",       g_u32Def,           "uin32_t"}},
};

const std::map<Plugin::AudioSampleFormat, const char*> g_auSampleFmtStrMap = {
    {Plugin::AudioSampleFormat::S8, "S8"},
    {Plugin::AudioSampleFormat::U8, "U8"},
    {Plugin::AudioSampleFormat::S8P, "S8P"},
    {Plugin::AudioSampleFormat::U8P, "U8P"},
    {Plugin::AudioSampleFormat::S16, "S16"},
    {Plugin::AudioSampleFormat::U16, "U16"},
    {Plugin::AudioSampleFormat::S16P, "S16P"},
    {Plugin::AudioSampleFormat::U16P, "U16P"},
    {Plugin::AudioSampleFormat::S24, "S24"},
    {Plugin::AudioSampleFormat::U24, "U24"},
    {Plugin::AudioSampleFormat::S24P, "S24P"},
    {Plugin::AudioSampleFormat::U24P, "U24P"},
    {Plugin::AudioSampleFormat::S32, "S32"},
    {Plugin::AudioSampleFormat::U32, "U32"},
    {Plugin::AudioSampleFormat::S32P, "S32P"},
    {Plugin::AudioSampleFormat::U32P, "U32P"},
    {Plugin::AudioSampleFormat::S64, "S64"},
    {Plugin::AudioSampleFormat::U64, "U64"},
    {Plugin::AudioSampleFormat::S64P, "S64P"},
    {Plugin::AudioSampleFormat::U64P, "U64P"},
    {Plugin::AudioSampleFormat::F32, "F32"},
    {Plugin::AudioSampleFormat::F32P, "F32P"},
    {Plugin::AudioSampleFormat::F64, "F64"},
    {Plugin::AudioSampleFormat::U24P, "F64P"},
};

const std::map<Plugin::AudioChannelLayout, const char*> g_auChannelLayoutStrMap = {
    {Plugin::AudioChannelLayout::MONO, "MONO"},
    {Plugin::AudioChannelLayout::STEREO, "STEREO"},
    {Plugin::AudioChannelLayout::CH_2POINT1, "CH_2POINT1"},
    {Plugin::AudioChannelLayout::CH_2_1, "CH_2_1"},
    {Plugin::AudioChannelLayout::SURROUND, "SURROUND"},
    {Plugin::AudioChannelLayout::CH_3POINT1, "CH_3POINT1"},
    {Plugin::AudioChannelLayout::CH_4POINT0, "CH_4POINT0"},
    {Plugin::AudioChannelLayout::CH_4POINT1, "CH_4POINT1"},
    {Plugin::AudioChannelLayout::CH_2_2, "CH_2_2"},
    {Plugin::AudioChannelLayout::QUAD, "QUAD"},
    {Plugin::AudioChannelLayout::CH_5POINT0, "CH_5POINT0"},
    {Plugin::AudioChannelLayout::CH_5POINT1, "CH_5POINT1"},
    {Plugin::AudioChannelLayout::CH_5POINT0_BACK, "CH_5POINT0_BACK"},
    {Plugin::AudioChannelLayout::CH_5POINT1_BACK, "CH_5POINT1_BACK"},
    {Plugin::AudioChannelLayout::CH_6POINT0, "CH_6POINT0"},
    {Plugin::AudioChannelLayout::CH_6POINT0_FRONT, "CH_6POINT0_FRONT"},
    {Plugin::AudioChannelLayout::HEXAGONAL, "HEXAGONAL"},
    {Plugin::AudioChannelLayout::CH_6POINT1, "CH_6POINT1"},
    {Plugin::AudioChannelLayout::CH_6POINT1_BACK, "CH_6POINT1_BACK"},
    {Plugin::AudioChannelLayout::CH_6POINT1_FRONT, "CH_6POINT1_FRONT"},
    {Plugin::AudioChannelLayout::CH_7POINT0, "CH_7POINT0"},
    {Plugin::AudioChannelLayout::CH_7POINT0_FRONT, "CH_7POINT0_FRONT"},
    {Plugin::AudioChannelLayout::CH_7POINT1, "CH_7POINT1"},
    {Plugin::AudioChannelLayout::CH_7POINT1_WIDE, "CH_7POINT1_WIDE"},
    {Plugin::AudioChannelLayout::CH_7POINT1_WIDE_BACK, "CH_7POINT1_WIDE_BACK"},
    {Plugin::AudioChannelLayout::OCTAGONAL, "OCTAGONAL"},
    {Plugin::AudioChannelLayout::HEXADECAGONAL, "HEXADECAGONAL"},
    {Plugin::AudioChannelLayout::STEREO_DOWNMIX, "STEREO_DOWNMIX"},
};
} // Pipeline
} // Media
} // OHOS
#endif // HISTREAMER_PIPELINE_PLUGIN_CAP_DESC_H

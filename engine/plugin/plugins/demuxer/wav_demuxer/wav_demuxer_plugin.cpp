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

#define HST_LOG_TAG "WavDemuxerPlugin"

#include "wav_demuxer_plugin.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
#include "foundation/log.h"
#include "osal/thread/scoped_lock.h"
#include "plugin/common/plugin_buffer.h"
#include "plugin/common/plugin_time.h"
#include "osal/utils/util.h"
#include "utils/constants.h"

struct WavHeadAttr {
    uint8_t  riff[4];     // 4
    uint32_t size;        // 4
    uint8_t  waveFlag[4]; // 4
    uint8_t  fmt[4];      // 4
    uint32_t fmtLen;      // 4
    uint16_t audioFormat; // 2
    uint16_t channels;    // 2
    uint32_t sampFreq;    // 4
    uint32_t byteRate;    // 4
    uint16_t blockAlign;  // 2
    uint16_t bitPerSample; //2
    uint8_t  dataFlag[4]; // 4
    uint32_t length;      // 4
}; // 根据wav协议构建的结构体，轻易勿动

namespace OHOS {
namespace Media {
namespace Plugin {
namespace WavPlugin {
namespace {
constexpr uint8_t  MAX_RANK = 100;
constexpr uint8_t  PROBE_READ_LENGTH  = 4;
constexpr uint32_t WAV_PER_FRAME_SIZE = 8192;
constexpr uint32_t MAX_WAV_HEAD_INFO_LEN = 256;
bool WavSniff(const uint8_t *inputBuf);

std::map<uint32_t, AudioSampleFormat> g_WavAudioSampleFormatPacked = {
    {8, AudioSampleFormat::U8},
    {16, AudioSampleFormat::S16},
    {32, AudioSampleFormat::S32},
};

enum class WavAudioFormat {
    WAVE_FORMAT_PCM = 0x0001,
    WAVE_FORMAT_IEEE_FLOAT = 0x0003,
    WAVE_FORMAT_ALAW = 0x0006,
    WAVE_FORMAT_MULAW = 0x0007,
    WAVE_FORMAT_EXTENSIBLE = 0xFFFE,
};
int Sniff(const std::string& pluginName, std::shared_ptr<DataSource> dataSource);
Status RegisterPlugin(const std::shared_ptr<Register>& reg);
}

WavDemuxerPlugin::WavDemuxerPlugin(std::string name)
    : DemuxerPlugin(std::move(name)),
      fileSize_(0),
      ioContext_(),
      dataOffset_(0),
      isSeekable_(true),
      isFirstFrame_(true)
{
    MEDIA_LOG_I("WavDemuxerPlugin, plugin name: " PUBLIC_LOG_S, pluginName_.c_str());
}

WavDemuxerPlugin::~WavDemuxerPlugin()
{
    MEDIA_LOG_I("~WavDemuxerPlugin");
}

Status WavDemuxerPlugin::SetDataSource(const std::shared_ptr<DataSource>& source)
{
    ioContext_.dataSource = source;
    if (ioContext_.dataSource != nullptr) {
        ioContext_.dataSource->GetSize(fileSize_);
    }
    if (fileSize_ == 0) {
        isSeekable_ = false;
    }
    MEDIA_LOG_I("fileSize_ " PUBLIC_LOG_ZU, fileSize_);
    return Status::OK;
}

uint8_t* WavDemuxerPlugin::GetWavMediaInfo(void)
{
    auto buffer = std::make_shared<Buffer>();
    auto bufData = buffer->AllocMemory(nullptr, MAX_WAV_HEAD_INFO_LEN);
    Status status = ioContext_.dataSource->ReadAt(0, buffer, MAX_WAV_HEAD_INFO_LEN);
    MEDIA_LOG_D("WAV_HEAD_INFO_LEN " PUBLIC_LOG_U32, WAV_HEAD_INFO_LEN);
    if (status != Status::OK) {
        MEDIA_LOG_E("Read Data Error");
        return nullptr;
    }
    uint8_t *dataPtr = const_cast<uint8_t *>(bufData->GetReadOnlyData());
    int pos = 0;
    while (pos < MAX_WAV_HEAD_INFO_LEN - 3) {
        if (dataPtr[pos] == 'd'&& dataPtr[pos + 1] == 'a' && dataPtr[pos + 2] == 't' &&  // 2,3
            dataPtr[pos + 3] == 'a') {
            dataOffset_ = pos + 8; // 8 ----the length of data size
            break;
        }
        pos++;
    }
    return dataPtr;
}

Status WavDemuxerPlugin::GetMediaInfo(MediaInfo& mediaInfo)
{
    WavHeadAttr *wavHeader = reinterpret_cast<WavHeadAttr *>(GetWavMediaInfo());
    if (wavHeader == nullptr) {
        return Status::ERROR_UNKNOWN;
    }
    mediaInfo.tracks.resize(1);
    if (wavHeader->channels == 1) {
        mediaInfo.tracks[0].insert({Tag::AUDIO_CHANNEL_LAYOUT, AudioChannelLayout::MONO});
    } else {
        mediaInfo.tracks[0].insert({Tag::AUDIO_CHANNEL_LAYOUT, AudioChannelLayout::STEREO});
    }
    mediaInfo.tracks[0].insert({Tag::AUDIO_SAMPLE_RATE, static_cast<uint32_t>(wavHeader->sampFreq)});
    mediaInfo.tracks[0].insert({Tag::MEDIA_BITRATE, static_cast<int64_t>(wavHeader->byteRate) * 8}); // 8  byte to bit
    mediaInfo.tracks[0].insert({Tag::AUDIO_CHANNELS, static_cast<uint32_t>(wavHeader->channels)});
    mediaInfo.tracks[0].insert({Tag::TRACK_ID, static_cast<uint32_t>(0)});
    mediaInfo.tracks[0].insert({Tag::MIME, std::string(MEDIA_MIME_AUDIO_RAW)});
    mediaInfo.tracks[0].insert({Tag::AUDIO_MPEG_VERSION, static_cast<uint32_t>(1)});
    mediaInfo.tracks[0].insert({Tag::AUDIO_SAMPLE_PER_FRAME, WAV_PER_FRAME_SIZE});
    if (wavHeader->audioFormat == static_cast<uint16_t>(WavAudioFormat::WAVE_FORMAT_PCM)
        || wavHeader->audioFormat == static_cast<uint16_t>(WavAudioFormat::WAVE_FORMAT_EXTENSIBLE)) {
        mediaInfo.tracks[0].insert({Tag::AUDIO_SAMPLE_FORMAT,
        g_WavAudioSampleFormatPacked[static_cast<uint32_t>(wavHeader->bitPerSample)]});
    } else if (wavHeader->audioFormat == static_cast<uint16_t>(WavAudioFormat::WAVE_FORMAT_IEEE_FLOAT)){
        mediaInfo.tracks[0].insert({Tag::AUDIO_SAMPLE_FORMAT, AudioSampleFormat::F32});
    } else {
        mediaInfo.tracks[0].insert({Tag::AUDIO_SAMPLE_FORMAT, AudioSampleFormat::NONE});
    }
    mediaInfo.tracks[0].insert({Tag::BITS_PER_CODED_SAMPLE, static_cast<uint32_t>(wavHeader->bitPerSample)});
    return Status::OK;
}

Status WavDemuxerPlugin::ReadFrame(Buffer& outBuffer, int32_t timeOutMs)
{
    Status retResult = Status::OK;
    std::shared_ptr<Buffer> outBufferPtr(&outBuffer, [](Buffer *) {});
    if (outBuffer.IsEmpty()) {
        outBuffer.AllocMemory(nullptr, WAV_PER_FRAME_SIZE);
    }
    if (!isSeekable_) {
        if (isFirstFrame_) {
            retResult = ioContext_.dataSource->ReadAt(dataOffset_, outBufferPtr, WAV_PER_FRAME_SIZE);
            isFirstFrame_ = false;
        } else {
            retResult = ioContext_.dataSource->ReadAt(0, outBufferPtr, WAV_PER_FRAME_SIZE);
        }
    } else {
        retResult = ioContext_.dataSource->ReadAt(dataOffset_, outBufferPtr, WAV_PER_FRAME_SIZE);
        dataOffset_ +=  outBuffer.GetMemory()->GetSize();
    }
    if (retResult != Status::OK) {
        MEDIA_LOG_E("Read Data Error");
    }
    return retResult;
}

Status WavDemuxerPlugin::SeekTo(int32_t trackId, int64_t hstTime, SeekMode mode)
{
    if (fileSize_ <= 0) {
        return Status::ERROR_INVALID_OPERATION;
    }
    return Status::ERROR_UNIMPLEMENTED;
}

Status WavDemuxerPlugin::Reset()
{
    isFirstFrame_ = true;
    dataOffset_ = 0;
    fileSize_ = 0;
    isSeekable_ = true;
    return Status::OK;
}

Status WavDemuxerPlugin::GetParameter(Tag tag, ValueType &value)
{
    return Status::ERROR_UNIMPLEMENTED;
}

Status WavDemuxerPlugin::SetParameter(Tag tag, const ValueType &value)
{
    return Status::ERROR_UNIMPLEMENTED;
}

std::shared_ptr<Allocator> WavDemuxerPlugin::GetAllocator()
{
    return nullptr;
}

Status WavDemuxerPlugin::SetCallback(Callback* cb)
{
    return Status::OK;
}

size_t WavDemuxerPlugin::GetTrackCount()
{
    return 0;
}
Status WavDemuxerPlugin::SelectTrack(int32_t trackId)
{
    return Status::OK;
}
Status WavDemuxerPlugin::UnselectTrack(int32_t trackId)
{
    return Status::OK;
}
Status WavDemuxerPlugin::GetSelectedTracks(std::vector<int32_t>& trackIds)
{
    return Status::OK;
}

namespace {
bool WavSniff(const uint8_t *inputBuf)
{
    // 解析数据起始位置的值，判断是否为wav格式文件
    return ((inputBuf[0] != 'R') || (inputBuf[1] != 'I') || (inputBuf[2] != 'F') || (inputBuf[3] != 'F')); // 0 1 2 3
}
int Sniff(const std::string& name, std::shared_ptr<DataSource> dataSource)
{
    MEDIA_LOG_I("Sniff in");
    Status status = Status::ERROR_UNKNOWN;
    auto buffer = std::make_shared<Buffer>();
    auto bufData = buffer->AllocMemory(nullptr, PROBE_READ_LENGTH);
    status = dataSource->ReadAt(0, buffer, PROBE_READ_LENGTH);
    if (status != Status::OK) {
        MEDIA_LOG_E("Sniff Read Data Error");
        return 0;
    }
    if (WavSniff(bufData->GetReadOnlyData())) {
        return 0;
    }
    return MAX_RANK;
}

Status RegisterPlugin(const std::shared_ptr<Register>& reg)
{
    MEDIA_LOG_I("RegisterPlugin called.");
    if (!reg) {
        MEDIA_LOG_I("RegisterPlugin failed due to nullptr pointer for reg.");
        return Status::ERROR_INVALID_PARAMETER;
    }

    std::string pluginName = "WavDemuxerPlugin";
    DemuxerPluginDef regInfo;
    regInfo.name = pluginName;
    regInfo.description = "adapter for wav demuxer plugin";
    regInfo.rank = MAX_RANK;
    regInfo.creator = [](const std::string &name) -> std::shared_ptr<DemuxerPlugin> {
        return std::make_shared<WavDemuxerPlugin>(name);
    };
    regInfo.sniffer = Sniff;
    auto rtv = reg->AddPlugin(regInfo);
    if (rtv != Status::OK) {
        MEDIA_LOG_I("RegisterPlugin AddPlugin failed with return " PUBLIC_LOG_D32, static_cast<int>(rtv));
    }
    return Status::OK;
}
}

PLUGIN_DEFINITION(WavDemuxer, LicenseType::APACHE_V2, RegisterPlugin, [] {});
} // namespace WavPlugin
} // namespace Plugin
} // namespace Media
} // namespace OHOS

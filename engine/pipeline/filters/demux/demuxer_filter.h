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

#ifndef MEDIA_PIPELINE_DEMUXER_FILTER_H
#define MEDIA_PIPELINE_DEMUXER_FILTER_H

#include <atomic>
#include <string>
#include "core/filter_base.h"
#include "data_packer.h"
#include "osal/thread/mutex.h"
#include "osal/thread/task.h"
#include "plugin/common/plugin_types.h"
#include "plugin/core/demuxer.h"
#include "plugin/core/plugin_meta.h"
#include "type_finder.h"
#include "utils/type_define.h"
#include "utils/utils.h"

namespace OHOS {
namespace Media {
namespace Pipeline {
class DemuxerFilter : public FilterBase {
public:
    explicit DemuxerFilter(std::string name);

    ~DemuxerFilter() override;

    void Init(EventReceiver* receiver, FilterCallback* callback) override;

    ErrorCode Prepare() override;

    ErrorCode Start() override;

    ErrorCode Stop() override;

    ErrorCode Pause() override;

    void FlushStart() override;

    void FlushEnd() override;

    ErrorCode PushData(const std::string& inPort, AVBufferPtr buffer) override;

    bool Negotiate(const std::string& inPort, const std::shared_ptr<const Plugin::Capability>& upstreamCap,
                   Capability& upstreamNegotiatedCap) override;

    bool Configure(const std::string& inPort, const std::shared_ptr<const Plugin::Meta>& upstreamMeta) override;

    ErrorCode SeekTo(int32_t msec);

    std::vector<std::shared_ptr<Plugin::Meta>> GetStreamMetaInfo() const;

    std::shared_ptr<Plugin::Meta> GetGlobalMetaInfo() const;

private:
    class DataSourceImpl;

    enum class DemuxerState { DEMUXER_STATE_NULL, DEMUXER_STATE_PARSE_HEADER, DEMUXER_STATE_PARSE_FRAME };

    struct StreamTrackInfo {
        uint32_t streamIdx = 0;
        std::shared_ptr<OutPort> port = nullptr;
        bool needNegoCaps = false;

        StreamTrackInfo(uint32_t streamIdx, std::shared_ptr<OutPort> port, bool needNegoCaps)
            : streamIdx(streamIdx), port(std::move(port)), needNegoCaps(needNegoCaps)
        {
        }
    };

    struct MediaMetaData {
        std::vector<StreamTrackInfo> trackInfos;
        std::vector<std::shared_ptr<Plugin::Meta>> trackMetas;
        std::shared_ptr<Plugin::Meta> globalMeta;
    };

    void Reset();

    void InitTypeFinder();

    bool CreatePlugin(std::string pluginName);

    bool InitPlugin(std::string pluginName);

    void ActivatePullMode();

    void ActivatePushMode();

    void MediaTypeFound(std::string pluginName);

    void InitMediaMetaData(const Plugin::MediaInfoHelper& mediaInfo);

    bool IsOffsetValid(int64_t offset) const;

    bool PrepareStreams(const Plugin::MediaInfoHelper& mediaInfo);

    ErrorCode ReadFrame(AVBuffer& buffer, uint32_t& streamIndex);

    std::shared_ptr<Plugin::Meta> GetStreamMeta(uint32_t streamIndex);

    void SendEventEos();

    void HandleFrame(const AVBufferPtr& buffer, uint32_t streamIndex);

    void NegotiateDownstream();

    void DemuxerLoop();

    std::string uriSuffix_;
    uint64_t mediaDataSize_;
    std::shared_ptr<OSAL::Task> task_;
    std::shared_ptr<TypeFinder> typeFinder_;
    std::shared_ptr<DataPacker> dataPacker_;

    std::string pluginName_;
    std::shared_ptr<Plugin::Demuxer> plugin_;
    std::atomic<DemuxerState> pluginState_;
    std::shared_ptr<Plugin::AllocatorHelper> pluginAllocator_;
    std::shared_ptr<DataSourceImpl> dataSource_;
    MediaMetaData mediaMetaData_;

    std::function<bool(uint64_t, size_t)> checkRange_;
    std::function<bool(uint64_t, size_t, AVBufferPtr&)> peekRange_;
    std::function<bool(uint64_t, size_t, AVBufferPtr&)> getRange_;
};
} // namespace Pipeline
} // namespace Media
} // namespace OHOS

#endif // MEDIA_PIPELINE_DEMUXER_FILTER_H

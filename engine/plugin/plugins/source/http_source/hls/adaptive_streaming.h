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

#ifndef HISTREAMER_ADAPTIVE_STREAMING_H
#define HISTREAMER_ADAPTIVE_STREAMING_H

#include <list>
#include "plugin/plugins/source/http_source/download/downloader.h"

namespace OHOS {
namespace Media {
namespace Plugin {
namespace HttpPlugin {
constexpr size_t PLAY_LIST_SIZE = 5 * 1024;
class AdaptiveStreaming {
public:
    explicit AdaptiveStreaming(const std::string &url);
    AdaptiveStreaming();
    virtual ~AdaptiveStreaming();

    virtual bool ProcessManifest() = 0;
    virtual bool UpdateManifest() = 0;
    virtual bool GetDownloadList(std::shared_ptr<BlockingQueue<std::string>> downloadList) = 0;

protected:
    void SavePlayListData(uint8_t* data, uint32_t len, int64_t offset);
    void OnDownloadPlayListStatus(DownloadStatus status, int32_t code);

    bool GetPlaylist(const std::string &url);

    void SetUri(std::string url)
    {
        uri_ = url;
    }
    std::string GetUri()
    {
        return uri_;
    }

protected:
    std::shared_ptr<Downloader> playListDownloader_;
    std::shared_ptr<DownloadRequest> playListRequest_;
    char playList[PLAY_LIST_SIZE] {0};
    std::string uri_;
};
}
}
}
}
#endif
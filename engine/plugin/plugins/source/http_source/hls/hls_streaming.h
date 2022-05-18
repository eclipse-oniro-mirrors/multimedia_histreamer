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

#ifndef HISTREAMER_HLS_STREAMING_H
#define HISTREAMER_HLS_STREAMING_H

#include "adaptive_streaming.h"
#include "m3u8.h"

namespace OHOS {
namespace Media {
namespace Plugin {
namespace HttpPlugin {
class HLSStreaming : public AdaptiveStreaming {
public:
    explicit HLSStreaming(const std::string &url);
    ~HLSStreaming() override;

    bool ProcessManifest() override;
    bool UpdateManifest() override;
    bool GetDownloadList(std::shared_ptr<BlockingQueue<std::string>> downloadList) override;

    void SetCurrentVariant(std::shared_ptr<M3U8VariantStream> &variant);
    bool UpdateM3U8();

private:

    std::shared_ptr<M3U8MasterPlaylist> master_;

    std::shared_ptr<M3U8VariantStream> currentVariant_;

    std::shared_ptr<M3U8VariantStream> previousVariant_;
};
}
}
}
}
#endif
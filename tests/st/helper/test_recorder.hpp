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

#include "audio_capture_creator.h"
#include "i_recorder_engine.h"

namespace OHOS::Media::Test {

class AudioRecordSource {
public:
    explicit AudioRecordSource(std::string pcmPath,
                               int32_t sampleRate,
                               int32_t channel,
                               int32_t bitRate,
                               AudioSourceType sourceType = AudioSourceType::AUDIO_MIC,
                               AudioCodecFormat encodeType = AudioCodecFormat::AAC_LC,
                               OutputFormatType outputFormat = OutputFormatType::FORMAT_M4A,
                               std::string outputPath = "audio_record_result") :  // current_dir/audio_record_result/
                               pcmPath_(std::move(pcmPath)),
                               sampleRate_(sampleRate),
                               channel_(channel),
                               bitRate_(bitRate),
                               sourceType_(sourceType),
                               encodeType_(encodeType),
                               outputFormat_(outputFormat),
                               outputPath_(std::move(outputPath))
                               {}
public:
    std::string getPcmPath() {return outputPath_;};
private:
    std::string pcmPath_;
    int32_t sampleRate_;
    int32_t channel_;
    int32_t bitRate_;
    AudioSourceType sourceType_;
    AudioCodecFormat encodeType_;
    OutputFormatType outputFormat_;
    std::string outputPath_;

    friend class TestRecorderImpl;
};

class TestRecorder {
public:
    static std::unique_ptr<TestRecorder> CreateAudioRecorder();
    virtual ~TestRecorder() = default;
    virtual int32_t Configure(const AudioRecordSource& recordSource) = 0;
    virtual int32_t Prepare() = 0;
    virtual int32_t Start() = 0;
    virtual int32_t Pause() = 0;
    virtual int32_t Resume() = 0;
    virtual int32_t Stop() = 0;
};
}
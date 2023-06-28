/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd.
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
#include <chrono>
#include <fcntl.h>
#ifndef WIN32
#include <sys/types.h>
#include <unistd.h>
#define O_BINARY 0 // which is not defined for Linux
#else
#include <direct.h>
#endif
#include <format.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include <sstream>
#include <thread>
#include "helper/test_recorder.hpp"
#include "helper/test_player.hpp"
#include "testngpp/testngpp.hpp"
#include "foundation/log.h"
#include "foundation/osal/filesystem/file_system.h"
#include "test_single_audio_recorder_fast.h"

using namespace OHOS::Media::Test;

// @fixture(tags=fast)
FIXTURE(dataDrivenSingleAudioRecorderTestFastPreapre)
{
    int32_t fd;
    bool CheckDurationMs(int64_t expectValue, int64_t actualValue)
    {
        MEDIA_LOG_I("expectValue : %d, actualValue : %d", expectValue, actualValue);
        return true;
    }

    // file name: 44100_2_02.pcm,  44100 - sample rate, 2 - channel count, 02 - file index
    DATA_PROVIDER(pcmSources, 4,
    DATA_GROUP(AudioRecordSource(std::string(RESOURCE_DIR "/PCM/44100_2_02.pcm"), 44100, 2, 320000)));

    DATA_PROVIDER(pcmWrongChannelSources, 4,
    DATA_GROUP(AudioRecordSource(std::string(RESOURCE_DIR "/PCM/44100_2_02.pcm"), 44100, -1, 320000)));

    DATA_PROVIDER(pcmWrongSampleRateSources, 4,
    DATA_GROUP(AudioRecordSource(std::string(RESOURCE_DIR "/PCM/44100_2_02.pcm"), -1, 2, 320000)));

    DATA_PROVIDER(pcmWrongBitRateSources, 4,
    DATA_GROUP(AudioRecordSource(std::string(RESOURCE_DIR "/PCM/44100_2_02.pcm"), 44100, 2, -1)));

    // The recorder can create and prepare
    // @test(data="pcmSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_0100)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, close(fd));
    }

    // The recorder start error prepare release
    // @test(data="pcmSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_0200)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_NE(0, recorder->Start());
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Release());
        ASSERT_EQ(0, close(fd));
    }

    // The recorder can prepare start pause prepare release
    // @test(data="pcmSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_0300)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Start());
        ASSERT_EQ(0, recorder->Pause());
        ASSERT_EQ(0, close(fd));
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_NE(0, recorder->Configure(recordSource));
        ASSERT_NE(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Release());
        ASSERT_EQ(0, close(fd));
    }

    // The recorder can prepare, start, pause, resume, prepare error, release
    // @test(data="pcmSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_0400)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Start());
        ASSERT_EQ(0, recorder->Pause());
        ASSERT_EQ(0, recorder->Resume());
        ASSERT_EQ(0, close(fd));

        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_NE(0, recorder->Configure(recordSource));
        ASSERT_NE(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Release());
        ASSERT_EQ(0, close(fd));
    }

    // The recorder can prepare, start, stop, reset, prepare, release
    // @test(data="pcmSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_0500)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);

        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Start());
        ASSERT_EQ(0, recorder->Stop());
        ASSERT_EQ(0, recorder->Reset());
        ASSERT_EQ(0, close(fd));

        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Release());
        ASSERT_EQ(0, close(fd));
    }

    // The recorder can prepare, start, reset, prepare, release
    // @test(data="pcmSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_0600)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Start());
        ASSERT_EQ(0, recorder->Reset());
        ASSERT_EQ(0, close(fd));

        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Release());
        ASSERT_EQ(0, close(fd));
    }

    // the recorder prepare start prepare stop prepare reset prepare release
    // @test(data="pcmSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_0800)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Start());
        ASSERT_EQ(0, close(fd));

        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_NE(0, recorder->Configure(recordSource));
        ASSERT_NE(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Stop());
        ASSERT_EQ(0, close(fd));

        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Reset());
        ASSERT_EQ(0, close(fd));

        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Release());
        ASSERT_EQ(0, close(fd));
    }

    // The recorder prepare start prepare stop prepare reset prepare release
    // @test(data="pcmSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_0900)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_EQ(0, recorder->Prepare());
        ASSERT_EQ(0, close(fd));

        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_NE(0, recorder->Configure(recordSource));
        ASSERT_NE(0, recorder->Prepare());
        ASSERT_EQ(0, close(fd));

        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_NE(0, recorder->Configure(recordSource));
        ASSERT_NE(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Release());
        ASSERT_EQ(0, close(fd));
    }

    // The recorder prepare wrong channel
    // @test(data="pcmWrongChannelSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_1000)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_NE(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Release());
        ASSERT_EQ(0, close(fd));
    }

    // The recorder prepare wrong samplerate
    // @test(data="pcmWrongSampleRateSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_1100)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_EQ(0, recorder->Configure(recordSource));
        ASSERT_NE(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Release());
        ASSERT_EQ(0, close(fd));
    }

    // The recorder prepare wrong bitrate
    // @test(data="pcmWrongBitRateSources", tags=audio_record_fast)
    PTEST((AudioRecordSource recordSource), SUB_MEDIA_RECORDER_AudioRecorder_Prepare_API_1200)
    {
        std::unique_ptr<TestRecorder> recorder = TestRecorder::CreateAudioRecorder();
        std::string filePath = std::string(recorder->GetOutputDir() + "/test.m4a");

        // Don't add O_APPEND, or else seek fail, can not write the file length.
        fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_BINARY, 0644); // 0644, permission
        ASSERT_TRUE(fd >= 0);
        recordSource.UseOutFd(fd);
        ASSERT_NE(0, recorder->Configure(recordSource));
        ASSERT_NE(0, recorder->Prepare());
        ASSERT_EQ(0, recorder->Release());
        ASSERT_EQ(0, close(fd));
    }
};
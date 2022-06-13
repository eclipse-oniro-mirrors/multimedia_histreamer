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
#define HST_LOG_TAG "DownloadMonitor"

#include "download_monitor.h"

namespace OHOS {
namespace Media {
namespace Plugin {
namespace HttpPlugin {
namespace {
    constexpr size_t MONITOR_QUEUE_SIZE = 50;
    constexpr int MAX_RETRY_TIMES = 3;
}
DownloadMonitor::DownloadMonitor(std::shared_ptr<MediaDownloader> downloader) noexcept
    : downloader_(std::move(downloader))
{
    taskQue_ = std::make_shared<BlockingQueue<std::function<void()>>>("monitorQue",
                                                                      MONITOR_QUEUE_SIZE);
    downloader_->SetMonitorCallback(this);
    taskProcess_ = std::make_shared<OSAL::Task>(std::string("DownloaderMonitorPop"));
    taskProcess_->RegisterHandler([this] { ProcessLoop(); });
    task_ = std::make_shared<OSAL::Task>(std::string("DownloaderMonitor"));
    task_->RegisterHandler([this] { HttpMonitorLoop(); });
}

void DownloadMonitor::HttpMonitorLoop()
{
    if (isPlaying_) {
        time_t nowTime;
        time(&nowTime);
        if ((lastReadTime_ != 0) && (nowTime - lastReadTime_ >= 10)) {  // 10
            MEDIA_LOG_E("HttpMonitorLoop" PUBLIC_LOG_D64, nowTime);
            Pause();
        }
    }
    OSAL::SleepFor(50); // 50
}

void DownloadMonitor::ProcessLoop()
{
    if (isPlaying_) {
        if (!taskQue_->Empty()) {
            auto f = taskQue_->Pop();
            f();
        }
        OSAL::SleepFor(50); // 50
    }
}

bool DownloadMonitor::Open(const std::string &url)
{
    isPlaying_ = true;
    url_ = url;
    return downloader_->Open(url);
}

void DownloadMonitor::Pause()
{
    downloader_->Pause();
    isPlaying_ = false;
}

void DownloadMonitor::Resume()
{
    downloader_->Resume();
    isPlaying_ = true;
}

void DownloadMonitor::Close()
{
    downloader_->Close();
    task_->Stop();
    isPlaying_ = false;
}

bool DownloadMonitor::Retry(std::string &url, int64_t offset)
{
    return downloader_->Retry(url, offset);
}

bool DownloadMonitor::Read(unsigned char *buff, unsigned int wantReadLength,
                           unsigned int &realReadLength, bool &isEos)
{
    bool ret = downloader_->Read(buff, wantReadLength, realReadLength, isEos);
    time(&lastReadTime_);
    Resume();
    return ret;
}

bool DownloadMonitor::Seek(int offset)
{
    isPlaying_ = true;
    return downloader_->Seek(offset);
}

size_t DownloadMonitor::GetContentLength() const
{
    return downloader_->GetContentLength();
}

double DownloadMonitor::GetDuration() const
{
    return downloader_->GetDuration();
}

bool DownloadMonitor::IsStreaming() const
{
    return downloader_->IsStreaming();
}

void DownloadMonitor::SetCallback(Callback* cb)
{
    callback_ = cb;
    downloader_->SetCallback(cb);
}

void DownloadMonitor::SetMonitorCallback(MonitorCallback *cb)
{
}

void DownloadMonitor::DealDownloaderEvent(const std::shared_ptr<DownloadRequest>& request)
{
    FALSE_RETURN(request->IsValidRequestFor(url_));
    downloader_->Retry(url_, request->GetDownloadPos());
}

bool DownloadMonitor::NeedRetry(const std::shared_ptr<DownloadRequest>& request)
{
    auto clientError = request->GetClientError();
    auto serverError = request->GetServerError();
    auto retryTimes = request->GetRetryTimes();
    if (clientError != NetworkClientErrorCode::ERROR_OK && clientError != NetworkClientErrorCode::ERROR_NOT_RETRY
        || serverError != 0) {
        MEDIA_LOG_I("NeedRetry: clientError = " PUBLIC_LOG_D32 ", serverError = " PUBLIC_LOG_D32
            ", retryTimes = " PUBLIC_LOG_D32, clientError, serverError, retryTimes);
        if (retryTimes > MAX_RETRY_TIMES) { // Report error to upper layer
            if (clientError != NetworkClientErrorCode::ERROR_OK) {
                MEDIA_LOG_I("Send http client error, code " PUBLIC_LOG_D32, static_cast<int32_t>(clientError));
                callback_->OnEvent({PluginEventType::CLIENT_ERROR, {clientError}, "http"});
            }
            if (serverError != 0) {
                MEDIA_LOG_I("Send http server error, code " PUBLIC_LOG_D32, serverError);
                callback_->OnEvent({PluginEventType::SERVER_ERROR, {serverError}, "http"});
            }
            return false;
        }
        return true;
    }
    return false;
}

void DownloadMonitor::OnDownloadStatus(std::shared_ptr<DownloadRequest>& request)
{
    if (NeedRetry(request)) {
        taskQue_->Push([this, request] { DealDownloaderEvent(request); });
    }
}
}
}
}
}
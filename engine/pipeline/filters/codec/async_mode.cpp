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

#define HST_LOG_TAG "AsyncMode"

#include "async_mode.h"
#include "common/plugin_utils.h"
#include "utils/dump_buffer.h"
#include "osal/utils/util.h"
#if !defined(OHOS_LITE) && defined(VIDEO_SUPPORT)
#include "plugin/common/surface_memory.h"
#endif

namespace {
constexpr uint32_t DEFAULT_TRY_DECODE_TIME = 20;
constexpr uint32_t DEFAULT_TRY_RENDER_TIME = 5;
}

namespace OHOS {
namespace Media {
namespace Pipeline {
AsyncMode::AsyncMode(std::string name) : CodecMode(std::move(name))
{
    MEDIA_LOG_I(PUBLIC_LOG_S " ThreadMode: ASYNC", codecName_.c_str());
    isNeedQueueInputBuffer_ = true;
}

AsyncMode::~AsyncMode()
{
    MEDIA_LOG_D("Async mode dtor called");
}

ErrorCode AsyncMode::Release()
{
    MEDIA_LOG_I("AsyncMode Release start.");
    stopped_ = true;

    // 先停止线程 然后释放bufferQ 如果顺序反过来 可能导致线程访问已经释放的锁
    if (inBufQue_) {
        inBufQue_->SetActive(false);
        inBufQue_.reset();
    }
    if (!isNeedQueueInputBuffer_) {
        OSAL::ScopedLock lock(mutex_);
        isNeedQueueInputBuffer_ = true;
        cv_.NotifyOne();
    }
    if (handleFrameTask_) {
        handleFrameTask_->Stop();
        handleFrameTask_.reset();
    }
    if (outBufPool_) {
        outBufPool_->SetActive(false);
    }
    if (decodeFrameTask_) {
        decodeFrameTask_->Stop();
        decodeFrameTask_.reset();
    }
    if (pushTask_ != nullptr) {
        pushTask_->Stop();
        pushTask_.reset();
    }
    {
        OSAL::ScopedLock l(renderMutex_);
        while (!outBufQue_.empty()) {
            outBufQue_.pop();
        }
    }
    MEDIA_LOG_I("AsyncMode Release end.");
    return ErrorCode::SUCCESS;
}

ErrorCode AsyncMode::Configure()
{
    stopped_ = false;
    FALSE_LOG_MSG_W(QueueAllBufferInPoolToPluginLocked() == ErrorCode::SUCCESS,
                    "Can not configure all output buffers to plugin before start.");
    FAIL_RETURN(CodecMode::Configure());
    if (!isNeedQueueInputBuffer_) {
        OSAL::ScopedLock lock(mutex_);
        isNeedQueueInputBuffer_ = true;
        cv_.NotifyOne();
    }
    if (handleFrameTask_) {
        handleFrameTask_->Start();
    }
    if (decodeFrameTask_) {
        decodeFrameTask_->Start();
    }
    if (pushTask_) {
        pushTask_->Start();
    }
    return ErrorCode::SUCCESS;
}

ErrorCode AsyncMode::PushData(const std::string &inPort, const AVBufferPtr& buffer, int64_t offset)
{
    DUMP_BUFFER2LOG("AsyncMode in", buffer, offset);
    MEDIA_LOG_DD("PushData.");
    if (buffer != nullptr && !stopped_) {
        inBufQue_->Push(buffer);
    } else {
        MEDIA_LOG_DD("PushData buffer = nullptr.");
        OSAL::SleepFor(DEFAULT_TRY_DECODE_TIME);
    }
    return ErrorCode::SUCCESS;
}

ErrorCode AsyncMode::Stop()
{
    MEDIA_LOG_I("AsyncMode stop start.");
    stopped_ = true;
    if (outBufPool_) {
        outBufPool_->SetActive(false);
    }
    if (decodeFrameTask_) {
        decodeFrameTask_->Stop();
    }
    if (pushTask_) {
        pushTask_->Stop();
    }
    inBufQue_->SetActive(false);
    {
        OSAL::ScopedLock l(renderMutex_);
        while (!outBufQue_.empty()) {
            outBufQue_.pop();
        }
    }
    if (!isNeedQueueInputBuffer_) {
        OSAL::ScopedLock lock(mutex_);
        isNeedQueueInputBuffer_ = true;
        cv_.NotifyOne();
    }
    if (handleFrameTask_) {
        handleFrameTask_->Stop();
    }
    outBufPool_.reset();
    MEDIA_LOG_I("AsyncMode stop end.");
    return ErrorCode::SUCCESS;
}

void AsyncMode::FlushStart()
{
    MEDIA_LOG_I("AsyncMode FlushStart entered.");
    stopped_ = true; // thread will pause, should not enter endless loop
    if (inBufQue_) {
        inBufQue_->SetActive(false);
    }
    if (!isNeedQueueInputBuffer_) {
        OSAL::ScopedLock lock(mutex_);
        isNeedQueueInputBuffer_ = true;
        cv_.NotifyOne();
    }
    if (handleFrameTask_) {
        handleFrameTask_->Pause();
    }
    if (outBufPool_) {
        outBufPool_->SetActive(false);
    }
    if (decodeFrameTask_) {
        decodeFrameTask_->Pause();
    }
    if (pushTask_) {
        pushTask_->Pause();
    }
    MEDIA_LOG_I("AsyncMode FlushStart exit.");
}

void AsyncMode::FlushEnd()
{
    MEDIA_LOG_I("AsyncMode FlushEnd entered");
    stopped_ = false;
    if (inBufQue_) {
        inBufQue_->SetActive(true);
    }
    if (!isNeedQueueInputBuffer_) {
        OSAL::ScopedLock lock(mutex_);
        isNeedQueueInputBuffer_ = true;
        cv_.NotifyOne();
    }
    if (handleFrameTask_) {
        handleFrameTask_->Start();
    }
    if (outBufPool_) {
        outBufPool_->SetActive(true);
    }
    if (decodeFrameTask_) {
        decodeFrameTask_->Start();
    }
    if (pushTask_) {
        pushTask_->Start();
    }
    if (plugin_) {
        QueueAllBufferInPoolToPluginLocked();
    }
    MEDIA_LOG_I("AsyncMode FlushEnd exit");
}

ErrorCode AsyncMode::HandleFrame()
{
    MEDIA_LOG_DD("AsyncMode handle frame called");
    auto oneBuffer = inBufQue_->Pop();
    if (oneBuffer == nullptr) {
        MEDIA_LOG_DD("decoder find nullptr in esBufferQ");
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    Plugin::Status status = Plugin::Status::OK;
    do {
        DUMP_BUFFER2LOG("AsyncMode QueueInput to Plugin", oneBuffer, -1);
        status = plugin_->QueueInputBuffer(oneBuffer, 0);
        if (status == Plugin::Status::OK || status == Plugin::Status::END_OF_STREAM
            || status != Plugin::Status::ERROR_AGAIN || stopped_) {
            break;
        }
        MEDIA_LOG_DD("Send data to plugin error: " PUBLIC_LOG_D32, status);
        OSAL::ScopedLock lock(mutex_);
        isNeedQueueInputBuffer_ = false;
        cv_.Wait(lock);
    } while (true);
    MEDIA_LOG_DD("Async handle frame finished");
    return TranslatePluginStatus(status);
}

ErrorCode AsyncMode::DecodeFrame()
{
    MEDIA_LOG_DD("AsyncMode decode frame called");
    Plugin::Status status = Plugin::Status::OK;
    auto newOutBuffer = outBufPool_->AllocateBuffer();
    if (CheckBufferValidity(newOutBuffer) == ErrorCode::SUCCESS) {
        newOutBuffer->Reset();
        status = plugin_->QueueOutputBuffer(newOutBuffer, 0);
        if (status == Plugin::Status::ERROR_NOT_ENOUGH_DATA) {
            OSAL::SleepFor(DEFAULT_TRY_DECODE_TIME);
        }
    } else {
        OSAL::SleepFor(DEFAULT_TRY_DECODE_TIME);
    }
    MEDIA_LOG_DD("Async decode frame finished");
    return ErrorCode::SUCCESS;
}

ErrorCode AsyncMode::FinishFrame()
{
    MEDIA_LOG_DD("FinishFrame begin");
    bool isRendered = false;
    std::shared_ptr<AVBuffer> frameBuffer = nullptr;
    {
        OSAL::ScopedLock l(renderMutex_);
        if (!outBufQue_.empty()) {
            frameBuffer = outBufQue_.front();
            outBufQue_.pop();
        }
    }
    if (frameBuffer != nullptr) {
        auto oPort = outPorts_[0];
        if (oPort->GetWorkMode() == WorkMode::PUSH) {
            DUMP_BUFFER2LOG("AsyncMode PushData to Sink", frameBuffer, -1);
            oPort->PushData(frameBuffer, -1);
            isRendered = true;
        } else {
            MEDIA_LOG_W("decoder out port works in pull mode");
            return ErrorCode::ERROR_INVALID_OPERATION;
        }
        frameBuffer.reset();
    }
    if (!isRendered) {
        OSAL::SleepFor(DEFAULT_TRY_RENDER_TIME);
    }
    MEDIA_LOG_DD("AsyncMode finish frame success");
    return ErrorCode::SUCCESS;
}

void AsyncMode::OnOutputBufferDone(const std::shared_ptr<Plugin::Buffer>& buffer)
{
    {
        OSAL::ScopedLock l(renderMutex_);
        outBufQue_.push(buffer);
    }
    if (!isNeedQueueInputBuffer_) {
        OSAL::ScopedLock lock(mutex_);
        isNeedQueueInputBuffer_ = true;
        cv_.NotifyOne();
    }
}

ErrorCode AsyncMode::Prepare()
{
    MEDIA_LOG_I("AsyncMode prepare called.");
    if (!inBufQue_) {
        inBufQue_ = std::make_shared<BlockingQueue<AVBufferPtr>>("asyncFilterInBufQue", GetInBufferPoolSize());
    } else {
        inBufQue_->SetActive(true);
    }
    if (!handleFrameTask_) {
        handleFrameTask_ = std::make_shared<OSAL::Task>(codecName_ + "AsyncHandleFrame");
        handleFrameTask_->RegisterHandler([this] { (void)HandleFrame(); });
    }
    if (!decodeFrameTask_) {
        decodeFrameTask_ = std::make_shared<OSAL::Task>(codecName_ + "AsyncDecodeFrame");
        decodeFrameTask_->RegisterHandler([this] { (void)DecodeFrame(); });
    }
    if (!pushTask_) {
        pushTask_ = std::make_shared<OSAL::Task>(codecName_ + "AsyncPush");
        pushTask_->RegisterHandler([this] { (void)FinishFrame(); });
    }
    return ErrorCode::SUCCESS;
}

ErrorCode AsyncMode::QueueAllBufferInPoolToPluginLocked()
{
    ErrorCode err = ErrorCode::SUCCESS;
    while (!outBufPool_->Empty()) {
        auto buf = outBufPool_->AllocateBuffer();
        if (CheckBufferValidity(buf) != ErrorCode::SUCCESS) {
            MEDIA_LOG_W("cannot allocate buffer in buffer pool");
            continue;
        }
        err = TranslatePluginStatus(plugin_->QueueOutputBuffer(buf, -1));
        if (err != ErrorCode::SUCCESS) {
            MEDIA_LOG_W("Queue output buffer error, plugin doesn't support queue all out buffers.");
            break;
        }
    }
    return err;
}

ErrorCode AsyncMode::CheckBufferValidity(std::shared_ptr<AVBuffer>& buffer)
{
    if (buffer == nullptr) {
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    auto memory = buffer->GetMemory();
    if (memory == nullptr) {
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
#if !defined(OHOS_LITE) && defined(VIDEO_SUPPORT)
    if (memory->GetMemoryType() == Plugin::MemoryType::SURFACE_BUFFER) {
        std::shared_ptr<Plugin::SurfaceMemory> surfaceMemory =
            Plugin::ReinterpretPointerCast<Plugin::SurfaceMemory>(memory);

        // trigger surface memory to request surface buffer again when it is surface buffer
        FALSE_RETURN_V_MSG_E(surfaceMemory->GetSurfaceBuffer() != nullptr, ErrorCode::ERROR_NO_MEMORY,
                             "get surface buffer fail");
    }
#endif
    return ErrorCode::SUCCESS;
}
} // namespace Pipeline
} // namespace Media
} // namespace OHOS

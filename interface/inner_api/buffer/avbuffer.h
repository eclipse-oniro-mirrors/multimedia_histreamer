/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

#ifndef AVBUFFER_H
#define AVBUFFER_H

#include <memory>
#include <string>
#include "avallocator.h"

namespace OHOS {
namespace Media {
class Meta;
/**
 * @brief Struct that encapsulates some info of media buffer.
 */
using AVBufferConfig = struct AVBufferConfig {
    int32_t size = 0;
    int32_t align = 0;
    MemoryType memoryType = MemoryType::UNKNOWN_MEMORY;
    MemoryFlag memoryFlag = MemoryFlag::MEMORY_READ_ONLY;
    BufferRequestConfig surfaceBufferConfig;
    int32_t capacity = 0; // get from buffer
    int32_t dmaFd = -1;   // to create dma buffer

    bool operator<=(const struct AVBufferConfig &config) const
    {
        if (memoryType != config.memoryType) {
            return false;
        }
        int32_t configAllocSize = config.align ? (config.capacity + config.align - 1) : config.capacity;
        switch (memoryType) {
            case MemoryType::VIRTUAL_MEMORY:
                return size <= configAllocSize;
            case MemoryType::SHARED_MEMORY:
                return size <= configAllocSize &&
                       (memoryFlag == config.memoryFlag || config.memoryFlag == MemoryFlag::MEMORY_READ_WRITE);
            case MemoryType::HARDWARE_MEMORY:
                return size <= configAllocSize &&
                       (memoryFlag == config.memoryFlag || config.memoryFlag == MemoryFlag::MEMORY_READ_WRITE);
            case MemoryType::SURFACE_MEMORY:
                return (surfaceBufferConfig.width == config.surfaceBufferConfig.width) &&
                       (surfaceBufferConfig.height == config.surfaceBufferConfig.height) &&
                       (surfaceBufferConfig.strideAlignment == config.surfaceBufferConfig.strideAlignment) &&
                       (surfaceBufferConfig.format == config.surfaceBufferConfig.format) &&
                       (surfaceBufferConfig.usage == config.surfaceBufferConfig.usage) &&
                       (surfaceBufferConfig.transform == config.surfaceBufferConfig.transform) &&
                       (surfaceBufferConfig.colorGamut == config.surfaceBufferConfig.colorGamut); // ignore timeout
            default:
                return false;
        }
    }
};
/**
 * @brief Class that encapsulates some types of media buffer.
 */
class __attribute__((visibility("default"))) AVBuffer {
public:
    virtual ~AVBuffer() = default;
    /**
     * @brief Create the AVBuffer by configuration.
     * @param config The configuration of AVBuffer, refer to {@link AVBufferConfig}
     * @return The shared pointer of AVBuffer.
     * @since 4.1
     * @version 1.0
     */
    static std::shared_ptr<AVBuffer> CreateAVBuffer(const AVBufferConfig &config);

    /**
     * @brief Create the AVBuffer by allocator.
     * @param allocator The instance of AVAllocator, refer to {@link AVAllocator}
     * @param capacity The capacity of the memory, bytes.
     * @param align The align of AVBuffer, bytes.
     * @return The shared pointer of AVBuffer.
     * @since 4.1
     * @version 1.0
     */
    static std::shared_ptr<AVBuffer> CreateAVBuffer(std::shared_ptr<AVAllocator> allocator, int32_t capacity = 0,
                                                    int32_t align = 0);

    /**
     * @brief Create the AVBuffer by alloced memory.
     * @param ptr The pointer of alloced memory, it requires users to manage the lifecycle.
     * @param capacity The capacity of the memory, bytes.
     * @param size The size of the memory, bytes. If it can not greater than capacity.
     * @return The shared pointer of AVBuffer.
     * @since 4.1
     * @version 1.0
     */
    static std::shared_ptr<AVBuffer> CreateAVBuffer(uint8_t *ptr, int32_t capacity, int32_t size = 0);

    /**
     * @brief Create the AVBuffer by MessageParcel.
     * @param parcel The MessageParcel that wirtten by remote buffer, refer to {@link MessageParcel}.
     * @param isSurfaceBuffer Whether the parcel was obtained directly through SurfaceBuffer's function, {@link
     * SurfaceBuffer}.
     * @return The shared pointer of AVBuffer.
     * @since 4.1
     * @version 1.0
     */
    static std::shared_ptr<AVBuffer> CreateAVBuffer(MessageParcel &parcel, bool isSurfaceBuffer = false);

    /**
     * @brief Create the AVBuffer.
     * @return The shared pointer of AVBuffer.
     * @since 4.1
     * @version 1.0
     */
    static std::shared_ptr<AVBuffer> CreateAVBuffer();

    /**
     * @brief Get the AVBufferConfig.
     * @return The config struct of AVBuffer.
     * @since 4.1
     * @version 1.0
     */
    const AVBufferConfig &GetConfig();

    /**
     * @brief Get the unique identifier of buffer.
     * @return The unique identifier of buffer.
     * @since 4.1
     * @version 1.0
     */
    uint64_t GetUniqueId();

    /**
     * @brief Wirte buffer info to MessageParcel.
     * @param parcel The MessageParcel wirtten by buffer, refer to {@link MessageParcel}.
     * @return Whether the write was successful.
     * @since 4.1
     * @version 1.0
     */
    bool WriteToMessageParcel(MessageParcel &parcel);

    using MetaData = std::vector<uint8_t>;

    int64_t pts_;
    int64_t dts_;
    int64_t duration_;
    uint32_t flag_;
    std::shared_ptr<Meta> meta_;
    std::shared_ptr<AVMemory> memory_;
    // TODO: 包装SurfaceBuffer的时候，需要 保存 fence 等参数

private:
    explicit AVBuffer();
    int32_t Init(std::shared_ptr<AVAllocator> allocator, int32_t capacity = 0, int32_t align = 0);
    int32_t Init(uint8_t *ptr, int32_t capacity, int32_t size = 0);
    int32_t Init(MessageParcel &parcel, bool isSurfaceBuffer = false);

    uint64_t uid_;
    AVBufferConfig config_;
};

constexpr int32_t INVALID_POSITION = -1;
/**
 * @brief AVBuffer's memory.
 */
class __attribute__((visibility("default"))) AVMemory {
public:
    friend class AVBuffer;
    virtual ~AVMemory();
    /**
     * @brief Get the memory's types set by the allocator, refer to {@link MemoryType}
     * @return the memory's types if the memory is valid, otherwise {@link VIRTUAL_MEMORY}.
     * @since 4.1
     * @version 1.0
     */
    virtual MemoryType GetMemoryType();

    /**
     * @brief Get the memory's capacity, which was set during creation and alloced by the allocator.
     * @return The memory's capacity, bytes. If the memory is valid, otherwise -1.
     * @since 4.1
     * @version 1.0
     */
    virtual int32_t GetCapacity();

    /**
     * @brief Get the memory's used size.
     * @return The memory's size, bytes.
     * @since 4.1
     * @version 1.0
     */
    virtual int32_t GetSize();

    /**
     * @brief Set the memory's used size.
     * @param size The memory's used size. If the size is greater than the capacity, it will be set to equal the
     * capacity.
     * @return Returns Status::OK if the execution is successful, otherwise returns a specific error code, refer to
     * {@link AVCodecServiceErrCode}
     * @since 4.1
     * @version 1.0
     */
    virtual int32_t SetSize(int32_t size);

    /**
     * @brief Get the memory's used size.
     * @return The memory's used size, bytes.
     * @since 4.1
     * @version 1.0
     */
    virtual int32_t GetOffset();

    /**
     * @brief Set the memory's offset.
     * @param offset The memory's offset, bytes.
     * @return Returns Status::OK if the execution is successful, otherwise returns a specific error code, refer to
     * {@link AVCodecServiceErrCode}
     * @since 4.1
     * @version 1.0
     */
    virtual int32_t SetOffset(int32_t offset);

    /**
     * @brief Get the memory's file descriptor.
     * @return The memory's file descriptor. If the memory type is {@link SURFACE_MEMORY} or {@link VIRTUAL_MEMORY}, it
     * will return -1.
     * @since 4.1
     * @version 1.0
     */
    virtual int32_t GetFileDescriptor();

    /**
     * @brief Get the memory's address.
     * @return The pointer of memory's address.
     * @since 4.1
     * @version 1.0
     */
    virtual uint8_t *GetAddr();

    /**
     * @brief Writing data to memory.
     * @param in The pointer to the data being written.
     * @param writeSize The size of writing data, bytes.
     * @param position The position of writing data in memory, if equal to INVALID_POSITION, write continuously after
     * existing data, bytes.
     * @return The length of the actual written data.
     * @since 4.1
     * @version 1.0
     */
    virtual int32_t Write(const uint8_t *in, int32_t writeSize, int32_t position = INVALID_POSITION);

    /**
     * @brief Reading data from memory.
     * @param out The pointer to save the read data.
     * @param readSize The size of reading data, bytes.
     * @param position The position of reading data in memory, if equal to INVALID_POSITION, read from begin, bytes.
     * @return The length of the actual read data.
     * @since 4.1
     * @version 1.0
     */
    virtual int32_t Read(uint8_t *out, int32_t readSize, int32_t position = INVALID_POSITION);

    /**
     * @brief Set the memory's used size to zero.
     * @since 4.1
     * @version 1.0
     */
    void Reset();

    /**
     * @brief Get the surface buffer of memory.
     * @return Returns the surface buffer if the memory type is {@link SURFACE_MEMORY},
     * otherwise returns nullptr.
     * @since 4.1
     * @version 1.0
     */
    virtual sptr<SurfaceBuffer> GetSurfaceBuffer();

    /**
     * @brief Start data synchronization, only used when the memory type is {@link HARDWARE_MEMORY}.
     * @return  Returns Status::OK if the execution is successful, otherwise returns a specific error code, refer to
     * {@link AVCodecServiceErrCode}
     * @since 4.1
     * @version 1.0
     */
    virtual int32_t SyncStart();

    /**
     * @brief End data synchronization, only used when the memory type is {@link HARDWARE_MEMORY}.
     * @return  Returns Status::OK if the execution is successful, otherwise returns a specific error code, refer to
     * {@link AVCodecServiceErrCode}
     * @since 4.1
     * @version 1.0
     */
    virtual int32_t SyncEnd();

protected:
    explicit AVMemory();
    virtual int32_t Init();
    virtual int32_t Init(MessageParcel &parcel);
    virtual bool WriteToMessageParcel(MessageParcel &parcel);

    int32_t ReadCommonFromMessageParcel(MessageParcel &parcel);
    bool WriteCommonToMessageParcel(MessageParcel &parcel);

    std::string name_;
    int32_t capacity_;
    int32_t align_;

    int32_t offset_;
    int32_t size_;
    uint8_t *base_;
    std::shared_ptr<AVAllocator> allocator_;

private:
    static std::shared_ptr<AVMemory> CreateAVMemory(const std::string &name, std::shared_ptr<AVAllocator> allocator,
                                                    int32_t capacity = 0, int32_t align = 0);
    static std::shared_ptr<AVMemory> CreateAVMemory(uint8_t *ptr, int32_t capacity, int32_t size);
    static std::shared_ptr<AVMemory> CreateAVMemory(MessageParcel &parcel, bool isSurfaceBuffer = false);
};
} // namespace Media
} // namespace OHOS
#endif // AVBUFFER_H
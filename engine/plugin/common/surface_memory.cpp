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

#if !defined(OHOS_LITE) && defined(VIDEO_SUPPORT)
#include "surface_memory.h"
#include <utility>
#include "foundation/log.h"
#include "surface_allocator.h"

namespace OHOS {
namespace Media {
namespace Plugin {
SurfaceMemory::SurfaceMemory(size_t capacity, std::shared_ptr<Allocator> allocator, size_t align)
    : Memory(capacity, std::move(allocator), align, MemoryType::SURFACE_BUFFER, false)
{
    size_t allocSize = align ? (capacity + align - 1) : capacity;
    FALSE_RETURN(this->allocator != nullptr && this->allocator->GetMemoryType() == MemoryType::SURFACE_BUFFER);
    auto surfaceAllocator = std::dynamic_pointer_cast<SurfaceAllocator>(this->allocator);
    surfaceBuffer_ = surfaceAllocator->AllocSurfaceBuffer(allocSize);
}

SurfaceMemory::~SurfaceMemory()
{
    auto surfaceAllocator = std::dynamic_pointer_cast<SurfaceAllocator>(this->allocator);
    if (surfaceAllocator && surfaceBuffer_) {
        surfaceAllocator->FreeSurfaceBuffer(surfaceBuffer_);
    }
}

sptr<SurfaceBuffer> SurfaceMemory::GetSurfaceBuffer()
{
    return surfaceBuffer_;
}

void SurfaceMemory::SetFenceFd(int32_t& fd)
{
    fenceFd_ = fd;
}

int32_t SurfaceMemory::GetFenceFd()
{
    return fenceFd_;
}

BufferHandle *SurfaceMemory::GetBufferHandle()
{
    if (surfaceBuffer_) {
        return surfaceBuffer_->GetBufferHandle();
    }
    return nullptr;
}

uint8_t* SurfaceMemory::GetRealAddr() const
{
    if (surfaceBuffer_) {
        return static_cast<uint8_t *>(surfaceBuffer_->GetVirAddr());
    }
    return nullptr;
}
} // namespace Plugin
} // namespace Media
} // namespace OHOS
#endif
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

#define HST_LOG_TAG "MediaSourceFilter"

#include "media_source_filter.h"
#include "compatible_check.h"
#include "factory/filter_factory.h"
#include "plugin/interface/source_plugin.h"
#include "plugin/core/plugin_meta.h"
#include "common/plugin_utils.h"
#include "utils/type_define.h"

namespace OHOS {
namespace Media {
namespace Pipeline {
using namespace Plugin;

static AutoRegisterFilter<MediaSourceFilter> g_registerFilterHelper("builtin.player.mediasource");

static std::map<std::string, ProtocolType> g_protocolStringToType = {
    {"http", ProtocolType::HTTP},
    {"https", ProtocolType::HTTPS},
    {"file", ProtocolType::FILE},
    {"stream", ProtocolType::STREAM}
};

MediaSourceFilter::MediaSourceFilter(const std::string& name)
    : FilterBase(name),
      taskPtr_(nullptr),
      protocol_(),
      uri_(),
      isSeekable_(false),
      position_(0),
      bufferSize_(DEFAULT_FRAME_SIZE),
      plugin_(nullptr),
      pluginAllocator_(nullptr)
{
    filterType_ = FilterType::MEDIA_SOURCE;
    MEDIA_LOG_D("ctor called");
}

MediaSourceFilter::~MediaSourceFilter()
{
    MEDIA_LOG_D("dtor called");
    if (taskPtr_) {
        taskPtr_->Stop();
    }
    if (plugin_) {
        plugin_->Deinit();
    }
}

ErrorCode MediaSourceFilter::SetSource(const std::shared_ptr<MediaSource>& source)
{
    MEDIA_LOG_I("SetSource entered.");
    if (source == nullptr) {
        MEDIA_LOG_E("Invalid source");
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    protocol_.clear();
    ErrorCode err = FindPlugin(source);
    if (err != ErrorCode::SUCCESS) {
        return err;
    }
    err = InitPlugin(source);
    if (err != ErrorCode::SUCCESS) {
        return err;
    }
    ActivateMode();
    return DoNegotiate(source);
}

ErrorCode MediaSourceFilter::InitPlugin(const std::shared_ptr<MediaSource>& source)
{
    MEDIA_LOG_D("IN");
    ErrorCode err = TranslatePluginStatus(plugin_->Init());
    if (err != ErrorCode::SUCCESS) {
        return err;
    }
    plugin_->SetCallback(this);
    pluginAllocator_ = plugin_->GetAllocator();
    return TranslatePluginStatus(plugin_->SetSource(source));
}

ErrorCode MediaSourceFilter::SetBufferSize(size_t size)
{
    MEDIA_LOG_I("SetBufferSize, size: %" PUBLIC_LOG "zu", size);
    bufferSize_ = size;
    return ErrorCode::SUCCESS;
}

bool MediaSourceFilter::IsSeekable() const
{
    MEDIA_LOG_D("IN, isSeekable_: %" PUBLIC_LOG "d", static_cast<int32_t>(isSeekable_));
    return isSeekable_;
}

std::vector<WorkMode> MediaSourceFilter::GetWorkModes()
{
    MEDIA_LOG_D("IN, isSeekable_: %" PUBLIC_LOG "d", static_cast<int32_t>(isSeekable_));
    if (isSeekable_) {
        return {WorkMode::PUSH, WorkMode::PULL};
    } else {
        return {WorkMode::PUSH};
    }
}

ErrorCode MediaSourceFilter::Prepare()
{
    MEDIA_LOG_I("Prepare entered.");
    if (plugin_ == nullptr) {
        return ErrorCode::ERROR_INVALID_OPERATION;
    }
    auto err = TranslatePluginStatus(plugin_->Prepare());
    if (err == ErrorCode::SUCCESS) {
        MEDIA_LOG_D("media source send EVENT_READY");
        OnEvent(Event{name_, EventType::EVENT_READY, {}});
    }
    return err;
}

ErrorCode MediaSourceFilter::Start()
{
    MEDIA_LOG_I("Start entered.");
    if (taskPtr_) {
        taskPtr_->Start();
    }
    return plugin_ ? TranslatePluginStatus(plugin_->Start()) : ErrorCode::ERROR_INVALID_OPERATION;
}

ErrorCode MediaSourceFilter::PullData(const std::string& outPort, uint64_t offset, size_t size, AVBufferPtr& data)
{
    MEDIA_LOG_D("IN, offset: %" PUBLIC_LOG PRIu64 ", size: %" PUBLIC_LOG "zu, outPort: %" PUBLIC_LOG "s",
                offset, size, outPort.c_str());
    if (!plugin_) {
        return ErrorCode::ERROR_INVALID_OPERATION;
    }
    ErrorCode err;
    auto readSize = size;
    if (isSeekable_) {
        size_t totalSize = 0;
        if ((plugin_->GetSize(totalSize) == Status::OK) && (totalSize != 0)) {
            if (offset >= totalSize) {
                MEDIA_LOG_W("offset: %" PUBLIC_LOG PRIu64 " is larger than totalSize: %" PUBLIC_LOG "zu",
                            offset, totalSize);
                return ErrorCode::END_OF_STREAM;
            }
            if ((offset + readSize) > totalSize) {
                readSize = totalSize - offset;
            }
            auto realSize = data->GetMemory()->GetCapacity();
            if (readSize > realSize) {
                readSize = realSize;
            }
            MEDIA_LOG_D("totalSize_: %" PUBLIC_LOG "zu", totalSize);
        }
        if (position_ != offset) {
            err = TranslatePluginStatus(plugin_->SeekTo(offset));
            if (err != ErrorCode::SUCCESS) {
                MEDIA_LOG_E("Seek to %" PUBLIC_LOG PRIu64 " fail", offset);
                return err;
            }
            position_ = offset;
        }
    }
    if (data == nullptr) {
        data = std::make_shared<AVBuffer>();
    }
    err = TranslatePluginStatus(plugin_->Read(data, readSize));
    if (err == ErrorCode::SUCCESS) {
        position_ += data->GetMemory()->GetSize();
    }
    return err;
}

ErrorCode MediaSourceFilter::Stop()
{
    MEDIA_LOG_I("Stop entered.");
    if (taskPtr_) {
        taskPtr_->Stop();
    }
    protocol_.clear();
    uri_.clear();
    ErrorCode ret = ErrorCode::SUCCESS;
    if (plugin_) {
        ret = TranslatePluginStatus(plugin_->Stop());
    }
    return ret;
}

void MediaSourceFilter::FlushStart()
{
    MEDIA_LOG_I("FlushStart entered.");
}

void MediaSourceFilter::FlushEnd()
{
    MEDIA_LOG_I("FlushEnd entered.");
}

void MediaSourceFilter::InitPorts()
{
    MEDIA_LOG_D("IN");
    auto outPort = std::make_shared<OutPort>(this);
    outPorts_.push_back(outPort);
}

void MediaSourceFilter::ActivateMode()
{
    MEDIA_LOG_D("IN");
    isSeekable_ = plugin_ && plugin_->IsSeekable();
    if (!isSeekable_) {
        taskPtr_ = std::make_shared<OSAL::Task>("DataReader");
        taskPtr_->RegisterHandler(std::bind(&MediaSourceFilter::ReadLoop, this));
    }
}

ErrorCode MediaSourceFilter::DoNegotiate(const std::shared_ptr<MediaSource>& source)
{
    MEDIA_LOG_D("IN");
    SourceType sourceType = source->GetSourceType();
    if (sourceType == SourceType::SOURCE_TYPE_URI) {
        std::string suffix = GetUriSuffix(source->GetSourceUri());
        if (!suffix.empty()) {
            std::shared_ptr<Plugin::Meta> suffixMeta = std::make_shared<Plugin::Meta>();
            suffixMeta->SetString(Media::Plugin::MetaID::MEDIA_FILE_EXTENSION, suffix);
            size_t fileSize = 0;
            if ((plugin_->GetSize(fileSize) == Status::OK) && (fileSize != 0)) {
                suffixMeta->SetUint64(Media::Plugin::MetaID::MEDIA_FILE_SIZE, fileSize);
            }
            Capability peerCap;
            auto tmpCap = MetaToCapability(*suffixMeta);
            Plugin::TagMap upstreamParams;
            Plugin::TagMap downstreamParams;
            if (!GetOutPort(PORT_NAME_DEFAULT)->Negotiate(tmpCap, peerCap, upstreamParams, downstreamParams) ||
                !GetOutPort(PORT_NAME_DEFAULT)->Configure(suffixMeta)) {
                MEDIA_LOG_E("Negotiate fail!");
                return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
            }
        }
    }
    return ErrorCode::SUCCESS;
}

std::string MediaSourceFilter::GetUriSuffix(const std::string& uri)
{
    MEDIA_LOG_D("IN");
    std::string suffix;
    auto const pos = uri.find_last_of('.');
    if (pos != std::string::npos) {
        suffix = uri.substr(pos + 1);
    }
    return suffix;
}

void MediaSourceFilter::ReadLoop()
{
    MEDIA_LOG_D("IN");
    AVBufferPtr bufferPtr = std::make_shared<AVBuffer>();
    ErrorCode ret = TranslatePluginStatus(plugin_->Read(bufferPtr, 4096)); // 4096: default push data size
    if (ret == ErrorCode::END_OF_STREAM) {
        Stop();
        return;
    }
    outPorts_[0]->PushData(bufferPtr, -1);
}

bool MediaSourceFilter::GetProtocolByUri()
{
    auto const pos = uri_.find("://");
    if (pos != std::string::npos) {
        auto prefix = uri_.substr(0, pos);
        protocol_.append(prefix);
    } else {
        protocol_.append("file");
    }
    auto ret = true;
    if (protocol_ == "file") {
        std::string fullPath;
        ret = OSAL::ConvertFullPath(uri_, fullPath);
        if (ret && !fullPath.empty()) {
            uri_ = fullPath;
        }
    }
    return ret;
}

bool MediaSourceFilter::ParseProtocol(const std::shared_ptr<MediaSource>& source)
{
    bool ret = true;
    SourceType srcType = source->GetSourceType();
    MEDIA_LOG_D("sourceType = %" PUBLIC_LOG "d", OHOS::Media::to_underlying(srcType));
    if (srcType == SourceType::SOURCE_TYPE_URI) {
        uri_ = source->GetSourceUri();
        ret = GetProtocolByUri();
    } else if (srcType == SourceType::SOURCE_TYPE_FD) {
        protocol_.append("fd");
        uri_ = source->GetSourceUri();
    } else if (srcType == SourceType::SOURCE_TYPE_STREAM) {
        protocol_.append("stream");
        uri_.append("stream://");
    }
    MEDIA_LOG_I("protocol: %" PUBLIC_LOG "s, uri: %" PUBLIC_LOG "s", protocol_.c_str(), uri_.c_str());
    return ret;
}

ErrorCode MediaSourceFilter::CreatePlugin(const std::shared_ptr<PluginInfo>& info, const std::string& name,
                                          PluginManager& manager)
{
    if ((plugin_ != nullptr) && (pluginInfo_ != nullptr)) {
        if (info->name == pluginInfo_->name && TranslatePluginStatus(plugin_->Reset()) == ErrorCode::SUCCESS) {
            MEDIA_LOG_I("Reuse last plugin: %" PUBLIC_LOG "s", name.c_str());
            return ErrorCode::SUCCESS;
        }
        if (TranslatePluginStatus(plugin_->Deinit()) != ErrorCode::SUCCESS) {
            MEDIA_LOG_E("Deinit last plugin: %" PUBLIC_LOG "s error", pluginInfo_->name.c_str());
        }
    }
    plugin_ = manager.CreateSourcePlugin(name);
    if (plugin_ == nullptr) {
        MEDIA_LOG_E("PluginManager CreatePlugin %" PUBLIC_LOG "s fail", name.c_str());
        return ErrorCode::ERROR_UNKNOWN;
    }
    pluginInfo_ = info;
    MEDIA_LOG_I("Create new plugin: \"%" PUBLIC_LOG "s\" success", pluginInfo_->name.c_str());
    return ErrorCode::SUCCESS;
}

ErrorCode MediaSourceFilter::FindPlugin(const std::shared_ptr<MediaSource>& source)
{
    if (!ParseProtocol(source)) {
        MEDIA_LOG_E("Invalid source!");
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    if (protocol_.empty()) {
        MEDIA_LOG_E("protocol_ is empty");
        return ErrorCode::ERROR_INVALID_PARAMETER_VALUE;
    }
    PluginManager& pluginManager = PluginManager::Instance();
    std::set<std::string> nameList = pluginManager.ListPlugins(PluginType::SOURCE);
    for (const std::string& name : nameList) {
        std::shared_ptr<PluginInfo> info = pluginManager.GetPluginInfo(PluginType::SOURCE, name);
        MEDIA_LOG_I("name: %" PUBLIC_LOG "s, info->name: %" PUBLIC_LOG "s", name.c_str(), info->name.c_str());
        auto val = info->extra[PLUGIN_INFO_EXTRA_PROTOCOL];
        if (val.SameTypeWith(typeid(std::vector<ProtocolType>))) {
            auto supportProtocols = OHOS::Media::Plugin::AnyCast<std::vector<ProtocolType>>(val);
            for(auto supportProtocol : supportProtocols){
                if (g_protocolStringToType[protocol_] == supportProtocol &&
                    CreatePlugin(info, name, pluginManager) == ErrorCode::SUCCESS) {
                    MEDIA_LOG_I("supportProtocol:%" PUBLIC_LOG "s CreatePlugin %" PUBLIC_LOG "s success",
                                protocol_.c_str(), name_.c_str());
                    return ErrorCode::SUCCESS;
                }
            }
        }
    }
    MEDIA_LOG_I("Cannot find any plugin");
    return ErrorCode::ERROR_UNSUPPORTED_FORMAT;
}
} // namespace Pipeline
} // namespace Media
} // namespace OHOS

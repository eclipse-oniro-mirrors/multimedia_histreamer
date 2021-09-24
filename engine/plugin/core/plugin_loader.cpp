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

#include "plugin_loader.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <Windows.h>
#include <iostream>
#include <utility>
#else
#include <dlfcn.h>
#endif

using namespace OHOS::Media::Plugin;

PluginLoader::PluginLoader(void* handler, std::string name) : handler_(handler), name_(std::move(name))
{
}

PluginLoader::~PluginLoader()
{
    UnLoadPluginFile();
}

std::shared_ptr<PluginLoader> PluginLoader::Create(const std::string& name, const std::string& path)
{
    if (name.empty() || path.empty()) {
        return std::shared_ptr<PluginLoader>();
    }
    return CheckSymbol(LoadPluginFile(path), name);
}

RegisterFunc PluginLoader::FetchRegisterFunction()
{
    return registerFunc_;
}

UnregisterFunc PluginLoader::FetchUnregisterFunction()
{
    return unregisterFunc_;
}

void* PluginLoader::LoadPluginFile(const std::string& path)
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    return ::LoadLibraryA(path.c_str());
#else
    auto pathStr = path.c_str();
    if (pathStr) {
        return ::dlopen(pathStr, RTLD_NOW);
    }
    return nullptr;
#endif
}

std::shared_ptr<PluginLoader> PluginLoader::CheckSymbol(void* handler, const std::string& name)
{
    if (handler) {
        std::string registerFuncName = "register_" + name;
        std::string unregisterFuncName = "unregister_" + name;
        RegisterFunc registerFunc = nullptr;
        UnregisterFunc unregisterFunc = nullptr;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
        registerFunc = (RegisterFunc)(::GetProcAddress((HMODULE)handler, registerFuncName.c_str()));
        unregisterFunc = (UnregisterFunc)(::GetProcAddress((HMODULE)handler, unregisterFuncName.c_str()));
#else
        registerFunc = (RegisterFunc)(::dlsym(handler, registerFuncName.c_str()));
        unregisterFunc = (UnregisterFunc)(::dlsym(handler, unregisterFuncName.c_str()));
#endif
        if (registerFunc && unregisterFunc) {
            std::shared_ptr<PluginLoader> loader(new PluginLoader(handler, name));
            loader->registerFunc_ = registerFunc;
            loader->unregisterFunc_ = unregisterFunc;
            return loader;
        }
    }
    return std::shared_ptr<PluginLoader>();
}

void PluginLoader::UnLoadPluginFile()
{
    if (handler_) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
        ::FreeLibrary((HMODULE)handler);
#else
        ::dlclose(const_cast<void*>(handler_));
#endif
    }
}

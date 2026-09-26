#include "dynamic_library.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace game_face::detail {

DynamicLibrary::~DynamicLibrary()
{
    if (!handle_)
        return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
}

std::string DynamicLibrary::open(const std::filesystem::path& path)
{
#ifdef _WIN32
    // Resolve the library's own dependencies from its directory, not the CWD.
    handle_ = LoadLibraryExW(path.c_str(), nullptr,
                             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (handle_)
        return {};
    return "LoadLibrary failed with error " + std::to_string(GetLastError());
#else
    handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle_)
        return {};
    const char* error = dlerror();
    return error ? error : "dlopen failed";
#endif
}

void* DynamicLibrary::symbol(const char* name) const
{
    if (!handle_)
        return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
#else
    return dlsym(handle_, name);
#endif
}

} // namespace game_face::detail

#pragma once

#include <filesystem>
#include <string>

namespace game_face::detail {

// A shared library opened at runtime (LoadLibrary / dlopen).
class DynamicLibrary {
public:
    DynamicLibrary() = default;
    ~DynamicLibrary();

    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    // Returns an empty string on success, otherwise the OS error.
    std::string open(const std::filesystem::path& path);
    void* symbol(const char* name) const;
    bool isOpen() const { return handle_ != nullptr; }

    template <class Fn>
    bool resolve(Fn& out, const char* name) const
    {
        out = reinterpret_cast<Fn>(symbol(name));
        return out != nullptr;
    }

private:
    void* handle_ = nullptr;
};

} // namespace game_face::detail

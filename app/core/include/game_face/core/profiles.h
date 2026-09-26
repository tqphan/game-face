#pragma once

#include "result.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace game_face {

// Fires start_command when the blendshape rises above threshold, and
// stop_command when it falls back below.
struct SimpleBinding {
    std::string blendshape = "_neutral";
    double threshold = 50;  // 0..100
    std::string start_command;
    std::string stop_command;
};

// Fires command once when expression becomes true (after it has stayed true
// for debounce_ms), re-arming when it becomes false.
struct Trigger {
    std::string expression;
    std::string command;
    double debounce_ms = 0;
};

struct Binding {
    std::string name;
    bool enabled = true;
    bool simplified = true;  // true: `simple`; false: `start`/`stop` triggers
    SimpleBinding simple;
    Trigger start;
    Trigger stop;
};

struct Profile {
    std::string name;
    std::vector<Binding> bindings;
};

struct ProfileSet {
    int selection = 0;  // index into profiles; 0 when empty
    std::vector<Profile> profiles;

    const Profile* selected() const;
};

// Reads the original app's user.profiles.json layout, plus the older layout
// where each binding has start/stop directly (default.profile.json).
// Lenient about numbers stored as strings; runtime-only fields are ignored.
Result<ProfileSet> profilesFromJson(std::string_view json);

// Writes the original layout (so the Python app can still read it) with a
// top-level "schemaVersion": 2 and no runtime state.
std::string profilesToJson(const ProfileSet& profiles);

Result<ProfileSet> loadProfiles(const std::filesystem::path& path);
// Atomic: writes a temporary file and renames it over `path`.
Result<void> saveProfiles(const ProfileSet& profiles, const std::filesystem::path& path);

} // namespace game_face

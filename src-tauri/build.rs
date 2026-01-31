fn main() {
    // Only include and apply the manifest when building for Windows.
    #[cfg(target_os = "windows")]
    {
        // Include both manifests and pick the correct one based on Cargo's PROFILE.
        // Cargo sets PROFILE to "release" for release builds and "debug" for debug builds.
        let manifest_release = include_str!("./app.release.manifest");
        let manifest_debug = include_str!("./app.debug.manifest");
        let profile = std::env::var("PROFILE").unwrap_or_default();
        let manifest = if profile == "release" { manifest_release } else { manifest_debug };

        let mut windows = tauri_build::WindowsAttributes::new();
        windows = windows.app_manifest(manifest);
        let attrs = tauri_build::Attributes::new().windows_attributes(windows);
        tauri_build::try_build(attrs).expect("failed to run build script");
    }

    // For non-Windows targets, run the build script without Windows attributes.
    #[cfg(not(target_os = "windows"))]
    {
        tauri_build::build();
    }
}

#!/usr/bin/env python3
import sys
import os
from ctypes import CDLL, CFUNCTYPE, POINTER, c_int, c_char_p, c_void_p, c_uint, byref

# Tobii constants
TOBII_ERROR_NO_ERROR = 0
TOBII_FIELD_OF_USE_INTERACTIVE = 1

# Define callback type
URL_RECEIVER_CALLBACK = CFUNCTYPE(None, c_char_p, c_void_p)


def test_tobii_installation():
    """Test if Tobii Stream Engine library can be loaded."""
    print("=" * 60)
    print("Tobii Stream Engine Installation Test")
    print("=" * 60)

    # Try to load library
    print("\n1. Loading Tobii Stream Engine library...")

    # Get script directory
    script_dir = os.path.dirname(os.path.abspath(__file__))

    library_names = [
        os.path.join(script_dir, "libtobii_stream_engine.so"),
        os.path.join(script_dir, "tobii_stream_engine.so"),
        "./libtobii_stream_engine.so"
    ]

    print(f"   Script directory: {script_dir}")
    print(f"   Current directory: {os.getcwd()}")
    print(f"   Trying {len(library_names)} possible locations...")

    tobii_lib = None
    loaded_lib = None

    for lib_name in library_names:
        try:
            # Check if file exists first
            if os.path.exists(lib_name):
                print(f"   → Found file: {lib_name}")
                tobii_lib = CDLL(lib_name)
                loaded_lib = lib_name
                print(f"   ✓ Successfully loaded: {lib_name}")
                break
            else:
                # Try loading anyway in case it's in system path
                tobii_lib = CDLL(lib_name)
                loaded_lib = lib_name
                print(f"   ✓ Successfully loaded: {lib_name}")
                break
        except OSError as e:
            # Only show error for files that actually exist
            if os.path.exists(lib_name):
                print(f"   ✗ File exists but failed to load: {lib_name}")
                print(f"      Error: {e}")
            continue

    if tobii_lib is None:
        print("\n❌ FAILED: Could not load Tobii Stream Engine library")
        print("\nSearched locations:")
        for lib_name in library_names:
            exists = "EXISTS" if os.path.exists(lib_name) else "not found"
            print(f"   - {lib_name} [{exists}]")
        print("\nTroubleshooting:")
        print("1. Place libtobii_stream_engine.so in the same directory as this script")
        print("2. Or add library to LD_LIBRARY_PATH:")
        print("   export LD_LIBRARY_PATH=/path/to/tobii/lib:$LD_LIBRARY_PATH")
        print("3. Or create a symlink:")
        print("   sudo ln -s /path/to/libtobii_stream_engine.so /usr/local/lib/")
        print("   sudo ldconfig")
        return False

    # Set up function signatures
    print("\n2. Setting up API function signatures...")
    try:
        tobii_lib.tobii_api_create.argtypes = [POINTER(c_void_p), c_void_p]
        tobii_lib.tobii_api_create.restype = c_int

        tobii_lib.tobii_api_destroy.argtypes = [c_void_p]
        tobii_lib.tobii_api_destroy.restype = c_int

        tobii_lib.tobii_enumerate_local_device_urls.argtypes = [c_void_p, URL_RECEIVER_CALLBACK, c_void_p]
        tobii_lib.tobii_enumerate_local_device_urls.restype = c_int

        print("   ✓ Function signatures configured")
    except Exception as e:
        print(f"   ✗ Failed to set up functions: {e}")
        return False

    # Create API
    print("\n3. Creating Tobii API...")
    api = c_void_p()
    result = tobii_lib.tobii_api_create(byref(api), None)

    if result != TOBII_ERROR_NO_ERROR:
        print(f"   ✗ Failed to create API (error code: {result})")
        return False

    print("   ✓ API created successfully")

    # Enumerate devices
    print("\n4. Searching for Tobii devices...")
    urls = []

    def url_receiver(url_c_str, user_data):
        url = url_c_str.decode('utf-8')
        urls.append(url)

    url_callback = URL_RECEIVER_CALLBACK(url_receiver)
    result = tobii_lib.tobii_enumerate_local_device_urls(api, url_callback, None)

    if result != TOBII_ERROR_NO_ERROR:
        print(f"   ✗ Failed to enumerate devices (error code: {result})")
        tobii_lib.tobii_api_destroy(api)
        return False

    if len(urls) == 0:
        print("   ✗ No Tobii devices found")
        print("\nTroubleshooting:")
        print("1. Ensure eye tracker is connected via USB")
        print("2. Check USB connection: lsusb | grep -i tobii")
        print("3. Try running with sudo: sudo python3 test_tobii.py")
        print("4. Verify Tobii service is running")
        tobii_lib.tobii_api_destroy(api)
        return False

    print(f"   ✓ Found {len(urls)} device(s):")
    for i, url in enumerate(urls, 1):
        print(f"      {i}. {url}")

    # Clean up
    print("\n5. Cleaning up...")
    tobii_lib.tobii_api_destroy(api)
    print("   ✓ Cleanup complete")

    print("\n" + "=" * 60)
    print("✓ ALL TESTS PASSED")
    print("=" * 60)
    print("\nYour Tobii Stream Engine installation is working correctly!")
    print("You can now run the main eye tracker application:")
    print("  python3 eye_tracker.py")

    return True


def main():
    """Main entry point."""
    success = test_tobii_installation()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()

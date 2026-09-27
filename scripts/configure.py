import subprocess
import os.path

build_dir = "build"
build_abis = ["arm64-v8a"]
build_api_level = 24
build_configuration = "Debug"
android_ndk = R"C:\AndroidSdk\ndk\30.0.16248370"
cmake = "cmake.exe"
ninja = "ninja.exe"
cxx_flags = "-s -Qunused-arguments"

cmake_toolchain = os.path.join(android_ndk, "build/cmake/android.toolchain.cmake")

for abi in build_abis:
    subprocess.call([
        "cmake.exe",
        "-GNinja",
        f"-B{os.path.join(build_dir, abi)}",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE",
        "-DCMAKE_SKIP_INSTALL_RULES=TRUE",
        "-DCMAKE_SYSTEM_NAME=Android",
        f"-DCMAKE_SYSTEM_VERSION={build_api_level}",
        f"-DCMAKE_ANDROID_ARCH_ABI={abi}",
        f"-DCMAKE_ANDROID_NDK={android_ndk}",
        f"-DCMAKE_TOOLCHAIN_FILE={cmake_toolchain}",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        f"-DCMAKE_CXX_FLAGS={cxx_flags}",
        f"-DCMAKE_BUILD_TYPE={build_configuration}",
        f"-DANDROID_PLATFORM=android-{build_api_level}",
        f"-DANDROID_ABI={abi}",
        f"-DANDROID_NDK={android_ndk}",
        "-DANDROID_STL=c++_static",
    ])
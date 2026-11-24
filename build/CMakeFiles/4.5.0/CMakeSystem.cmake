set(CMAKE_HOST_SYSTEM "Linux-7.0.0-1020-azure")
set(CMAKE_HOST_SYSTEM_NAME "Linux")
set(CMAKE_HOST_SYSTEM_VERSION "7.0.0-1020-azure")
set(CMAKE_HOST_SYSTEM_PROCESSOR "x86_64")

set(_CMAKE_INPUT_TOOLCHAIN_FILE [==[/usr/local/lib/android/sdk/ndk/28.2.13676358/build/cmake/android.toolchain.cmake]==])
set(_CMAKE_SYSTEM_TOOLCHAIN_FILE "/usr/local/lib/android/sdk/ndk/28.2.13676358/build/cmake/android.toolchain.cmake")
include("${_CMAKE_SYSTEM_TOOLCHAIN_FILE}")

set(CMAKE_SYSTEM "Android-1")
set(CMAKE_SYSTEM_NAME "Android")
set(CMAKE_SYSTEM_VERSION "1")
set(CMAKE_SYSTEM_PROCESSOR "aarch64")

set(CMAKE_CROSSCOMPILING "TRUE")

set(CMAKE_SYSTEM_LOADED 1)

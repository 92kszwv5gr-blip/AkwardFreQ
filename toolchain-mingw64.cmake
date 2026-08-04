# CMake toolchain for cross-compiling AkwardFreQ's Windows VST3 from Linux
# using mingw-w64. Not part of the normal build (see README) — this exists
# specifically to attempt a real Windows binary from a Linux-only CI/dev
# container, without a Windows machine or Visual Studio available.
#
# One-time environment fix needed before building (see README's "Tried it"
# section for why): mingw-w64 only ships lowercase windows.h, but part of
# the VST3 SDK includes <Windows.h>, which fails to resolve on a
# case-sensitive filesystem.
#   ln -s windows.h /usr/x86_64-w64-mingw32/include/Windows.h
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}-ar)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_CXX_STANDARD 17)

# Without this, every binary dynamically links libgcc_s_seh-1.dll,
# libstdc++-6.dll, and libwinpthread-1.dll from the mingw runtime — DLLs a
# real Windows machine (and Ableton) won't have installed, so the plugin
# would fail to load with a missing-DLL error. Static-link the toolchain's
# own runtime so the only external runtime dependency left is onnxruntime.dll
# (already documented as something that ships alongside the plugin).
set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static -lwinpthread")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static -lwinpthread")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static -lwinpthread")

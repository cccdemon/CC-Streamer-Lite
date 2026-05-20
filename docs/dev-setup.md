# Developer Setup

## Required Tools

- Windows 10/11 x64.
- Visual Studio 2022 Build Tools with C++ workload.
- CMake 3.20 or newer.
- Ninja.
- Vulkan SDK.

## Optional Later Dependencies

- FFmpeg development libraries with libopus and libsrt support.
- Windows SDK capture headers.

## Configure

```powershell
cmake --preset vs2019-x64-debug
```

## Build

```powershell
cmake --build --preset vs2019-x64-debug
```

## Run

```powershell
.\build\vs2019-x64\Debug\CCStreamer.exe
```

## Notes

The current app is a Win32 foundation shell. Capture, encoding, and streaming modules are intentionally not implemented yet.

# Developer Setup

## Required Tools

- Windows 10/11 x64.
- Visual Studio 2022 Build Tools with C++ workload.
- CMake 3.24 or newer.
- Ninja.
- Qt 6 with Widgets.
- Vulkan SDK.

## Optional Later Dependencies

- FFmpeg development libraries with libopus and libsrt support.
- Windows SDK capture headers.

## Configure

```powershell
cmake --preset windows-debug
```

## Build

```powershell
cmake --build --preset windows-debug
```

## Run

```powershell
.\build\windows-debug\CCStreamer.exe
```

## Notes

The current app is a foundation shell. Capture, encoding, and streaming modules are intentionally not implemented yet.


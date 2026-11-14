# rtxdiasm-nvapi

ACuda NVAPI compatibility library, split from ACuda as a standalone Windows-only module.

## Outputs
- **64-bit:** `nvapi64.dll`
- **32-bit:** `nvapi.dll`

Linux is not supported.

## Build (Windows)
```powershell
./build_windows.ps1 -Configuration Release
```
Or configure/build with CMake for `Win32`/`x64`.

## Copyright and authorship
Copyright 2026 by Magnus Olsen.

This code had assistance by models DeepFlash 4.1, Muse Spark 1.3, and Iceman.

## License
MIT (see `LICENSE`).

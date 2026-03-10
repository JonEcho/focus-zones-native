# focus-zones-native

Dynamic focus-ratio window manager for Windows. Pure C / Win32. No runtime dependencies.

## What it does

When you focus a window in a column, it expands to claim more vertical space (default 75%) while sibling windows shrink. This is the core differentiator — no other tiling manager does asymmetric focus-ratio resizing.

## Building

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## Config

Edit `config.json` next to the executable. Changes are hot-reloaded.

## License

MIT

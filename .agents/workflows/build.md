---
description: Build and run focus-zones-native C project
---

# Build & Run focus-zones-native

## Prerequisites

Ensure cJSON vendor files exist. If not:

// turbo
1. Download cJSON vendor files:
```
cd vendor/cJSON && curl -o cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h && curl -o cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
```

## Build

// turbo
2. Generate CMake build files:
```
cmake -B build -G "Visual Studio 17 2022"
```

// turbo
3. Build Release:
```
cmake --build build --config Release
```

## Run

4. Run the executable:
```
.\build\Release\focus_zones.exe
```

## Build Debug

// turbo
5. Build Debug (for development):
```
cmake --build build --config Debug
```

## Clean Rebuild

// turbo
6. Clean and rebuild:
```
Remove-Item -Recurse -Force build; cmake -B build -G "Visual Studio 17 2022"; cmake --build build --config Release
```

## Plan: JSON Config Upload And Runtime Migration

Enable filesystem-backed JSON config on the ESP32 in two phases: first guarantee JSON files are uploaded with firmware workflow, then migrate runtime config loading from compile-time headers to JSON with strict fail-fast behavior. This keeps rollout controlled while matching your long-term goal of replacing Config.h/Secrets.h sources.

**Steps**
1. Phase 1 - Filesystem and upload pipeline baseline
2. Update [platformio.ini](platformio.ini) to enable LittleFS data partition handling in the active env (`board_build.filesystem = littlefs`) and ensure upload targets for firmware and filesystem are available. *Blocks step 3.*
3. Move JSON assets into runtime upload directory by creating [data/wifi_credentials.json](data/wifi_credentials.json) and [data/bus_targets.json](data/bus_targets.json) (keep existing [config/wifi_credentials.json](config/wifi_credentials.json) and [config/bus_targets.json](config/bus_targets.json) only as editable source templates if desired). *Depends on 2.*
4. Decide developer workflow command convention and document it in [README.md](README.md): `uploadfs` for config updates, `upload` for firmware, and a combined upload sequence for first-time flashing. *Parallel with step 5 after step 2.*
5. Add gitignore rules for sensitive runtime config source copies (if source templates stay under [config/](config/), keep [config/wifi_credentials.json](config/wifi_credentials.json) ignored). *Parallel with step 4.*
6. Phase 2 - Runtime loader and fail-fast behavior
7. Add config-loader module (new files under include/src, for example [include/ConfigLoader.h](include/ConfigLoader.h) and [src/ConfigLoader.cpp](src/ConfigLoader.cpp)) to mount LittleFS, read `/wifi_credentials.json` and `/bus_targets.json`, deserialize via ArduinoJson, validate required fields, and return typed runtime config.
8. Integrate loader into startup flow in [src/main.cpp](src/main.cpp): initialize filesystem early, load/validate JSON, and if any required field is invalid, display an explicit error state and halt (per your fail-fast decision). *Depends on 7.*
9. Replace compile-time target initialization path in [src/main.cpp](src/main.cpp) that currently copies `MY_TARGETS` into mutable array, with JSON-provided targets and runtime target count (bounded by a defined max target cap). *Depends on 8.*
10. Replace compile-time Wi-Fi credential use in [src/main.cpp](src/main.cpp) and downstream initialization path with loaded JSON credentials passed to existing network setup. *Depends on 8.*
11. Preserve non-config constants in [include/Config.h](include/Config.h) temporarily (debug toggles and feature flags), but remove credentials/targets from runtime path and mark as deprecated comments for later cleanup. *Parallel with steps 9-10 once loader works.*
12. Phase 3 - Cleanup and source-of-truth hardening
13. Update docs to define JSON as the runtime source of truth and explain migration: [README.md](README.md), [include/README](include/README), and optional new config docs file.
14. Add explicit serial logs and display messages for these states: FS mount failure, JSON parse failure, schema validation failure, and successful config load counts.
15. Optional hardening: add schema version field (for example `"version": 1`) in both JSON files and validate it to avoid future breaking changes.

**Relevant files**
- [platformio.ini](platformio.ini) - enable LittleFS and upload behavior.
- [src/main.cpp](src/main.cpp) - startup orchestration, runtime config loading, fail-fast halt behavior.
- [include/Config.h](include/Config.h) - retain only non-runtime static defaults; deprecate targets/credentials runtime use.
- [include/Secrets.h](include/Secrets.h) - remove from active runtime path in final migration phase.
- [include/Structs.h](include/Structs.h) - reuse `WifiCredentialsData` and `BusTarget`; add runtime config container if needed.
- [src/Network/NetworkManager.cpp](src/Network/NetworkManager.cpp) and [include/Network/NetworkManager.h](include/Network/NetworkManager.h) - verify integration still receives credentials cleanly.
- [config/wifi_credentials.json](config/wifi_credentials.json) and [config/bus_targets.json](config/bus_targets.json) - editable templates/source files.
- [data/wifi_credentials.json](data/wifi_credentials.json) and [data/bus_targets.json](data/bus_targets.json) - files actually uploaded to device filesystem.
- [README.md](README.md) - flashing and configuration instructions.
- [.gitignore](.gitignore) - protect secrets.

**Verification**
1. Run filesystem upload and verify successful image creation and flash in terminal logs.
2. Boot device and confirm serial log reports LittleFS mount success and config load success with target count.
3. Corrupt [data/wifi_credentials.json](data/wifi_credentials.json) intentionally and confirm fail-fast behavior (error shown and no Wi-Fi connect attempts).
4. Corrupt [data/bus_targets.json](data/bus_targets.json) and confirm fail-fast behavior before fetcher starts.
5. Restore valid files and verify normal bus fetch/render loop resumes.
6. Validate config-only update workflow by changing JSON values, running filesystem upload, rebooting, and confirming new values apply without recompiling core logic.

**Decisions**
- Filesystem: LittleFS.
- Error handling: fail-fast on missing/invalid JSON (no fallback to Config.h/Secrets.h).
- Scope included now: planning for upload pipeline plus migration path.
- Scope excluded for this iteration: OTA update strategy, remote config editing UI, and encrypted secrets at rest.

**Further Considerations**
1. Runtime limits recommendation: cap JSON targets to a safe max (for example 10) and reject files that exceed it to avoid memory pressure.
2. Operational recommendation: keep [config/](config/) as local editable source and copy/sync into [data/](data/) as pre-upload step to avoid accidental device/runtime mismatch.
3. Future cleanup recommendation: once stable, remove credentials/targets from [include/Config.h](include/Config.h) and [include/Secrets.h](include/Secrets.h) entirely to prevent dual-source drift.

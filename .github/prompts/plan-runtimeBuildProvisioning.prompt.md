## Plan: Runtime Build Provisioning Portal

Build an explicit-call provisioning flow that starts SoftAP + captive portal, waits up to 40s for first client connection, collects runtime-build inputs, writes 3 LittleFS JSON files, then attempts STA connection as best-effort and returns status to caller. No auto-start behavior will be added unless requested later.

**Steps**
1. Phase 1: Define final JSON contracts and versioning.
2. Keep 3 files exactly as agreed: [data/build_state.json](data/build_state.json), [data/wifi_credentials.json](data/wifi_credentials.json), and a renamed build-info file replacing [data/bus_targets.json](data/bus_targets.json).
3. State payload rules in build-info:
4. BUS_BY_STATION: stationId only.
5. BUS_BY_LINES: stationId + line numbers only.
6. NY_METRO_BY_STATION: same input shape as BUS_BY_STATION.
7. USE_CURRENT_BUILD: no extra fields required in UI.
8. Phase 2: Refactor persistence and loading for the new file naming/schema.
9. Update manager-side save/load paths in [src/RuntimeBuild/ConfigManager.cpp](src/RuntimeBuild/ConfigManager.cpp) and declarations in [include/RuntimeBuild/ConfigManager.h](include/RuntimeBuild/ConfigManager.h).
10. Add Wi-Fi credentials save API in manager so portal submit can persist credentials.
11. Refactor runtime loader in [src/RuntimeBuild/ConfigLoader.cpp](src/RuntimeBuild/ConfigLoader.cpp) and [include/RuntimeBuild/ConfigLoader.h](include/RuntimeBuild/ConfigLoader.h) to parse build-info conditionally by BuildState.
12. Update build-state dependent consumption checks in [src/RuntimeBuild/Builder.cpp](src/RuntimeBuild/Builder.cpp) and [include/RuntimeBuild/Builder.h](include/RuntimeBuild/Builder.h).
13. Phase 3: Add new portal module (.h + .cpp in Network folders).
14. Implement one public blocking function to run provisioning session and return result enum.
15. Inside that function: start AP, DNS captive redirect, HTTP server routes, and client-connection timer.
16. Enforce timeout rule: if no client connects in 40s, stop AP/server and return timeout/failure.
17. Serve form UI reusing/extending [include/Network/PortalStubHtml.h](include/Network/PortalStubHtml.h) for BuildState-specific fields.
18. On submit: validate by mode, save all JSON files first, then try STA connect best-effort, then return result.
19. Phase 4: Integration and naming cleanup.
20. Wire explicit trigger callsite in [src/main.cpp](src/main.cpp) (manual invocation only).
21. Replace remaining bus_targets naming references in loader/manager/docs and update [README.md](README.md).

**Relevant files**
- [include/RuntimeBuild/ConfigManager.h](include/RuntimeBuild/ConfigManager.h)
- [src/RuntimeBuild/ConfigManager.cpp](src/RuntimeBuild/ConfigManager.cpp)
- [include/RuntimeBuild/ConfigLoader.h](include/RuntimeBuild/ConfigLoader.h)
- [src/RuntimeBuild/ConfigLoader.cpp](src/RuntimeBuild/ConfigLoader.cpp)
- [include/RuntimeBuild/Builder.h](include/RuntimeBuild/Builder.h)
- [src/RuntimeBuild/Builder.cpp](src/RuntimeBuild/Builder.cpp)
- [include/Network/PortalStubHtml.h](include/Network/PortalStubHtml.h)
- [src/main.cpp](src/main.cpp)
- [data/build_state.json](data/build_state.json)
- [data/wifi_credentials.json](data/wifi_credentials.json)
- [data/bus_targets.json](data/bus_targets.json)
- [README.md](README.md)

**Verification**
1. Compile with PlatformIO and confirm no broken references to old bus-target APIs/paths.
2. Timeout scenario: run provisioning, do not connect client, confirm return at ~40s and AP shutdown.
3. Captive scenario: connect to AP, confirm portal is reachable via captive redirect.
4. Submit BUS_BY_STATION and verify saved state + Wi-Fi + build-info content parse successfully.
5. Submit BUS_BY_LINES and verify stationId + lines persist and parse.
6. Submit NY_METRO_BY_STATION and verify stationId-only path works.
7. Submit USE_CURRENT_BUILD and verify no extra field requirement.
8. Confirm save-first-then-connect behavior by testing both connect success and failure paths.
9. Confirm normal app path remains unchanged when provisioning function is not called.

**Decisions captured**
- Keep 3 output files.
- Rename bus-target config to build-info with state-dependent schema.
- NY_METRO_BY_STATION is selectable and reuses stationId-only input.
- AP starts only when explicit function is called.
- 40s with no client returns failure/timeout.
- On submit: save JSON first, then attempt STA best-effort.

If this plan looks right, approve and I’ll hand off for implementation.

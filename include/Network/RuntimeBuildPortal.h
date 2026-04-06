#ifndef RUNTIME_BUILD_PORTAL_H
#define RUNTIME_BUILD_PORTAL_H

#include <Arduino.h>

#include "RuntimeBuild/Builder.h"

#define RUNTIME_BUILD_PORTAL_NO_CLIENT_TIMEOUT_MS 40000UL
#define RUNTIME_BUILD_PORTAL_STA_CONNECT_TIMEOUT_MS 10000UL

enum RuntimeBuildPortalResult {
    PORTAL_SAVE_AND_CONNECT_OK = 0,
    PORTAL_SAVE_OK_CONNECT_FAILED,
    PORTAL_TIMEOUT_NO_CLIENT,
    PORTAL_VALIDATION_ERROR,
    PORTAL_INTERNAL_ERROR,
};

/**
 * @brief Starts AP + captive portal runtime-build provisioning flow.
 *
 * The function returns when:
 * 1) no client connects to the AP within RUNTIME_BUILD_PORTAL_NO_CLIENT_TIMEOUT_MS,
 * 2) a user submits valid config (save-first, then STA best-effort connect), or
 * 3) an internal/validation error occurs.
 */
RuntimeBuildPortalResult runRuntimeBuildPortal();

#endif // RUNTIME_BUILD_PORTAL_H

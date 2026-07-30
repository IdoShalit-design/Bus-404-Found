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
    PORTAL_TIMEOUT_NO_SUBMIT,
    PORTAL_INTERNAL_ERROR,
};

/**
 * @brief Starts AP + captive portal runtime-build provisioning flow.
 *
 * The function returns when:
 * 1) no client connects to the AP within RUNTIME_BUILD_PORTAL_NO_CLIENT_TIMEOUT_MS,
 * 2) a client stays connected but never submits, before the session cap elapses,
 * 3) a user submits valid config (save-first, then STA best-effort connect), or
 * 4) an unrecoverable internal error occurs.
 *
 * Rejected submissions are not terminal: the portal answers with HTTP 400 and keeps
 * serving so the user can correct the form and resubmit.
 */
RuntimeBuildPortalResult runRuntimeBuildPortal();

#endif // RUNTIME_BUILD_PORTAL_H

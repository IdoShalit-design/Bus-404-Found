#ifndef RUNTIME_BUILD_PORTAL_H
#define RUNTIME_BUILD_PORTAL_H

#include <Arduino.h>
#include <functional>

#include "RuntimeBuild/Builder.h"

enum RuntimeBuildPortalResult {
    PORTAL_SAVE_AND_CONNECT_OK = 0,
    PORTAL_SAVE_OK_CONNECT_FAILED,
    PORTAL_TIMEOUT_NO_CLIENT,
    PORTAL_TIMEOUT_NO_SUBMIT,
    PORTAL_INTERNAL_ERROR,
};

/**
 * @brief Shows a short line of text to whoever is standing at the device.
 *
 * The portal has no display of its own; the caller supplies this so the AP
 * passphrase can be read off the panel. Kept as a callback rather than an
 * IRenderer so this header stays independent of the display layer.
 */
using PortalNotifyFn = std::function<void(const char* message)>;

/**
 * @brief Starts AP + captive portal runtime-build provisioning flow.
 *
 * The function returns when:
 * 1) no client connects to the AP within the idle timeout,
 * 2) a client stays connected but never submits, before the session cap elapses,
 * 3) a user submits valid config (save-first, then STA best-effort connect), or
 * 4) an unrecoverable internal error occurs.
 *
 * Rejected submissions are not terminal: the portal answers with HTTP 400 and keeps
 * serving so the user can correct the form and resubmit.
 *
 * @return How the session ended. Callers are expected to act on this: the two
 *         timeouts mean the saved config still stands and boot should continue,
 *         while the failure results should be surfaced to the user, since the
 *         device will otherwise look stalled with no explanation.
 *
 * @param notify Called with the AP credentials once the access point is up, so
 *               the user can join it. May be empty, in which case the
 *               credentials are only written to the serial log.
 */
RuntimeBuildPortalResult runRuntimeBuildPortal(PortalNotifyFn notify);

#endif // RUNTIME_BUILD_PORTAL_H

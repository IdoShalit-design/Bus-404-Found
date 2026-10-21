#ifndef RUNTIME_BUILD_PORTAL_H
#define RUNTIME_BUILD_PORTAL_H

// Deliberately self-contained: this header declares one enum, one callback alias
// and one function. It does not include Builder.h, which would pull in both the
// fetcher and renderer hierarchies for the sake of a single method call.
#include <functional>

/**
 * @brief How a provisioning session ended.
 */
enum class RuntimeBuildPortalResult {
    /// Config was written and the device joined the user's network.
    SaveAndConnectOk,
    /// Config was written, but the device could not join the network it names.
    SaveOkConnectFailed,
    /// Nobody associated with the AP; whatever was on flash still stands.
    TimeoutNoClient,
    /// A client associated but never completed the form before the session cap.
    TimeoutNoSubmit,
    /// The filesystem or the access point could not be brought up.
    InternalError,
};

/**
 * @brief Shows a short line of text to whoever is standing at the device.
 *
 * The portal has no display of its own, but it must reach the person holding the
 * board: nothing else signals that the device is waiting in setup mode, which
 * network to join, or where to point a browser once joined. The panel is the only
 * channel the user is guaranteed to have, and the caller supplies it here.
 *
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
 * @param notify Called with the network to join once the access point is up, and
 *               again with the portal address once a client has associated. May
 *               be empty, in which case both are only written to the serial log.
 * @return How the session ended. Callers are expected to act on this: the two
 *         timeouts mean the saved config still stands and boot should continue,
 *         while the failure results should be surfaced to the user, since the
 *         device will otherwise look stalled with no explanation.
 */
RuntimeBuildPortalResult runRuntimeBuildPortal(PortalNotifyFn notify);

#endif // RUNTIME_BUILD_PORTAL_H

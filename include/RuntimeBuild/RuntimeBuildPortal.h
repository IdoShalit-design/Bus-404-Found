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
 * board: the AP passphrase is generated per-device from the MAC, so it cannot be
 * printed in a manual and the panel is the only channel the user is guaranteed to
 * have. The caller supplies that channel here.
 *
 * ## Why a callback and not an IRenderer*
 *
 * Least privilege. IRenderer exposes init(), render(), clear(), setBrightness()
 * and showMessage(); the portal needs exactly one of those. Handing over the whole
 * interface would let this code re-init the panel mid-provisioning or render bus
 * arrivals over the setup screen -- bugs that are worth making unrepresentable
 * rather than merely discouraged. A one-argument callback is the narrowest type
 * that still expresses "emit a line of text", so no other capability leaks in.
 *
 * Passing a callback also keeps this header independent of the display layer, so
 * the portal stays reusable on a device whose status channel is not an IRenderer
 * at all -- a serial console or an OLED would satisfy this signature unchanged.
 *
 * ## Cost
 *
 * std::function is type-erased, so it costs one indirect call per invocation --
 * paid once per boot here, on a path that already waits up to 40s for a phone. The
 * canonical caller passes a lambda capturing only `this` (4 bytes on this target),
 * which fits std::function's small-object buffer, so there is no heap allocation.
 *
 * ## Contract for implementers
 *
 * `message` is valid only for the duration of the call. Callers may build it from
 * a temporary, so an implementation that needs the text later must copy the bytes,
 * not store the pointer. Implementations should return promptly: the portal is
 * mid-startup with an AP up and a DNS server running.
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
 * @param notify Called with the AP credentials once the access point is up, so
 *               the user can join it. May be empty, in which case the
 *               credentials are only written to the serial log.
 * @return How the session ended. Callers are expected to act on this: the two
 *         timeouts mean the saved config still stands and boot should continue,
 *         while the failure results should be surfaced to the user, since the
 *         device will otherwise look stalled with no explanation.
 */
RuntimeBuildPortalResult runRuntimeBuildPortal(PortalNotifyFn notify);

#endif // RUNTIME_BUILD_PORTAL_H

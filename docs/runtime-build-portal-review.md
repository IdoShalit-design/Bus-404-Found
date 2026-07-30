# RuntimeBuildPortal — code review and fixes

Review of `src/RuntimeBuild/RuntimeBuildPortal.cpp`, `include/RuntimeBuild/RuntimeBuildPortal.h`
and their integration into the program (`App.cpp`, `ConfigLoader`, `ConfigManager`,
`data/portal/index.html`).

**Date:** 2026-07-30 · **Branch:** `auto-fix-issues` · **Issues:** #29–#46 (all closed)
· **Commits:** `fbf6a9f`..`a738c5b`, one per issue · **Build:** clean from scratch after every commit

---

## Summary

18 findings, all fixed. Seven were behavioural defects, one was a security
weakness, and ten were design or cleanliness problems in code that worked.

| Severity | Count | Issues |
|---|---|---|
| Correctness — user-visible failure | 6 | #29 #30 #31 #32 #33 #34 |
| Security | 1 | #35 |
| Design / maintainability | 11 | #36–#46 |

Three of the correctness bugs (#29, #30, #31) could each leave a device
unusable without a power cycle or a reflash, and all three were reachable on a
first-run provisioning attempt.

The headline structural change: `runRuntimeBuildPortal()` went from **~260 lines
to 39**, and the `BuildState` ↔ string mapping went from **five copies in four
files to one**.

---

## Correctness

### #29 — Portal shut down after a single validation error
`fbf6a9f`

Every validation failure in `/submit` set `submissionState.finished`, so the main
loop tore down the AP on its next iteration. One typo — an empty station ID, a
half-filled line pair — ended provisioning for good: the user got a 400 page, the
network they were connected to vanished, and only a power cycle brought it back.
This was the most likely first-run experience.

**Fix.** Split the nine copy-pasted fail-and-respond blocks into two helpers that
make the policy explicit: `rejectSubmission()` (400, portal keeps serving, session
state untouched) and `abortSubmission()` (500, session ends — flash write failures
only). `PORTAL_VALIDATION_ERROR` was removed from the result enum, since a rejected
submission is no longer a terminal outcome and the value could never be returned.

> The uniformity of the bug across all nine branches was itself the tell: the
> behaviour had been copy-pasted rather than decided. That is why #36 exists.

### #30 — A connected-but-idle client could block boot forever
`dfb06f5`

The 40 s no-client timeout was reset on every loop iteration in which a station
was associated, so it could never fire while a client stayed connected. Since the
portal runs *before* `loadConfig()`/`connectWifi()`/`buildPipeline()`, any phone
that auto-joined the open AP and then idled would hang the device on "Loading..."
indefinitely.

**Fix.** Two clocks: `sessionStart`, never reset, capped at `kMaxSessionMs`
(5 min); and `idleStart`, reset by client activity, capped at the existing
no-client timeout. Added `TimeoutNoSubmit` to distinguish "someone connected but
never finished" from "nobody showed up".

### #31 — "Keep current build" could brick the next boot
`1f6ac73`

Selecting `USE_CURRENT_BUILD` wrote `build_state.json` and reported success
without checking that the state it defers to exists. On a device with no
`last_concrete_build_state.json` — i.e. a fresh one — the next boot failed in
`loadRuntimeConfig()` and `App::loadConfig()` called `halt()`, an infinite delay
loop escapable only by reflashing. One click, permanently bricked boot, with a
success message.

**Fix.** Added `validateBuildInfoConfig()` to `ConfigLoader`, which runs the
*same parser boot uses* against a throwaway config so the check cannot drift from
the real one. The portal calls it via `hasKeepableBuild()` **before persisting
anything** and rejects the submission if the fallback is missing or unusable.

### #32 — Portal result was logged and discarded
`a6a9f4a`

`App::setup()` printed the result and then proceeded identically for all five
outcomes. A portal internal error resurfaced later as an unrelated config error
message; a save-but-cannot-connect result presented as a silent 20 s hang.

**Fix.** Added `portalResultToDisplayMessage()` beside the existing
`wifiFailureToDisplayMessage()`. Failures still don't stop boot — the saved
config may be fine, and `loadConfig()` already halts with a specific message when
it isn't — but they are now explained on the panel.

### #33 — `stationId` and Wi-Fi fields were not trimmed
`33ecb5d`

Only `parseLinePairs()` trimmed. `isLengthValid()` only rejects the empty string,
so a whitespace-only station ID was **accepted in station mode and rejected in
line mode**. It reached `build_info.json` and the device queried the API forever
for a blank station, showing "No data" with no diagnostic.

**Fix.** All text fields route through one `readTrimmedArg()`.

> The Wi-Fi **password** is a deliberate exception: whitespace can be meaningful,
> and it is typed into a masked field where trimming is invisible. The reasoning
> is recorded on the helper so the exception doesn't later read as an oversight.

### #34 — Form-field arrays not tied to `MAX_RUNTIME_TARGETS`
`cf0b69a`

`kLineStationFields[MAX_RUNTIME_TARGETS]` had only three initialisers. Raising
that constant would zero-fill the extra elements and hand `nullptr` to
`WebServer::arg()` — no compiler warning, and nothing at the change site pointing
here.

**Fix.** Size the arrays from their initialisers and `static_assert` the count.
**Verified** by temporarily setting the constant to 4: the build fails with
`Add a station field name per runtime target, here and in data/portal/index.html`,
a message that also points at the HTML no assert can cover.

---

## Security

### #35 — Open AP, and `/wifi-status` disclosed the saved SSID
`d24d758`

The provisioning AP was started with `WiFi.softAP(kApSsid)` — the single-argument,
**open network** overload — and `/wifi-status` answered any unauthenticated caller
with the SSID of the network the device belongs to. For the whole of every boot,
anyone in range could learn the home network name and rewrite
`wifi_credentials.json`, `build_state.json` and `build_info.json`.

**Fix, both halves:**

- **WPA2**, with a passphrase derived from the last three bytes of the interface
  MAC — deterministic (same every boot), unique per device (one leak doesn't open
  every unit), and derived rather than stored (no secret in the repo).
- The passphrase reaches the user through a new `PortalNotifyFn` callback that
  `App` wires to `showMessage()`. A callback rather than an `IRenderer` keeps the
  portal header free of the display layer.
- `/wifi-status` now returns only `{"hasSaved":<bool>}` — all the form needs to
  offer its toggle. `index.html` no longer renders an SSID.

---

## Design and maintainability

### #36 — A 260-line function with nine copies of the same block
`d56d31b`

`runRuntimeBuildPortal()` covered mount, radio teardown, AP bring-up, six route
registrations (one a 130-line lambda), the event loop and cleanup.

**Now 39 lines**, delegating to `startProvisioningAp` / `registerRoutes` /
`runPortalLoop`. The submit handler became `handleSubmit()`, expressed as three
steps — `selectBuildState`, `resolveWifiCredentials`, `persistSelection` — each
returning a `StepResult` (`Ok` / `Rejected` / `Failed`), with `applyStep()`
holding the recoverable-vs-terminal policy in one place. `StepResult` mirrors the
existing `ConfigResult` in `ConfigLoader.h` rather than inventing a convention.

### #37 — Four identical captive-portal redirect handlers
`db47bf5`

`/generate_204`, `/hotspot-detect.html`, `/fwlink` and `onNotFound` each carried
a byte-for-byte identical body. The subtle reasoning for answering with a 302
instead of the response each OS expects was documented on only one of the four.
Now one `redirectToPortal` lambda registered from a list, with the explanation on
the single implementation.

### #38 — Teardown differed on every early-return path
`3f08f08`

Four exit paths restored three different subsets of the radio state: two returned
with the chip still in `WIFI_AP_STA`, and the LittleFS path left auto-reconnect
off entirely. It only worked because `App::connectWifi()` happens to call
`WiFi.mode(WIFI_STA)` itself — an undocumented dependency on the caller cleaning
up. A `RadioRestore` scope guard now makes every return restore identical state.

### #39 / #40 / #41 — Header cleanup
`8272cd9` `802fb37` `f9da413`

- Timeout `#define`s → `constexpr` in the .cpp's anonymous namespace (neither is
  part of the caller contract). Follows #24.
- `RuntimeBuildPortalResult` → `enum class` with unprefixed, individually
  documented enumerators. Follows #23. Removing the now-redundant `default:` in
  `portalResultToDisplayMessage()` means a future enumerator becomes a `-Wswitch`
  warning.
- Dropped `Arduino.h` and `Builder.h`, neither of which the header used.
  `Builder.h` was dragging `IBusFetcher.h`, `IRenderer.h` and `<memory>` into
  every consumer that just wanted an enum.

### #42 — `parseLinePairs` had six parameters and a `0` error sentinel
`6386628`

Four out-params, buffers the caller had to declare with matching bounds (a
mismatch would be a silent overrun, not a compile error), two arrays that were
pure ceremony (`stationPtrs[i]` was only ever `stationStorage[i]`), and `0`
overloaded as both "no pairs" and "error" with the reason arriving via a separate
out-param that could be left null on its way to `Serial.printf("%s")`.

Now `parseLinePairs(server)` returns one `LineTargets` owning its storage, count
and `nullptr`-on-success error — the `ConfigResult` shape again.

### #43 — `BuildState` ↔ string mapping duplicated five times
`996fb6b`

Two character-identical `parseBuildState` copies, a `String`-based variant in the
portal, and two `buildStateToString` copies whose `default` branches disagreed in
a **load-bearing** way: `ConfigManager` returned `nullptr` so it could refuse to
persist an unknown state, `App` returned `"UNKNOWN"` for logging. Every switch had
a `default`, so no `-Wswitch` warning would flag a new enumerator — adding a mode
meant editing five sites to avoid shipping a mode the portal offers but boot
rejects.

New `RuntimeBuild/BuildStateNames.{h,cpp}` holds one table. `grep '"BUS_BY_STATION"'`
now matches exactly one file. `App` keeps its logging fallback as a thin
`buildStateForLog()` wrapper, preserving the `nullptr` contract for callers that
persist.

### #44 — Dead code
`b31ecaf`

- `include/PortalStubHtml.h` (73 lines) — no includers anywhere; superseded by
  `data/portal/index.html`, from which its field names had silently diverged.
- `saveBuildInfoCurrentConfig()` — not merely unused but **unusable**: it
  overwrites `build_info.json` with `{"useCurrentBuild":true}`, destroying exactly
  the payload the `USE_CURRENT_BUILD` path needs. Re-confirmed before deleting
  that `useCurrentBuild` is read nowhere in the tree.
- `saveBusTargetsConfig()` — a "backward-compatible" writer with no caller left to
  be compatible with, hardcoding `3` instead of `MAX_RUNTIME_TARGETS`.

### #45 — Magic numbers
`de88ad2`

Nine literals named. The two `delay(100)` calls got *separate* names
(`kRadioDisconnectSettleMs`, `kModeChangeSettleMs`) because they wait for
different things and only coincidentally share a value.

### #46 — `buildResultHtml` didn't escape its message
`a738c5b`

Not exploitable today — every caller passes a literal — but the signature reads
as "render this text" while the body only safely accepts literals, and the
obvious next change (echoing the offending value back) would introduce reflected
XSS on an over-the-air page with no review signal. Added `escapeHtml()` so the
function is safe for any caller rather than relying on a remembered rule.

---

## Cross-cutting themes

**Copy-paste hid the decisions.** Three of the largest findings (#29, #37, #43)
share a root cause: logic duplicated 4–9 times, so the behaviour was inherited
rather than chosen. In #43 the copies had already silently diverged.

**The portal blocks boot, which raised the stakes.** `runRuntimeBuildPortal()`
runs before every other startup step, so #30 (no absolute timeout) and #31
(unvalidated `USE_CURRENT_BUILD`) were not degraded-experience bugs — they were
device-unusable bugs.

**Compile-time enforcement replaced convention.** #34 (`static_assert`), #40 and
#43 (removed `default:` labels enabling `-Wswitch`) each convert a class of
future runtime failure into a build error.

## Verification

Every commit was validated with a full `pio run` for `esp32-s3-devkitc-1`, plus a
from-scratch rebuild at the end: **SUCCESS**, no warnings in any touched file.
Flash 30.0% → 29.9%, RAM unchanged at 14.2%.

The `static_assert` in #34 was additionally verified to *fire* by temporarily
raising `MAX_RUNTIME_TARGETS`, then reverting.

No runtime/on-device testing was performed — the fixes are validated by
compilation and code inspection only. The Wi-Fi behaviour changes in #30, #35 and
#38 in particular touch radio state that only hardware can confirm.

## Follow-ups not addressed

Out of scope for this review, but noted while reading:

- `data/portal/index.html` still hardcodes three station/line pairs; no assert can
  reach it (#34's message points a maintainer there, which is the best available).
- `AppConfig` in `Builder.h` is unused and still carries `#define`-style constants
  (`WIFI_CREDENTIAL_MAX_LEN`, `MAX_BUS_TARGETS`, `STATION_ID_MAX_LEN`) that
  duplicate the `Structs.h` / `ConfigLoader.h` limits.
- Wi-Fi credentials are stored in plaintext on LittleFS — inherent to the current
  design, worth a decision rather than a silent default.

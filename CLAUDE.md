# PX4-Autopilot-3WD-Prod — DYX 3WD production firmware

PX4 firmware for the **DYX 3WD precision ground-marking rover** (Pixhawk 6X).
Safety-critical C/C++. This file is the only preloaded context.

## Repo facts

- **Repo:** `Vetri2425/PX4-Autopilot-3WD-Prod` (plain new repo, **NOT** a GitHub fork — the
  account's fork network for `PX4/PX4-Autopilot` is already occupied by the customized
  `Vetri2425/PX4-Autopilot` used for the *old* v1.16.2 3WD firmware. Content/history here is
  a byte-identical mirror of upstream instead).
- **Pinned base:** PX4 `v1.17.0` stable == commit `d6f12ad1c4f70ad3230afd7d86e971421e02fef4`.
  Verified `git describe --tags` == `v1.17.0` exactly (not RC/beta); present on `release/1.17`
  and `stable`, not `main`. Same pinned base as the 4WD repo.
- **Production branch:** `dyx-3wd-production`, branched from the pinned commit.
- **Target hardware:** Pixhawk 6X — production CI target is **`px4_fmu-v6x_rover`**
  (switched from `px4_fmu-v6x_default` in `1416913c85`).
- **Upstream remote:** `upstream` → `https://github.com/PX4/PX4-Autopilot.git` (for reviewing
  future PX4 fixes selectively — never merge `upstream/main` wholesale).
- Firmware: roadmap F1 code complete at `b19901b004` (2026-10-07). See "Current status".
- **GPS-driver submodule** `src/drivers/gps/devices` → **`Vetri2425/PX4-GPSDrivers`** (a real GitHub
  fork of `PX4/PX4-GPSDrivers`), branch `dyx-3wd-production`, based on upstream `0b96958` (the
  commit v1.17.0 pins). Local clone: `3WD_PROD/PX4-GPSDrivers-3WD-Prod` (remote `upstream` = PX4).
  **Fork rule:** minimal — only audited deviations, one per commit, each followed by its own
  submodule-bump commit here. Prefer cherry-picking an upstream fix over writing our own.
  Deviations so far: `80fe20d` (C3, no receiver configuration).

## ⛔ PX4 must not auto-configure the GNSS receiver (hard requirement, 2026-10-07)

The UM982 owns its production configuration in its own persistent memory (`SAVECONFIG`).
PX4 **only consumes** GNSS / heading data and **injects RTCM**. It must never send receiver
configuration — not at boot, not on reconnect, not on heading loss.

- **Fixed in code (C3), `7b583c9eb9`:** fork commit `80fe20d` removes
  `GPSDriverNMEA::request_unicore_messages()`, its call and `_unicore_heading_received_last`. Stock
  v1.17 wrote 6 unsaved log commands (`GPGGA`, `UNIAGRICA`, `UNIHEADINGA` @ 0.2 s; `GPGST`, `GPGSA`,
  `GPRMC` @ 1 s, hardcoded `COM1`) on every `UNIAGRICA` while heading was missing > 1 s. Never
  reintroduce it or add a parameter that does. **Bench acceptance still pending** (contract below).
- Compliant: RTCM injection (`gps.cpp:625`); NMEA `configure()` only probes PX4's own baud.
- ⚠ **Parameter dependency:** `GPS_1_PROTOCOL` must be **6 (NMEA)**. Stock default is 1 (u-blox):
  the UBX driver writes its configuration frames to the UM982 at every (re)connect; 0 (auto)
  cycles UBX/MTK/Ashtech/… and never tries NMEA. Open decision for the human: change the rover
  default to 6 in firmware, or enforce it as a bench-checklist item.
- Consequence of C3: if the receiver's saved configuration lacks `UNIHEADINGA`, heading is now
  simply absent (no PX4 repair) — the companion health check must flag it.
- Open GNSS fixes (C2 restart cycle, C3 config spam, F7 variance-as-σ) are judged under this
  rule: **never fix a reconnect/timeout/heading-loss problem by reconfiguring the receiver.**
- Receiver message set/rates to store, and acceptance test: `DYX_3WD/docs/contracts/GNSS_receiver_configuration.md`.

## ⚠ How this repo differs from the 4WD baseline

`PX4-Autopilot-4WD-Prod-Baseline` stays pristine. **This one does not.** It is the vehicle for
re-anchoring the 3WD rover patch set from v1.16.2 onto v1.17.0 — see
`3WD_PROD/DYX_3WD/docs/architecture/DYX_3WD_Production_Stack_Architecture_V1.md`, Track F.

⛔ **Never `cp`-overlay files in CI.** The old 3WD firmware
(`Vetri2425/PX4-Autopilot` + `build_rover.yml`) copied 27 fork files onto a clean v1.16.2
checkout without committing. Consequences already paid: `ver_sw` can never identify a build
(every build reports base hash `54f0455f`), `git log`/`blame` lie about every overlaid file,
and it produced a confident-but-wrong "`DifferentialVelControl` isn't compiled" audit of a
module that was flying. **Patches land here as real commits on `dyx-3wd-production`.**

Re-anchor by diffing each overlaid file against its **v1.16.2 stock ancestor** and re-applying
that semantic diff — never by copying the v1.16.2 file onto v1.17.0. EKF2 is not a stable API
across 1.16 → 1.17: `common.h`, `ekf.h` and `control.cpp` all moved.

### Planned patch set (Track F1 — all code landed 2026-10-07; field gates pending)

| Patch | Origin | Notes |
|---|---|---|
| EKF2 wheel-encoder fusion (13 files) | `255453d967` lever-arm fix | **Not upstream in v1.17.** Field-verified: pivot wobble 1.52 → 0.50 cm median, net walk 2.02 → 0.83 cm, 34 ulogs. |
| RoboClaw QPPS + timestamp fix | `1d82e616f8` | **QPPS half done in F1.1 (`14e3fb88b2`).** Timestamp half still pending (F1.4): taken *before* the UART transaction; closes ~264 ms jitter into WENC fusion. |
| GNSS-yaw innovation floor | `015c67484e` + `a6e9e12e2a` | Adds `EKF2_GPS_YAW_N` + `EKF2_GPS_YAW_G`. Neither is upstream — v1.17 has only `EKF2_GPS_YAW_OFF`. |
| Logger topics | `9641ea9a99`, `53940ead83` | `wheel_encoders`, `estimator_aid_src_wheel_encoder` from cold boot. |
| `rover.px4board` | new | Add `CONFIG_MODULES_UXRCE_DDS_CLIENT=y` — **absent from every stock `rover.px4board`**. |
| `dds_topics.yaml` | new | Add `/fmu/in/gps_inject_data` (RTCM over DDS — 2 lines; `gps_inject_data` is already subscribed by `src/drivers/gps/gps.cpp`). Optionally `ulog_stream`/`ulog_stream_ack`. |

**GATE 2:** every re-anchored EKF2 change must be replay-verified against the existing log
corpus *before* flashing. Pass criterion is decision-identical `estimator_aid_src_*` output at
default parameters — not "it builds".

## Current status — 2026-10-07 (night)

**Roadmap F1 firmware code complete. CI-verified, GATE 2 replay-verified where replay applies.**

⚠ **NOTHING HAS BEEN FLASHED OR HARDWARE-VALIDATED.** Candidate for the first flash:
**`7b583c9eb9`** — `3WD_PROD/PX4-Firmware/3WD/7b583c9eb9-fix-gps-use-dyx-gps-driver-fork-px4-never-configures-the-um982/`
(FLASH 1,789,004 B = 90.99 %, AXI_SRAM 19.22 %, 0 compiler warnings, CI run 37618859283,
`.px4` sha256 `304b22bdfbb59033df15012d687f07d352cd01d3ddc62112c35cec24ab779483`).

| Item | Commit | Evidence |
|---|---|---|
| CI → `px4_fmu-v6x_rover` | `1416913c85` | CI |
| F1.1 RoboClaw QPPS 35/36, raw UART, select, 255, deadbands, `RBCLW_QPPS_MAX`=0 | `14e3fb88b2` | CI |
| F1.2 logger `wheel_encoders` + all multi-EKF WENC aid-source instances | `385034b7ef`, `809b3561c0` | CI |
| F1.3 WENC fusion + IMU lever arm; `fuseBodyFrameVelocity` → `aid_sources/body_velocity_fusion.cpp` | `b5189bd734` | GATE 2 neutrality + compatibility vs v1.16.2 |
| F1.4A RoboClaw encoder timestamp before UART | `07bcfdc601` | CI (replay can't test timing) |
| F1.4B `EKF2_GPS_YAW_N` / `EKF2_GPS_YAW_G` | `1cc5c364b8` | GATE 2: reproduces 08-05 failure, floor fixes it, 0 reverse actions |
| F1.5 RTCM over DDS `/fmu/in/gps_inject_data` (PublicationMulti) | `07741c2f26` | CI |
| C5 DDS reconnect after agent restart (upstream #26848 backport) | `1bc34ef933` | CI |
| F1.8/C3 ULog streaming over DDS | `9f07777a32` | CI |
| F1.7 explicit rover setpoints in OFFBOARD velocity mode (path A) | `b19901b004` | CI; GATE 1 bench pending |
| A1 RoboClaw RX resync (`tcflush`) | `420768d812` | CI |
| A2 speed-only encoder reads (`wheel_angle` = NaN) | `4eb9465e3c` | CI |
| C3 GNSS ownership: GPS-driver fork `80fe20d`, no receiver configuration | `7b583c9eb9` | CI (fork checked out in CI log); object −464 B, no `COM1` strings; bench pending |

Facts that changed the plan:
- `uxrce_dds_client`, Ethernet and netman (fallback 10.41.10.2) are **already** in the v6x rover
  build — `rover.px4board` is a variant of `default.px4board`. No board change was needed for DDS.
- F1.6 board: WENC Kconfig is in; spray valve output deferred (parameter only).
- F1.7 B2 decisions + explicit-control contract: `DYX_3WD/docs/contracts/F1.7_B2_firmware_decisions.md`.
  Rows 15/16 close only after GATE 1 bench.
- Defaults are flash-safe: `RBCLW_QPPS_MAX=0` (no motion), `EKF2_WENC_CTRL=0`,
  `EKF2_GPS_YAW_G=0`. Keep WENC off until `EKF2_IMU_POS_*` is re-measured on the 6X mount.

**Since the F1 batch:** A1 RoboClaw RX resync `420768d812`, A2 speed-only encoder reads
(`wheel_angle` NaN) `4eb9465e3c` and C3 `7b583c9eb9` — all CI green, archived. A9 (WENC must not refresh global velocity-fusion timers) is **HELD**: neutral in
replay but its mechanism was never observed and `_time_last_hor_vel_fuse` also drives
dead-reckoning classification — patch in the working tree + `PX4-Firmware/3WD/_held_patches/`.
Heading-fault recovery (replay): inherent EKF2 reset-on-continuous-rejection logic, identical in
v1.16.2 / stock v1.17 / current; a mostly-accepted heading fault can persist ~190 s unflagged —
mitigate in the companion (`reject_yaw`, GNSS-yaw test ratio).

**Still open for fine tracking** (from `PX4_DXP/docs/FIRMWARE_PENDING_PATCHES.md`, confirmed in
v1.17): A1 RoboClaw serial never resyncs (no `tcflush`), A2 encoder-read decimation,
**A9 WENC refreshes the global velocity-fusion timers (masks GNSS loss, blocks GSF yaw rescue —
fix before enabling WENC)** — HELD; A6 no-slip constraint during pivots (investigation). A1/A2/C3 done.
C1/C4/F5 are resolved by dropping the always-landed land-detector patch.

**GNSS work order (human, 2026-10-07):** C3 ✅ → **F7** → **C2** → A6/A11 investigation. Each its
own commit, fork commit + separate submodule bump.
- **F7** (variance used as σ in `s_variance_m_s` from `UNIAGRICA`): already fixed upstream —
  **cherry-pick `PX4/PX4-GPSDrivers` `2fb6c6b` (#234)** onto the fork (σ = sqrt of the sum of the
  three per-axis variances). Do not write a custom version. Feeds EKF2 `sacc` (`EKF2.cpp:2558`).
- **C2** (restart cycle): NMEA `receive()` returns −1 after 500 ms with no useful packet
  (`nmea.cpp` "abort after timeout"); the **outer driver in this repo** (`src/drivers/gps/gps.cpp`,
  receive loop ~`:1013`, then UART close, 500 ms sleep, reopen + baud probe) tears the link down.
  RTCM is only injected from `pollOrRead()`, so injection pauses ≥ 500 ms. The fix therefore lands
  at least partly here in `gps.cpp`, not only in the fork. Recover local parser/link state only —
  never by writing to the receiver.

**GATE 2 replay how-to:** v1.16.2 logs need a byte-level `SensorGps` converter for v1.17
replay (else zero GNSS fusion); replay is approximate-timestamp and run-to-run nondeterministic
on every binary. Judge neutrality by `estimator_states` bit identity, not compare_ab "interior".

### ✅ The build-identification problem is solved, and proven

The first artifact records **`git_identity = f3de5d1`** — *our* commit.

Every build from the old `Vetri2425/PX4-Autopilot` fork recorded base hash `54f0455f`
regardless of content, because CI `cp`'d files onto a tagged checkout without committing.
That is why `ver_sw` could never identify a build and a flash was undetectable from
firmware version or FCU parameters. Building from a real committed tree fixes it
structurally. **Do not regress this.**

### ⚠ Workflow trigger quirk — first push is SLOW, not silent

After the initial 481 MB push, GitHub took **~20 minutes** to index the repository. During
that window `gh workflow list` returned nothing and no run appeared, so the first build was
started manually with `gh workflow run`. The queued push event then fired on its own at
20:21 UTC — producing **two runs of the same commit** `f3de5d1ccd` (dispatch `33914115176`,
push `33915833210`).

**The correct action on a large first push is to wait, not to dispatch.** An earlier version
of this note said workflows "do not auto-trigger" on a first push; that was wrong, and acting
on it is what created the duplicate.

`paths-ignore` is confirmed working: the `CLAUDE.md`-only commits `c0e429d918` and
`ac2e9173a2` triggered no run at all.

### F1 progress, per `DYX_3WD/docs/Firmware/F-tasks.md`

The 39 commits of the old v1.16.2 fork were audited: **13 must carry, 7 re-evaluate,
19 drop**. Roughly half does not survive — mostly CI scaffolding for the abolished
`cp`-overlay build, plus CubeOrange board files superseded by v1.17's stock
`fmu-v6x/rover.px4board`.

```
F1.1  drivetrain      RoboClaw QPPS, raw mode, creep, deadband   DONE 14e3fb88b2
F1.2  instrumentation logger topics                              DONE 385034b7ef + 809b3561c0
F1.3  estimator       WENC fusion + IMU lever arm                DONE b5189bd734   GATE 2 passed
F1.4  GNSS yaw        EKF2_GPS_YAW_N/_G + encoder timestamp      DONE 07bcfdc601 + 1cc5c364b8   GATE 2 passed
F1.5  transport       DDS client on rover target + RTCM over DDS DONE 07741c2f26 (DDS client already on v6x rover) + C5 1bc34ef933
F1.6  board           px4_fmu-v6x_rover                          DONE (1416913c85 target, WENC Kconfig in F1.3); spray pin deferred
F1.7  decisions       B2 rows → recorded decisions + F1.7 gate   DONE b19901b004; rows 15/16 close at GATE 1 bench
F1.8  optional        ULog streaming over DDS                    DONE 9f07777a32
```

Two verified facts that shape F1.1 and F1.7:
- **The RoboClaw QPPS patch was needed — now applied in F1.1 (`14e3fb88b2`).** Stock v1.17's
  `setMotorSpeed()` sent `DriveForwardMotor1` via `sendUnsigned7Bit` — open-loop. Upstream's
  entire v1.16.2→v1.17 diff for that driver is 4 insertions / 10 deletions.
- **Two rows must not be re-applied blind.** `RoverLandDetector` grew waypoint-distance
  logic upstream, and `mission_block.cpp` now has `VEHICLE_TYPE_ROVER` handling at line
  213 that v1.16.2 lacked.

### Upstream blockers tracked against this firmware

- **#27514** (`risk:safety-critical`) — stale setpoint applied ~900 ms after an external
  process dies. 31 cm at 0.35 m/s.
- **#27497** — rover differential does not turn in Mission Mode on v1.17 stable.
- **#27388** — `uxrce_dds_client` silently stops publishing; only an FC reboot recovers.
- **#28519** — timesync takes 5–10 min to converge; ~40 ms offset ≈ 1.4 cm.
- **#27860** — DDS client never retries if the agent is absent at boot. Our exact boot order.

---

## Build — CI only, one commit → one trigger → one artifact

```sh
git add -A && git commit      # ONE commit per logical change
git push origin dyx-3wd-production   # auto-triggers exactly one workflow
```

`.github/workflows/build_fmu_v6x.yml` is the only active workflow — every other stock PX4
workflow (Build all targets, Checks, EKF Update Change Indicator, SITL Tests, …) is disabled
via `gh workflow disable`, **not deleted or edited**, so upstream source stays pristine. This
guarantees exactly one CI run and one firmware artifact per push.

Build container: `ghcr.io/px4/px4-dev:v1.16.0-rc1-258-g0369abd556` — the same pinned toolchain
PX4's own release CI uses for this target. **Do not build with a bare `ubuntu-*` runner +
`Tools/setup/ubuntu.sh`** — Ubuntu's apt-packaged `gcc-arm-none-eabi` produces larger code and
overflows `px4_fmu-v6x_default`'s FLASH by ~16 KB even on unmodified source (confirmed on the
4WD repo 2026-09-03: apt → FLASH 100.82 %, fails; pinned container → 99.70 %, builds).

```sh
gh run watch <run-id> --repo Vetri2425/PX4-Autopilot-3WD-Prod --exit-status
gh run download <run-id> --repo Vetri2425/PX4-Autopilot-3WD-Prod --dir /tmp/dl
```

## Local artifact archive

After every successful build, copy the `.px4` into:

```
3WD_PROD/PX4-Firmware/3WD/<short-sha>-<slugified-commit-message>/
  px4_fmu-v6x_rover.px4     # px4_fmu-v6x_default.px4 for builds before 1416913c85
  build_info.txt   # SHA, branch, message, target, CI run URL
```

One subfolder per successful build — **never overwrite a prior build's folder.**

Builds are namespaced **per vehicle**: `3WD_PROD/PX4-Firmware/3WD/` here, `Way_to_Mark/PX4-Firmware/4WD/` for
`Vetri2425/PX4-Autopilot-4WD-Prod-Baseline`. Both vehicles pin the same base (`v1.17.0` ==
`d6f12ad1c4`) and build the same target name, so a flat archive would be ambiguous.

The artifact directory name is the **only** thing that identifies a build. `ver_sw` cannot —
CI checks out a tagged base, so the recorded PX4 version is identical across every build.
Preserve the naming.

⚠ **No local build.** Submodules are not initialized here on purpose — this machine's network
stalls on PX4's larger submodules (`Tools/simulation/*`), which GitHub's runners do not hit.
CI is the only build path. To inspect submodule source, `git submodule update --init <path>`
one at a time, never `--recursive`.

**No local production builds — the CI artifact is authoritative.** On 2026-10-07 a one-off local
compile check of `px4_fmu-v6x_rover` was run during F1.1 (Homebrew gcc 9-2020-q2, not the pinned
container), which initialized 12 non-simulation submodules and created the git-ignored `build/`
here. That build is **not** an artifact and must never be flashed or archived.

## Commit rules

- One commit per logical change. One push == one CI trigger == one artifact folder.
- Conventional commits: `type(scope): description`.
- **No AI attribution in commit messages** — no `Co-Authored-By: Claude`, no `Generated
  with` footer. Matches `PX4-Autopilot-4WD-Prod-Baseline` and the old 3WD fork. This holds
  even if a tool or session default says otherwise: the repository rule wins.

## Do not

- Do not merge `upstream/main` wholesale, or code from `Vetri2425/PX4-Autopilot` (the old
  v1.16.2 3WD fork) as-is — re-anchor by semantic diff instead.
- Do not re-enable disabled stock workflows unless a specific one is needed — each adds
  another trigger per push and breaks the 1:1 commit→artifact rule.
- Do not `cp`-overlay in CI. Ever. See above.

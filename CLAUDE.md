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
- Firmware changes so far: F1.1 RoboClaw drivetrain only (`14e3fb88b2`). No EKF2, DDS topic,
  logger or board-config changes yet.

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

### Planned patch set (Track F1 — F1.1 drivetrain applied, rest pending)

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

## Current status — 2026-10-07

**v1.17.0 + CI (rover target) + F1.1 RoboClaw drivetrain. Nothing else applied.**

⚠ **THIS ARTIFACT HAS NOT BEEN FLASHED OR HARDWARE-VALIDATED YET.** "CI green" proves it
compiles and links; it says nothing about motor behaviour on the vehicle.

| | |
|---|---|
| HEAD | `14e3fb88b2` — `feat(roboclaw): add closed-loop QPPS drivetrain control` |
| CI workflow | `.github/workflows/build_fmu_v6x.yml` → `make px4_fmu-v6x_rover` |
| CI target switch | `1416913c85` — `ci(build): switch 3WD production CI to fmu-v6x rover` ([run 37590295682](https://github.com/Vetri2425/PX4-Autopilot-3WD-Prod/actions/runs/37590295682), green; stock open-loop RoboClaw, not a field candidate) |
| F1.1 build | ✅ green, [run 37591482228](https://github.com/Vetri2425/PX4-Autopilot-3WD-Prod/actions/runs/37591482228) — `Roboclaw.cpp` compiled, 0 compiler warnings |
| F1.1 memory | FLASH 1,784,812 B / 1920 KB = **90.78%** · AXI_SRAM 100,700 B / 512 KB = **19.21%** · SRAM4 2 KB / 64 KB = 3.12% (+272 B FLASH vs `1416913c85`) |
| Artifact | `3WD_PROD/PX4-Firmware/3WD/14e3fb88b2-feat-roboclaw-add-closed-loop-qpps-drivetrain-control/` — **production candidate, NOT FLASHED** |
| Workflows | 1 active (`Build px4_fmu-v6x_rover`), **27 disabled** via `gh workflow disable` |

### F1.1 drivetrain — what `14e3fb88b2` does (verified in source + CI)

- RoboClaw Drive-With-Signed-Speed commands **35/36** (closed-loop QPPS on the RoboClaw's
  encoders) replace open-loop opcodes 0/1/4/5 in `setMotorSpeed()`.
- **`RBCLW_QPPS_MAX`**, default **0** — a fresh flash commands zero speed. Must be set to the
  Motion Studio autotune max QPPS before the wheels will move.
- `RBCLW_MAX1`/`RBCLW_MAX2` default **255** (was 256), so armed-zero lands exactly on center 128.
- UART configured raw 8-bit binary mode.
- Fresh local `fd_set` + `timeval` before every `select()`.
- Zero-command deadbands: ~0.03 normalized in `updateOutputs()`, ~0.01 backstop in
  `setMotorSpeed()`; input constrained to [-1, +1]; signed int32 QPPS sent big-endian via its
  `uint32_t` bit pattern.
- v1.17 `OutputModuleInterface` API unchanged (no `stop_motors` arg; disarm = mixer value 128).
- **Not** ported: encoder timestamp (`1d82e616`, → F1.4), baud fallback (`b4fa9acf`), startup
  retry (`bfe914ce`).

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
F1.1  drivetrain      RoboClaw QPPS, raw mode, creep, deadband   COMPLETE / CI VERIFIED / NOT FLASHED  (14e3fb88b2)
F1.2  instrumentation logger topics                              NEXT  ← BEFORE the estimator work, or GATE 2 is unevaluable
F1.3  estimator       WENC fusion + IMU lever arm                pending  ⟵ GATE 2
F1.4  GNSS yaw        EKF2_GPS_YAW_N/_G + encoder timestamp      pending  ⟵ GATE 2
F1.5  transport       DDS client on rover target + RTCM over DDS pending
F1.6  board           px4_fmu-v6x_rover                          CI target switched (1416913c85); board-config changes pending
F1.7  decisions       7 re-evaluate rows → one recorded decision each   pending
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

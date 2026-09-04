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
- **Target hardware:** Pixhawk 6X — currently building `px4_fmu-v6x_default`.
- **Upstream remote:** `upstream` → `https://github.com/PX4/PX4-Autopilot.git` (for reviewing
  future PX4 fixes selectively — never merge `upstream/main` wholesale).
- First commit adds CI only. No firmware source, drivers, DDS topics, parameters or board
  config modified yet.

## ⚠ How this repo differs from the 4WD baseline

`PX4-Autopilot-4WD-Prod-Baseline` stays pristine. **This one does not.** It is the vehicle for
re-anchoring the 3WD rover patch set from v1.16.2 onto v1.17.0 — see
`Way_to_Mark/DYX_3WD/docs/architecture/DYX_3WD_Production_Stack_Architecture_V1.md`, Track F.

⛔ **Never `cp`-overlay files in CI.** The old 3WD firmware
(`Vetri2425/PX4-Autopilot` + `build_rover.yml`) copied 27 fork files onto a clean v1.16.2
checkout without committing. Consequences already paid: `ver_sw` can never identify a build
(every build reports base hash `54f0455f`), `git log`/`blame` lie about every overlaid file,
and it produced a confident-but-wrong "`DifferentialVelControl` isn't compiled" audit of a
module that was flying. **Patches land here as real commits on `dyx-3wd-production`.**

Re-anchor by diffing each overlaid file against its **v1.16.2 stock ancestor** and re-applying
that semantic diff — never by copying the v1.16.2 file onto v1.17.0. EKF2 is not a stable API
across 1.16 → 1.17: `common.h`, `ekf.h` and `control.cpp` all moved.

### Planned patch set (Track F1 — none applied yet)

| Patch | Origin | Notes |
|---|---|---|
| EKF2 wheel-encoder fusion (13 files) | `255453d967` lever-arm fix | **Not upstream in v1.17.** Field-verified: pivot wobble 1.52 → 0.50 cm median, net walk 2.02 → 0.83 cm, 34 ulogs. |
| RoboClaw QPPS + timestamp fix | `1d82e616f8` | Timestamp taken *before* the UART transaction; closes ~264 ms jitter into WENC fusion. `src/drivers/roboclaw` **does** exist in v1.17. |
| GNSS-yaw innovation floor | `015c67484e` + `a6e9e12e2a` | Adds `EKF2_GPS_YAW_N` + `EKF2_GPS_YAW_G`. Neither is upstream — v1.17 has only `EKF2_GPS_YAW_OFF`. |
| Logger topics | `9641ea9a99`, `53940ead83` | `wheel_encoders`, `estimator_aid_src_wheel_encoder` from cold boot. |
| `rover.px4board` | new | Add `CONFIG_MODULES_UXRCE_DDS_CLIENT=y` — **absent from every stock `rover.px4board`**. |
| `dds_topics.yaml` | new | Add `/fmu/in/gps_inject_data` (RTCM over DDS — 2 lines; `gps_inject_data` is already subscribed by `src/drivers/gps/gps.cpp`). Optionally `ulog_stream`/`ulog_stream_ack`. |

**GATE 2:** every re-anchored EKF2 change must be replay-verified against the existing log
corpus *before* flashing. Pass criterion is decision-identical `estimator_aid_src_*` output at
default parameters — not "it builds".

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
Way_to_Mark/PX4-Firmware/3WD/<short-sha>-<slugified-commit-message>/
  px4_fmu-v6x_default.px4
  build_info.txt   # SHA, branch, message, target, CI run URL
```

One subfolder per successful build — **never overwrite a prior build's folder.** The 4WD
archive lives flat under `PX4-Firmware/`; 3WD builds are namespaced under `PX4-Firmware/3WD/`.

The artifact directory name is the **only** thing that identifies a build. `ver_sw` cannot —
CI checks out a tagged base, so the recorded PX4 version is identical across every build.
Preserve the naming.

⚠ **No local build.** Submodules are not initialized here on purpose — this machine's network
stalls on PX4's larger submodules (`Tools/simulation/*`), which GitHub's runners do not hit.
CI is the only build path. To inspect submodule source, `git submodule update --init <path>`
one at a time, never `--recursive`.

## Commit rules

- One commit per logical change. One push == one CI trigger == one artifact folder.
- Conventional commits: `type(scope): description`.

## Do not

- Do not merge `upstream/main` wholesale, or code from `Vetri2425/PX4-Autopilot` (the old
  v1.16.2 3WD fork) as-is — re-anchor by semantic diff instead.
- Do not re-enable disabled stock workflows unless a specific one is needed — each adds
  another trigger per push and breaks the 1:1 commit→artifact rule.
- Do not `cp`-overlay in CI. Ever. See above.

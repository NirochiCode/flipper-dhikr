# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

## [1.0.3] – 2026-11-23

Menus, navigation and a new app icon. No new statistics, same settings format.

### Changed
- **Main menu:** `Start Counting`, `Dhikr Profiles`, `Feedback Settings`, `About`.
  `Target Settings` and `Reset Counter` were removed. Both always applied to the active
  profile; they are now part of each profile's details.
- **Profile details** (hold OK on a profile, or `Edit Profiles...`), header = profile name:
  `View Progress`, `Edit Target (N)`, `Rename Profile`, `Reset Progress`, `Delete Profile`.
  Built-in profiles offer View Progress, Edit Target and Reset Progress. Opening or using the
  details never changes the active profile.
- **Edit Profiles list:** shows only the profile names. Built-in profiles no longer carry
  the `(fixed)` suffix; they still cannot be renamed or deleted.
- **Edit Target** opens the familiar 33 / 99 / custom choice for exactly that profile, saves
  at once and returns to its details.
- **Counter screen:** the progress bar shows the percentage again (as in v1.0.2, without the
  former `Rounds: N`). At the target it shows `33/33 Target reached!` for about 1 s, records
  one completed set and restarts at 0, as before.
- Wording: a completed pass is called a **set** (`Completed Sets`, `Set undone`, dialogs,
  About page).

### Added
- **New app icon:** a crescent with a small five-pointed star (10×10, 1-bit), generated
  from a fixed pixel matrix by `scripts/generate_icon.py` (also validates the file).
- **Screenshots:** unmodified qFlipper captures of v1.0.3 in `screenshots/` (README gallery
  and Apps Catalog set), checked by `scripts/check_screenshots.py`.
- **Apps Catalog texts:** `docs/catalog-description.md` and `docs/catalog-changelog.md`.
- **View Progress** (read-only) per profile: `Current: 15/100`, `Completed Sets: 4`,
  `Target: 100`. Uses the already stored rounds; no new data.
- **Reset Progress** per profile: dialog `Reset progress?` with the profile name and its
  count/target/sets, **No** pre-selected, Back cancels. Yes sets count and sets of that profile
  to 0, clears its undo information, keeps target, name and id, leaves all other profiles and
  the active profile unchanged, saves at once and returns to the same profile's details.

### Fixed
- **Reset changed the wrong profile from the user's point of view.** Investigation of the
  previous build: the reset logic itself was correct. It set count **and** sets to 0 and
  cleared undo. But the only reset, `Reset Counter` in the main menu, always acted on the
  *active* profile, not on the profile highlighted in the profile list. Highlighting a profile
  does not activate it, so a reset could hit a different profile than intended. The reset now
  exists only inside a profile's details and is applied by the profile's stable id
  (`dhikr_state_reset_profile`), never through the active-profile state.

### Unchanged
- Settings format v3: completed sets are still stored as `profile.<id>.rounds`. A file
  written by v1.0.2 is read as-is and written back byte-identical (tested). No migration.
- Counting, automatic restart, undo, the four feedback switches, stealth handling, profile
  limits and all confirmations.

### Tests and build
- 74 host unit tests (50 in v1.0.2). New: U1–U12 (menus, Edit Target, v1.0.2 file
  compatibility) and P1–P11 (profile details, View Progress, per-profile reset including
  inactive profiles, undo after reset, cancel, persistence, save failure, percentage text,
  `Target reached!`) and P12 (Edit Profiles labels without `(fixed)`). Checked with intentionally broken code (5 variants, all detected).
- GCC `-fanalyzer` (29 firmware sources) and clang `--analyze` (logic): no findings.
- Momentum `mntm-012` (API 87.1) and official `1.5.1` (API 88.2): build without warnings,
  APPCHK passed. Output in `dist/v1.0.3/`.

### Hardware
- **The current v1.0.3 build is not yet validated on hardware.** An earlier v1.0.3 build was
  tried on the device; that test led to the reset and counter-screen changes above.

## [1.0.2] – 2026-11-22

### Added
- **Independent feedback switches:** `Counter Vibration`, `Counter Sound`,
  `Target Vibration` and `Target Sound`, each ON/OFF. Any combination works, e.g. silent
  counting with vibration + sound only at 33/33.
- Feedback Settings menu: `Counter Feedback >`, `Target Feedback >` (each with Vibration,
  Sound and Preview; OK toggles), stealth status and Help.

### Changed
- **Settings format version 3:** four `feedback.*` keys replace `feedback=<mode>`.
  Migration from v1.0.1 (format 2) and v1.0 (format 1) keeps the old behaviour: Off → all
  OFF, Vibration → counter + target vibration, Sound → counter + target sound, Both → all ON.
  Profiles, counts, rounds and targets are unchanged. The original is kept as
  `settings.v2.bak` / `settings.v1.bak`.
- New installs default to counter OFF/OFF and target ON/ON.
- A completed target plays only the target pattern (or nothing if both target switches are
  off). It never falls back to the counter tick.

### Tests and build
- 50 host unit tests (previously 43), including all 16 switch combinations, migration from
  format 1 and 2, and corrupt feedback keys.
- GCC `-fanalyzer` (27 firmware sources) and clang `--analyze`: no findings.
- Momentum `mntm-012` (API 87.1) and official `1.5.1` (API 88.2): build without warnings,
  APPCHK passed. Output in `dist/v1.0.2/`.

### Hardware
- v1.0.1: the 9 prioritised hardware tests passed on Momentum (reported by the user).
- v1.0.2: not yet tested on hardware.

## [1.0.1] – 2026-11-22

Fixes and improvements after the first hardware test on Momentum.

### Fixed
- **Target feedback did not play** (vibration/sound when reaching the target).
  - *Investigation:* the notification usage was checked against the Momentum `mntm-012`
    sources (`notification_app.c`, `notification_messages*.c`). It uses the correct API, its
    sequences are well-formed and NULL-terminated, and the relocations in the built FAP
    resolve to the right firmware symbols. Nothing there explains silence. The service
    suppresses vibration/sound only through stealth mode, the system Vibro switch or volume
    0.
  - *Cause in the app:* the target signal was edge-triggered. It fired only when the count
    went from below the target to the target, and never while the count was at or above it.
    That state was easy to get into and never ended without a manual reset: counting on after
    the target (`Done +N`), lowering the target below the current count, or reopening the app
    with such a saved state. From then on, no target feedback was possible.
  - *Fix:* rounds (see below). The counter never stays at or above the target. Every count
    that reaches it completes a round and plays the target signal, and a count above the
    target (e.g. after lowering it) completes the round on the next press. Shorter target
    patterns (~0.5 s) and skipping feedback while a pattern still plays keep the notification
    queue from filling up and blocking the UI.
  - *System settings:* the app now detects **stealth mode**. It shows "Muted" on the counter
    screen, a `Stealth mode` line and a help page in Feedback Settings, and README
    documentation. System Vibro/Volume cannot be read through the SDK, so they are explained
    in the help.
  - *Not yet hardware-verified.*
- Selecting a profile no longer jumps back to the main menu.

### Added
- **Rounds with automatic restart:** reaching the target shows "Target reached!" for ~1 s,
  plays the completion signal once, increments the round counter and resets the count to 0
  immediately (consistent state even if the app is closed during the message). A press during
  the message counts as the first of the next round. Completed rounds are shown and stored
  per profile.
- **Undo:** hold Down to undo the last count (exactly once per hold, never below 0, active
  profile only, saved). Undo directly after a completed round restores the previous count
  and removes the round. Short on-screen messages: "Undo: -1", "Round undone",
  "Nothing to undo".
- **Custom profiles:** add (name and target), rename, change the target and delete with a
  confirmation; up to 16 profiles in total. Unique, stable ids (`c1`, `c2`, … never reused).
  Duplicate and empty names are rejected. Built-in profiles cannot be renamed or deleted.
- **Profile list:** OK selects and stays in the list (`*` marks the active profile, saved
  immediately), hold OK opens the edit menu, plus `+ Add Profile` and `Edit Profiles...`.
- Feedback Settings: stealth mode status and a "No feedback? Help" page.
- Reset now also clears the completed rounds of the active profile.

### Changed
- **Settings format version 2** with a `custom_profiles` list, `next_custom_id` and
  per-profile `rounds`. **Migration from v1.0:** all counts, targets, names, the active
  profile and the feedback mode are kept. The v1.0 "Custom Dhikr" becomes a normal custom
  profile, and counts beyond the target are folded into rounds without loss
  (`39/33` → `6/33`, 1 round). The original file is kept as `settings.v1.bak`.
- Removed the `Done +N` display.
- Immediate saves (completed round, undo) are rate-limited to one per 3 s; the 15 s autosave
  and the save on exit remain.
- Stack size 4 → 6 KiB. Build output per app version: `dist/v<version>/<sdk>/`.

### Tests and checks
- 43 host unit tests (previously 28), including migration, interrupted saves, profile
  management, undo, auto restart and feedback modes. The checks were also run with
  intentionally broken code to make sure they detect failures.
- GCC `-fanalyzer` over all firmware sources and clang `--analyze` over the logic: no
  findings.

### Build
- Momentum `mntm-012`, API 87.1: builds, passes APPCHK.
- Official `1.5.1`, API 88.2: builds, passes APPCHK.

## [1.0.0] – 2026-11-22

First version.

### Added
- Tasbih counter screen with profile name, large count, target and progress bar.
- OK counts on short press only; Left/Right switch profiles from the counter screen.
- Targets 33, 99 or custom 1–9999 per profile; four built-in profiles with independent
  counts; the custom profile can be renamed.
- Reset dialog with No pre-selected.
- Feedback modes Off / Vibration / Sound / Both.
- Persistent storage (versioned text format, CRC-32, atomic temp-file + rename, write
  skipped if unchanged, backup of damaged files, read-only protection after read errors).
- 28 host unit tests; `scripts/build.sh`.

# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

## [1.0.0] – 2026-11-22

First version.

### Added
- Tasbih counter screen: profile name, large count, target, progress bar with percentage or
  "Target reached!" / "Done +N", and a "+1" button hint.
- OK counts on short press only; Left/Right switch profiles from the counter screen.
- Count saturates at 999,999 (no overflow).
- Targets: 33, 99 or a custom value 1–9999 (NumberInput), separate for each profile. The count
  is never cleared by a target change.
- Four built-in profiles (SubhanAllah, Alhamdulillah, Allahu Akbar, Custom Dhikr) with
  independent counts and targets; the custom profile can be renamed (TextInput, max 16 chars).
- Reset dialog `Reset counter? [No] [Yes]` with No pre-selected; Back cancels; only the
  active profile is reset and the result is saved immediately.
- Feedback modes Off / Vibration / Sound / Both, with previews. A distinct target pattern
  plays once when the target is first reached.
- Persistent storage in `apps_data/flipper_dhikr/settings.txt`: versioned text format,
  CRC-32, atomic temp-file + rename, write skipped if unchanged, 15 s autosave while
  counting, backup of damaged or newer files, read-only protection after read errors.
- Startup notice when saved data could not be used.
- Host unit test suite (28 tests, ASan/UBSan).
- `scripts/build.sh` for labelled builds against Momentum and official SDKs.

### Build
- Momentum `mntm-012`, API 87.1: builds, passes APPCHK.
- Official firmware `1.5.1`, API 88.2: builds, passes APPCHK.

### Known issues
- Not yet tested on physical hardware.

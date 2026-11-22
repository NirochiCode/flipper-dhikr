# Flipper Dhikr

A digital **tasbih counter** for the [Flipper Zero](https://flipperzero.one/), written in C
as a native `.fap` application.

Count with the OK button, keep separate counters for several dhikr profiles, set a target
(33, 99 or any value from 1 to 9999), and get a subtle vibration or sound on each count plus
a distinct pattern when the target is reached. Progress is stored on the SD card and restored
the next time you open the app.

> **Status: v1.0, development build.** The app compiles against Momentum `mntm-012` and the
> official firmware SDK `1.5.1`, and all host unit tests pass. **It has not been tested on a
> physical Flipper Zero yet.** See [Development status](#development-status).

---

## Features

| | |
|---|---|
| **Tasbih counter** | Large count display, target, progress bar and a "+1" hint. OK adds exactly one. |
| **Targets** | 33, 99 or a custom value 1–9999 per profile. Changing the target never clears the count. |
| **Profiles** | SubhanAllah, Alhamdulillah, Allahu Akbar and Custom Dhikr, each with its own count and target. The custom profile can be renamed. |
| **Safe reset** | `Reset counter?` dialog with **No** pre-selected. Back cancels. Only the active profile is reset. |
| **Feedback** | Off / Vibration / Sound / Both. Short tick per count, a distinct pattern (plus a green LED blink) the first time the target is reached. Respects the system volume, vibration and stealth settings. |
| **Persistence** | Versioned, CRC-protected settings file with crash-safe writes (temp file + rename) and no unnecessary SD writes. |
| **Robustness** | The counter saturates at 999,999 instead of overflowing. Only completed short presses count. Damaged or newer-version files are kept as a backup, and defaults are loaded. |

The default profiles are plain, configurable counters. The app makes no claim about how often
any dhikr should be recited.

## Controls

**Counter screen**

| Button | Action |
|---|---|
| OK (short press) | Count +1 |
| Left / Right | Previous / next profile (counts are kept) |
| Back | Back to the main menu (progress is saved) |

Holding OK or auto-repeat does **not** count. Up/Down do nothing on this screen.

**Menus**: Up/Down to move, OK to select, Back to go up one level. Back on the main menu exits
the app.

**Reset dialog**: Left = No, Right = Yes, OK confirms the highlighted choice, Back cancels.

### Main menu

```
Flipper Dhikr
 > Start Counting
   Dhikr Profiles      choose profile, rename "Custom Dhikr"
   Target Settings     33 / 99 / custom (1-9999) for the active profile
   Feedback Settings   Off / Vibration / Sound / Both, with previews
   Reset Counter       resets the active profile after confirmation
   About
```

## Requirements

- Flipper Zero with an SD card
- Firmware: **Momentum** (`mntm-012` or a newer version with the same API major version 87),
  or official firmware `1.5.x` (API 88) using the matching build
- To build: macOS (Apple Silicon or Intel) or Linux with Python 3.8+ and
  [uFBT](https://github.com/flipperdevices/flipperzero-ufbt)

A `.fap` file only runs on firmware whose API major version matches the SDK it was built with.
Use the build that matches your firmware.

## Build (macOS)

```bash
# 1. Get uFBT into a project-local virtual environment
python3 -m venv .venv
.venv/bin/pip install --upgrade ufbt

# 2a. Build for Momentum and official firmware (SDKs are downloaded on first run
#     into .ufbt-momentum/ and .ufbt-official/; ~/.ufbt is not touched)
scripts/build.sh

# 2b. ...or only one of them
scripts/build.sh momentum
```

Output:

```
dist/momentum-mntm-012/flipper_dhikr.fap   (+ BUILD_INFO.txt with SDK version/API)
dist/official-1.5.1/flipper_dhikr.fap
```

Plain uFBT also works. It uses whatever SDK is active in `~/.ufbt` and writes `dist/flipper_dhikr.fap`:

```bash
.venv/bin/ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
.venv/bin/ufbt
```

### Run the unit tests

The counter, profile, confirmation, file-format and save/load logic has no SDK dependency
and is tested on the host with AddressSanitizer and UBSan:

```bash
make -C tests
```

## Install on the Flipper Zero

Choose **one** of these options:

1. **qFlipper** – connect the Flipper, open *File manager* and copy
   `dist/momentum-mntm-012/flipper_dhikr.fap` to `SD Card/apps/Tools/`.
2. **SD card reader** – copy the file to `apps/Tools/` on the card.
3. **uFBT over USB** – builds, uploads to `/ext/apps/Tools/` and starts the app:
   ```bash
   UFBT_HOME=$PWD/.ufbt-momentum .venv/bin/ufbt launch
   ```
   Close qFlipper first, because it blocks the serial port.

Then open it on the Flipper under **Apps → Tools → Flipper Dhikr**.

None of these methods change the firmware.

## Data storage

Settings and counts are stored in `SD:/apps_data/flipper_dhikr/settings.txt`, a small
human-readable text file with a version number and a CRC-32 checksum.

| When | What happens |
|---|---|
| Leaving the counter screen, returning to the main menu, resetting, exiting the app | Saved immediately (only if something changed) |
| While counting | Autosave at most every 15 s after the first unsaved change |
| First start / missing file | Defaults are created and saved |
| Damaged file or file from a newer version | Renamed to `settings.bak`, defaults loaded, a notice is shown |
| SD card not readable | Defaults loaded, **saving disabled for this session** so a possibly intact file is never overwritten, a notice is shown |

> **Power loss:** If the Flipper loses power or crashes, counts from the last ~15 seconds
> may be lost. The previous saved state stays intact, because the file is written to a
> temporary file first and then renamed into place.

## Development status

| Area | Status |
|---|---|
| F01–F07 implemented | ✅ |
| Build, Momentum `mntm-012` (API 87.1) | ✅ compiles without warnings, passes `APPCHK` |
| Build, official `1.5.1` (API 88.2) | ✅ compiles without warnings, passes `APPCHK` |
| Host unit tests (28) | ✅ all pass (ASan + UBSan) |
| Tested on real hardware | ❌ **not yet.** See [docs/HARDWARE_TESTS.md](docs/HARDWARE_TESTS.md) |
| Screenshots | none yet (screenshots will only be added once they are taken on a device) |

## Known limitations

- Not yet verified on a physical device: UI layout, feedback intensity, NumberInput/TextInput
  behaviour on Momentum.
- Up to ~15 s of counts can be lost on power loss or a crash (see above).
- Only the "Custom Dhikr" profile can be renamed. Its name is limited to 16 printable ASCII
  characters (Flipper keyboard).
- No undo or decrement yet. A mistaken press can only be fixed with a reset.
- English UI only. The built-in profile names are transliterations; Arabic script cannot be
  shown with the built-in Flipper fonts.
- A `.fap` built for one firmware API major version will not load on another.

## Roadmap

Planned next: undo/decrement, user-defined profiles, morning/evening adhkar, a dua library,
statistics, Ramadan and Umrah helpers, more languages and NFC profiles. See
[docs/ROADMAP.md](docs/ROADMAP.md).

## Project layout

```
application.fam      app manifest (appid flipper_dhikr)
src/                 C sources (main, logic, storage, feedback, views, scenes)
include/             headers
assets/              10x10 app icon
tests/               host unit tests (make -C tests)
scripts/build.sh     labelled builds for Momentum and official SDKs
docs/                ARCHITECTURE, ROADMAP, CHANGELOG, HARDWARE_TESTS
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for design decisions.

## License

[MIT](LICENSE)

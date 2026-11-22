# Flipper Dhikr

A digital **tasbih counter** for the [Flipper Zero](https://flipperzero.one/), written in C
as a native `.fap` application.

Count with the OK button in rounds: when the target (33, 99 or any value from 1 to 9999) is
reached, the app shows **"Target reached!"**, gives a distinct vibration/sound signal, counts
the round and starts again at 0. Mistakes can be undone. Built-in profiles and up to 13 custom
ones keep independent counts, and everything is stored on the SD card.

> **Status: v1.0.2 – Hardware validated on Flipper Zero (Momentum).**
> All 50 host unit tests and all 9 prioritised v1.0.2 hardware
> tests passed. Builds for Momentum mntm-012 and official
> firmware 1.5.1 completed successfully.
> The official firmware build has not been tested on hardware.
> The full extended hardware checklist is not yet complete.

---

## Features

|                           |                                                                                                                                                                                                                                                                                |
| ------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Tasbih counter**        | Large count display, target, progress bar, completed rounds. OK adds exactly one.                                                                                                                                                                                              |
| **Rounds (auto restart)** | The count that reaches the target completes a round. "Target reached!" shows for about 1 s, the completion signal plays once, the round counter goes up and the count restarts at 0.                                                                                           |
| **Undo**                  | Hold **Down** to take back the last count (never below 0). Directly after a completed round, undo restores that round.                                                                                                                                                         |
| **Targets**               | 33, 99 or a custom value 1–9999 per profile. Changing the target never clears the count.                                                                                                                                                                                       |
| **Profiles**              | SubhanAllah, Alhamdulillah, Allahu Akbar (built-in, cannot be renamed or deleted), Custom Dhikr, plus your own profiles. Add, rename and delete custom profiles (max. 16 profiles in total).                                                                                   |
| **Safe reset / delete**   | `Reset counter?` and `Delete profile?` dialogs with **No** pre-selected. Back cancels.                                                                                                                                                                                         |
| **Feedback**              | Four independent switches: **Counter** vibration/sound (short tick per count) and **Target** vibration/sound (distinct pattern plus a green LED when a round is complete). Default: quiet counting, vibration + sound at the target. Respects the system settings (see below). |
| **Persistence**           | Versioned, CRC-protected settings file with crash-safe writes and automatic migration of v1.0 data.                                                                                                                                                                            |

The default profiles are plain, configurable counters. The app makes no claim about how often
any dhikr should be recited.

## Controls

**Counter screen**

| Button           | Action                                              |
| ---------------- | --------------------------------------------------- |
| OK (short press) | Count +1                                            |
| Down (hold)      | Undo the last count (once per hold, no auto-repeat) |
| Left / Right     | Previous / next profile (counts are kept)           |
| Back             | Back to the main menu (progress is saved)           |

Holding OK does **not** count. Up does nothing. The bottom row reminds you: `▼Undo`, `(+1)`
and `Muted` when stealth mode is silencing the feedback.

**What you see at the target (example: target 33):** `32/33` → OK → **`33/33 Target
reached!`** (about 1 s, signal plays once) → `0/33 Rounds: 1`. A press during the message
already counts as 1 of the next round, so no count is lost.

**Undo directly after a completed round:** `0/33 Rounds: 1` → hold Down → `32/33`, "Round
undone", and the round counter goes back to 0. After any further count, undo works normally
on the current count. When the count is 0 and no round can be restored, the app shows
"Nothing to undo" and the count stays at 0.

**Dhikr Profiles**

```
Dhikr Profiles
* SubhanAllah          <- "*" = active profile
  Alhamdulillah
  Allahu Akbar
  Custom Dhikr
  Istighfar
  + Add Profile
  Edit Profiles...
```

| Button               | Action                                                         |
| -------------------- | -------------------------------------------------------------- |
| OK on a profile      | Make it active. The list stays open and the `*` moves at once. |
| Hold OK on a profile | Edit menu: Target, Rename, Delete (built-ins: target only)     |
| `+ Add Profile`      | Enter a name, then a target. The new profile becomes active.   |
| `Edit Profiles...`   | Same edit menu, for users who prefer a menu over holding OK    |
| Back                 | Main menu                                                      |

Names: 1–16 printable characters. Names must be unique (ignoring upper/lower case) and must
not be empty. The Flipper keyboard only offers ASCII characters.

**Menus**: Up/Down to move, OK to select, Back to go up one level. Back on the main menu exits.

## Vibration and sound: system settings

Flipper Dhikr never overrides global settings. If you feel or hear nothing:

| Setting (on the Flipper)                    | Effect                                                                                                                              |
| ------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| **Stealth mode** (Momentum: lock menu)      | Mutes all vibration and sound. The app shows **"Muted"** on the counter screen and `Stealth mode: ON (muted)` in Feedback Settings. |
| **Settings → Notifications → Vibro** = Off  | No vibration (the app cannot read this setting)                                                                                     |
| **Settings → Notifications → Volume** = 0 % | No sound (the app cannot read this setting)                                                                                         |
| App: all four feedback switches OFF         | No vibration, no sound, no LED                                                                                                      |

## Feedback Settings

```
Feedback Settings          Counter Feedback        Target Feedback
  Counter Feedback  >        Vibration: OFF          Vibration: ON
  Target Feedback  >         Sound: OFF              Sound: ON
  Stealth: Off               Preview                 Preview
  Help
```

- **OK** on `Vibration` / `Sound` switches it ON ↔ OFF. The label changes at once.
- **Preview** plays this event's pattern with the current switches; if both are OFF,
  nothing plays.
- **Back** goes up one level. Changes are saved when you leave the page.
- `Stealth: ON (muted)` shows that stealth mode is silencing everything; OK on it or on
  `Help` opens the help page.

The four switches are independent. Examples:

| Counter vib. | Counter sound | Target vib. | Target sound | Result                                                                                                                             |
| ------------ | ------------- | ----------- | ------------ | ---------------------------------------------------------------------------------------------------------------------------------- |
| OFF          | OFF           | ON          | ON           | Silent counting, vibration + melody at 33/33 (**default for new installs**)                                                        |
| ON           | OFF           | OFF         | ON           | Tick vibration on each count, melody only at the target                                                                            |
| ON           | ON            | OFF         | OFF          | Feedback on every count, nothing extra at the target (the target press stays silent; the "Target reached!" message is still shown) |
| OFF          | OFF           | OFF         | OFF          | No vibration or sound at all                                                                                                       |

A completed target plays **only** the target pattern, never the counter tick in addition.

**Updating from v1.0 / v1.0.1:** your previous setting is converted so nothing changes for
you: Off → all OFF, Vibration → both vibration switches ON, Sound → both sound switches ON,
Both → all ON.

## Requirements

- Flipper Zero with an SD card
- Firmware: **Momentum** `mntm-012` (or newer with API 87.x), or official `1.5.x` (API 88)
  with the matching build
- To build: macOS or Linux with Python 3.8+ and
  [uFBT](https://github.com/flipperdevices/flipperzero-ufbt)

A `.fap` only runs on firmware whose API major version matches the SDK it was built with.

## Build (macOS)

```bash
python3 -m venv .venv
.venv/bin/pip install --upgrade ufbt

scripts/build.sh            # Momentum + official (SDKs go to .ufbt-momentum/, .ufbt-official/)
scripts/build.sh momentum   # only one
```

Output (one folder per app version, older builds are kept):

```
dist/v1.0.2/momentum-mntm-012/flipper_dhikr.fap   (+ BUILD_INFO.txt)
dist/v1.0.2/official-1.5.1/flipper_dhikr.fap
```

### Tests

```bash
make -C tests     # 50 host unit tests, AddressSanitizer + UBSan
```

## Install on the Flipper Zero

Choose **one** of these options:

1. **qFlipper**: copy `dist/v1.0.2/momentum-mntm-012/flipper_dhikr.fap` to
   `SD Card/apps/Tools/`, replacing the old file.
2. **SD card reader**: copy the file to `apps/Tools/` on the card.
3. **uFBT over USB** (close qFlipper first):
   `UFBT_HOME=$PWD/.ufbt-momentum .venv/bin/ufbt launch`

Then open it under **Apps → Tools → Flipper Dhikr**. Existing v1.0 / v1.0.1 data is migrated
automatically on first start. None of these methods change the firmware.

## Data storage

`SD:/apps_data/flipper_dhikr/settings.txt` is a small text file with a format version and a
CRC-32 checksum.

| When                                                | What happens                                                                                                                      |
| --------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| Completed round, undo                               | Saved immediately (at most every 3 s; otherwise via autosave)                                                                     |
| Profile selected / added / renamed / deleted, reset | Saved immediately                                                                                                                 |
| Leaving the counter, main menu, exit                | Saved if anything changed                                                                                                         |
| While counting                                      | Autosave at most every 15 s after the first unsaved change                                                                        |
| First start after an update                         | Older formats are migrated to format v3; the original is kept as `settings.v1.bak` (from v1.0) or `settings.v2.bak` (from v1.0.1) |
| Damaged file / file from a newer version            | Renamed to `settings.bak`, defaults loaded, notice shown                                                                          |
| SD card not readable                                | Defaults loaded, saving disabled for the session, notice shown                                                                    |

> **Power loss:** counts from the last ~15 s may be lost. The previous saved state stays
> intact, because writes go to a temporary file first and are then renamed.

## Development status

| Area                                  | Status                                                                                |
| ------------------------------------- | ------------------------------------------------------------------------------------- |
| v1.0 features                         | ✅ tested on hardware (Momentum) by the user                                          |
| v1.0.1 features                       | ✅ the 9 prioritised hardware tests passed (Momentum)                                 |
| v1.0.2 independent feedback           | ✅ 9/9 prioritised hardware tests passed (Momentum); extended checklist still open    |
| Build, Momentum `mntm-012` (API 87.1) | ✅ no warnings, APPCHK passed                                                         |
| Build, official `1.5.1` (API 88.2)    | ✅ no warnings, APPCHK passed                                                         |
| Host unit tests                       | ✅ 50/50 (ASan + UBSan), including all 16 switch combinations                         |
| Static analysis                       | ✅ clang `--analyze` (logic) and GCC `-fanalyzer` (all firmware sources): no findings |
| Screenshots                           | none yet (screenshots will only be added once they are taken on a device)             |

## Known limitations

- While the target pattern plays (~0.6 s), further vibration/sound ticks are skipped. The counts
  themselves are always kept. With very small targets (e.g. 1) and fast pressing, not every
  completed round gets its own sound/vibration, but each one shows "Target reached!".
- System Vibro/Volume cannot be read via the public SDK, so only stealth mode is detected.
- Undo restores a completed round only directly after it, and only within the same app session.
- Up to ~15 s of counts can be lost on power loss.
- English UI and ASCII names only.

## Roadmap

See [docs/ROADMAP.md](docs/ROADMAP.md).

## Project layout

```
application.fam      app manifest (appid flipper_dhikr)
src/, include/       C sources and headers (logic, storage, feedback, views, scenes)
assets/              10x10 app icon
tests/               host unit tests (make -C tests)
scripts/build.sh     labelled builds for Momentum and official SDKs
docs/                ARCHITECTURE, ROADMAP, CHANGELOG, HARDWARE_TESTS
```

## License

[MIT](LICENSE)

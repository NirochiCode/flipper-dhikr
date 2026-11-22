# Architecture

Flipper Dhikr is a standard Flipper `ViewDispatcher` + `SceneManager` application. The main
design rule: **all business logic is plain C with no Flipper SDK dependency** and is unit
tested on the host. Code that talks to the SDK is a thin layer around it.

```
                    ┌──────────────────────────────────────────────┐
  Flipper SDK  ◄────┤  main.c            lifecycle, save policy    │
  (gui, storage,    │  scene_*.c         menus & navigation        │
   notification)    │  ui_*_view.c       custom views (draw/input) │
                    │  storage_flipper.c DhikrFileIo on Storage API│
                    │  feedback_player.c notification sequences    │
                    └───────────────┬──────────────────────────────┘
                                    │ uses
                    ┌───────────────▼──────────────────────────────┐
  host-testable     │  counter.c         count/target/saturation   │
  (tests/)          │  profiles.c        profiles + DhikrState     │
                    │  confirm.c         [No]/[Yes] state machine  │
                    │  settings_codec.c  versioned text + CRC-32   │
                    │  storage.c         load/save policy (via IO) │
                    │  feedback.c        feedback mode helpers     │
                    └──────────────────────────────────────────────┘
```

## Modules

| Module | Responsibility |
|---|---|
| `counter` | `DhikrCounter {count, target}`. Increments by exactly one, saturates at `DHIKR_COUNT_MAX` (999,999), reports `TargetReached` only when the count *crosses* the target. |
| `profiles` | `DhikrProfile` (stable `id`, name, counter, editable flag) and `DhikrState` (profiles, active index, feedback mode). Selection, cycling, reset of the active profile, name validation and rename. |
| `confirm` | Pure state machine for the reset dialog. Starts at **No**, Left/Right move, OK commits, Back cancels. |
| `settings_codec` | Serializes `DhikrState` to and from the versioned text format with a CRC-32. |
| `storage` | Load/save *policy* on top of the `DhikrFileIo` interface (read, write, rename, remove). |
| `storage_flipper` | `DhikrFileIo` implemented with the Flipper `Storage` API. |
| `feedback` / `feedback_player` | Mode helpers (pure) and the notification sequences (SDK). |
| `ui_counter_view`, `ui_confirm_view` | Custom `View`s. They only draw and turn key presses into events. |
| `scene_*` | One file per screen. The list is in `include/scenes.h` (X-macro). |
| `main` | Allocation/free of all resources, dispatcher callbacks, dirty tracking and autosave. |

## Key decisions

### "Target reached" is derived, not stored
`complete = count >= target`. The target feedback fires only on the transition from
`count < target` to `count >= target`. As a result:
- it fires once per round, never again while you keep counting (requirement 11);
- after a reset it can fire again;
- lowering the target below the current count completes it silently, and no stray feedback
  follows later;
- there is no extra flag that could get out of sync or need persisting.

### Saturation instead of overflow
The count stops at 999,999, which is also the most the large-digit font can show. Further OK
presses return `DhikrIncrementSaturated`, give no feedback and change nothing.

### Input handling
Views act only on `InputTypeShort`. That event comes once per completed short press, so holding
OK (long/repeat) never counts and one physical press is one count. The OK press that opens
the counter or the reset dialog is used up by the submenu, so it cannot count or confirm
anything.

### Stable profile ids as storage keys
Profiles are saved as `profile.<id>.count`, not by position. New built-in profiles can be
appended later. Old files then load fine and the new profile keeps its defaults. Unknown ids
and keys are ignored, so a file can carry data for features this version does not know.

### File format and crash safety
```
FlipperDhikr
version=1
active=subhanallah
feedback=1
profile.subhanallah.count=12
profile.subhanallah.target=33
...
profile.custom.name=Custom Dhikr
crc32=1A2B3C4D
```
- **Version first.** It is checked before the CRC. A newer major format gives
  `UnsupportedVersion`, the file is kept as `settings.bak` and is never overwritten.
- **CRC-32 last.** It detects truncation and bit errors. A known key with an invalid value
  (e.g. target 0) makes the whole file corrupt, and nothing is partially applied.
- **Atomic replace.** The app writes `settings.tmp` and then renames it over `settings.txt`.
  If it is interrupted, the old file is still intact. A complete orphaned temp file is
  recovered on the next start.
- **Read errors make the store read-only**, so a temporary SD problem cannot replace a good
  file with defaults.

### Avoiding unnecessary writes
- Changes set a *dirty* flag. Saving happens on leaving the counter, on returning to the main
  menu, after a reset and on exit, and while counting at most once every 15 s after the first
  unsaved change.
- On top of that, the store keeps the CRC of the content that was last loaded or saved. If the
  serialized state is identical, it skips the write entirely.
- The autosave check runs from the dispatcher tick (1 s, only while idle) and after each count,
  because the tick does not fire while keys are pressed continuously.

### Non-blocking feedback
`notification_message()` only queues a sequence, which the notification service plays on its
own thread. Sequences are short (count ≈ 50 ms, target ≈ 0.6 s) and always end with sound
and vibration off. No `force` messages are used, so the system volume, vibration and stealth
settings apply.

### Memory and resources
- One heap-allocated `DhikrApp`. All modules and views are allocated in `dhikr_app_alloc()`
  and freed in reverse order in `dhikr_app_free()`. Records are opened and closed in pairs.
- Load/save use one 2 KiB heap buffer per call, freed before returning.
- Menu labels live in fixed buffers inside `DhikrApp`, so they never depend on whether a GUI
  module copies strings.
- Stack: 4 KiB (`application.fam`).

## Extending

- **New screen:** add `X(Id, name)` to `DHIKR_SCENE_LIST` and create `src/scene_name.c` with
  `on_enter/on_event/on_exit`.
- **New built-in profile:** append to `dhikr_profile_templates` in `profiles.c` (max
  `DHIKR_PROFILE_MAX` = 8). Never change existing ids.
- **New persisted setting:** add a key to the codec. Older files simply lack it, so use a
  default. Bump `DHIKR_SETTINGS_VERSION` only for incompatible changes.
- **Future features** (adhkar, duas, statistics, Ramadan/Umrah helpers) fit in as new scenes
  plus their own pure-logic modules. Content such as dua texts would go to `fap_file_assets`
  on the SD card rather than into the binary.
- **Languages:** UI strings are currently literals in the scenes and views. A string table is
  planned (see ROADMAP).

## Testing

`tests/test_main.c` builds the pure modules with the host compiler (`-Wall -Wextra -Werror
-Wconversion`, ASan, UBSan) and uses an in-memory fake `DhikrFileIo` that can inject read,
write and rename failures. Hardware-dependent behaviour is covered by the manual checklist in
`docs/HARDWARE_TESTS.md`.

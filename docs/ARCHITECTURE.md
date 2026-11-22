# Architecture

Flipper Dhikr is a standard Flipper `ViewDispatcher` + `SceneManager` application. The main
design rule: **all business logic is plain C with no Flipper SDK dependency** and is unit
tested on the host. Code that talks to the SDK is a thin layer around it.

```
                    ┌──────────────────────────────────────────────────┐
  Flipper SDK  ◄────┤  main.c             lifecycle, save policy       │
  (gui, storage,    │  scene_*.c          menus & navigation           │
   notification,    │  ui_*_view.c        custom views (draw/input)    │
   furi_hal_rtc)    │  storage_flipper.c  DhikrFileIo on Storage API   │
                    │  feedback_player.c  notification sequences       │
                    └────────────────┬─────────────────────────────────┘
                                     │ uses
                    ┌────────────────▼─────────────────────────────────┐
  host-testable     │  counter.c          count/target/rounds/undo     │
  (tests/)          │  counter_input.c    key mapping, banner, actions │
                    │  profiles.c         built-in + custom profiles   │
                    │  confirm.c          [No]/[Yes] state machine     │
                    │  settings_codec.c   format v2 + v1 migration     │
                    │  storage.c          load/save policy (via IO)    │
                    │  feedback.c         modes and feedback plan      │
                    └──────────────────────────────────────────────────┘
```

## Modules

| Module | Responsibility |
|---|---|
| `counter` | `DhikrCounter {count, target, rounds}` plus the undo information that is not saved. Increment completes a round at the target; undo; reset; normalisation of v1.0 counts. |
| `counter_input` | Maps keys to actions (`OK short` → +1, `Down long` → undo, `Left/Right short` → profile). Banner timing. `dhikr_counter_apply()` applies an action to the state and returns which feedback to play and whether to save. The counter scene is only a thin wrapper around it. |
| `profiles` | `DhikrProfile` (stable `id`, name, counter, `builtin`) and `DhikrState` (≤ 16 profiles, active index, feedback mode, `next_custom_id`). Add, rename, delete, name check (valid and unique), list labels. |
| `confirm` | [No]/[Yes] state machine used by reset and delete. Starts at **No**. |
| `settings_codec` | Serialises format v2 and parses v1 and v2 (with migration). CRC-32. |
| `storage` | Load/save policy on top of `DhikrFileIo`: atomic writes, skipping unchanged writes, backups, migration hand-off. |
| `storage_flipper` | `DhikrFileIo` on the Flipper `Storage` API. |
| `feedback` / `feedback_player` | `dhikr_feedback_plan(mode, event)` (pure) and the notification sequences and stealth-mode check (SDK). |
| `ui_counter_view`, `ui_confirm_view` | Custom `View`s. The counter view owns a one-shot `FuriTimer` that redraws the screen when a banner expires. |
| `scene_*` | One file per screen. The list is in `include/scenes.h` (X-macro). |
| `main` | Allocation and release, dispatcher callbacks, dirty tracking, autosave, rate-limited immediate saves, start-up notice. |

## Screens and navigation

```
MainMenu ─┬─ Counter                         (OK +1, hold Down undo, Left/Right profile)
          ├─ Profiles ─┬─ (OK: select, stay) 
          │            ├─ (hold OK) ─────────┐
          │            ├─ + Add ─ ProfileName ─ TargetCustom(new) ─► back to Profiles
          │            └─ Edit Profiles... ─ ProfileEdit ─┐
          │                                               ▼
          │                         ProfileActions ─┬─ TargetCustom(edit)
          │                                         ├─ ProfileName(rename)
          │                                         └─ ProfileDelete (confirm) ─► back to list
          ├─ Target ─ TargetCustom(active) ─► MainMenu
          ├─ Feedback ─ Notice(help)
          ├─ Reset (confirm)
          └─ About
Notice: generic message screen (start-up storage problems, profile limit, help)
```

The shared input screens (`ProfileName`, `TargetCustom`) take their purpose from
`app->name_mode` and `app->target_mode`, so one scene serves several flows without duplicated
code.

## Key decisions

### Rounds instead of counting past the target (v1.0.1)
The increment that reaches the target (`count + 1 >= target`) sets `count = 0` and
`rounds++` **in the state immediately**. The "Target reached!" display is only a banner
holding the completed value. As a result:
- the saved data is always consistent, even if the app is left or closed while the banner
  is still showing;
- every round gives exactly one target event (requirement: feedback once per round);
- a press during the banner is a normal count of the new round and closes the banner, so no
  input is lost or counted twice, and no blocking delay is needed;
- a count that is at or above the target (after lowering the target, or from v1.0 data)
  completes the round on the next press. The edge-trigger dead state of v1.0 cannot happen.

### Undo semantics
- `count > 0` → `count - 1`.
- Directly after a completed round (`undo_round_available`), undo restores the exact count
  before that press (normally `target - 1`, or more if the target had been lowered) and sets
  `rounds - 1`.
- Otherwise, at 0: "Nothing to undo", no change.
- The undo window ends after any increment, target change or reset. It is not persisted, so
  after an app restart a completed round can no longer be undone. This is deliberate and
  avoids having to store an undo history.
- Undo is triggered only by `InputTypeLong` on Down, which the input service sends once per
  hold. `InputTypeRepeat` is ignored, so holding longer never undoes more than one count.

### Feedback and system settings
- The notification service (Momentum `notification_app.c`) applies the system settings itself:
  vibration needs Vibro = on, sound is scaled by the volume, and stealth mode
  (`FuriHalRtcFlagStealthMode`) suppresses both unless forced. The app never forces.
- Only stealth mode can be read through the public API. The app shows "Muted" on the
  counter screen and the status in Feedback Settings.
- `notification_message()` posts to a queue with 8 slots that the input service also uses
  (3 backlight messages per key press). A full queue blocks the sender, so the target pattern
  is kept short (~0.5 s), and any further feedback is skipped while it plays (~0.65 s).
  Counts are never skipped.

### Dynamic profiles with stable ids
- Built-ins (`subhanallah`, `alhamdulillah`, `allahu_akbar`) are always first and fixed.
- `custom` (from v1.0) and `c<N>` are custom profiles. `N` comes from `next_custom_id`, which is
  saved and only ever increases, so deleting and re-creating never reuses an id.
- Deleting the active profile activates the one before it. Deleting another profile keeps the
  active one.
- Limit: `DHIKR_PROFILE_MAX` = 16 in total (3 built-in + 13 custom). The worst case (16
  profiles, 16-character names, maximum values) measures 2,283 bytes; the codec buffer is
  4 KiB.

### File format v2 and migration
```
FlipperDhikr
version=2
active=c1
feedback=1
next_custom_id=2
custom_profiles=custom,c1
profile.subhanallah.count=12
profile.subhanallah.target=33
profile.subhanallah.rounds=4
...
profile.c1.name=Istighfar
profile.c1.count=0
profile.c1.target=100
profile.c1.rounds=0
crc32=1A2B3C4D
```
- **Two passes:** pass 1 reads the settings and `custom_profiles` (creating the profile slots),
  pass 2 reads the profile values. The order of lines in the file does not matter.
- **Strict where it matters:** every listed custom profile needs a name, count and target.
  Duplicate or invalid ids, too many profiles, or any out-of-range value make the file
  corrupt as a whole.
- **Lenient where it helps:** unknown keys and profiles are ignored, built-ins without keys
  keep their defaults, and an unknown `active` id falls back to the first profile.
- **v1 → v2:** the v1 parser path keeps the fixed four profiles. `custom` becomes the first
  custom profile, and counts at or above the target are folded into rounds without loss
  (`count / target` rounds, remainder `count % target`).
- **Storage flow:** `dhikr_store_load()` returns `DhikrLoadMigrated` and writes the original
  bytes to `settings.v1.bak`. The app saves format v2 immediately through temp file + rename.
  If that save is interrupted, the v1 file is still in place and is migrated again on the
  next start (tested).
- A leftover temp file is only recovered if it is a complete, valid **v2** file.

### Avoiding unnecessary writes
- Dirty flag plus a CRC of the last saved content. An identical state is never written again.
- Immediate saves (completed round, undo) happen at most every 3 s. Within that window the
  change is left to the 15 s autosave and the save on exit, so target 1 does not write once
  per press.

### Memory and resources
- One heap-allocated `DhikrApp`. All modules, views and the banner timer are allocated in
  `dhikr_app_alloc()` and freed in reverse order. The timer is stopped before it is freed.
- Load and save use one 4 KiB heap buffer per call, freed before returning.
- Menu labels live in fixed buffers inside `DhikrApp`.
- Stack: 6 KiB, because the parser keeps a temporary `DhikrState` (~1 KiB) on the stack.

## Extending

- **New screen:** add `X(Id, name)` to `DHIKR_SCENE_LIST` and create `src/scene_name.c`.
- **New persisted setting:** add a key. Older files simply lack it, so use a default. Bump
  `DHIKR_SETTINGS_VERSION` only for incompatible changes and add a migration path like v1.
- **Future features** (adhkar, duas, statistics, Ramadan/Umrah helpers) fit in as new scenes
  plus their own pure-logic modules. Larger content belongs in `fap_file_assets`.

## Testing and checks

- `tests/test_main.c`: 43 tests against the pure modules with an in-memory fake
  `DhikrFileIo`, built with `-Wall -Wextra -Werror -Wconversion` and ASan/UBSan.
  Requirements R1–R20 are marked in the source.
- The tests were checked with intentionally broken code: breaking undo or rounds makes the
  related tests fail.
- GCC `-fanalyzer` was run over every firmware source with the real uFBT compile flags, and
  clang `--analyze` over the logic modules: no findings.
- Hardware behaviour: `docs/HARDWARE_TESTS.md`.

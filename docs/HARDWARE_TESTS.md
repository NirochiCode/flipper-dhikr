# Hardware test checklist

Install the build that matches your firmware (see README), then work through the list. Record
the firmware version and the results in this file. Screenshots: see
[SCREENSHOT_CHECKLIST.md](SCREENSHOT_CHECKLIST.md).

Since v1.0.3 the UI says **Sets** where older sections below say *rounds*, and the target
message is `Set N complete!` instead of `Target reached!`. The older sections are kept as they
were tested.

## v1.0.3 – menu and UX (NOT yet validated on hardware)

**Status: not tested on a Flipper Zero yet.** Host unit tests: 62/62. Builds: Momentum
`mntm-012` and official `1.5.1`, no warnings.

Install `dist/v1.0.3/momentum-mntm-012/flipper_dhikr.fap` over v1.0.2. Keep a copy of
`SD:/apps_data/flipper_dhikr/settings.txt` first. Rows marked ★ are the priority set.

### Update

| # | ★ | Test | Expected | Result |
|---|---|---|---|---|
| V1 | ★ | Install v1.0.3 over v1.0.2 with existing data | All profiles, names, counts, completed rounds (now shown as Sets), targets, active profile and the four feedback switches are unchanged; no new `.bak` file; `settings.txt` still has `version=3` | ☐ |
| V2 | | About | Shows `Flipper Dhikr v1.0.3`, a "Sets" and a "Profiles" section | ☐ |

### Main menu

| # | ★ | Test | Expected | Result |
|---|---|---|---|---|
| V3 | ★ | Open the app | Menu: `Start Counting`, `Dhikr Profiles`, `Feedback Settings`, `Reset Counter`, `About`; **no** `Target Settings`; all readable, scrolling works | ☐ |
| V4 | | Open each entry, then Back | Returns to the main menu each time; Back on the main menu exits | ☐ |

### Profiles and Edit Target

| # | ★ | Test | Expected | Result |
|---|---|---|---|---|
| V5 | ★ | Dhikr Profiles → OK on another profile | `*` moves at once, list stays open | ☐ |
| V6 | ★ | Hold OK on a profile that is **not** active | Header = that profile's name; `Edit Target (N)`, plus `Rename Profile` / `Delete Profile` for custom profiles | ☐ |
| V7 | ★ | … → `Edit Target` → `99 times` | Back in the same edit menu showing `Edit Target (99)`; Back → list: `*` still on the previously active profile; that profile's count unchanged | ☐ |
| V8 | ★ | … → `Edit Target` → `Custom value...` → 100 | Back in the edit menu with `Edit Target (100)` (not the main menu); Back → list → Back → main menu | ☐ |
| V9 | | Edit Target on the **active** profile with count 10 and Sets 2, then Start Counting | Count 10, Sets 2, new target shown | ☐ |
| V10 | ★ | Hold OK (or Edit Profiles...) on SubhanAllah / Alhamdulillah / Allahu Akbar | Only `Edit Target (N)` | ☐ |
| V11 | | Edit Profiles... → custom profile → Rename Profile; Delete Profile (No, then Yes) | Rename works; delete asks with No pre-selected and only deletes on Yes | ☐ |
| V12 | | Add profiles until 10+ exist | List scrolls; `+ Add Profile` and `Edit Profiles...` reachable at the end | ☐ |
| V13 | | Exit after a target change, reopen | New target kept | ☐ |

### Sets and completion message (target 33; feedback default Counter OFF / Target ON)

| # | ★ | Test | Expected | Result |
|---|---|---|---|---|
| V14 | | Counter at 0/33 and at ~30/33 | Count large and readable; bar text `0%  Sets: 0` / `90%  Sets: 0` | ☐ |
| V15 | ★ | Count 32 → 33 | `33/33` with `Set 1 complete!` for ~1 s (text fully inside the bar), target feedback once, then `0/33` and `Sets: 1` | ☐ |
| V16 | | Complete a second set | `Set 2 complete!`, then `Sets: 2` | ☐ |
| V17 | ★ | Hold Down right after a completed set | `32/33`, `Set undone`, Sets back by 1 | ☐ |
| V18 | | Press OK quickly during `Set N complete!` | Every press counts once, the message closes, UI never freezes | ☐ |
| V19 | ★ | Exit the app and reopen | Sets and count kept per profile | ☐ |
| V20 | | Reset Counter | Dialog shows `Count N, sets M`, No pre-selected; Yes clears count and sets of the active profile only | ☐ |

### Unchanged behaviour (spot checks)

| # | ★ | Test | Expected | Result |
|---|---|---|---|---|
| V21 | | Counter ON / Target OFF | Tick on counts 1–32, silent at 33, `Set 1 complete!` shown | ☐ |
| V22 | | Stealth mode on | `Muted` on the counter screen | ☐ |
| V23 | | Left/Right on the counter screen | Switches profile, each with its own count and Sets | ☐ |

Firmware tested: _______  Date: _______

## v1.0 – results reported by the user (Momentum)

| # | Test | Result |
|---|---|---|
| — | App starts without crashing | ✅ |
| — | OK increases the counter by exactly 1 | ✅ |
| — | Targets 33 and 99, custom target | ✅ |
| — | The four profiles are selectable and keep their own counts | ✅ |
| — | Reset confirmation | ✅ |
| — | Settings and counts survive a restart | ✅ |
| — | UI and navigation readable | ✅ |
| — | Vibration and sound at the target | ❌ → addressed in v1.0.1 (F1) |

## v1.0.2 – independent feedback switches (hardware validated)

**Status: 9/9 prioritised hardware tests PASSED.**

**Test date:** 2026-11-22  
**Device:** Flipper Zero  
**Firmware:** Momentum (exact installed version not recorded)  
**App version:** v1.0.2  
**Host unit tests:** 50/50 passed  
**Hardware validation:** 9/9 prioritised tests passed

The following results were reported after testing
v1.0.2 on a physical Flipper Zero.

### Prioritised hardware test results

| # | Test | Result |
|---|---|---|
| P1 | Update and migration from v1.0.1 | ✅ PASS |
| P2 | Counter OFF / Target ON | ✅ PASS |
| P3 | Counter ON / Target OFF | ✅ PASS |
| P4 | Both feedback events enabled | ✅ PASS |
| P5 | Both feedback events disabled | ✅ PASS |
| P6 | Independent vibration and sound switches | ✅ PASS |
| P7 | Settings persist after restarting the app | ✅ PASS |
| P8 | Stealth mode handling | ✅ PASS |
| P9 | Existing features remain functional | ✅ PASS |

### Validation summary

The hardware testing confirmed:

- Successful update and preservation of existing data.
- Independent Counter and Target feedback operation.
- Vibration and sound can be configured separately.
- Silent counting with feedback only at the target.
- Counter feedback without target feedback.
- Correct behaviour with both feedback events enabled.
- Correct behaviour with both feedback events disabled.
- Persistent feedback settings after restarting.
- Correct handling of system Stealth mode.
- No observed regression in previously tested features.

### Remaining validation

The 9 prioritised hardware checks do not constitute
complete execution of the detailed S1-S25 checklist.

The detailed test cases below remain unchecked unless
their complete expected behaviour has been individually
verified and recorded.

In particular, not all 16 feedback combinations have
been tested on physical hardware. All 16 combinations
passed the host unit tests.

The official firmware build has not yet been
validated on a physical Flipper Zero.

### Detailed hardware test checklist (S1-S25)


Before you start: keep a copy of `SD:/apps_data/flipper_dhikr/settings.txt` from v1.0.1, set
Settings → Notifications → Vibro on and Volume > 0, and turn stealth mode off. Use a profile
with target 3 to complete rounds quickly.

### Migration and defaults

| # | Test | Expected | Result |
|---|---|---|---|
| S1 | Install v1.0.2 over v1.0.1 (your current mode, e.g. "Both") | Switches match the old mode (Both → all ON; Vibration → both Vibration ON; Sound → both Sound ON; Off → all OFF); profiles, counts, rounds, targets unchanged; `settings.v2.bak` exists; `settings.txt` has `version=3` | ☐ |
| S2 | Fresh install (rename `apps_data/flipper_dhikr` first) | Counter Vibration OFF, Counter Sound OFF, Target Vibration ON, Target Sound ON | ☐ |
| S3 | Change switches, exit app, reopen | All four values kept | ☐ |

### Menu

| # | Test | Expected | Result |
|---|---|---|---|
| S4 | Feedback Settings | `Counter Feedback >`, `Target Feedback >`, `Stealth: …`, `Help` | ☐ |
| S5 | Counter Feedback → OK on Vibration | `Vibration: ON` ↔ `OFF` changes at once; one press = one toggle | ☐ |
| S6 | Hold OK on a switch | Does not toggle repeatedly | ☐ |
| S7 | Back from a feedback page / from Feedback Settings | One level up each time | ☐ |
| S8 | Preview on each page | Plays exactly that event's enabled outputs; nothing if both are OFF | ☐ |
| S9 | Help / Stealth line | Help page opens; stealth status correct | ☐ |

### Switches (counting from 0 to 3 with target 3)

`CV` = Counter Vibration, `CS` = Counter Sound, `TV` = Target Vibration, `TS` = Target Sound.

| # | CV | CS | TV | TS | Expected on counts 1, 2 | Expected at 3/3 | Result |
|---|---|---|---|---|---|---|---|
| S10 | OFF | OFF | ON | ON | silent | long vibration + melody + green LED, once | ☐ |
| S11 | ON | ON | OFF | OFF | short vibration + click | **silent** (no tick), "Target reached!" shown | ☐ |
| S12 | ON | OFF | OFF | OFF | short vibration only | silent | ☐ |
| S13 | OFF | ON | OFF | OFF | click only | silent | ☐ |
| S14 | OFF | OFF | ON | OFF | silent | long vibration pattern only (+ LED) | ☐ |
| S15 | OFF | OFF | OFF | ON | silent | melody only (+ LED) | ☐ |
| S16 | ON | ON | ON | ON | vibration + click | target pattern only, no extra tick | ☐ |
| S17 | OFF | OFF | OFF | OFF | silent | silent, no LED, banner shown | ☐ |
| S18 | ON | OFF | OFF | ON | short vibration | melody only | ☐ |
| S19 | OFF | ON | ON | OFF | click | long vibration only | ☐ |

The remaining 6 of the 16 combinations are covered by the unit tests. Test them on the device
if time allows.

### Interaction with existing features

| # | Test | Expected | Result |
|---|---|---|---|
| S20 | Stealth mode on, any switch ON | "Muted" on the counter screen, no output | ☐ |
| S21 | Stealth mode on, all switches OFF | No "Muted" hint | ☐ |
| S22 | System Vibro off, TV+TS ON | Melody only at the target | ☐ |
| S23 | Undo (hold Down) with all switches ON | Undo works, no feedback sound/vibration | ☐ |
| S24 | Auto reset, rounds, profile switching (Left/Right) | Unchanged from v1.0.1 | ☐ |
| S25 | Fast pressing over several rounds with everything ON | UI never freezes | ☐ |

## v1.0.1 – hardware results

The user reported that **all 9 prioritised v1.0.1 hardware tests passed** on Momentum. The
individual results were not recorded row by row, so the table below stays as the full
reference checklist. Rows F1.1–F1.5 now refer to the old single Feedback mode, which v1.0.2
replaced (see S10–S19).

### Before you start
- Keep a copy of `SD:/apps_data/flipper_dhikr/settings.txt` from v1.0 (for M1).
- Settings → Notifications: Vibro **on**, Volume > 0. Stealth mode **off**.

### F1 – Feedback

| # | Test | Expected | Result |
|---|---|---|---|
| F1.1 | Feedback = Vibration, count from 0 to target 33 | Short tick on each count; at 33 two long pulses + green LED | ☐ |
| F1.2 | Feedback = Sound, same | Short click; at the target a rising 4-note melody + green LED | ☐ |
| F1.3 | Feedback = Both, same | Both | ☐ |
| F1.4 | Feedback = Off, same | No vibration, no sound, no LED; banner still shows | ☐ |
| F1.5 | Feedback Settings → Preview count / Preview target | As above for the selected mode | ☐ |
| F1.6 | Turn stealth mode on, open the counter | "Muted" at bottom right; Feedback Settings shows `Stealth mode: ON (muted)`; no vibration or sound | ☐ |
| F1.7 | System Vibro off (stealth off), mode Both | Sound only | ☐ |
| F1.8 | System volume 0 %, mode Both | Vibration only | ☐ |
| F1.9 | Press OK fast (about 5/s) over several rounds | UI never freezes; each round shows the banner | ☐ |

### F2 – Undo

| # | Test | Expected | Result |
|---|---|---|---|
| F2.1 | Count to 10, hold Down | 9, "Undo: -1" | ☐ |
| F2.2 | Hold Down for 3 s | Undo exactly **once** | ☐ |
| F2.3 | At 0 (nothing completed), hold Down | Stays 0, "Nothing to undo" | ☐ |
| F2.4 | Complete a round (33), then hold Down right away | 32/33, "Round undone", rounds −1 | ☐ |
| F2.5 | Short press Down | Nothing happens | ☐ |
| F2.6 | Undo, then Back, exit and reopen | Undone value is restored | ☐ |
| F2.7 | Left/Right/Back still work as before | ✓ | ☐ |

### F3 – Rounds / automatic restart

| # | Test | Expected | Result |
|---|---|---|---|
| F3.1 | Target 33: 31 → OK → 32 → OK | Shows `33/33 Target reached!` for ~1 s, feedback once, then `0/33 Rounds: 1` | ☐ |
| F3.2 | Next OK | 1/33 | ☐ |
| F3.3 | Target 99 | Same behaviour at 99 | ☐ |
| F3.4 | Target 1 | Every press completes a round, banner each time | ☐ |
| F3.5 | Press OK during the banner | Counts as 1 in the new round, banner closes | ☐ |
| F3.6 | Complete a round, immediately press Back and exit, reopen | Count 0, rounds +1 | ☐ |
| F3.7 | Other profiles after a round | Unchanged | ☐ |
| F3.8 | Lower the target below the current count, press OK | Round completes, count 0 | ☐ |
| F3.9 | No `Done +N` anywhere | ✓ | ☐ |

### F4 – Custom profiles

| # | Test | Expected | Result |
|---|---|---|---|
| F4.1 | Profiles → + Add Profile, name "Istighfar", target 100 | Appears in the list, marked `*`, target 100 | ☐ |
| F4.2 | Add "Salawat" and "Morning Dhikr" | All independent, counts stay separate | ☐ |
| F4.3 | Add a duplicate ("salawat") | Keyboard shows "Name already exists" | ☐ |
| F4.4 | Empty name / only spaces | Not accepted | ☐ |
| F4.5 | Hold OK on a custom profile → Rename | New name everywhere, count kept | ☐ |
| F4.6 | Hold OK → Target | Target changes, count kept | ☐ |
| F4.7 | Hold OK → Delete → OK on "No" | Nothing deleted | ☐ |
| F4.8 | Hold OK → Delete → Right → OK | Deleted, back in the list | ☐ |
| F4.9 | Hold OK on SubhanAllah | Only "Target" offered (no Rename/Delete) | ☐ |
| F4.10 | Edit Profiles... → same actions | Same as hold OK | ☐ |
| F4.11 | Add until 16 profiles, then + Add Profile | "Profile limit" notice | ☐ |
| F4.12 | 16-character name on the counter screen | Readable (smaller font if needed) | ☐ |
| F4.13 | Exit and reopen | All profiles, names, targets, counts, rounds restored | ☐ |

### F5 – Profile selection

| # | Test | Expected | Result |
|---|---|---|---|
| F5.1 | Profiles → OK on another profile | `*` moves at once, list **stays open** | ☐ |
| F5.2 | Back | Main menu | ☐ |
| F5.3 | Start Counting | Selected profile is active | ☐ |
| F5.4 | Exit right after selecting, reopen | Selection was saved | ☐ |

### M – Migration and storage

| # | Test | Expected | Result |
|---|---|---|---|
| M1 | Install v1.0.1 over v1.0 with existing data | All counts/targets/names/active profile/feedback kept; counts past target became rounds; `settings.v1.bak` exists; `settings.txt` has `version=2` | ☐ |
| M2 | Corrupt `settings.txt` by hand | "Storage notice", defaults, `settings.bak` | ☐ |
| M3 | Remove the SD card while the app runs, then count and exit | No crash | ☐ |

### General

| # | Test | Expected | Result |
|---|---|---|---|
| G1 | All main menu entries open and Back returns | ✓ | ☐ |
| G2 | Open/close the app 10×, compare free heap (`free` in CLI) | Same value (no leak) | ☐ |
| G3 | Official firmware 1.5.x with the official build | Starts and works | ☐ |

Firmware tested: _______  Date: _______

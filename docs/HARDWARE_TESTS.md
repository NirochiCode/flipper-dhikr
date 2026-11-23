# Hardware test checklist

Install the build that matches your firmware (see README), then work through the list. Record
the firmware version and the results in this file. Screenshots: see
[SCREENSHOT_CHECKLIST.md](SCREENSHOT_CHECKLIST.md).

Since v1.0.3 the UI says **sets** where older sections below say *rounds*, `Reset Counter` is
now `Reset Progress` in each profile's details, and targets are edited there too. The older
sections are kept as they were tested.

## v1.0.3 – menu, profile details and reset (validated on Momentum hardware: core functions)

**Status:** the nine prioritised release checks (R1–R9) **passed** on the user's Flipper
Zero with **Momentum** firmware (`dist/v1.0.3/momentum-mntm-012/flipper_dhikr.fap`), reported
by the user on 2026-11-23. Host unit tests: 74/74. Builds: Momentum `mntm-012` and official
`1.5.1`, no warnings. **The official firmware build has not been tested on hardware.**

The nine release checks do not replace all 31 single tests. Below, a single test is marked
**PASS** only where a release check clearly covers it; all others are **NOT TESTED**.

### Release checks (user report, Momentum)

| # | Release check | Result | Single tests marked PASS |
|---|---|---|---|
| R1 | Update and data transfer: existing profiles, targets, sets and feedback settings kept | PASS | V1 |
| R2 | Main menu and counter: four menu entries, percentage, `Target reached!`, undo | PASS | V2, V5, V6 |
| R3 | Profile management: inactive profiles can be edited without changing the active profile | PASS | V10 |
| R4 | View Progress: Current, Target and Completed Sets show the right values | PASS | V12 |
| R5 | Reset Progress of an inactive profile: only its count and sets go to 0 | PASS | V15, V17, V18 |
| R6 | Reset cancel and undo: No and Back change nothing; after a confirmed reset nothing can be restored | PASS | V16, V19 |
| R7 | Saving after restart: reset values kept, other profiles unchanged | PASS | V20 |
| R8 | Feedback: counter and target vibration/sound still work independently | PASS | (none: no single combination reported, see V23) |
| R9 | New app icon: crescent and star recognisable in the menu, app starts correctly | PASS | V27, V28, V29, V31 |

Result: 16 single tests PASS, 15 NOT TESTED. Notes on the mapping:
- V1: the user confirmed that data was kept; the file-level details (no new `.bak`,
  `version=3`) were not reported separately.
- R2 confirms undo in general, but not the set restore directly after a target (V7).
- R3 confirms editing an inactive profile in general; V9 (`*` moves on OK), V11 (built-in
  details) and V14 (Edit Target on an inactive profile) were not reported individually.
- V29: the user reported the star as recognisable.
- Earlier v1.0.3 build: a reset there only changed the active profile, because it always used
  the active profile rather than the one highlighted in the list (see CHANGELOG). This build
  replaced it with Reset Progress per profile, and R5–R7 confirm the fix.

**Screenshot evidence (2026-11-23):** qFlipper screenshots of this build (`screenshots/`) also
show the expected screens for V11 and V26. Screenshots are not counted as test results.

Install `dist/v1.0.3/momentum-mntm-012/flipper_dhikr.fap`. Keep a copy of
`SD:/apps_data/flipper_dhikr/settings.txt` first. Rows marked ★ are the priority set.

Suggested starting state for V9–V16: SubhanAllah active with about 20/33 and 2 sets;
a custom profile "Istighfar" (target 100) with about 15/100 and some sets.

### Update and main menu

| # | ★ | Test | Expected | Result |
|---|---|---|---|---|
| V1 | ★ | Install over the existing data (v1.0.2 or the earlier v1.0.3 build) | All profiles, names, counts, completed sets, targets, active profile and feedback switches unchanged; no new `.bak` file; `settings.txt` still `version=3` | PASS |
| V2 | ★ | Open the app | Menu: `Start Counting`, `Dhikr Profiles`, `Feedback Settings`, `About`; **no** `Target Settings`, **no** `Reset Counter`; fully readable | PASS |
| V3 | | Open each entry, then Back | Returns to the main menu; Back on the main menu exits | NOT TESTED |
| V4 | | About | `Flipper Dhikr v1.0.3`; Sets and Profiles sections mention the details menu | NOT TESTED |

### Counter screen

| # | ★ | Test | Expected | Result |
|---|---|---|---|---|
| V5 | ★ | Counter at 29/33 | Name, large `29`, `/33`, bar with `87%`; **no** `Sets` text; `▼Undo` and `(+1)` at the bottom | PASS |
| V6 | ★ | Count 32 → 33 | `33/33` and `Target reached!` for ~1 s, target feedback once, then `0/33` and `0%` | PASS |
| V7 | | Hold Down right after a completed set | `32/33`, `Set undone`; View Progress shows one set fewer | NOT TESTED |
| V8 | | Press OK quickly during `Target reached!`; Left/Right; Back | Every press counts once, UI never freezes; profile switch and Back as before | NOT TESTED |

### Profile details and View Progress

| # | ★ | Test | Expected | Result |
|---|---|---|---|---|
| V9 | ★ | Dhikr Profiles → OK on a profile | `*` moves at once, list stays open | NOT TESTED |
| V10 | ★ | Highlight Istighfar (not active), hold OK | Header `Istighfar`; `View Progress`, `Edit Target (100)`, `Rename Profile`, `Reset Progress`, `Delete Profile`; all reachable by scrolling | PASS |
| V11 | ★ | Hold OK on SubhanAllah / Alhamdulillah / Allahu Akbar | `View Progress`, `Edit Target (N)`, `Reset Progress` only | NOT TESTED |
| V12 | ★ | Istighfar → View Progress | `Istighfar`, `Current: 15/100`, `Completed Sets: N`, `Target: 100`, matching the data; nothing changes on OK/Up/Down; Back → Istighfar details; `*` still on SubhanAllah | PASS |
| V13 | | Long custom name (16 characters) → View Progress | Name fully readable (smaller font) | NOT TESTED |
| V14 | | Istighfar → Edit Target → 99 / Custom 100 | Back in Istighfar's details with the new value; active profile unchanged | NOT TESTED |

### Reset Progress

| # | ★ | Test | Expected | Result |
|---|---|---|---|---|
| V15 | ★ | Istighfar → Reset Progress | Dialog `Reset progress?`, `Istighfar`, `15/100, sets N`; **No** highlighted | PASS |
| V16 | ★ | … → OK on No; again → Back | Nothing changes (check View Progress) | PASS |
| V17 | ★ | … → Right → OK (Yes) | Back in Istighfar's details; View Progress: `Current: 0/100`, `Completed Sets: 0`, `Target: 100`; name unchanged | PASS |
| V18 | ★ | After V17: Start Counting | Still SubhanAllah with its old count (e.g. 20/33); its View Progress still shows its sets | PASS |
| V19 | ★ | Reset the **active** profile right after completing a set, then Start Counting and hold Down | Count 0, `Nothing to undo`; no sets come back | PASS |
| V20 | ★ | Exit the app after a reset, reopen | Reset values kept; other profiles unchanged | PASS |
| V21 | | Count in the reset profile up to its target | Counts normally from 1; one set recorded at the target | NOT TESTED |

### Unchanged behaviour (spot checks)

| # | ★ | Test | Expected | Result |
|---|---|---|---|---|
| V22 | | Rename and Delete a custom profile | Work as in v1.0.2; delete asks, No pre-selected | NOT TESTED |
| V23 | | Counter ON / Target OFF | Tick on counts 1–32, silent at 33, `Target reached!` shown | NOT TESTED |
| V24 | | Stealth mode on | `Muted` on the counter screen | NOT TESTED |
| V25 | | 10+ profiles | Profile list scrolls; `+ Add Profile` and `Edit Profiles...` reachable | NOT TESTED |
| V26 | | Dhikr Profiles → `Edit Profiles...` | Only names (`SubhanAllah`, `Alhamdulillah`, `Allahu Akbar`, custom names); no `(fixed)`; built-ins still offer no Rename/Delete | NOT TESTED |
| V27 | ★ | Open the app menu (Apps → Tools) | Flipper Dhikr icon is visible | PASS |
| V28 | ★ | Look at the icon | Crescent clearly recognisable | PASS |
| V29 | ★ | Look at the icon | Star to the right of the crescent reads as five-pointed, not as a cross | PASS |
| V30 | | Move the selection on and off Flipper Dhikr | Icon readable both highlighted and not highlighted | NOT TESTED |
| V31 | ★ | Start the app from the menu | App starts normally (main menu, counting works) | PASS |

Firmware tested: Momentum (user's device; FAP built for `mntm-012`)  Date: 2026-11-23
Official firmware 1.5.1: build only, NOT TESTED on hardware.

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

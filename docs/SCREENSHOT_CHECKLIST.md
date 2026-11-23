# Screenshot checklist (v1.0.3)

Screenshots are only taken on a real Flipper Zero. No simulated or edited images. They are
taken after the v1.0.3 hardware test (see [HARDWARE_TESTS.md](HARDWARE_TESTS.md)).

**Status: no screenshots taken yet.**

## How to take them

1. Connect the Flipper over USB and open **qFlipper**.
2. Start Flipper Dhikr on the Flipper.
3. In qFlipper, open the screen streaming view and use its screenshot button. The image is
   the real 128 × 64 display content.
4. Save each image under the file name from the table into `docs/screenshots/` (PNG, not
   edited; scale up only with nearest-neighbour if a larger copy is needed).

Suggested data: SubhanAllah active with target 33; a custom profile "Istighfar" with target
100, some count and a few completed sets. Feedback switches can stay at the defaults (Counter
OFF/OFF, Target ON/ON).

## Status (2026-11-23)

Captured with qFlipper on a real Flipper Zero; 9 of 10 shots exist (shot 7 is still missing).
The files were renamed by content and are stored unmodified under `screenshots/` (see
[screenshots/README.md](../screenshots/README.md) for checksums and original names).

## Shots

| # | File | Screen | How to get there | Check before saving | Done |
|---|---|---|---|---|---|
| 1 | `01_main_menu.png` | Main menu | Start the app | Four entries: Start Counting, Dhikr Profiles, Feedback Settings, About; **no** Target Settings or Reset Counter | ☑ `screenshots/qflipper/main-menu.png` (first three entries visible) |
| 2 | `02_counter_29_of_33.png` | Counter, e.g. 29/33 | Start Counting, press OK until 29 | Name, large count, `/33`, bar with `87%`, `▼Undo`, `(+1)`; no Sets text | ☑ `screenshots/catalog/01-counter.png` (30/33, `90%`) |
| 3 | `03_target_reached.png` | Target reached | From 32/33 press OK once, take the shot within ~1 s | `33/33`, full bar, `Target reached!` | ☑ `screenshots/catalog/02-target-reached.png` |
| 4 | `04_profiles_active_star.png` | Dhikr Profiles | Main menu → Dhikr Profiles | `*` on the active profile; another profile highlighted if possible | ☑ `screenshots/catalog/03-dhikr-profiles.png` |
| 5 | `05_profile_details.png` | Profile details | Hold OK on Istighfar | Header `Istighfar`; View Progress, Edit Target (100), Rename Profile, Reset Progress (Delete may need scrolling) | ☑ `screenshots/catalog/04-profile-details.png` (SubhanAllah) and `screenshots/qflipper/custom-profile-details.png` (custom, scrolled) |
| 6 | `06_view_progress.png` | View Progress | Details → View Progress | Name, `Current: …/100`, `Completed Sets: …`, `Target: 100` | ☑ `screenshots/catalog/05-view-progress.png` |
| 7 | `07_edit_target.png` | Edit Target | Details → Edit Target | Header `Target: Istighfar`; `33 times`, `99 times`, `Custom value...` / `* Custom: 100` | ☐ not captured yet |
| 8 | `08_reset_confirmation.png` | Reset confirmation | Details → Reset Progress | `Reset progress?`, profile name, count/sets line, **No** highlighted | ☑ `screenshots/catalog/06-reset-confirmation.png` |
| 9 | `09_counter_feedback.png` | Counter Feedback | Feedback Settings → Counter Feedback | `Vibration: …`, `Sound: …`, `Preview` | ☑ `screenshots/catalog/07-counter-feedback.png` |
| 10 | `10_target_feedback.png` | Target Feedback | Feedback Settings → Target Feedback | `Vibration: …`, `Sound: …`, `Preview` | ☑ `screenshots/catalog/08-target-feedback.png` |

Do not press Yes in the reset dialog for shot 8 unless you want to reset that profile.

## After taking them

- Note firmware and app version: Firmware _______, app v1.0.3, date _______.
- Only then reference the images from the README.

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
   scaled or edited; scale up only with nearest-neighbour if a larger copy is needed).

Prepare a profile with target 33 so the counter states are quick to reach. Feedback switches
can stay at the defaults (Counter OFF/OFF, Target ON/ON).

## Shots

| # | File | Screen | How to get there | Check before saving | Done |
|---|---|---|---|---|---|
| 1 | `01_main_menu.png` | Main menu | Start the app | Five entries, **no** `Target Settings`; `Start Counting` highlighted | ☐ |
| 2 | `02_counter_30_of_33.png` | Counter at about 30/33 | Start Counting, press OK until 30 (or 29–31) | Profile name, large count, `/33`, bar with `90%  Sets: N` | ☐ |
| 3 | `03_set_complete.png` | Completion at 33/33 | From 32/33 press OK once, take the shot within ~1 s | `33/33`, full bar, `Set N complete!` | ☐ |
| 4 | `04_counter_after_set.png` | Counter after a completed set | Wait ~1 s after shot 3 | `0/33`, `Sets: 1` (or more) | ☐ |
| 5 | `05_profiles_active_star.png` | Profile list | Main menu → Dhikr Profiles | `*` on the active profile, `+ Add Profile`, `Edit Profiles...` visible if possible | ☐ |
| 6 | `06_profile_edit_menu.png` | Profile edit menu | Hold OK on a custom profile | Header = profile name; `Edit Target (N)`, `Rename Profile`, `Delete Profile` | ☐ |
| 7 | `07_edit_target.png` | Target choice inside a profile | Edit menu → Edit Target | Header `Target: <name>`; `33 times`, `99 times`, `Custom value...` with `*` on the current one | ☐ |
| 8 | `08_counter_feedback.png` | Counter Feedback settings | Feedback Settings → Counter Feedback | `Vibration: …`, `Sound: …`, `Preview` | ☐ |
| 9 | `09_target_feedback.png` | Target Feedback settings | Feedback Settings → Target Feedback | `Vibration: …`, `Sound: …`, `Preview` | ☐ |

Optional, if useful for the README or a catalog listing: the custom target number input
(Edit Target → Custom value...) and the `Reset counter?` dialog.

## After taking them

- Note firmware and app version: Firmware _______, app v1.0.3, date _______.
- Only then reference the images from the README.

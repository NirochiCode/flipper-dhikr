# Hardware test checklist

**Status: not yet run.** No Flipper Zero was connected during v1.0 development, so everything
below is still open. The host unit tests (`make -C tests`) cover the logic but not the display,
buttons, vibration motor, speaker or SD card.

Install the build that matches your firmware (see README), then work through the list. Record
the firmware version, the results and any screenshots (qFlipper → screen streaming →
screenshot) in this file.

| # | Test | Expected | Result |
|---|---|---|---|
| H1 | Fresh install, open app | Main menu appears; `apps_data/flipper_dhikr/settings.txt` is created | ☐ |
| H2 | Start Counting, press OK once | Count 0 → 1, short feedback for the selected mode | ☐ |
| H3 | Hold OK for 2 s | No count (long press ignored) | ☐ |
| H4 | Press OK rapidly 20× | Exactly +20, UI stays responsive | ☐ |
| H5 | Reach target 33 | Distinct target pattern + green LED **once**; bar shows "Target reached!" | ☐ |
| H6 | Keep counting past the target | Normal ticks only; bar shows "Done +N" | ☐ |
| H7 | Left/Right on counter | Profile changes, each keeps its own count | ☐ |
| H8 | Target Settings → 99 / Custom 250 | Target changes, count unchanged | ☐ |
| H9 | Custom target input: 0 and 10000 | Not accepted | ☐ |
| H10 | Reset Counter → OK immediately | "No" is pre-selected → nothing reset | ☐ |
| H11 | Reset Counter → Right → Back | Cancelled, nothing reset | ☐ |
| H12 | Reset Counter → Right → OK | Only the active profile reset to 0 | ☐ |
| H13 | Rename "Custom Dhikr" | New name shown in profiles and counter; survives restart | ☐ |
| H14 | Feedback: each mode + previews | Off silent; Vibration only motor; Sound only speaker; Both both | ☐ |
| H15 | System vibration off / volume 0 / stealth | Feedback follows system settings | ☐ |
| H16 | Count, Back to menu, exit, reopen | All counts, targets, active profile and feedback restored | ☐ |
| H17 | Count, wait 20 s without leaving counter, pull battery/reset | Counts up to the last autosave restored | ☐ |
| H18 | Corrupt `settings.txt` with a text editor, reopen | Notice shown, defaults loaded, `settings.bak` exists | ☐ |
| H19 | Long custom name (16 chars), count 999999, target 9999 | Everything fits on screen | ☐ |
| H20 | Exit app via Back from main menu | App closes cleanly, no crash/log errors | ☐ |
| H21 | Memory check: open/close app 10× | Free heap (Settings → System / `free` in CLI) returns to the same value | ☐ |

Firmware tested: _______  Date: _______

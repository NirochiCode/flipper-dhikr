# Flipper Dhikr

A digital tasbih counter for the Flipper Zero. Count your dhikr with the OK button, set a target per profile and keep your progress on the SD card.

## Features

- **Counter:** large count, target and a progress bar with percentage. OK adds exactly one.
- **Targets:** 33, 99 or any value from 1 to 9999, set separately for each profile.
- **Automatic restart:** at the target the app shows "Target reached!", gives a distinct signal, records one completed set and starts again at 0.
- **Undo:** hold Down to take back the last count. Directly after a completed set, undo restores that set.
- **Profiles:** SubhanAllah, Alhamdulillah and Allahu Akbar are built in, plus an editable Custom Dhikr profile. Add your own profiles (up to 16 profiles in total), each with its own count, target and completed sets.
- **Profile details:** View Progress (current count, completed sets and target), Edit Target and Reset Progress for every profile; Rename and Delete for your own profiles.
- **Safe reset:** Reset Progress only changes the chosen profile and asks for confirmation first, with No pre-selected.
- **Feedback:** independent vibration and sound switches for every count and for reaching the target, each with a preview.
- **Storage:** progress is saved automatically with crash-safe writes. Data from older versions is migrated automatically.

## Controls

- **OK:** count +1
- **Hold Down:** undo the last count
- **Left / Right:** previous or next profile
- **Back:** return to the main menu (progress is saved)
- **Dhikr Profiles:** OK makes a profile active, hold OK opens its details

## Notes

- Vibration and sound also follow the system settings. Stealth mode mutes the app, which is shown on screen.
- The built-in profiles are plain, configurable counters. The app makes no claim about how often any dhikr should be recited.
- Source code, full documentation and issue tracker: [github.com/NirochiCode/flipper-dhikr](https://github.com/NirochiCode/flipper-dhikr)

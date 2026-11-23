# Roadmap

Versions and scope are a plan, not a promise. Religious content (adhkar, duas, recommended
counts) will only be added with a cited, verifiable source and should be reviewed by
knowledgeable people before release.

## v1.0 – Tasbih counter ✅
- Counter with targets 33 / 99 / custom (1–9999), four profiles, reset with confirmation
- Feedback: Off / Vibration / Sound / Both
- Persistent, versioned storage on the SD card

## v1.0.1 – Hardware feedback and usability ✅
- Rounds: automatic restart at the target, completed rounds per profile
- Undo (hold Down), including taking back a just-completed round
- Custom profiles: add, rename, set target, delete (max. 16 profiles)
- Profile list stays open on selection; hold OK to edit
- Stealth-mode detection and feedback help
- Settings format v2 with migration from v1.0

## v1.0.2 – Independent feedback switches ✅ (hardware validated, Momentum)
- Counter and target vibration/sound as four independent switches
- Settings format v3 with migration from v1/v2

## v1.0.3 – Menu and UX ✅ (hardware validated, Momentum)
- Main menu: Start Counting, Dhikr Profiles, Feedback Settings, About
- Profile details: View Progress, Edit Target (N), Rename, Reset Progress, Delete
- Per-profile reset by id with a named confirmation
- Counter screen with percentage and `Target reached!`; completed sets in View Progress
- New crescent-and-star app icon
- qFlipper screenshots (9 of 10 checklist shots) ✅
- Official Flipper Apps Catalog submission (catalog version 1.0), after the hardware test

## v1.1 – Quality of life
- Remaining extended hardware checklist
- Optional "keep backlight on" while counting
- Optional per-profile feedback override
- Reorder custom profiles
- Show storage errors directly on the counter screen
- Optional: keep the undo history across app restarts

## v1.2 – Statistics
- Daily totals and streaks (needs the RTC), stored as daily buckets on the SD card
- Simple charts on the 128×64 display

## v2.0 – Content
- Morning and evening adhkar sequences (guided: text plus target per item)
- Dua library, with content shipped as SD-card assets (`fap_file_assets`) instead of in the binary
- Content source and attribution file

## Later
- **Ramadan companion**: day counter, daily goals
- **Umrah mini helper**: tawaf/sa'i round counter with a clear lap display
- **Multilingual UI**: string table. Arabic script would need custom fonts or bitmaps.
- **NFC dhikr profiles**: start a profile by scanning a tag

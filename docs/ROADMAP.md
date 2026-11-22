# Roadmap

Versions and scope are a plan, not a promise. Religious content (adhkar, duas, recommended
counts) will only be added with a cited, verifiable source and should be reviewed by
knowledgeable people before release.

## v1.0 – Tasbih counter (current)
- Counter with targets 33 / 99 / custom (1–9999)
- Four profiles with independent counts. The custom profile can be renamed.
- Reset with confirmation
- Feedback: Off / Vibration / Sound / Both
- Persistent, versioned storage on the SD card

## v1.1 – Quality of life
- Hardware test pass and UI fixes based on it, real screenshots
- Undo the last count (e.g. long-press Down) and a decrement option
- Optional "rounds" mode: restart automatically after the target and count completed rounds
- Optional "keep backlight on" while counting
- Show storage errors directly on the counter screen

## v1.2 – Custom profiles
- Create, rename, reorder and delete user profiles (up to `DHIKR_PROFILE_MAX`)
- Per-profile feedback override

## v1.3 – Statistics
- Daily totals and streaks (needs the RTC). Append-only log or daily buckets on the SD card.
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

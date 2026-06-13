#pragma once

// In-game 2D HUD / sidebar widescreen fix, applied to the loaded Game.exe image in memory and toggled
// by game state so it only ever affects the in-mission interface, never the front-end main menu.
//
// The fix is a set of small machine-code edits that make the in-game HUD lay out at the real screen
// aspect instead of the original 4:3. It is gated on the game's mission pointer (0x817c00): applied
// while a mission is loaded, removed (vanilla bytes restored) while in the menu, so the menu renders
// exactly as it does without it.
//
// The in-mission HUD widescreen patch itself is Moro's work, shared freely in the Emperor community on
// Discord and used here with permission and thanks. The edit bytes and the full credit are in
// InGameHudFix.cpp.

// Capture the vanilla bytes for every patched region and make the pages writable. Call once at startup,
// before the patch is ever toggled on.
void initHudFix();

// Apply (write the fix) or remove (restore the vanilla bytes) for the whole fix. No-op if already in the
// requested state. Call every frame / on the mission-state change with `inMission = (*(DWORD*)0x817c00 != 0)`.
void hudFixSetState(bool apply);

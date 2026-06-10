# Release notes template

Copy this into the GitHub release body and fill it in. Keep it plain and specific. Say what
actually changed and what you actually tested. Skip the marketing.

---

## Emperor Reborn vX.Y

What changed since the last release:

- (the actual change, in one line. e.g. "Fixed the launcher hanging when the host field was empty")
- (another change)
- (if something is known-broken or half-done, say so here instead of hiding it)

Tested on:
- (the OS/build you actually ran it on, e.g. "Windows 11, build XXXXX")
- (anything a friend or tester confirmed, if any)

Install: drop `EmperorReborn.exe` and `EmperorHooks.dll` into a folder with your own copy of EA's
`EM109EN.EXE`, run the exe. Full steps in [INSTALL.txt](INSTALL.txt). You supply your own game discs
and the EA patch; nothing copyrighted ships here.

Files:
- `EmperorReborn-vX.Y.zip` - SHA256: `...`

Unsigned exe, so SmartScreen will warn (More info, Run anyway). Build it yourself from source if you
would rather not trust the binary.

Forked from [wheybags' patch](https://github.com/wheybags/EmperorLauncher).

---

### Notes for writing these

- One line per change. If you can't explain a change in a line, it is probably two changes.
- Put the SHA256 in. People do check.
- "Known issues" is fine and reads as honest. A perfect-sounding release reads as fake.
- No "comprehensive", "seamless", "robust", "powerful". No em dashes.

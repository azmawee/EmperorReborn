# Emperor Reborn

A launcher I wrote to play *Emperor: Battle for Dune* (Westwood, 2001) on Windows 10 and 11.

![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6?logo=windows)
![License](https://img.shields.io/badge/license-MIT--0-green)
![Build](https://img.shields.io/badge/build-Visual%20Studio%202022%20%7C%20x86-5C2D91?logo=visualstudio)

I wanted to play Emperor again and ran into the usual problems with a 25-year-old game on modern
Windows: the installer fights the OS, and even once it runs the game sits in a small box in the
middle of the monitor. Multiplayer has been dead since Westwood Online shut down.

Tom Mason (wheybags) already wrote a [patch](https://github.com/wheybags/EmperorLauncher) that
handles the hard part: installing and launching cleanly without the original disc check. This is a
fork of his work with the few extra things I wanted, wrapped in a launcher so you do not have to
edit the registry by hand.

## What this adds on top of wheybags' patch

- **True widescreen (16:9).** The headline feature. Every other way of running Emperor wide either
  stretches it fat or crops the top and bottom off. This widens the actual view instead: the
  battlefield renders at the real aspect with no stretching, and the harder half (sidebar, cursor,
  edge scroll and menus) lines up correctly too. As far as I can tell it is the first real widescreen
  for this game in the 25 years since release.
- **Fullscreen scaling.** Your chosen resolution fills the whole monitor instead of pillarboxing.
  On a 4K screen the original resolutions are unusably tiny otherwise.
- **Resolution dropdown.** 16:9 widescreen (1280x720, 1600x900, 1920x1080, 2560x1440) or the
  original 4:3 modes (640x480 up to 1152x864), or match-desktop. Defaults to 1280x720. Saved between
  runs.
- **Hostname / DDNS in multiplayer.** Connect to `something.duckdns.org` instead of a raw IP, which
  is handy when the host's home IP keeps changing.
- **Single exe, no VC++ Redist.** C runtime linked statically.
- **Builds its own clean copy** of the game from your discs. It does not touch any existing install.

It is still the original 2001 game with all its original content. The one real change is optional
widescreen; leave it on a 4:3 resolution and it runs exactly like it always did. Preservation, not a
remaster.

## Install

You provide your own copy of the game (English version) and EA's official v1.09 patch,
`EM109EN.EXE`. That patch is copyrighted by EA and is not included here; it was a free download and
is easy to find by searching the filename. No game data is distributed in this repo.

1. Put `EmperorReborn.exe`, `EmperorHooks.dll`, and your own `EM109EN.EXE` in the same folder.
2. Run `EmperorReborn.exe`. First launch asks for each disc in turn (Disc 1 install, then Atreides,
   Harkonnen, Ordos) and builds a clean copy of the game. It does not need an existing install.
3. Pick your resolution and Fullscreen in the launcher, then Play.

First-run setup happens once. There is a plain-English walkthrough in
[`INSTALL.txt`](INSTALL.txt).

### Resolution

The launcher saves your pick. Lower resolutions give bigger, more readable text; higher ones are
sharper with smaller UI.

| Resolution | Notes |
|---|---|
| `1280 x 720` | Widescreen, default, readable text |
| `1600 x 900` | Widescreen, sharper |
| `1920 x 1080` | Widescreen, sharp, smaller UI |
| `2560 x 1440` | Widescreen, sharpest |
| `640 x 480` to `1152 x 864` | Original 4:3 modes |
| Desktop (match screen) | Native sharpness, but the UI gets small on 4K |

On a 4K monitor, skip "Desktop" and use 1280x720 widescreen (or 1920x1080 if you want it sharper)
with Fullscreen ticked. If the image still sits centered in a small box, your GPU is set to "no
scaling": switch it to Full-screen in the NVIDIA Control Panel (*Adjust desktop size and position*),
or the AMD/Intel equivalent.

## Multiplayer

Westwood Online is gone, so this uses direct IP.

- Host: pick **Host game / singleplayer**, forward **port 4005 (UDP and TCP)** on the router.
- Others: pick **Connect to server**, enter the host's IP, hostname, or DDNS name. Use **Test
  connection** only after the host has clicked Play.
- In game: **Multiplayer**, username + 8-character password, **Login**, **Custom**. Host clicks
  **Host**; others **Refresh** if they don't see it. Pick Deathmatch or Co-op Campaign, everyone
  **Accept**, host clicks **Play**.

Only the parts needed for this flow are wired up, so stick to these steps. The connect box takes a
hostname or DDNS name (DuckDNS, No-IP, Cloudflare) as well as a raw IP, which keeps the address
stable when a home IP changes. DDNS does not open the port for you, you still forward 4005.

## Building

Needs **Visual Studio 2022** with the **Desktop development with C++** workload. If it complains
about the toolset, right-click the solution and Retarget. `EM109EN.EXE` is not needed to build and
is never embedded; the launcher reads it at runtime from the folder next to the exe.

Build Release / x86, or from a Developer PowerShell:

```powershell
msbuild EmperorLauncher.sln /t:Build /p:Configuration=Release /p:Platform=x86
```

Output is `Release/EmperorReborn.exe` and `Release/EmperorHooks.dll`. Both are needed and sit in the
same folder. The exe links the C runtime statically, so no VC++ Redist on the player's side.

To package a redistributable zip (no copyrighted EA content):

```powershell
powershell -ExecutionPolicy Bypass -File .\package.ps1 -Version 2.0
```

wheybags wrote a good [blog post](https://wheybags.com/blog/emperor.html) on the technical details
of the original patch.

## If something's off

- **Already have a working install (wheybags' version, or an old one)?** You can drop
  `EmperorReborn.exe` and `EmperorHooks.dll` straight into that folder and run. If the game files
  are already compatible it skips the disc step and goes to the launcher. People have reported this
  working over an existing install with no reinstall.
- **Windows Security flags it.** It does that with unsigned exes. Add the install folder as an
  exclusion in Windows Security, or hit More info then Run anyway. Source is on GitHub if you would
  rather build it.
- **Image sits in a black box in fullscreen.** That is the GPU upscaler set to "no scaling". Fix it
  in the GPU control panel:
  - NVIDIA: Adjust desktop size and position, Scaling: Full-screen.
  - AMD, including handhelds like the ROG/Xbox Ally: turn off GPU scaling and set scaling mode to
    preserve aspect ratio.
  - On a 1080p screen, 1024x768 fullscreen looks good.

## Credits and license

- Original project: [EmperorLauncher](https://github.com/wheybags/EmperorLauncher) by Tom Mason
  (wheybags), MIT-0.
- This fork keeps MIT-0 (see [`license.txt`](license.txt)). Third-party components are listed in
  [`THIRD-PARTY-NOTICES.txt`](THIRD-PARTY-NOTICES.txt).
- Maintained by azmawee ([GitHub](https://github.com/azmawee)).

MIT-0 covers only the source in this repo. It grants no rights to *Emperor: Battle for Dune*, its
data, or the EA patch, which stay with their owners. This is an unofficial fan-made tool, not
affiliated with or endorsed by Electronic Arts or Westwood Studios. You need to own the game to use
it.

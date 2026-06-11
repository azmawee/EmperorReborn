---
title: "Emperor Reborn - Emperor: Battle for Dune with true widescreen on Windows 10/11"
description: "Play Emperor: Battle for Dune on Windows 10 and 11 with real 16:9 widescreen, fullscreen, original resolutions, and multiplayer over direct IP or DDNS."
image: /screenshots/gameplay-2.png
---

# Emperor Reborn

A launcher I wrote to play *Emperor: Battle for Dune* on modern Windows, now with real 16:9 widescreen.

![Emperor: Battle for Dune running in 16:9 widescreen]({{ "/screenshots/gameplay-2.png" | relative_url }})

[**Download the latest release**](https://github.com/azmawee/EmperorReborn/releases/latest)
&nbsp;|&nbsp;
[**Source on GitHub**](https://github.com/azmawee/EmperorReborn)
&nbsp;|&nbsp;
[**Install guide**](https://github.com/azmawee/EmperorReborn/blob/main/INSTALL.txt)

## Why this exists

I wanted to play Emperor again on my Windows 11 machine and hit the usual wall. The old installer
fights modern Windows, and even once it runs the game sits in a small box in the middle of the
monitor. Multiplayer has been dead since Westwood Online shut down years ago.

Tom Mason (wheybags) already wrote a patch that solves the hardest parts: getting the game to
install and launch cleanly without the original disc check. I started from his work and added the
few things I personally wanted on top, then wrapped it in a launcher so you do not have to touch
the registry.

## What I added on top of wheybags' patch

**True widescreen (16:9).** This is the big one. Emperor was built for 4:3, and every other way of
running it wide just stretches the picture fat or crops the top and bottom off. This one actually
widens the view: pick a 16:9 resolution and the battlefield renders at the real aspect, no
stretching, no black bars. The harder half was the interface, and it is done too: the in-game
sidebar, the cursor, edge scrolling and the menus all sit where they should at 16:9. As far as I can
tell nobody had pulled off real widescreen on this game in the 25 years since it came out.

![The main menu in 16:9 widescreen]({{ "/screenshots/menu.png" | relative_url }})

![Another menu view in widescreen]({{ "/screenshots/menu-2.png" | relative_url }})

![A bigger battle in widescreen]({{ "/screenshots/gameplay-3.png" | relative_url }})

**Fullscreen scaling.** Pick a resolution and it fills the monitor instead of pillarboxing. On a 4K
screen the game looks like a real fullscreen game again instead of a postage stamp.

**Pick your resolution in the launcher.** 16:9 widescreen (1280x720, 1600x900, 1920x1080, 2560x1440)
or the original 4:3 modes (640x480 up to 1152x864), or match the desktop. It defaults to 1280x720
widescreen now. The choice is saved between runs.

**Connect by hostname or DDNS in multiplayer.** Type something like `yourname.duckdns.org` instead
of a raw IP. Home connections rotate their public IP, and I got tired of looking it up and sending
friends a new number every time. A DDNS name stays put.

**Single exe, no VC++ Redist.** The C runtime is linked in statically. One less thing to install,
one less thing to break.

## Install

You need three things:

- The English version of *Emperor: Battle for Dune* (the 4 CDs, or 4 ISO images you can mount)
- EA's official v1.09 patch, `EM109EN.EXE` (free patch, still floating around the usual archives; I
  do not ship it, it is EA's)
- This launcher

Then:

1. Put `EmperorReborn.exe`, `EmperorHooks.dll`, and your own `EM109EN.EXE` in one folder.
2. Run `EmperorReborn.exe`. The first run asks for each disc in turn (Disc 1, then Atreides,
   Harkonnen, Ordos) and builds its own clean copy of the game. It does not touch or need an
   existing install.
3. Pick a resolution, tick Fullscreen, hit Play.

![The launcher]({{ "/screenshots/launcher-ui.png" | relative_url }})

First-run setup is a one-time thing. After that it goes straight to the launcher. Full step by step
is in the [install guide](https://github.com/azmawee/EmperorReborn/blob/main/INSTALL.txt).

## Multiplayer

Westwood Online is gone, so this uses direct IP.

- The host picks **Host game / singleplayer** and forwards **port 4005 (UDP and TCP)** on their
  router.
- Everyone else picks **Connect to server** and types the host's IP, hostname, or DDNS name.
- Hit **Test connection** only after the host has clicked Play.
- In game: **Multiplayer**, pick a username and an 8-character password, **Login**, **Custom**.
  Host clicks **Host**, others click **Refresh** if the game does not show up. Pick Deathmatch or
  Co-op Campaign, everyone **Accept**, host clicks **Play**.

DDNS only keeps the address stable. You still have to forward the port yourself.

## On a 4K monitor

Do not use the "Desktop (match screen)" option on 4K. It renders at full 4K and the original fonts
and tooltips end up too small to read. Pick a set resolution and tick Fullscreen so it scales up:

- **1280x720 (widescreen) + Fullscreen** is the sweet spot: real 16:9 with text that stays readable.
- **1920x1080 (widescreen) + Fullscreen** if you want it sharper and don't mind smaller UI.
- **1024x768 + Fullscreen** if you would rather keep the original 4:3 look.

If it still sits in a small box, your GPU is set to "no scaling". Set scaling to Full-screen in
your GPU control panel (NVIDIA Control Panel under *Adjust desktop size and position*; AMD and Intel
have the same thing under a different name). On AMD handhelds like the ROG/Xbox Ally, the setting
that works is GPU scaling off, scaling mode set to preserve aspect ratio.

## Tested on

Windows 11, which is what I run it on daily. Windows 10 should be fine too. If you run it somewhere
else and it works (or it doesn't), open an issue and tell me.

## FAQ

**Does it work on Windows 11?**
Yes, that is what I use. No VM, no compatibility shim.

**Where do I get `EM109EN.EXE`?**
EA does not host it anymore. Search the filename, it is on the usual patch archives.

**Is there a GOG or Steam version?**
No. EA never released one. Original CDs (or your own backup images) only.

**Do I need to reinstall the game?**
Not if you already have a working install. Drop `EmperorReborn.exe` and `EmperorHooks.dll` into the
folder and run; if the game files are compatible it skips the disc step. People have run it over an
existing wheybags install with no reinstall. A fresh setup from discs only happens if it does not
find a compatible install.

**Windows Security is complaining.**
Unsigned exe, so it warns. Add the folder as an exclusion in Windows Security, or click More info
then Run anyway.

**Is multiplayer Westwood Online?**
No, that shut down. This is direct IP, with hostname and DDNS support so you do not have to
memorise addresses.

**Does this change the game?**
It is the same 2001 game with all the original content. The one real change is optional true
widescreen, which fixes the rendering and the interface for 16:9 instead of stretching. Want the
original look? Pick a 4:3 resolution and it runs exactly like it always did. I am not remastering
anything.

**Is the binary safe?**
Source is on GitHub, build it yourself if you would rather. Each release has SHA256 hashes. Windows
SmartScreen will warn about an unsigned exe from an unknown publisher, that is normal: More info,
Run anyway.

## Source

[github.com/azmawee/EmperorReborn](https://github.com/azmawee/EmperorReborn). MIT-0, same license as
wheybags' original.

## Legal

Not affiliated with EA, Westwood, or wheybags. This is a fan preservation project.

It ships no game files. You bring your own copy and your own EA patch. The launcher patches things
at runtime, it does not redistribute anything copyrighted. *Emperor: Battle for Dune*, *Dune*,
Westwood Studios, and the related trademarks belong to their owners.

Built on top of [wheybags' patch](https://wheybags.com/blog/emperor.html). His
[blog post](https://wheybags.com/blog/emperor.html) explains the hard technical parts and is worth
a read.

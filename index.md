---
title: "Emperor: Battle for Dune on Windows 10 and 11 - real widescreen, fullscreen, multiplayer"
description: "Download Emperor: Battle for Dune for Windows 10 and 11 with real 16:9 widescreen, fullscreen scaling, the original resolutions and direct-IP multiplayer. Free and open source."
image: /screenshots/gameplay-2.png
---

<script type="application/ld+json">
{
  "@context": "https://schema.org",
  "@type": "SoftwareApplication",
  "name": "Emperor Reborn",
  "alternateName": "Emperor: Battle for Dune widescreen launcher",
  "applicationCategory": "GameApplication",
  "operatingSystem": "Windows 10, Windows 11",
  "offers": { "@type": "Offer", "price": "0", "priceCurrency": "USD" },
  "softwareVersion": "2.3",
  "downloadUrl": "https://github.com/azmawee/EmperorReborn/releases/latest",
  "url": "https://azmawee.github.io/EmperorReborn/",
  "author": { "@type": "Person", "name": "azmawee" },
  "description": "Play Emperor: Battle for Dune on Windows 10 and 11 with real 16:9 widescreen, fullscreen scaling, the original resolutions and direct-IP multiplayer."
}
</script>

# Emperor Reborn

Emperor Reborn is a free, open-source launcher that runs *Emperor: Battle for Dune* (Westwood, 2001) on
modern Windows, tested on Windows 11 and expected to work on Windows 10, with real 16:9 widescreen,
fullscreen scaling and working multiplayer.

![Emperor: Battle for Dune in 16:9 widescreen]({{ "/screenshots/gameplay-2.png" | relative_url }})

[**Download the latest release**](https://github.com/azmawee/EmperorReborn/releases/latest)
&nbsp;|&nbsp;
[**Source on GitHub**](https://github.com/azmawee/EmperorReborn)
&nbsp;|&nbsp;
[**Install guide**](https://github.com/azmawee/EmperorReborn/blob/main/INSTALL.txt)

## Verify your download

Every release is checksummed so you can confirm a download is the real build and not a tampered copy
with something slipped in. These are the official SHA256 hashes for v2.3, served over HTTPS from this
site. If a copy you got from anywhere does not match, do not run it.

```
zip                e7499a26420154359fe642ea518ff3fd685bfdcfec1feb523660ac5152114444
EmperorReborn.exe  a9aabb251c664b952c4fddebe36d8a29c2497673bf5a8214ad4a47a13a108f46
EmperorHooks.dll   dd100fd99882363060b57c89a0ad2222c4df15ea4f1a4368e9231f10faa54a5d
```

Check the zip in PowerShell with `Get-FileHash .\EmperorReborn-v2.3.zip -Algorithm SHA256`, or with
`certutil -hashfile EmperorReborn-v2.3.zip SHA256`, and compare it to the line above. The zip also
carries a `SHA256SUMS.txt` listing the two executables, so you can verify those after extracting.

## Why this exists

I wanted to play Emperor again on my Windows 11 machine and hit the usual wall. The old installer
fights modern Windows, and even once it runs the game sits in a small box in the middle of the
monitor. Multiplayer has been dead since Westwood Online shut down years ago.

Tom Mason (wheybags) already wrote a patch that solves the hardest parts, getting the game to
install and launch cleanly without the original disc check. I started from his work and added the
few things I personally wanted on top, then wrapped it in a launcher so you do not have to touch
the registry.

## What I added on top of wheybags' patch

**True widescreen (16:9).** This is the big one. Emperor was built for 4:3, and every other way of
running it wide just stretches the picture fat or crops the top and bottom off. This one actually
widens the view, pick a 16:9 resolution and the battlefield renders at the real aspect, no
stretching, no black bars. The harder half was the interface, and it is done too, the in-game
sidebar, the cursor, edge scrolling and the menus all sit where they should at 16:9. The in-game
sidebar/HUD half builds on Moro's widescreen patch, shared freely in the Emperor community on Discord
and used with thanks; the camera widening and the front-end menus, star map and briefings are mine. As
far as I can tell nobody had pulled off real widescreen on this game, the whole interface included, in
the 25 years since it came out.

[Read how I got the widescreen working]({{ "/widescreen-story" | relative_url }}), including the last
bug that nearly beat me.

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

**No VC++ Redistributable needed.** The C runtime is linked in statically, so there is nothing extra to
install. You run two files, `EmperorReborn.exe` and `EmperorHooks.dll`, plus your own `EM109EN.EXE` on
first setup.

## Install

You need three things,

- The English version of *Emperor: Battle for Dune* (the 4 CDs, or 4 ISO images you can mount)
- EA's official v1.09 patch, `EM109EN.EXE` (free patch, still floating around the usual archives; I
  do not ship it, it is EA's)
- This launcher

Then,

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
- In game, **Multiplayer**, pick a username and an 8-character password, **Login**, **Custom**.
  Host clicks **Host**, others click **Refresh** if the game does not show up. Pick Deathmatch or
  Co-op Campaign, everyone **Accept**, host clicks **Play**.

DDNS only keeps the address stable. You still have to forward the port yourself.

## On a 4K monitor

Do not use the "Desktop (match screen)" option on 4K. It renders at full 4K and the original fonts
and tooltips end up too small to read. Pick a set resolution and tick Fullscreen so it scales up,

- **1280x720 (widescreen) + Fullscreen** is the sweet spot, real 16:9 with text that stays readable.
- **1920x1080 (widescreen) + Fullscreen** if you want it sharper and don't mind smaller UI.
- **1024x768 + Fullscreen** if you would rather keep the original 4:3 look.

If it still sits in a small box, your GPU is set to "no scaling". Set scaling to Full-screen in
your GPU control panel (NVIDIA Control Panel under *Adjust desktop size and position*; AMD and Intel
have the same thing under a different name). On AMD handhelds like the ROG/Xbox Ally, the setting
that works is GPU scaling off, scaling mode set to preserve aspect ratio.

## Tested on

Tested on Windows 11, which is what I run it on daily. Windows 10 is expected to work but is not
verified yet, community verification is needed. If you run it on Windows 10 or anywhere else and it
works (or it doesn't), open an issue and tell me.

## FAQ

**Does it work on Windows 11?**
Yes, that is what I use. No VM, no compatibility shim.

**Where do I get `EM109EN.EXE`?**
EA does not host it anymore. Search the filename, it is on the usual patch archives. Get it from a
source you trust.

**Is there a GOG or Steam version?**
No. EA never released one. Original CDs (or your own backup images) only.

**Do I need to reinstall the game?**
Not if you already have a working install. Drop `EmperorReborn.exe` and `EmperorHooks.dll` into the
folder and run; if the game files are compatible it skips the disc step. People have run it over an
existing wheybags install with no reinstall. A fresh setup from discs only happens if it does not
find a compatible install.

**Windows Security or antivirus is complaining.**
An unsigned exe trips SmartScreen, and a launcher that injects a DLL into the game can trip antivirus
heuristics too even though it is clean. Check the download against the SHA256 above first, then click
More info then Run anyway. Add a Windows Security folder exclusion only as a last resort, if it keeps
getting quarantined.

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
SmartScreen will warn about an unsigned exe from an unknown publisher, that is normal, More info,
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
a read. The in-game HUD widescreen patch is Moro's work, shared freely in the Emperor community on
Discord and used with thanks.

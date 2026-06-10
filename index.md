---
title: "Emperor Reborn - Emperor: Battle for Dune on Windows 10/11"
description: "A launcher that gets Emperor: Battle for Dune running on Windows 10 and 11: fullscreen scaling, original resolutions, and multiplayer over direct IP or DDNS."
image: /screenshots/gameplay-windows11.png
---

# Emperor Reborn

A launcher I wrote to play *Emperor: Battle for Dune* on modern Windows.

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

**Fullscreen scaling.** Pick a resolution and it fills the monitor instead of pillarboxing. The
4:3 aspect is kept, so nothing stretches. On a 4K screen the original 800x600 looks like a real
fullscreen game again instead of a postage stamp.

**Pick your resolution in the launcher.** 640x480, 800x600, 1024x768, 1152x864, or match the
desktop. The choice is saved between runs. Lower resolutions keep the UI and fonts readable, which
matters a lot on a big monitor.

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
and tooltips end up too small to read. Pick a lower resolution and tick Fullscreen so it scales up:

- **1024x768 + Fullscreen** is the best balance of readable UI and sharpness.
- **800x600 + Fullscreen** gives the biggest text.

If it still sits in a small box, your GPU is set to "no scaling". Set scaling to Full-screen in
your GPU control panel (NVIDIA Control Panel under *Adjust desktop size and position*; AMD and Intel
have the same thing under a different name).

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

**Is multiplayer Westwood Online?**
No, that shut down. This is direct IP, with hostname and DDNS support so you do not have to
memorise addresses.

**Does this change the game?**
No. Same 2001 game, original resolutions. I am not remastering anything, just getting it to run.

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

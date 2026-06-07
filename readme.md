# Emperor Reborn: Play *Emperor: Battle for Dune* on Windows 11 & Modern PCs

> **Emperor Reborn** is a free, open-source launcher and patcher that makes the classic
> Westwood real-time strategy game **_Emperor: Battle for Dune_ (2001)** run smoothly on
> **Windows 10 and Windows 11** and modern PCs, at the **original resolutions**, looking
> and playing exactly **as it was originally intended**, with **online multiplayer** working again.

![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6?logo=windows)
![Game](https://img.shields.io/badge/game-Emperor%3A%20Battle%20for%20Dune-D4A017)
![License](https://img.shields.io/badge/license-MIT--0-green)
![Build](https://img.shields.io/badge/build-Visual%20Studio%202022%20%7C%20x86-5C2D91?logo=visualstudio)

If you have been searching for how to **play Emperor: Battle for Dune on Windows 11**, how to
**fix the resolution**, how to get **multiplayer working again**, or how to run this classic
**Dune RTS** on a modern gaming PC without crashes, this is the tool for you.

---

## Table of contents

- [What is Emperor: Battle for Dune?](#what-is-emperor-battle-for-dune)
- [Why Emperor Reborn?](#why-emperor-reborn)
- [Features](#features)
- [Quick start: how to install and play](#quick-start-how-to-install-and-play)
- [Choosing your resolution](#choosing-your-resolution)
- [Online multiplayer](#online-multiplayer)
- [Building from source](#building-from-source)
- [FAQ](#faq)
- [Troubleshooting](#troubleshooting)
- [Credits & license](#credits--license)

---

## What is *Emperor: Battle for Dune*?

*Emperor: Battle for Dune* is a **3D real-time strategy (RTS) game** released by **Westwood
Studios** and **Electronic Arts (EA)** in **2001**, set in Frank Herbert's *Dune* universe. You
command one of the three great Houses, **House Atreides, House Harkonnen, or House Ordos**,
fighting for control of the desert planet **Arrakis** and its precious resource, the **spice
melange**. Built on the same lineage as **Command & Conquer** and **Dune 2000**, it is widely
remembered as one of the best Dune strategy games ever made.

The catch: it is a 25-year-old game built for Windows 98/2000. On a modern PC it can refuse to
install, crash on launch, render at a tiny unreadable size, or lose its online servers entirely.
**Emperor Reborn fixes all of that.**

## Why Emperor Reborn?

The whole point of this project is simple: **let you play Emperor: Battle for Dune on a modern
Windows 11 PC, at its original resolution, exactly as it was originally intended.** No broken
stretching, no microscopic 4K UI, no compatibility-mode guesswork, no dead multiplayer.

Emperor Reborn:

- **Installs and runs the game cleanly on Windows 10 / 11** and modern hardware.
- **Keeps the original 4:3 resolutions and authentic look** so the menus, fonts, and HUD stay
  the right size and the game feels the way it did in 2001: preserved, not "remastered".
- **Scales properly to fullscreen** so a 640x480 or 800x600 game fills your whole monitor
  instead of sitting in a tiny box in the middle of a black 4K screen.
- **Brings back online multiplayer** with direct IP connection and Co-op Campaign, since the
  official Westwood Online (WOL) servers are long gone.
- **Lets the host be reached by name**, not just a raw IP address. You can enter a hostname or a
  free dynamic DNS (DDNS) name when connecting, which is ideal for home internet connections
  where the public IP changes over time.

It is a maintained fork of [**wheybags/EmperorLauncher**](https://github.com/wheybags/EmperorLauncher)
by **Tom Mason**, with extra quality-of-life options (selectable resolution, proper fullscreen
scaling, and a friendlier first-run experience).

## Features

- Runs *Emperor: Battle for Dune* on **modern Windows (10 & 11)** with no compatibility hacks needed
- **Selectable game resolution**: `640x480`, `800x600`, `1024x768`, `1152x864`, or
  **Desktop (match screen)**, so the UI and fonts stay readable instead of being locked to a
  tiny-looking desktop resolution
- **Proper fullscreen scaling**: your chosen resolution is scaled up to fill the entire
  screen instead of a small centered box on a 4K/ultrawide desktop
- **Working online multiplayer** via direct IP connection (no dead Westwood Online required)
- **Connect by hostname or dynamic DNS (DDNS)**, not just a raw IP, so a host on a
  home connection with a changing IP can share one stable name (for example a free
  `duckdns.org` or `no-ip.com` address)
- **Co-op Campaign** mode
- **Self-contained**: links the C runtime statically, so **no Visual C++ Redistributable**
  is needed on the player's PC
- **Clean install**: builds its own fresh copy of the game from your original CDs/discs
- Free and **open source** (MIT-0)

## Quick start: how to install and play

You must **own a copy of _Emperor: Battle for Dune_ (English version)**, the original **4 CDs**
or mountable disc images. This project does **not** include any game data; you provide your own discs.

> **You must also supply the official Westwood/EA v1.09 patch `EM109EN.EXE` yourself.**
> It is copyrighted by EA/Westwood and is **not** distributed with Emperor Reborn. Place your own
> copy of `EM109EN.EXE` in the **same folder** as `EmperorReborn.exe` before first run. On first
> run the launcher checks for it and will prompt you if it is missing.

1. **Put the files together.** Unzip Emperor Reborn anywhere (for example `C:\Games\EmperorReborn\`).
   Keep `EmperorReborn.exe`, `EmperorHooks.dll`, and your own `EM109EN.EXE` **in the same folder**.
2. **Run `EmperorReborn.exe`.** On first launch it will ask you to insert or mount your Emperor
   CDs one at a time (Disc 1 install, then Atreides, Harkonnen, Ordos). It builds its own clean
   copy of the game for you; it does **not** touch or require an existing install.
3. **Pick your resolution and fullscreen option** in the launcher, then click **Play**.

The first-run setup is a one-time thing. After that, the game starts straight away.

A plain-English, step-by-step guide is also included as [`INSTALL.txt`](INSTALL.txt).

## Choosing your resolution

In the launcher there is a **Resolution** dropdown. Your choice is **saved** and kept between runs.

| Resolution | Best for |
|---|---|
| `640 x 480` | Biggest, most readable text and buttons |
| `800 x 600` | **Default**, a nice balance of clarity and detail |
| `1024 x 768` | Sharper, slightly smaller UI |
| `1152 x 864` | Sharpest 4:3 option |
| **Desktop (match screen)** | Fills your monitor at native resolution (sharpest, but UI gets small on big/4K screens) |

Tick **Fullscreen** to have the game fill the screen at that resolution.

> If the game still appears small or centered in fullscreen, your GPU is set to **"no scaling"**.
> Set scaling to **Full-screen** in your GPU control panel:
> - **NVIDIA:** NVIDIA Control Panel, *Adjust desktop size and position*, Scaling: **Full-screen**
> - **AMD / Intel:** look for the equivalent **Scaling Mode, Full panel** option.

Lower resolutions give a bigger, easier-to-read UI. Higher resolutions are sharper but with smaller text.

## Online multiplayer

The official Westwood Online servers are gone, so Emperor Reborn uses **direct IP** connections.

- One player **hosts**: choose **Host game / singleplayer** in the launcher. The host must open
  **port 4005 (UDP and TCP)** on their router.
- Everyone else **connects**: choose **Connect to server** and enter the host's IP address.
- Use **Test connection** only **after** the host has clicked Play.
- In-game: **Multiplayer**, choose any unique username plus an **8-character password**, **Login**, **Custom**.
- The host clicks **Host**; other players click **Refresh** if they don't see the game.
- Host picks **Deathmatch** or **Co-op Campaign**; everyone clicks **Accept**; host clicks **Play**.

> Only the bare minimum needed for multiplayer is wired up, so stick to the steps above and avoid
> clicking unrelated buttons or changing other settings.

### Connecting by hostname or dynamic DNS (DDNS)

The **Connect to server** box accepts more than a raw IP address. You can type:

- an **IPv4 address**, for example `203.0.113.7`
- a **hostname or dynamic DNS (DDNS) name**, for example `myhost.duckdns.org`

This matters because most home internet connections have a **dynamic public IP** that changes
every few days. Instead of looking up and re-sharing your IP every time, the host can register a
free DDNS name (for example with **DuckDNS**, **No-IP**, or **Cloudflare**) that always points at
their current IP, and just share that one stable name. Emperor Reborn resolves the name (its A
record) when you connect and when you press **Test connection**.

The host still needs to open / forward **port 4005 (UDP and TCP)** on their router. DDNS only
keeps the address stable; it does not open the port for you.

## Building from source

Requirements:

- **Visual Studio 2022** (or newer) with the **Desktop development with C++** workload
- The build retargets to the toolset / Windows SDK installed on your machine; if it complains,
  right-click the solution in Visual Studio and choose **Retarget solution**

> The official Westwood/EA v1.09 patch (`EM109EN.EXE`) is **not** required to build and is
> **not** embedded into the executable (it is copyrighted by EA/Westwood). The launcher reads it
> at runtime from the folder next to `EmperorReborn.exe`, so end users supply their own copy.

Open `EmperorLauncher.sln` and build the **Release / x86** configuration, or from a Developer PowerShell:

```powershell
msbuild EmperorLauncher.sln /t:Build /p:Configuration=Release /p:Platform=x86
```

Output is `Release/EmperorReborn.exe` and `Release/EmperorHooks.dll`. Both are needed and must sit
in the same folder. The exe links the C runtime statically, so end users do **not** need the
Visual C++ Redistributable.

To produce a redistributable zip (without any copyrighted EA content), run:

```powershell
powershell -ExecutionPolicy Bypass -File .\package.ps1 -Version 1.0
```

The original author wrote a great
[blog post explaining the technical details](https://wheybags.com/blog/emperor.html).

## FAQ

**Does Emperor: Battle for Dune work on Windows 11?**
Yes. With Emperor Reborn, *Emperor: Battle for Dune* installs and runs on Windows 11 (and
Windows 10) without compatibility mode or manual hacks.

**How do I fix the resolution / tiny screen in Emperor: Battle for Dune?**
Use the launcher's Resolution dropdown to pick a comfortable resolution, tick Fullscreen, and set
your GPU scaling to "Full-screen" (see [Choosing your resolution](#choosing-your-resolution)).

**Can I still play Emperor: Battle for Dune multiplayer online?**
Yes. The original Westwood Online servers are dead, but Emperor Reborn restores online play via
direct IP connection, including Co-op Campaign. See [Online multiplayer](#online-multiplayer).

**Can I host without sharing my IP every time, or with a changing home IP?**
Yes. The connect box accepts a hostname or dynamic DNS (DDNS) name, not just a raw IP. Register a
free DDNS name (DuckDNS, No-IP, Cloudflare, and similar) that tracks your current IP and share
that stable name instead. You still need to forward port 4005 (UDP and TCP) on your router. See
[Connecting by hostname or dynamic DNS (DDNS)](#connecting-by-hostname-or-dynamic-dns-ddns).

**Does it support IPv6?**
The host address is resolved over IPv4 (A record) today, which covers the vast majority of
players. Full IPv6 transport is possible as a future step but is intentionally not enabled yet,
to keep the proven IPv4 multiplayer path stable.

**Do I need the original game?**
Yes. You must own *Emperor: Battle for Dune* (English) and provide your own CDs/disc images, plus
your own copy of the official EA v1.09 patch `EM109EN.EXE`. No game data is distributed here.

**Do I need to install the Visual C++ Redistributable?**
No. The C runtime is linked statically into `EmperorReborn.exe`.

**Is this a remaster or does it change the game?**
No. It preserves the **original game and its original resolutions**. The goal is to play it on a
modern PC **as it was originally intended**, not to remaster or alter it.

**Which game versions/languages are supported?**
The **English** version of the game with the official **v1.09** patch.

## Troubleshooting

- **"It won't start":** make sure `EmperorReborn.exe` and `EmperorHooks.dll` are in the **same folder**.
- **"EM109EN.EXE required" popup:** copy your own `EM109EN.EXE` into the same folder as `EmperorReborn.exe`.
- **Tiny/centered image in fullscreen:** set your GPU scaling to **Full-screen** (see above).
- Install the game into a folder whose path uses **English-only characters** (no special letters).
- Only the **English** version of the game is supported.

## Credits & license

- Original project: **EmperorLauncher** by **Tom Mason** ([wheybags](https://github.com/wheybags/EmperorLauncher)), licensed MIT-0.
- This fork keeps the same **MIT-0** license (see [`license.txt`](license.txt)).
- Third-party components and their licenses are listed in [`THIRD-PARTY-NOTICES.txt`](THIRD-PARTY-NOTICES.txt).
- Maintained by **azmawee** ([GitHub](https://github.com/azmawee), [Facebook](https://facebook.com/azmawee)).

The MIT-0 license covers **only** the source code in this repository. It does **not** grant any
rights to *Emperor: Battle for Dune*, its data, or the official EA/Westwood patch, which remain the
property of their respective owners. **Emperor Reborn is an unofficial fan-made tool and is not
affiliated with, endorsed by, or supported by Electronic Arts or Westwood Studios.** *Emperor:
Battle for Dune*, *Dune*, Westwood Studios, and all related trademarks are the property of their
respective owners. You must own the game to use this tool.

---

<sub>Keywords: Emperor Battle for Dune Windows 11, Emperor Battle for Dune Windows 10, play Emperor
Battle for Dune modern PC, Emperor Battle for Dune resolution fix, Emperor Battle for Dune
multiplayer online, Emperor Battle for Dune LAN, Emperor Battle for Dune hostname, Emperor Battle
for Dune dynamic DNS, Emperor Battle for Dune DDNS, Emperor Battle for Dune connect by name,
Emperor Battle for Dune patch, Westwood Dune RTS, Dune real-time strategy, EmperorLauncher fork,
Emperor Reborn launcher.</sub>

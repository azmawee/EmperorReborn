---
title: "How I got real widescreen working on Emperor: Battle for Dune"
description: "The long version: reverse engineering a 25 year old Westwood game to add true 16:9 widescreen, including the interface, the part everyone else gave up on."
image: /screenshots/gameplay-2.png
---

# How I got real widescreen working on Emperor

[Back to the main page]({{ "/" | relative_url }})

This is the long story behind the one line on the front page that says the widescreen interface
"is done too". It took a lot longer than that sentence makes it sound, and the last bug nearly beat
me, so I wrote down how it went.

## Why this was supposed to be impossible

*Emperor: Battle for Dune* came out in 2001, built for 4:3 monitors. Run it on a modern 16:9 screen
and you get one of two bad outcomes. Either a wrapper stretches the 4:3 picture sideways to fill the
screen and everyone looks fat, or the game zooms in and crops the top and bottom off, which shoves
the menu buttons clean off the edge.

Real widescreen, the kind where you keep the vertical view and reveal more of the world to the left
and right, is a different job. You have to change how the game thinks about the camera, not just how
the final image is stretched. In 25 years nobody had done it on this game.

Tom Mason (wheybags) wrote the patch that makes Emperor install and run on modern Windows at all, and
I started from his work. He kept the game at 4:3 in a letterbox on purpose, which is the sensible
call. I wanted the hard one.

## The first win was a trap

The obvious move is to grab the one Direct3D projection matrix the game renders through and rewrite
it for the right aspect. I did that, and visually it worked on the first try. The battlefield and the
3D menus went properly wide and looked correct. Five minute job, I thought.

Then I clicked a menu button and nothing happened. The picture had moved but the click areas had not.
You had to click below where the button was drawn. The rendering and the mouse were using two
different ideas of where things are, and fixing the picture had only moved one of them.

I spent a while trying to force the game's mouse code to read my corrected matrix. It never worked,
because the game never asks Direct3D for the matrix back. It recomputes its own projection somewhere
inside the executable and never tells the graphics card. So there was no shortcut. I had to go into
the binary.

## Reading a game with no source

I do not have IDA Pro. There was an old analysis file for the executable lying around from someone
years ago, and it turns out a free Python library can read that file directly and hand you the
function names and cross references. Pair that with a disassembler running over the raw game, and you
can read anything. That combination carried the whole project.

The catch was that whoever did the old analysis had named the menus, the networking and the options
code, and left the entire camera and rendering section as anonymous blobs. The exact part I needed
was the exact part nobody had labelled.

## The thing that finally worked

Emperor runs on Westwood 3D, the same engine as Command and Conquer: Renegade, which EA released the
source for in early 2025. I cannot copy that code, but I can read it to understand how the engine
thinks, and the key fact is this, in this engine the rendering and the mouse picking both go through
one shared camera value. They are not independent. Move the one shared thing and they move together.

Emperor's camera does not have a normal field of view setting. It has a focal "distance", and the
width of the view is the screen width divided by that distance. Scale that distance by the right
amount for the aspect ratio and the whole 3D scene goes properly wide, render and mouse together, no
stretching.

I do this from outside the game, every frame, without ever modifying the game on disk. I set a
hardware breakpoint on the instruction right after the render code loads its camera, catch it with an
exception handler, and adjust the two distance values in memory before the frame draws. The
battlefield, the cursor, the edge scrolling and the menus all went to true 16:9 and stayed clickable.
The in-game sidebar needed its own 2D fix, which a community patcher called Moro had already worked
out and shared freely on the Emperor Discord, so I folded that in with thanks.

At that point almost everything was done. Almost.

## The bug that nearly won

Three screens stayed broken. The campaign star map where you pick which territory to attack, the
Mentat briefing, and the house select. On those, the picture was wide and correct, but every click
landed in the wrong place, pulled toward the middle of the screen by exactly three quarters. Click a
territory, it selects the one next to it. Click "Main Menu", you hit empty space.

That number, three quarters, is 960 divided by 1280, the ratio between a 4:3 width and a 16:9 width.
So somewhere those screens were still thinking in 4:3 while everything else had moved on. The trouble
was finding where.

I went down a long list of wrong answers. I checked whether Moro's patch for the in-game sidebar would
fix these screens too. It did not, they use a different system. I dumped the game's whole pile of
global variables on a working screen and again after the bug appeared and compared every single one,
looking for the value that had quietly flipped to a 4:3 number. Nothing had changed. I found the
function the game itself runs when you leave a battle, the one that resets everything cleanly, and
called it on the broken screens, hoping to borrow the game's own fix. It turned out to be a full
display reset and it black screened the game on the spot.

The thing that cracked it was almost stupid. Since the click was off by a clean ratio, there had to
be a number that cancelled it out. So I wired up a live tuner, two keys that nudge the camera
distance on those screens up and down while the game runs, so I could watch the clicks move and dial
them in by hand. A few taps and the clicks snapped into place. The value it landed on was four
thirds, dead on.

Four thirds is the exact inverse of the widescreen correction. In plain terms, on these three
screens, the fix I was applying everywhere else was the bug. The interface on the star map draws and
clicks through that same camera distance, so when I scaled it for the wide look I dragged the clicks
out from under the buttons. Leaving the distance alone on those screens, while still widening the 3D
background a different way, puts the clicks right back where the buttons are.

So the fix is two lines that say "do not apply the widescreen scale to the click math on these three
screens." After days of chasing it, that was the whole thing.

## Why it matters

People have run Emperor stretched fat for years, and a couple of folks managed to widen the picture
while leaving the menus broken or off 4:3. Moro had cracked the in-mission sidebar and HUD, the
in-battle half, and shared it freely on the Emperor Discord, which I built on. What was still missing
was the front-end, the menus, the star map and the briefings, in true widescreen with every click
intact. As far as I can find, nobody had that, the world genuinely wider and the whole interface still
working, clicks and all. The front-end is the part everyone, wheybags included, sidestepped, because
it is the hard half and the game gives you no help finding it. That half is done now. You can play the whole campaign, menus, star map, briefings and
battles, at a real widescreen aspect, and everything lands where you click it.

It is the same 2001 game with all its content untouched. If you would rather have the original look,
pick a 4:3 resolution and it runs exactly as it always did.

[Back to the main page]({{ "/" | relative_url }})
&nbsp;|&nbsp;
[Download]({{ "https://github.com/azmawee/EmperorReborn/releases/latest" }})
&nbsp;|&nbsp;
[Source](https://github.com/azmawee/EmperorReborn)

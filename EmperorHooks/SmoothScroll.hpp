#pragma once

// Framerate-normalized smooth map scrolling. The vanilla game advances the map scroll/pan by a fixed
// step every frame, so the motion stutters on a modern display. This scales the scroll deltas by the
// real frame time instead, so the scroll speed stays consistent and reads smoothly regardless of how
// the frame intervals jitter.
//
// The two scroll-delta call sites and the idea of scaling them by a frame-time multiplier are Moro's,
// shared freely in the Emperor: Battle for Dune community on Discord. Credit and thanks to Moro. The
// delta-time math here (QueryPerformanceCounter, the clamp, the reference-fps multiplier) is our own,
// and the patch is installed at runtime onto a vanilla Game.exe rather than shipped as a patched exe.

// Install the scroll-site patches once at startup. No-op when enabled is false (leaves Game.exe vanilla).
void initSmoothScroll(bool enabled);

// Refresh the per-frame scroll multiplier from the elapsed frame time. Call once per rendered frame from
// EndScene. Cheap no-op until smooth scroll is installed.
void smoothScrollUpdateFrame();

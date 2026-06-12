#pragma once

// Temporary diagnostic: locate the game's mouse-picking / world-to-screen code at runtime by
// hardware-breakpointing the game-space mouse coordinate and logging everything that reads it.
// Remove once the picking projection is found and patched.
void startPickingTracer();

// Call once per frame from the render thread (EndScene) so the tracer knows which thread to set the
// hardware breakpoint on. Cheap no-op after the first call.
void tracerNoteThread();

// True when the active screen should be letterboxed into a centred 4:3 box (everything except the
// battlefield and the main menu, and only when the launcher "Pillarbox" option is on). HookD3D7 reads
// this in SetViewport to narrow the viewport so the 4:3 content keeps its aspect with black side bars.
bool tracerPillarboxWanted();

// Call once per frame from the render thread (EndScene, end of frame). When the conquest/briefing/house
// front-end screens are active, this re-runs the game's own 2D-UI reinit/rebuild (0x4d4da0) once, a short
// delay after entry, so their button hit-rects re-bake against the real widescreen resolution (fixes the
// 0.75 front-end click offset). No-op on every other screen and frame.
void tracerServiceScreenReset();

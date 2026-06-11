#pragma once

// Temporary diagnostic: locate the game's mouse-picking / world-to-screen code at runtime by
// hardware-breakpointing the game-space mouse coordinate and logging everything that reads it.
// Remove once the picking projection is found and patched.
void startPickingTracer();

// Call once per frame from the render thread (EndScene) so the tracer knows which thread to set the
// hardware breakpoint on. Cheap no-op after the first call.
void tracerNoteThread();

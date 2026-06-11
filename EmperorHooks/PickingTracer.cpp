#include <windows.h>
#include <cmath>
#include <cstring>
#include "PickingTracer.hpp"
#include "InGameHudFix.hpp"
#include "Log.hpp"

// Widescreen via focal-distance scaling. The camera has no explicit FOV: width_half = (W/2)/dist,
// so horizontal FOV is fixed and the vertical shrinks as the screen widens (4:3 UI cropped at 16:9).
// Scaling dist by (4/3)/aspect restores the 4:3 vertical view and widens horizontally (Hor+).
//
// Two dist fields get scaled at an execute breakpoint early in the per-frame render
// (sub_41fe20: edi=camera, ebx=scene): camera+0x8C (GPU projection) and scene+0xE4 (CPU
// world-to-screen / picking, with 1/dist at +0xE8). g_scaleMode (registry "WidescreenScaleMode")
// selects which to scale for A/B testing.

#define WIDESCREEN_DIST_DIAGNOSTIC 0

// Cursor diagnostic: data-BP the live in-game mouse var (input mgr 0x818718 + 0x8160 + 0x14) and log
// every instruction that reads it. The reader that scales it about the screen centre is the broken
// cursor draw (wheybags' offset). The poll proved this address holds the true client mouse in-game.
#define WIDESCREEN_CURSOR_DIAGNOSTIC 0

#define WIDESCREEN_ANY_DIAGNOSTIC (WIDESCREEN_DIST_DIAGNOSTIC || WIDESCREEN_CURSOR_DIAGNOSTIC)

static const DWORD HOOK_EIP = 0x0041ffe5;
static const DWORD CAMERA_DIST = 0x8c;
static const DWORD SCENE_DIST = 0xe4;
static const DWORD SCENE_INVDIST = 0xe8;
static const DWORD SCENE_WIDTH = 0xdc;
static const DWORD SCENE_HEIGHT = 0xe0;

static const DWORD INPUT_MGR_ADDR = 0x00818718;
static const DWORD MOUSE_VAR_ADDR = INPUT_MGR_ADDR + 0x8160 + 0x14; // 0x82088C, live mouse X (float)

// Game-state flag used to gate the in-game HUD widescreen fix. 0x817c0c is the battlefield sidebar/HUD
// world object: the sidebar's Open method sets it (0x4b5440) and its teardown clears it (0x4b7284), so
// it is non-null ONLY on the actual battlefield and 0 everywhere else - the front-end menu, the campaign
// conquest/star-map, and the Mentat briefing. (The older 0x817c00 mission-world pointer stays set through
// the post-mission conquest map, which wrongly patched those screens.) The render hook toggles the fix
// from this each frame so only the in-game HUD is patched. See InGameHudFix.hpp.
static const DWORD MISSION_FLAG_ADDR = 0x00817c0c;

static volatile DWORD g_renderThreadId = 0;
static PVOID g_veh = nullptr;
static int g_scaleMode = 3;

// Per-object record of the last dist value WE wrote, so we never re-scale our own output. A single
// "last value" global breaks when two scenes render each frame (the main view + the small EVA "enemy
// attack" alert preview): their dists interleave, the change-check keeps firing, and the value gets
// re-multiplied by 0.75 every frame until it collapses toward 0 and the screen goes black. Keying the
// record by object pointer keeps the two independent.
struct DistMark { DWORD obj; float scaled; };
// Big enough to hold every camera/scene object a session touches (main menu, conquest map, briefing,
// battlefield, the EVA alert preview, minimap, ...) so a long-lived object like the menu camera is
// never evicted and re-scaled.
static const int kDistMarks = 32;
static DistMark g_camMarks[kDistMarks] = {};
static DistMark g_sceneMarks[kDistMarks] = {};

static bool needScale(DistMark* marks, DWORD obj, float cur)
{
  for (int i = 0; i < kDistMarks; i++)
    if (marks[i].obj == obj)
      return fabsf(cur - marks[i].scaled) > 0.01f; // false = it is still our scaled value, leave it
  return true; // first time we have seen this object
}
static void markScaled(DistMark* marks, DWORD obj, float scaled)
{
  for (int i = 0; i < kDistMarks; i++)
    if (marks[i].obj == obj) { marks[i].scaled = scaled; return; }
  for (int i = 0; i < kDistMarks; i++)
    if (marks[i].obj == 0) { marks[i].obj = obj; marks[i].scaled = scaled; return; }
  marks[0].obj = obj; marks[0].scaled = scaled; // cache full (very unlikely), recycle a slot
}

#if WIDESCREEN_ANY_DIAGNOSTIC
static DWORD g_dataBpAddr = 0;
static DWORD g_seenEips[128] = {};
static int g_seenCount = 0;

static void noteReaderEip(DWORD eip)
{
  for (int i = 0; i < g_seenCount; i++)
    if (g_seenEips[i] == eip) return;
  if (g_seenCount < (int)(sizeof(g_seenEips) / sizeof(g_seenEips[0])))
  {
    g_seenEips[g_seenCount++] = eip;
    Log("DIAG: mouse-var read near EIP=%08X (distinct #%d)\n", eip, g_seenCount);
  }
}
#endif

void tracerNoteThread()
{
  if (!g_renderThreadId)
    g_renderThreadId = GetCurrentThreadId();
}

static LONG CALLBACK widescreenVeh(EXCEPTION_POINTERS* ep)
{
  if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
    return EXCEPTION_CONTINUE_SEARCH;

  CONTEXT* c = ep->ContextRecord;

#if WIDESCREEN_ANY_DIAGNOSTIC
  // Dr0 (cursor diag) or Dr1 (dist diag) data breakpoint: log the reader and continue.
  if (c->Dr6 & 0x3)
  {
    c->Dr6 = 0;
    noteReaderEip(c->Eip);
    return EXCEPTION_CONTINUE_EXECUTION;
  }
#endif

#if !WIDESCREEN_CURSOR_DIAGNOSTIC
  // Our two breakpoints: Dr0 = execute BP at the render hook (0x41ffe5; edi=camera, ebx=scene),
  // Dr1 = write data BP on the mission pointer 0x817c00. Handle both in ONE entry so a frame where
  // they coincide never runs the dist-scale with the wrong registers or skips a frame entirely.
  if (c->Dr6 & 0x3)
  {
    c->Dr6 = 0;

    // Keep the in-game HUD patch in sync with the mission flag (idempotent). The Dr1 write BP catches
    // the exact instant it flips - before the menu re-creates its hit-rects; the render BP is a
    // per-frame backup. NOTE: do NOT clear the per-object dist marks on a transition - the menu camera
    // is the SAME object across a mission, and clearing its record makes us re-scale its already-scaled
    // dist (double-scale -> the menu zooms in and its buttons mis-click after you have played a mission).
    bool inMission = *(volatile DWORD*)MISSION_FLAG_ADDR != 0;
    hudFixSetState(inMission);

    // Only scale at the render BP, where edi/ebx are the camera/scene. When Dr1 fired instead, EIP is
    // at the 0x817c00 writer and those registers are meaningless.
    if (c->Eip == HOOK_EIP)
    {
      DWORD camera = c->Edi;
      DWORD scene = c->Ebx;
      if (camera && scene && !IsBadReadPtr((void*)(scene + SCENE_HEIGHT), 4))
      {
        int w = *(int*)(scene + SCENE_WIDTH);
        int h = *(int*)(scene + SCENE_HEIGHT);
        if (w > 0 && h > 0)
        {
          float aspect = float(w) / float(h);
          if (aspect > 1.34f)
          {
            float invS = (4.0f / 3.0f) / aspect;
            if (g_scaleMode & 1)
            {
              float* cd = (float*)(camera + CAMERA_DIST);
              if (needScale(g_camMarks, camera, *cd)) { *cd *= invS; markScaled(g_camMarks, camera, *cd); }
            }
            if (g_scaleMode & 2)
            {
              float* sd = (float*)(scene + SCENE_DIST);
              if (needScale(g_sceneMarks, scene, *sd))
              {
                *sd *= invS;
                if (*sd != 0.0f) *(float*)(scene + SCENE_INVDIST) = 1.0f / *sd;
                markScaled(g_sceneMarks, scene, *sd);
              }
            }
          }
        }
      }
      c->EFlags |= 0x10000; // Resume Flag: step past HOOK_EIP without re-triggering
    }

    return EXCEPTION_CONTINUE_EXECUTION;
  }
#endif
  return EXCEPTION_CONTINUE_SEARCH;
}

// Arm Dr0 as an execute breakpoint (dist-scale / dist diag).
static void setExecuteBp(DWORD tid, DWORD eip)
{
  HANDLE h = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
  if (!h) { Log("WIDE: OpenThread failed %lu\n", GetLastError()); return; }
  SuspendThread(h);
  CONTEXT ctx = {};
  ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
  if (GetThreadContext(h, &ctx))
  {
    ctx.Dr0 = eip;                  // execute BP: render hook (dist-scale + backup patch toggle)
    ctx.Dr1 = MISSION_FLAG_ADDR;    // write data BP on 0x817c0c: toggle patch the instant battlefield state flips
    // NOTE: Dr2/Dr3 on the star-map unproject (sub_41f520/41f760) are OFF - the main menu also calls
    // sub_41f520, so BPing it regressed the menu pick, and the diag proved the mouse is already in REAL
    // pixels (reaches ~1269 on a 1280 screen), so the conquest/briefing offset is NOT a mouse-units issue.
    // Dr7: L0 (Dr0 exec) + L1 (Dr1) + Dr1 R/W=01(write) bits 20-21 + Dr1 LEN=11 bits 22-23.
    ctx.Dr7 = 0x1 | 0x4 | (0b01u << 20) | (0b11u << 22);
    ctx.Dr6 = 0;
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (SetThreadContext(h, &ctx))
      Log("WIDE: execute BP at %08X + data BP on %08X, thread %lu (scaleMode=%d)\n", eip, MISSION_FLAG_ADDR, tid, g_scaleMode);
  }
  ResumeThread(h);
  CloseHandle(h);
}

// Arm Dr0 as a 4-byte read/write data breakpoint (cursor diag, watches a fixed address).
static void setDataBp0(DWORD tid, DWORD addr)
{
  HANDLE h = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
  if (!h) { Log("WIDE: OpenThread failed %lu\n", GetLastError()); return; }
  SuspendThread(h);
  CONTEXT ctx = {};
  ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
  if (GetThreadContext(h, &ctx))
  {
    ctx.Dr0 = addr;
    ctx.Dr7 = 0x1 | (0b11u << 16) | (0b11u << 18); // L0, R/W0=read+write, LEN0=4 bytes
    ctx.Dr6 = 0;
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (SetThreadContext(h, &ctx))
      Log("WIDE: data BP armed on %08X thread %lu\n", addr, tid);
  }
  ResumeThread(h);
  CloseHandle(h);
}

static DWORD WINAPI widescreenThread(LPVOID)
{
  while (!g_renderThreadId)
    Sleep(50);
  Sleep(300);
  for (int i = 0; i < 8; i++)
  {
#if WIDESCREEN_CURSOR_DIAGNOSTIC
    setDataBp0(g_renderThreadId, MOUSE_VAR_ADDR);
#else
    setExecuteBp(g_renderThreadId, HOOK_EIP);
#endif
    Sleep(1500);
  }
  return 0;
}

static int readScaleMode()
{
  char buf[16] = {};
  DWORD sz = sizeof(buf);
  if (RegGetValueA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings",
        "WidescreenScaleMode", RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS)
  {
    int m = atoi(buf);
    if (m >= 0 && m <= 3)
      return m;
  }
  return 3;
}

void startPickingTracer()
{
  g_scaleMode = readScaleMode();
  g_veh = AddVectoredExceptionHandler(1, widescreenVeh);
  CreateThread(nullptr, 0, widescreenThread, nullptr, 0, nullptr);
#if WIDESCREEN_CURSOR_DIAGNOSTIC
  Log("WIDE: CURSOR diagnostic - data BP on live mouse var %08X, logging readers\n", MOUSE_VAR_ADDR);
#elif WIDESCREEN_DIST_DIAGNOSTIC
  Log("WIDE: DIST diagnostic - tracing scene+0xE4 readers (scaling off)\n");
#else
  Log("WIDE: dist-scale started, scaleMode=%d (bit0=camera bit1=scene)\n", g_scaleMode);
#endif
}

#include <windows.h>
#include <tlhelp32.h>
#include <cmath>
#include <cstring>
#include "PickingTracer.hpp"
#include "InGameHudFix.hpp"
#include "MovieFix.hpp"
#include "Log.hpp"

// Widescreen via focal-distance scaling. The camera has no explicit FOV: width_half = (W/2)/dist,
// so horizontal FOV is fixed and the vertical shrinks as the screen widens (4:3 UI cropped at 16:9).
// Scaling dist by (4/3)/aspect restores the 4:3 vertical view and widens horizontally (Hor+).
//
// Two dist fields get scaled at an execute breakpoint early in the per-frame render
// (sub_41fe20: edi=camera, ebx=scene): camera+0x8C (GPU projection) and scene+0xE4 (CPU
// world-to-screen / picking, with 1/dist at +0xE8). g_scaleMode (registry "WidescreenScaleMode")
// selects which to scale for A/B testing.
//
// ----------------------------------------------------------------------------------------------------
// DEV NOTE - the front-end click-offset hunt (kept on purpose so the next person sees the road).
// After the battlefield/menu went widescreen, three 3D front-end screens (conquest star-map "Campaign",
// Mentat "Briefing", house select "House") still mis-clicked by exactly 0.75 = 960/1280: territory picks
// and the bottom buttons landed pulled toward screen centre. The diagnostics below were each built to
// chase a theory, and each is left here (all #defined 0, so they compile out) with the result it gave:
//   FRONTEND_DIAG  - data-BP the live mouse var on those screens. RESULT: the hit-test does NOT read the
//                    raw mouse var 0x82088C at all, so the offset is not a scaled mouse copy there.
//   MOUSEDUMP      - dump the input-mgr mouse fields menu vs conquest. RESULT: mouse is real pixels on
//                    both; no 4:3 field.
//   CLICKTRACE     - BP CHouseAccept::ProcessClick, log the caller. RESULT: the dispatch is shared across
//                    all screens, so the hit-test code is not per-screen; the difference is the data.
//   MENUSCAN/MEMDIFF - the offset PERSISTS into the menu after visiting conquest but resets after a
//                    mission, so something global looked stale. RESULT: a full .data byte-diff (fresh menu
//                    vs after-conquest menu) found NOTHING change by a 0.75/1.333 ratio -> not a global.
//   SCREENRESET    - call the game's own UI reinit 0x4d4da0 on those screens (what a mission runs).
//                    RESULT: 0x4d4da0 is a full DEVICE reinit and BLACK-SCREENS mid-game. Do not call it.
//   SCENELOG/PICKTRACE - log the unproject's scene+camera and the mouse it receives. RESULT: the pick
//                    mouse equals the raw mouse and the scene W is the real 1280, so the 3D pick input is
//                    correct - the offset is in the projection the overlay draws/picks through.
//   UNPROJ_TUNER   - live ']'/'[' keys to scale the unproject dist (scene+0xE4) until clicks line up.
//                    RESULT: it settled on factor 4/3, i.e. invS*4/3 = 1.0 at 16:9 = leave scene+0xE4
//                    NATIVE on those screens. THAT is the fix (see the render hook below). The cause was
//                    our own dist-scale: scene+0xE4 is the projection the conquest overlay uses to BOTH
//                    draw and hit-test itself, so scaling it for the round look desynced its draw from its
//                    pick. camera+0x8C still rounds the GPU 3D, so the look is unchanged.
// ----------------------------------------------------------------------------------------------------

#define WIDESCREEN_DIST_DIAGNOSTIC 0

// Cursor diagnostic: data-BP the live in-game mouse var (input mgr 0x818718 + 0x8160 + 0x14) and log
// every instruction that reads it. The reader that scales it about the screen centre is the broken
// cursor draw (wheybags' offset). The poll proved this address holds the true client mouse in-game.
#define WIDESCREEN_CURSOR_DIAGNOSTIC 0

// Front-end hit-test diagnostic: keep the full dist-scale running (Dr0/Dr1/Dr2 as normal) but ALSO arm
// Dr3 as a read/write data BP on the live mouse var, and log every distinct reader EIP - but ONLY while
// the active screen is one of the 4:3-pinned front-end screens (conquest "Campaign" / "Briefing" /
// "House"). RESULT: the front-end hit-test never reads the raw mouse var, so the offset is not here.
#define WIDESCREEN_FRONTEND_DIAG 0

// Mouse-field dump: dump the input-mgr mouse object fields + the resolution globals once every N render
// frames, tagged with the active screen, to spot a 4:3-scaled (0.75x) copy on conquest vs the real one on
// the main menu. RESULT: the mouse is real pixels on both; no 4:3 field.
#define WIDESCREEN_MOUSEDUMP 0

// Click-trace: execute BP on CHouseAccept::ProcessClick (0x4ca790), which fires AFTER the conquest UI
// hit-test has already matched a button. Logging the caller (return address) reveals the container walk.
// RESULT: the dispatch is shared by all screens, so the hit-test code is not the per-screen difference.
#define WIDESCREEN_CLICKTRACE 0
static const DWORD CLICKTRACE_FN = 0x004ca790;

// Menu-scan: the offset PERSISTS into the main menu after visiting conquest (but NOT after a mission), so
// it looked like conquest writes a global 4:3 value and never restores it. Scan the writable globals on
// each switch to MainMenu for 4:3-artifact values. RESULT: the live scale globals (1.6=realW/800,
// 1.2=realH/600, 1.3333) are constant across screens - not the culprit.
#define WIDESCREEN_MENUSCAN 0

// Scene-log: log the conquest/briefing unproject's scene width/height/dist and camera fields, deduped.
// RESULT: the conquest scene W is the real 1280 (centre 640), dist 1500 - the 3D scene is NOT 4:3-pinned,
// so the offset is not a stale scene size.
#define WIDESCREEN_SCENELOG 0

// Screen-reset: call the game's own UI reinit 0x4d4da0 on the problem screens (what a mission runs) to
// re-bake the 2D layout. RESULT: 0x4d4da0 is a FULL DEVICE reinit (re-creates the render surface) and
// BLACK-SCREENS mid-game. Do not call it. (It is why a real mission "auto-fixes" the menu afterward.)
#define WIDESCREEN_SCREENRESET 0

// Memory byte-diff: snapshot the entire writable .data on the fresh main menu, snapshot again after
// visiting conquest, and diff every DWORD for a value that changed by the 960/1280 = 0.75/1.333 ratio.
// RESULT: NOTHING in .data changed by that ratio -> the persistent state is not a global variable. This
// is what pointed at the projection (scene+0xE4) being the thing out of step, found via the tuner below.
#define WIDESCREEN_MEMDIFF 0

// Pick-trace: execute BP inside the unproject (sub_41f520 @ 0x41f56e, edi=mouse-vec, ebx=scene). Log the
// mouse the unproject RECEIVES vs the raw live mouse + scene W. RESULT: when settled, pickX == rawMouse
// exactly, so the 3D pick INPUT is correct; the offset is in the projection, not the mouse.
#define WIDESCREEN_PICKTRACE 0
static const DWORD PICKTRACE_FN = 0x0041f56e;

// Live unproject-tuner: the territory-pick offset changes direction with the dist scaling, so there is a
// scene+0xE4 scale that lines the pick up with the render. Press ']' / '[' in-game to nudge a factor on
// scene+0xE4 (only) until clicks land, '\' resets. RESULT: it landed on 4/3, i.e. invS*4/3 = 1.0 = leave
// scene+0xE4 native on those screens. That is the shipped fix; this tuner stays off.
#define WIDESCREEN_UNPROJ_TUNER 0

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
// Extra multiplier on the unproject (scene+0xE4) dist scaling only - used by the live tuner (off) to
// align the conquest territory pick with the render. 1.0 = same scale as the render.
static volatile float g_unprojFactor = 1.0f;

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

// Active front-end screen name, recorded by the SwitchToScreen BP (defined fully below). Forward-declared
// here so the diagnostic loggers and the front-end fix gate can read it.
static char g_screenName[40];

#if WIDESCREEN_ANY_DIAGNOSTIC || WIDESCREEN_FRONTEND_DIAG || WIDESCREEN_CLICKTRACE
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
    Log("DIAG: mouse-var read near EIP=%08X tid=%lu scr=%s (distinct #%d)\n",
        eip, GetCurrentThreadId(), g_screenName[0] ? g_screenName : "?", g_seenCount);
  }
}

// Returns true if we have logged this address before; otherwise records it and returns false.
static bool seenEipBefore(DWORD eip)
{
  for (int i = 0; i < g_seenCount; i++)
    if (g_seenEips[i] == eip) return true;
  if (g_seenCount < (int)(sizeof(g_seenEips) / sizeof(g_seenEips[0])))
    g_seenEips[g_seenCount++] = eip;
  return false;
}
#endif

void tracerNoteThread()
{
  if (!g_renderThreadId)
    g_renderThreadId = GetCurrentThreadId();
}

// The game's own screen manager calls GlobalStuff::SwitchToScreen(name) (0x4d6a40, named in the .idb)
// every time the active front-end screen changes, passing the screen's name ("MainMenu", "Campaign",
// "Briefing", "Options", ...). We put an execute breakpoint there and record the name; it fires only on
// a screen change (not per frame) so it never disturbs the menu.
static const DWORD SWITCHSCREEN_FN = 0x004d6a40;
// g_screenName forward-declared and zero-initialised near the top of this file.

// The 3D front-end screens whose 2D territory/UI overlay draws and hit-tests through the CPU world-to-
// screen dist (scene+0xE4): the conquest star-map ("Campaign"), the Mentat briefing ("Briefing") and the
// house select ("House"). On these we leave scene+0xE4 NATIVE (see the render hook) so the overlay's draw
// and its own click hit-test stay in the same projection. The main menu is NOT here - its 2D UI uses a
// different (resolution-correct) path and works with the scene dist scaled.
static bool screenNeedsMoroUiFix(const char* n)
{
  return strcmp(n, "Campaign") == 0
      || strcmp(n, "Briefing") == 0
      || strcmp(n, "House") == 0;
}

// Moro's patch is the in-game HUD 2D-UI fix ONLY. RE-CONFIRMED: applying it on conquest/briefing/house
// does NOT fix their click offset (those screens use a separate front-end 2D system Moro never touched).
static bool moroUiFixWanted()
{
  return *(volatile DWORD*)MISSION_FLAG_ADDR != 0; // battlefield only
}

// HookD3D7 reads this in SetViewport for the (disabled) 4:3 letterbox path. Full widescreen is the
// shipped behaviour, so this always returns false and the viewport is never narrowed.
bool tracerPillarboxWanted() { return false; }

// 0x4d4da0: ScreenMgr method that re-syncs 2D res + rebuilds the UI layout. thiscall, no stack args.
// SCREENRESET experiment only - it turned out to be a full DEVICE reinit that black-screens mid-game.
typedef void (__thiscall *ReinitFn)(void* self);
static const DWORD SCREENMGR_THIS = 0x00818718; // same object SwitchToScreen (0x4d6a40) uses as 'this'
static volatile LONG g_resetArm = 0;       // counting frames on a problem screen
static volatile LONG g_resetDone = 0;      // one-shot per screen entry
static volatile LONG g_reinitPending = 0;  // set by the render BP, serviced by EndScene

// Called from My_IDirect3DDevice7_EndScene (HookD3D7) at end of frame, normal render-thread context.
// Always defined (no-op when the experiment is off) so HookD3D7 always links.
void tracerServiceScreenReset()
{
#if WIDESCREEN_SCREENRESET
  if (!g_reinitPending) return;
  g_reinitPending = 0;
  __try
  {
    ((ReinitFn)0x004d4da0)((void*)SCREENMGR_THIS);
    Log("SCREENRESET: 0x4d4da0 reinit done (this=%08X)\n", SCREENMGR_THIS);
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    Log("SCREENRESET: 0x4d4da0 reinit FAULTED (code %08lX) - backing off\n", GetExceptionCode());
  }
#endif
}

#if WIDESCREEN_MENUSCAN
static volatile LONG g_scanPending = 0;
static int g_scanNo = 0;

// Scan [lo,hi) (walking only committed readable pages) for 4-byte values that look like a 4:3 artifact.
static void scanRange(DWORD lo, DWORD hi, int& found)
{
  DWORD a = lo;
  MEMORY_BASIC_INFORMATION mbi;
  while (a < hi && VirtualQuery((void*)a, &mbi, sizeof(mbi)))
  {
    DWORD regEnd = (DWORD)mbi.BaseAddress + (DWORD)mbi.RegionSize;
    bool ok = mbi.State == MEM_COMMIT
              && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY))
              && !(mbi.Protect & PAGE_GUARD);
    if (ok)
    {
      DWORD e = regEnd < hi ? regEnd : hi;
      for (DWORD p = (a & ~3u); p + 4 <= e; p += 4)
      {
        DWORD v = *(DWORD*)p;
        // 0.75f, 960.0f, 1.3333f (two roundings), 0.625f, 1.2f, 1.6f - candidate 4:3 scale values
        if (v == 0x3F400000 || v == 0x44700000 || v == 0x3FAAAAAB || v == 0x3FAAAAAA ||
            v == 0x3F200000 || v == 0x3F99999A || v == 0x3FCCCCCD)
        {
          Log("  MS @ %08X = %08X (%.4f)\n", p, v, *(float*)p);
          if (++found > 400) { Log("  ...capped\n"); return; }
        }
      }
    }
    a = regEnd;
  }
}

static void doMenuScan()
{
  g_scanNo++;
  Log("MENUSCAN #%d (scr=%s) resW=%d resH=%d | @602B38=%.4f @614420=%.4f @61440C=%.4f\n",
      g_scanNo, g_screenName, *(int*)0x00602458, *(int*)0x0060245c,
      *(float*)0x00602B38, *(float*)0x00614420, *(float*)0x0061440C);
  int found = 0;
  scanRange(0x00600000, 0x00680000, found);
  scanRange(0x007d0000, 0x007e0000, found);
  scanRange(0x00810000, 0x00826000, found);
  Log("MENUSCAN #%d done, %d hits\n", g_scanNo, found);
}
#endif

#if WIDESCREEN_MEMDIFF
// Snapshot the ENTIRE writable .data section (0x5EF000..0xB856C5). The persistent global can be anywhere
// in it, so we diff all of it; the int/float ratio filter (below) keeps the output tiny. RESULT: nothing
// changed by the 0.75/1.333 ratio, which ruled out a global and pointed at the projection.
static const DWORD g_diffLo[] = { 0x005EF000 };
static const DWORD g_diffHi[] = { 0x00B85000 };
static const int kDiffRegions = 1;
static unsigned char* g_snapA = nullptr;
static unsigned char* g_snapB = nullptr;
static DWORD g_snapBytes = 0;
static bool g_snap1Done = false, g_snap2Done = false;
static volatile LONG g_visitedConquest = 0;
static volatile LONG g_memdiffAction = 0; // 1 = take snapshot A, 2 = take snapshot B + diff

static DWORD diffTotal()
{
  DWORD t = 0;
  for (int i = 0; i < kDiffRegions; i++) t += (g_diffHi[i] - g_diffLo[i]);
  return t;
}
static void takeSnapshot(unsigned char* buf)
{
  DWORD off = 0;
  for (int i = 0; i < kDiffRegions; i++)
  {
    for (DWORD p = g_diffLo[i]; p < g_diffHi[i]; p += 0x1000)
    {
      DWORD chunk = (g_diffHi[i] - p < 0x1000) ? (g_diffHi[i] - p) : 0x1000;
      if (!IsBadReadPtr((void*)p, chunk)) memcpy(buf + off + (p - g_diffLo[i]), (void*)p, chunk);
      else memset(buf + off + (p - g_diffLo[i]), 0, chunk);
    }
    off += (g_diffHi[i] - g_diffLo[i]);
  }
}
// Is r ~0.75 or ~1.333 (the 960/1280 signature)?
static bool ratioHit(float r)
{
  return (r > 0.742f && r < 0.758f) || (r > 1.320f && r < 1.347f);
}
static void doMemDiff()
{
  Log("MEMDIFF: changes with int/float ratio ~0.75 or ~1.333 (the 960<->1280 signature) ----\n");
  DWORD off = 0; int logged = 0;
  for (int i = 0; i < kDiffRegions; i++)
  {
    DWORD sz = g_diffHi[i] - g_diffLo[i];
    for (DWORD j = 0; j + 4 <= sz; j += 4)
    {
      DWORD ov = *(DWORD*)(g_snapA + off + j);
      DWORD nv = *(DWORD*)(g_snapB + off + j);
      if (ov == nv) continue;

      bool hit = false; const char* kind = "";
      // int interpretation (e.g. 1280 -> 960)
      if (ov >= 8 && ov <= 100000 && nv >= 8 && nv <= 100000)
      {
        float ir = (float)nv / (float)ov;
        if (ratioHit(ir)) { hit = true; kind = "INT"; }
      }
      // float interpretation (e.g. 1.6 -> 1.2, or 0.001562 -> 0.002083)
      if (!hit)
      {
        float of, nf; memcpy(&of, &ov, 4); memcpy(&nf, &nv, 4);
        if (of == of && nf == nf && of > 1e-5f && of < 1e6f && nf > 1e-5f && nf < 1e6f)
        {
          float fr = nf / of;
          if (ratioHit(fr)) { hit = true; kind = "FLT"; }
        }
      }
      if (hit)
      {
        float of, nf; memcpy(&of, &ov, 4); memcpy(&nf, &nv, 4);
        Log("  @ %08X [%s] : %08X (i=%d f=%.5f) -> %08X (i=%d f=%.5f)\n",
            g_diffLo[i] + j, kind, ov, (int)ov, of, nv, (int)nv, nf);
        if (++logged >= 600) { Log("  ...capped\n"); return; }
      }
    }
    off += sz;
  }
  Log("MEMDIFF done, %d ratio-0.75/1.333 changes\n", logged);
}
// Called EVERY render frame (safe context). Self-triggering off the current screen + visited flag, so a
// missed screen-switch event can never stall it: snapshot A on the first main-menu frame, snapshot B+diff
// on the first main-menu frame after conquest has been visited.
static void serviceMemDiff()
{
  if (g_snapBytes == 0)
  {
    g_snapBytes = diffTotal();
    g_snapA = (unsigned char*)malloc(g_snapBytes);
    g_snapB = (unsigned char*)malloc(g_snapBytes);
  }
  if (!g_snapA || !g_snapB) return;

  bool onMenu = (strcmp(g_screenName, "MainMenu") == 0);
  if (screenNeedsMoroUiFix(g_screenName)) g_visitedConquest = 1; // belt-and-suspenders

  if (!g_snap1Done && onMenu)
  {
    takeSnapshot(g_snapA);
    g_snap1Done = true;
    Log("MEMDIFF: snapshot A taken (fresh main menu, %lu bytes)\n", g_snapBytes);
  }
  else if (g_snap1Done && g_visitedConquest && !g_snap2Done && onMenu)
  {
    takeSnapshot(g_snapB);
    g_snap2Done = true;
    Log("MEMDIFF: snapshot B taken (main menu after conquest), diffing...\n");
    doMemDiff();
  }
}
#endif

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

#if WIDESCREEN_FRONTEND_DIAG
  // Dr3 = read/write data BP on the live mouse var. Log the reader's EIP, but ONLY while the active
  // screen is a 4:3-pinned front-end one (conquest / briefing / house). RESULT: nothing here - the
  // front-end hit-test does not read the raw mouse var.
  if (c->Dr6 & 0x8)
  {
    c->Dr6 = 0;
    if (screenNeedsMoroUiFix(g_screenName))
      noteReaderEip(c->Eip);
    return EXCEPTION_CONTINUE_EXECUTION;
  }
#endif

#if WIDESCREEN_CLICKTRACE
  // Dr3 = execute BP at CHouseAccept::ProcessClick. On entry ESP -> return address, [ESP+4] -> arg1
  // (the click event). Log the caller (the hit-test loop) and the event fields, distinct callers only.
  if (c->Dr6 & 0x8)
  {
    c->Dr6 = 0;
    if (c->Eip == CLICKTRACE_FN && !IsBadReadPtr((void*)c->Esp, 8))
    {
      DWORD caller = *(DWORD*)(c->Esp);
      DWORD ev = *(DWORD*)(c->Esp + 4);
      if (!seenEipBefore(caller))
      {
        Log("CLICKTRACE: ProcessClick caller=%08X event=%08X scr=%s\n", caller, ev,
            g_screenName[0] ? g_screenName : "?");
        if (ev && !IsBadReadPtr((void*)ev, 0x28))
        {
          DWORD* e = (DWORD*)ev;
          Log("  ev +08=%08X +0c=%08X +10=%08X +14=%08X +18=%08X +1c=%08X +20=%08X +24=%08X\n",
              e[2], e[3], e[4], e[5], e[6], e[7], e[8], e[9]);
        }
      }
    }
    c->EFlags |= 0x10000; // step past the BP
    return EXCEPTION_CONTINUE_EXECUTION;
  }
#endif

#if WIDESCREEN_PICKTRACE
  // Dr3 = execute BP inside the unproject (edi=mouse-vec, ebx=scene). Log the pick mouse vs raw mouse,
  // gated to conquest/briefing/house, throttled to the first ~40 hits. RESULT: when settled pickX equals
  // rawMouse, so the pick input is correct - the offset is in the projection.
  if (c->Dr6 & 0x8)
  {
    c->Dr6 = 0;
    if (c->Eip == PICKTRACE_FN)
    {
      static int g_pickN = 0;
      if (g_pickN < 40 && screenNeedsMoroUiFix(g_screenName)
          && !IsBadReadPtr((void*)(c->Edi + 8), 4) && !IsBadReadPtr((void*)(c->Ebx + 0xe0), 4))
      {
        g_pickN++;
        float* mv = (float*)c->Edi;
        Log("PICKTRACE scr=%s pickX=%.1f pickY=%.1f | rawMouse=%.1f,%.1f | sceneW=%d resW=%d resH=%d\n",
            g_screenName, mv[0], mv[1],
            *(float*)MOUSE_VAR_ADDR, *(float*)(MOUSE_VAR_ADDR + 4),
            *(int*)(c->Ebx + SCENE_WIDTH), *(int*)0x00602458, *(int*)0x0060245c);
      }
    }
    c->EFlags |= 0x10000; // step past the BP
    return EXCEPTION_CONTINUE_EXECUTION;
  }
#endif

#if !WIDESCREEN_CURSOR_DIAGNOSTIC
  // Dr2 = execute BP at GlobalStuff::SwitchToScreen: record the name of the screen the game is switching
  // to (arg1 at [esp+4]). Fires only on a screen change (not per frame), so it never disturbs the menu.
  if (c->Dr6 & 0x4)
  {
    c->Dr6 = 0;
    if (c->Eip == SWITCHSCREEN_FN && !IsBadReadPtr((void*)(c->Esp + 4), 4))
    {
      const char* nm = *(const char**)(c->Esp + 4);
      if (nm && !IsBadReadPtr((void*)nm, 1))
      {
        // Popups ("PopUp", "DefeatPopUp", "RetreatPopUp") are transient overlays drawn ON TOP of the
        // current screen - they must NOT change the underlying screen's aspect, or the campaign map
        // flips to widescreen the moment a popup appears over it. Keep the base screen name.
        if (!strstr(nm, "PopUp"))
        {
          int i = 0;
          for (; i < (int)sizeof(g_screenName) - 1 && nm[i]; i++) g_screenName[i] = nm[i];
          g_screenName[i] = 0;
#if WIDESCREEN_MENUSCAN
          // Queue a globals scan (done next frame in the render handler, off the screen-switch stack) on
          // the main menu AND the conquest/briefing/house screens.
          if (strcmp(g_screenName, "MainMenu") == 0 || screenNeedsMoroUiFix(g_screenName))
            g_scanPending = 1;
#endif
#if WIDESCREEN_MEMDIFF
          if (screenNeedsMoroUiFix(g_screenName))
            g_visitedConquest = 1;
          else if (strcmp(g_screenName, "MainMenu") == 0)
          {
            if (!g_snap1Done) g_memdiffAction = 1;                              // fresh menu -> snapshot A
            else if (g_visitedConquest && !g_snap2Done) g_memdiffAction = 2;    // menu after conquest -> snapshot B + diff
          }
#endif
#if WIDESCREEN_SCREENRESET
          if (screenNeedsMoroUiFix(g_screenName)) { g_resetArm = 0; g_resetDone = 0; } // arm on conquest/briefing/house
          else g_resetDone = 1;                                                        // disarm elsewhere
#endif
        }
      }
    }
    c->EFlags |= 0x10000; // step past the BP without re-triggering
    return EXCEPTION_CONTINUE_EXECUTION;
  }

  // Our two breakpoints: Dr0 = execute BP at the render hook (0x41ffe5; edi=camera, ebx=scene),
  // Dr1 = write data BP on the mission flag. Handle both in ONE entry so a frame where they coincide
  // never runs the dist-scale with the wrong registers or skips a frame entirely.
  if (c->Dr6 & 0x3)
  {
    c->Dr6 = 0;

    // Keep the in-game HUD patch in sync with the mission flag (idempotent). The Dr1 write BP catches the
    // exact instant it flips; the render BP is a per-frame backup. NOTE: do NOT clear the per-object dist
    // marks on a transition - the menu camera is the SAME object across a mission, and clearing its record
    // makes us re-scale its already-scaled dist (double-scale -> the menu zooms in and its buttons
    // mis-click after you have played a mission).
    hudFixSetState(moroUiFixWanted());

    // Only scale at the render BP, where edi/ebx are the camera/scene. When Dr1 fired instead, EIP is at
    // the mission-flag writer and those registers are meaningless.
    if (c->Eip == HOOK_EIP)
    {
#if WIDESCREEN_SCREENRESET
      // Count frames on a problem screen; at a couple of thresholds flag a reinit (serviced in EndScene).
      // SCREENRESET experiment only - 0x4d4da0 black-screens, so this stays off.
      if (screenNeedsMoroUiFix(g_screenName) && !g_resetDone)
      {
        LONG f = ++g_resetArm;
        if (f == 150 || f == 320) g_reinitPending = 1;
        if (f >= 320) g_resetDone = 1;
      }
#endif
#if WIDESCREEN_MENUSCAN
      if (g_scanPending)
      {
        g_scanPending = 0;
        doMenuScan();
      }
#endif
#if WIDESCREEN_MEMDIFF
      serviceMemDiff();
#endif
#if WIDESCREEN_MOUSEDUMP
      // Throttled dump: input-mgr mouse object (base 0x818718+0x8160 = 0x820878) fields as floats, plus
      // the resolution globals, tagged with the active screen. RESULT: mouse is real pixels on every
      // screen; no 4:3-scaled field.
      {
        static int g_mdumpFrame = 0;
        if (((g_mdumpFrame++) % 45) == 0)
        {
          const float* mf = (const float*)(INPUT_MGR_ADDR + 0x8160);
          if (!IsBadReadPtr(mf, 0x2c))
            Log("MDUMP scr=%-10s resW=%d resH=%d | +14=%.1f +18=%.1f +1c=%.1f +20=%.1f +24=%.1f +28=%.1f\n",
                g_screenName[0] ? g_screenName : "?", *(int*)0x00602458, *(int*)0x0060245c,
                mf[5], mf[6], mf[7], mf[8], mf[9], mf[10]);
        }
      }
#endif
      DWORD camera = c->Edi;
      DWORD scene = c->Ebx;
      if (camera && scene && !IsBadReadPtr((void*)(scene + SCENE_HEIGHT), 4))
      {
        int w = *(int*)(scene + SCENE_WIDTH);
        int h = *(int*)(scene + SCENE_HEIGHT);
#if WIDESCREEN_SCENELOG
        // Log the conquest/briefing scene + camera numbers, deduped by (scene, visited-phase). RESULT:
        // the conquest scene W is the real 1280, dist 1500 - the 3D scene is not 4:3-pinned.
        {
          static volatile LONG s_visited = 0;
          if (screenNeedsMoroUiFix(g_screenName)) s_visited = 1;
          bool onMenu = (strcmp(g_screenName, "MainMenu") == 0);
          if (onMenu || screenNeedsMoroUiFix(g_screenName))
          {
            DWORD key = scene ^ (s_visited ? 0x80000000u : 0u);
            static DWORD g_loggedScenes[40] = {};
            bool seen = false;
            for (int i = 0; i < 40; i++) if (g_loggedScenes[i] == key) { seen = true; break; }
            if (!seen)
            {
              for (int i = 0; i < 40; i++) if (g_loggedScenes[i] == 0) { g_loggedScenes[i] = key; break; }
              Log("SCENELOG scr=%-9s phase=%ld scene=%08X W=%d H=%d sDist=%.2f sInv=%.6f | cam=%08X cDist=%.2f cFOV(+98)=%.5f c+9c=%.5f c+a0=%.5f\n",
                  g_screenName, (long)s_visited, scene, w, h, *(float*)(scene + SCENE_DIST), *(float*)(scene + SCENE_INVDIST),
                  camera, *(float*)(camera + CAMERA_DIST), *(float*)(camera + 0x98), *(float*)(camera + 0x9c), *(float*)(camera + 0xa0));
            }
          }
        }
#endif
        if (w > 0 && h > 0)
        {
          float aspect = float(w) / float(h);
          if (aspect > 1.34f)
          {
            float invS = (4.0f / 3.0f) / aspect;
            // A Bink cutscene reuses the menu's 3D scene object and sets its dist to 1.0 for its own
            // draw. Scaling that (and overwriting our per-object mark) corrupts the menu's dist, so after
            // the movie the menu double-scales (1500 -> 1125 every frame) and clicks land off. Leave
            // every dist and mark untouched while a movie is on screen; the menu then matches its own
            // mark again the moment the movie ends.
            bool moviePlaying = movieIsPlaying();
            if ((g_scaleMode & 1) && !moviePlaying)
            {
              float* cd = (float*)(camera + CAMERA_DIST);
              if (needScale(g_camMarks, camera, *cd)) { *cd *= invS; markScaled(g_camMarks, camera, *cd); }
            }
            // *** THE FRONT-END CLICK FIX ***
            // scene+0xE4 is the CPU world-to-screen / unproject dist that the conquest/briefing territory
            // OVERLAY uses to BOTH draw and hit-test its 2D regions. Scaling it (for the round look) moves
            // that overlay's projection out of step with its own pick -> a 0.75 = 960/1280 click offset on
            // the territory and the bottom buttons. The live tuner (UNPROJ_TUNER above) settled on factor
            // 4/3, i.e. invS*4/3 = 1.0 at 16:9 = leave scene+0xE4 NATIVE on those screens. The GPU sphere
            // still rounds via camera+0x8C above, so the look is unchanged.
            //
            // CRITICAL: gate on the mission flag too, not just the screen name. Entering a mission from the
            // Mentat leaves g_screenName as "Briefing", so a screen-name-only test wrongly left scene+0xE4
            // native IN THE BATTLE, which knocked the in-game cursor / unit picking / sidebar off (the
            // offset grew from screen centre). On the battlefield the mission flag (0x817c0c) is set, so we
            // always scale there; we only leave scene+0xE4 native on the genuine front-end screens, where
            // the flag is clear. Battle and menu keep the original scene-dist scaling.
            bool frontEndNative = screenNeedsMoroUiFix(g_screenName)
                                  && (*(volatile DWORD*)MISSION_FLAG_ADDR == 0);
            if ((g_scaleMode & 2) && !frontEndNative && !moviePlaying)
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

// Arm Dr0 = render-hook execute BP, Dr1 = mission-flag write BP, Dr2 = SwitchToScreen execute BP. The
// diagnostics above optionally add Dr3 (a data/execute BP) for their trace.
static void setExecuteBp(DWORD tid, DWORD eip)
{
  HANDLE h = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
  if (!h) { Log("WIDE: OpenThread failed %lu\n", GetLastError()); return; }
  SuspendThread(h);
  CONTEXT ctx = {};
  ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
  if (GetThreadContext(h, &ctx))
  {
    ctx.Dr0 = eip;                  // execute BP: render hook (dist-scale + HUD patch toggle)
    ctx.Dr1 = MISSION_FLAG_ADDR;    // write data BP on 0x817c0c: toggle patch the instant battlefield flips
    ctx.Dr2 = SWITCHSCREEN_FN;      // execute BP on GlobalStuff::SwitchToScreen: record the active screen
    // Dr7: L0 (Dr0 exec) + L1 (Dr1) + L2 (Dr2 exec) + Dr1 R/W=01(write) bits 20-21 + Dr1 LEN=11 bits 22-23.
    // Dr0/Dr2 are execute BPs so their R/W=00 LEN=00 bits stay 0.
    ctx.Dr7 = 0x1 | 0x4 | 0x10 | (0b01u << 20) | (0b11u << 22);
#if WIDESCREEN_FRONTEND_DIAG
    ctx.Dr3 = MOUSE_VAR_ADDR;       // read/write data BP on the live mouse var: log front-end hit-test readers
    // L3 (bit 6) + Dr3 R/W=11(read/write) bits 28-29 + Dr3 LEN=11 (4 bytes) bits 30-31.
    ctx.Dr7 |= 0x40 | (0b11u << 28) | (0b11u << 30);
#elif WIDESCREEN_CLICKTRACE
    ctx.Dr3 = CLICKTRACE_FN;        // execute BP on CHouseAccept::ProcessClick: log the hit-test caller
    ctx.Dr7 |= 0x40;               // L3 only; Dr3 R/W=00 LEN=00 (execute)
#elif WIDESCREEN_PICKTRACE
    ctx.Dr3 = PICKTRACE_FN;         // execute BP inside the unproject: log pick-mouse vs raw mouse
    ctx.Dr7 |= 0x40;               // L3 only; execute
#endif
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

#if WIDESCREEN_FRONTEND_DIAG
// Arm Dr3 (mouse-var read/write data BP) on EVERY thread except the render thread and ourselves. The
// front-end button hit-test runs on the message-pump thread, NOT the render thread, so the render-only
// BP missed it. The VEH is process-wide, so a Dr3 hit on any thread lands in the Dr6&0x8 handler.
static void armMouseBpOtherThreads()
{
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE) return;
  THREADENTRY32 te = {}; te.dwSize = sizeof(te);
  DWORD pid = GetCurrentProcessId();
  DWORD self = GetCurrentThreadId();
  if (Thread32First(snap, &te))
  {
    do
    {
      if (te.th32OwnerProcessID != pid) continue;
      DWORD tid = te.th32ThreadID;
      if (tid == g_renderThreadId || tid == self) continue;
      HANDLE h = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
      if (!h) continue;
      SuspendThread(h);
      CONTEXT ctx = {}; ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
      if (GetThreadContext(h, &ctx))
      {
        ctx.Dr3 = MOUSE_VAR_ADDR;
        ctx.Dr7 |= 0x40 | (0b11u << 28) | (0b11u << 30); // L3 + Dr3 R/W=11 + LEN=11 (4 bytes)
        ctx.Dr6 = 0;
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        SetThreadContext(h, &ctx);
      }
      ResumeThread(h);
      CloseHandle(h);
    } while (Thread32Next(snap, &te));
  }
  CloseHandle(snap);
}
#endif

static DWORD WINAPI widescreenThread(LPVOID)
{
  while (!g_renderThreadId)
    Sleep(50);
  Sleep(300);
#if WIDESCREEN_FRONTEND_DIAG
  // Keep re-arming for ~3 minutes so the message-pump thread (and any thread spawned after launch) gets
  // the mouse BP. Gives time to navigate menu -> conquest -> click.
  for (int i = 0; i < 120; i++)
  {
    setExecuteBp(g_renderThreadId, HOOK_EIP);
    armMouseBpOtherThreads();
    Sleep(1500);
  }
#else
  for (int i = 0; i < 8; i++)
  {
#if WIDESCREEN_CURSOR_DIAGNOSTIC
    setDataBp0(g_renderThreadId, MOUSE_VAR_ADDR);
#else
    setExecuteBp(g_renderThreadId, HOOK_EIP);
#endif
    Sleep(1500);
  }
#endif
#if WIDESCREEN_UNPROJ_TUNER
  // Live tuner: ']' = +0.02, '[' = -0.02, '\' = reset to 1.0. Logs g_unprojFactor on each change. It was
  // used to find the front-end fix value (4/3 = leave scene+0xE4 native); kept here for the record.
  bool prevUp = false, prevDn = false, prevRst = false;
  for (;;)
  {
    bool up = (GetAsyncKeyState(VK_OEM_6) & 0x8000) != 0;  // ']'
    bool dn = (GetAsyncKeyState(VK_OEM_4) & 0x8000) != 0;  // '['
    bool rst = (GetAsyncKeyState(VK_OEM_5) & 0x8000) != 0; // '\'
    if (up && !prevUp)   { g_unprojFactor += 0.02f; Log("TUNER: unprojFactor = %.3f\n", g_unprojFactor); }
    if (dn && !prevDn)   { g_unprojFactor -= 0.02f; Log("TUNER: unprojFactor = %.3f\n", g_unprojFactor); }
    if (rst && !prevRst) { g_unprojFactor = 1.0f;   Log("TUNER: unprojFactor RESET = %.3f\n", g_unprojFactor); }
    prevUp = up; prevDn = dn; prevRst = rst;
    Sleep(40);
  }
#endif
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
#if WIDESCREEN_FRONTEND_DIAG
  Log("WIDE: FRONTEND diagnostic - Dr3 data BP on mouse var %08X, logging readers on conquest/briefing/house only\n", MOUSE_VAR_ADDR);
#endif
#if WIDESCREEN_MOUSEDUMP
  Log("WIDE: MOUSEDUMP active - dumping mouse-object fields + res globals every 45 frames\n");
#endif
}

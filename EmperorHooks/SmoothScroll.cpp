#include <windows.h>
#include "SmoothScroll.hpp"
#include "Log.hpp"

// See SmoothScroll.hpp for what this does and the credit to Moro for the sites + the scaling idea.
//
// Two call sites in Game.exe feed the same movement function (0x532b20). Each loads the float scroll
// deltas off the stack with a pair of stack-load movs. We steal that 8-byte run, redirect to a small
// trampoline that multiplies the float deltas on the stack by g_factor, replays the stolen movs, then
// jumps back. g_factor is refreshed once per frame from the real frame time, so a longer frame scrolls
// proportionally further and the motion stays smooth. When g_factor is 1.0 the behaviour is vanilla,
// which is also the failsafe if the per-frame update never runs.

namespace
{
  bool          g_enabled = false;
  double        g_factor = 1.0;        // per-frame scroll multiplier, 1.0 == vanilla speed at g_refFps
  LARGE_INTEGER g_qpcFreq = {};
  LARGE_INTEGER g_lastCounter = {};    // QPC at the previous frame, 0 until the first frame seeds it

  double        g_refFps = 60.0;       // fps at which g_factor is 1.0 (the game's locked frame rate)
  double        g_clampSeconds = 0.05; // ignore frame gaps longer than this (alt-tab, loading hitches)

  unsigned char* g_trampoline = nullptr;

  // A scroll-delta call site: patchVa is the 8-byte run we overwrite with a jmp, returnVa is where the
  // trampoline jumps back to after replaying the stolen instructions.
  struct Site { DWORD patchVa; DWORD returnVa; };
  const Site kSite1 = { 0x004bba99, 0x004bbaa1 }; // scales [esp+0x18] and [esp+0x10], replays mov ecx/edx
  const Site kSite2 = { 0x004b5f34, 0x004b5f3c }; // scales st(0) and [esp+0x14], replays mov eax/ecx

  void put32(unsigned char* p, DWORD v) { memcpy(p, &v, 4); }

  // Emit "jmp targetVa" (E9 rel32) at p, advancing p past it. rel32 is relative to the byte after the jmp.
  void emitJmp(unsigned char*& p, DWORD targetVa)
  {
    *p++ = 0xE9;
    DWORD afterJmp = (DWORD)(DWORD_PTR)p + 4;
    put32(p, targetVa - afterJmp);
    p += 4;
  }

  // Emit "fmul qword ptr [&g_factor]" (DC /1 with disp32) at p, advancing p past it.
  void emitFmulFactor(unsigned char*& p, DWORD factorAddr)
  {
    *p++ = 0xDC; *p++ = 0x0D; put32(p, factorAddr); p += 4;
  }

  // Overwrite the 8 vanilla bytes at va with "jmp tramp" + NOP padding.
  bool patchSite(DWORD va, DWORD tramp)
  {
    DWORD oldProt = 0;
    if (!VirtualProtect((void*)va, 8, PAGE_EXECUTE_READWRITE, &oldProt))
    {
      Log("SmoothScroll: VirtualProtect failed at %08X (err %lu)\n", va, GetLastError());
      return false;
    }
    unsigned char buf[8];
    buf[0] = 0xE9;
    DWORD rel = tramp - (va + 5);
    memcpy(buf + 1, &rel, 4);
    buf[5] = buf[6] = buf[7] = 0x90; // pad the rest of the stolen run with NOPs
    memcpy((void*)va, buf, sizeof(buf));
    FlushInstructionCache(GetCurrentProcess(), (void*)va, sizeof(buf));
    VirtualProtect((void*)va, 8, oldProt, &oldProt);
    return true;
  }

  // Optional registry overrides so the feel can be calibrated in-game without a rebuild. Both live under
  // the launcher settings key. SmoothScrollRefFps (e.g. 60) and SmoothScrollClampMs (e.g. 50).
  void readTunables()
  {
    char buf[16] = {};
    DWORD sz = sizeof(buf);
    if (RegGetValueA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings",
          "SmoothScrollRefFps", RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS)
    {
      double v = atof(buf);
      if (v >= 1.0 && v <= 1000.0)
        g_refFps = v;
    }
    sz = sizeof(buf);
    if (RegGetValueA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings",
          "SmoothScrollClampMs", RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS)
    {
      double v = atof(buf);
      if (v >= 1.0 && v <= 1000.0)
        g_clampSeconds = v / 1000.0;
    }
  }
}

void smoothScrollUpdateFrame()
{
  if (!g_enabled)
    return;

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);

  if (g_lastCounter.QuadPart == 0)
  {
    g_lastCounter = now; // first frame just seeds the clock, factor stays 1.0
    return;
  }

  double dt = double(now.QuadPart - g_lastCounter.QuadPart) / double(g_qpcFreq.QuadPart);
  g_lastCounter = now;

  if (dt < 0.0) dt = 0.0;
  if (dt > g_clampSeconds) dt = g_clampSeconds;

  g_factor = dt * g_refFps;
}

void initSmoothScroll(bool enabled)
{
  if (!enabled)
  {
    Log("SmoothScroll: off (Game.exe left vanilla)\n");
    return;
  }

  if (!QueryPerformanceFrequency(&g_qpcFreq) || g_qpcFreq.QuadPart == 0)
  {
    Log("SmoothScroll: QPC unavailable, not installing\n");
    return;
  }

  readTunables();

  g_trampoline = (unsigned char*)VirtualAlloc(nullptr, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (!g_trampoline)
  {
    Log("SmoothScroll: VirtualAlloc failed (err %lu)\n", GetLastError());
    return;
  }

  const DWORD facAddr = (DWORD)(DWORD_PTR)&g_factor;

  // Trampoline 1: scale the two scroll floats at [esp+0x18] and [esp+0x10], replay the stolen
  // "mov ecx,[esp+0x18]; mov edx,[esp+0x10]", then jump back to 0x4bbaa1.
  unsigned char* const t1 = g_trampoline;
  unsigned char* p = t1;
  *p++ = 0xD9; *p++ = 0x44; *p++ = 0x24; *p++ = 0x18;   // fld  dword [esp+0x18]
  emitFmulFactor(p, facAddr);                            // fmul qword [g_factor]
  *p++ = 0xD9; *p++ = 0x5C; *p++ = 0x24; *p++ = 0x18;   // fstp dword [esp+0x18]
  *p++ = 0xD9; *p++ = 0x44; *p++ = 0x24; *p++ = 0x10;   // fld  dword [esp+0x10]
  emitFmulFactor(p, facAddr);                            // fmul qword [g_factor]
  *p++ = 0xD9; *p++ = 0x5C; *p++ = 0x24; *p++ = 0x10;   // fstp dword [esp+0x10]
  *p++ = 0x8B; *p++ = 0x4C; *p++ = 0x24; *p++ = 0x18;   // mov  ecx,[esp+0x18]   (stolen)
  *p++ = 0x8B; *p++ = 0x54; *p++ = 0x24; *p++ = 0x10;   // mov  edx,[esp+0x10]   (stolen)
  emitJmp(p, kSite1.returnVa);                           // jmp  0x4bbaa1

  // Trampoline 2: the value already in st(0) here is the third scroll component (the next vanilla
  // instruction stores it to [esp+0x30]); scale it, then scale [esp+0x14], replay the stolen
  // "mov eax,[esp+0x14]; mov ecx,[esp+0x14]", then jump back to 0x4b5f3c.
  unsigned char* const t2 = p;
  emitFmulFactor(p, facAddr);                            // fmul qword [g_factor]   (scales st(0))
  *p++ = 0xD9; *p++ = 0x44; *p++ = 0x24; *p++ = 0x14;   // fld  dword [esp+0x14]
  emitFmulFactor(p, facAddr);                            // fmul qword [g_factor]
  *p++ = 0xD9; *p++ = 0x5C; *p++ = 0x24; *p++ = 0x14;   // fstp dword [esp+0x14]
  *p++ = 0x8B; *p++ = 0x44; *p++ = 0x24; *p++ = 0x14;   // mov  eax,[esp+0x14]   (stolen)
  *p++ = 0x8B; *p++ = 0x4C; *p++ = 0x24; *p++ = 0x14;   // mov  ecx,[esp+0x14]   (stolen)
  emitJmp(p, kSite2.returnVa);                           // jmp  0x4b5f3c

  FlushInstructionCache(GetCurrentProcess(), g_trampoline, (SIZE_T)(p - g_trampoline));

  bool ok1 = patchSite(kSite1.patchVa, (DWORD)(DWORD_PTR)t1);
  bool ok2 = patchSite(kSite2.patchVa, (DWORD)(DWORD_PTR)t2);
  if (!ok1 || !ok2)
  {
    Log("SmoothScroll: patch incomplete (site1=%d site2=%d)\n", ok1, ok2);
    return;
  }

  g_lastCounter.QuadPart = 0; // seed on the first frame
  g_enabled = true;
  Log("SmoothScroll: installed (refFps %.1f, clamp %.0fms, factor stays 1.0 until first frame)\n",
      g_refFps, g_clampSeconds * 1000.0);
}

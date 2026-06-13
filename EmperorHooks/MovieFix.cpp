#include <windows.h>
#include "MovieFix.hpp"
#include "Log.hpp"

// IAT entry (Game.exe ImageBase 0x400000, no ASLR):
//   0x5d03ac = binkw32!BinkDoFrame - CONFIRMED: fires once per decoded movie frame.
// We hook ONLY this one. The earlier RE guesses for BinkOpen / BinkCopyToBuffer / the "blit" at
// [0x5d02e0] were wrong (0x5d02e0 was a USER32 function), and hooking an unverified slot with a
// guessed calling convention hung the movie. So we keep just the confirmed decode signal, which is
// enough to know a movie is on screen; the 4:3 blit rewrite will gate on it from the DirectDraw side.
static void** const kBinkDoFrameIat = reinterpret_cast<void**>(0x005d03ac);

typedef int (__stdcall* BinkDoFrame_t)(void* hbink);
static BinkDoFrame_t  s_realBinkDoFrame = nullptr;
static volatile LONG  s_frames = 0;
static volatile DWORD s_lastFrameTick = 0;
static bool           s_cutscene43 = false;

static void logAddrModule(const char* label, void* addr)
{
  HMODULE m = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCSTR>(addr), &m);
  char name[MAX_PATH] = "?";
  if (m)
    GetModuleFileNameA(m, name, MAX_PATH);
  Log("MovieFix: %s -> %p  (module %s, base %p, off +%X)\n", label, addr, name, reinterpret_cast<void*>(m),
      m ? static_cast<unsigned>(reinterpret_cast<char*>(addr) - reinterpret_cast<char*>(m)) : 0u);
}

static int __stdcall MyBinkDoFrame(void* hbink)
{
  s_lastFrameTick = GetTickCount();
  LONG n = InterlockedIncrement(&s_frames);
  if (n <= 3)
    Log("MovieFix: BinkDoFrame #%ld hbink=%p\n", n, hbink);
  return s_realBinkDoFrame(hbink);
}

bool movieIsPlaying()
{
  // True for a short window after the last decoded frame (gates the future 4:3 blit rewrite).
  return (GetTickCount() - s_lastFrameTick) < 250;
}

void initMovieFix(bool cutscene43)
{
  s_cutscene43 = cutscene43;
  Log("MovieFix: init (cutscene43=%d)\n", static_cast<int>(cutscene43));

  if (!IsBadReadPtr(kBinkDoFrameIat, sizeof(void*)) && *kBinkDoFrameIat)
  {
    s_realBinkDoFrame = reinterpret_cast<BinkDoFrame_t>(*kBinkDoFrameIat);
    logAddrModule("[0x5d03ac] BinkDoFrame", reinterpret_cast<void*>(s_realBinkDoFrame));

    DWORD oldProt = 0;
    if (VirtualProtect(kBinkDoFrameIat, sizeof(void*), PAGE_READWRITE, &oldProt))
    {
      *kBinkDoFrameIat = reinterpret_cast<void*>(&MyBinkDoFrame);
      VirtualProtect(kBinkDoFrameIat, sizeof(void*), oldProt, &oldProt);
      Log("MovieFix: BinkDoFrame hooked (movie-active signal ready)\n");
    }
    else
      Log("MovieFix: VirtualProtect on BinkDoFrame IAT failed (err %lu)\n", GetLastError());
  }
  else
    Log("MovieFix: BinkDoFrame IAT not readable / null\n");
}

#include <windows.h>
#include "MovieFix.hpp"
#include "Log.hpp"

// Reverse-engineered addresses (Game.exe ImageBase 0x400000, no ASLR):
//   0x5d03ac = IAT entry for binkw32!BinkDoFrame, called once per decoded movie frame.
//   0x5d02e0 = function pointer the game CALLs (call dword [0x5d02e0]) to put the movie surface on
//              screen at the chosen resolution. The call sites at 0x4d465d / 0x4d46e5 push the screen
//              width/height (globals 0x602458 / 0x60245c) as the destination, which is what stretches
//              the 4:3 movie across the widescreen. This is the seam we will rewrite to a 4:3 box.
static void** const kBinkDoFrameIat = reinterpret_cast<void**>(0x005d03ac);
static void** const kBlitFnPtr      = reinterpret_cast<void**>(0x005d02e0);

// BinkDoFrame(HBINK) -> s32, RADLINK == __stdcall.
typedef int(__stdcall* BinkDoFrame_t)(void* hbink);
static BinkDoFrame_t s_realBinkDoFrame = nullptr;
static volatile LONG  s_binkFrames = 0;
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
  LONG n = InterlockedIncrement(&s_binkFrames);
  if (n <= 5)
    Log("MovieFix: BinkDoFrame #%ld hbink=%p\n", n, hbink);
  return s_realBinkDoFrame(hbink);
}

void initMovieFix(bool cutscene43)
{
  s_cutscene43 = cutscene43;
  Log("MovieFix: init (cutscene43=%d). Diagnostic build, play a cutscene and check these lines.\n",
      static_cast<int>(cutscene43));

  // What does the game CALL to blit the movie to screen? (resolve module + offset)
  if (!IsBadReadPtr(kBlitFnPtr, sizeof(void*)) && *kBlitFnPtr)
    logAddrModule("[0x5d02e0] blit fn", *kBlitFnPtr);
  else
    Log("MovieFix: [0x5d02e0] not readable / null yet\n");

  // Confirm the Bink decode path and install a tiny counter so we can tell a movie is playing.
  if (!IsBadReadPtr(kBinkDoFrameIat, sizeof(void*)) && *kBinkDoFrameIat)
  {
    s_realBinkDoFrame = reinterpret_cast<BinkDoFrame_t>(*kBinkDoFrameIat);
    logAddrModule("[0x5d03ac] BinkDoFrame", reinterpret_cast<void*>(s_realBinkDoFrame));

    DWORD oldProt = 0;
    if (VirtualProtect(kBinkDoFrameIat, sizeof(void*), PAGE_READWRITE, &oldProt))
    {
      *kBinkDoFrameIat = reinterpret_cast<void*>(&MyBinkDoFrame);
      VirtualProtect(kBinkDoFrameIat, sizeof(void*), oldProt, &oldProt);
      Log("MovieFix: BinkDoFrame IAT hooked, waiting for a cutscene\n");
    }
    else
      Log("MovieFix: VirtualProtect on BinkDoFrame IAT failed (err %lu)\n", GetLastError());
  }
  else
    Log("MovieFix: [0x5d03ac] BinkDoFrame IAT not readable / null\n");
}

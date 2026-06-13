#include <windows.h>
#include "MovieFix.hpp"
#include "Log.hpp"

// Reverse-engineered IAT entries for binkw32 (Game.exe ImageBase 0x400000, no ASLR):
//   0x5d03a0 = BinkOpen(name, flags)            - a movie file is opened
//   0x5d03ac = BinkDoFrame(hbink)               - decode the current frame (turned out NOT to fire for
//                                                 these movies, so we also watch the two below)
//   0x5d0394 = BinkCopyToBuffer(hbink, dst, pitch, h, x, y, flags) - put the frame on a surface
// Diagnostic build: log what the movie path actually uses and the on-screen geometry, so the 4:3
// rewrite can target the right seam. No drawing change.
static void** const kBinkOpenIat     = reinterpret_cast<void**>(0x005d03a0);
static void** const kBinkDoFrameIat  = reinterpret_cast<void**>(0x005d03ac);
static void** const kBinkCopyIat     = reinterpret_cast<void**>(0x005d0394);

typedef void* (__stdcall* BinkOpen_t)(const char* name, unsigned flags);
typedef int   (__stdcall* BinkDoFrame_t)(void* hbink);
typedef int   (__stdcall* BinkCopy_t)(void* hbink, void* dest, int destpitch, unsigned destheight,
                                      unsigned destx, unsigned desty, unsigned flags);

static BinkOpen_t    s_realBinkOpen = nullptr;
static BinkDoFrame_t s_realBinkDoFrame = nullptr;
static BinkCopy_t    s_realBinkCopy = nullptr;
static volatile LONG s_frames = 0;
static volatile LONG s_copies = 0;
static bool          s_cutscene43 = false;

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

static void* __stdcall MyBinkOpen(const char* name, unsigned flags)
{
  Log("MovieFix: BinkOpen name=\"%s\" flags=0x%X\n", name ? name : "(null)", flags);
  s_frames = 0; s_copies = 0;
  return s_realBinkOpen(name, flags);
}

static int __stdcall MyBinkDoFrame(void* hbink)
{
  LONG n = InterlockedIncrement(&s_frames);
  if (n <= 3)
    Log("MovieFix: BinkDoFrame #%ld hbink=%p\n", n, hbink);
  return s_realBinkDoFrame(hbink);
}

static int __stdcall MyBinkCopy(void* hbink, void* dest, int destpitch, unsigned destheight,
                                unsigned destx, unsigned desty, unsigned flags)
{
  LONG n = InterlockedIncrement(&s_copies);
  if (n <= 3)
    Log("MovieFix: BinkCopyToBuffer #%ld dest=%p pitch=%d destH=%u destX=%u destY=%u flags=0x%X\n",
        n, dest, destpitch, destheight, destx, desty, flags);
  return s_realBinkCopy(hbink, dest, destpitch, destheight, destx, desty, flags);
}

static bool hookIat(void** iat, void* hook, void** realOut, const char* label)
{
  if (IsBadReadPtr(iat, sizeof(void*)) || !*iat)
  {
    Log("MovieFix: %s IAT not readable / null\n", label);
    return false;
  }
  *realOut = *iat;
  logAddrModule(label, *iat);
  DWORD oldProt = 0;
  if (!VirtualProtect(iat, sizeof(void*), PAGE_READWRITE, &oldProt))
  {
    Log("MovieFix: VirtualProtect on %s failed (err %lu)\n", label, GetLastError());
    return false;
  }
  *iat = hook;
  VirtualProtect(iat, sizeof(void*), oldProt, &oldProt);
  return true;
}

void initMovieFix(bool cutscene43)
{
  s_cutscene43 = cutscene43;
  Log("MovieFix: init (cutscene43=%d). Diagnostic build, play a cutscene and check these lines.\n",
      static_cast<int>(cutscene43));

  hookIat(kBinkOpenIat,    reinterpret_cast<void*>(&MyBinkOpen),    reinterpret_cast<void**>(&s_realBinkOpen),    "[0x5d03a0] BinkOpen");
  hookIat(kBinkDoFrameIat, reinterpret_cast<void*>(&MyBinkDoFrame), reinterpret_cast<void**>(&s_realBinkDoFrame), "[0x5d03ac] BinkDoFrame");
  hookIat(kBinkCopyIat,    reinterpret_cast<void*>(&MyBinkCopy),    reinterpret_cast<void**>(&s_realBinkCopy),    "[0x5d0394] BinkCopyToBuffer");
  Log("MovieFix: Bink IAT hooks installed, play a movie now\n");
}

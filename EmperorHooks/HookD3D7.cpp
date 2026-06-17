#define INITGUID
#define CINTERFACE
#include <windows.h>
#include "dx7sdk_include/ddraw.h"
#include "dx7sdk_include/d3d.h"
#include <detours.h>
#include <timeapi.h>
#include <profileapi.h>
#include <cstdint>
#include "Error.hpp"
#include "Log.hpp"
#include "PickingTracer.hpp"
#include "MovieFix.hpp"


LARGE_INTEGER lastFrameTime = {};
LARGE_INTEGER qpcFreq = {};
DWORD targetFps = 60;

// Set by HookD3D7() from the launcher settings. Widescreen itself is done in PickingTracer (camera
// dist-scale); here we only need the flag so EndScene can hand the render thread to that hook.
static bool gWidescreen = false;
static bool gPillarbox = false;          // launcher option: letterbox the 4:3 screens instead of stretching
static bool gCutscene43 = false;         // launcher option: pillarbox the Bink cutscenes to 4:3
static bool gUpscale = false;            // launcher option: render at gScreenWidth/Height, stretch to the full desktop on present
static int gScreenWidth = 0;             // the render resolution (what the game thinks it is)
static int gScreenHeight = 0;
static bool gClearedThisFrame = false;   // black-fill the backbuffer once per frame before narrowing

// In upscale mode the game renders into this offscreen surface (the render target passed to CreateDevice)
// at the render resolution, then blits it onto the primary to present. We catch that one blit and stretch
// it to fill the whole (e.g. 4K) desktop, so the low-res frame fills the screen and the fonts stay large.
static IDirectDrawSurface7* gRenderTarget = nullptr;


// Limit FPS to 60, some effects in the game just don't work at high FPS.
void* OriginalPointer_IDirect3DDevice7_EndScene = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3DDevice7_EndScene)(IDirect3DDevice7* This) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3DDevice7_EndScene(IDirect3DDevice7* This)
{
  if (gWidescreen)
    tracerNoteThread();

  LARGE_INTEGER nextFrameTime = {};
  nextFrameTime.QuadPart = lastFrameTime.QuadPart + qpcFreq.QuadPart / targetFps;

  LARGE_INTEGER now = {};
  QueryPerformanceCounter(&now);

  int64_t remainingTicks = int64_t(nextFrameTime.QuadPart) - int64_t(now.QuadPart);
  if (remainingTicks > 0)
  {
    DWORD remainingMs = DWORD(remainingTicks * 1000ULL / qpcFreq.QuadPart);
    if (remainingMs)
      Sleep(remainingMs);
  }

  HRESULT result = Real_IDirect3DDevice7_EndScene(This);

  if (gWidescreen)
    tracerServiceScreenReset(); // end-of-frame: rebuild conquest/briefing 2D UI layout if flagged

  QueryPerformanceCounter(&lastFrameTime);

  gClearedThisFrame = false; // next frame may need a fresh black fill for the side bars

  return result;
}


// Letterbox the 4:3 front-end screens (campaign star-map, briefing, videos, ...): when PickingTracer
// flags the frame, force the game's full-screen viewport into a centred 4:3 box so the content keeps its
// aspect and the sides stay black. The battlefield and main menu are never flagged (they stay widescreen).
void* OriginalPointer_IDirect3DDevice7_SetViewport = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3DDevice7_SetViewport)(IDirect3DDevice7* This, LPD3DVIEWPORT7 lpViewport) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3DDevice7_SetViewport(IDirect3DDevice7* This, LPD3DVIEWPORT7 lpViewport)
{
  if (gWidescreen && gPillarbox && lpViewport && gScreenHeight > 0
      && lpViewport->dwWidth == (DWORD)gScreenWidth && tracerPillarboxWanted())
  {
    DWORD w43 = (DWORD)(gScreenHeight * 4 / 3);
    if (w43 < (DWORD)gScreenWidth)
    {
      // Paint the whole backbuffer black once this frame so the area outside the 4:3 box reads as bars
      // instead of stale/garbage pixels (the game only renders inside the narrowed viewport).
      if (!gClearedThisFrame)
      {
        Real_IDirect3DDevice7_SetViewport(This, lpViewport); // full-screen first
        This->lpVtbl->Clear(This, 0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
        gClearedThisFrame = true;
      }
      D3DVIEWPORT7 v = *lpViewport;
      v.dwX = ((DWORD)gScreenWidth - w43) / 2;
      v.dwY = 0;
      v.dwWidth = w43;
      v.dwHeight = (DWORD)gScreenHeight;
      return Real_IDirect3DDevice7_SetViewport(This, &v);
    }
  }
  return Real_IDirect3DDevice7_SetViewport(This, lpViewport);
}


// On the 4:3 screens the viewport is narrowed to a centred 4:3 box. The game's projection was built for
// the full 16:9 framebuffer, so without this it gets squashed horizontally into the box (a circle looks
// like a tall ellipse). Scale the projection's horizontal term by screenW/box4:3W so the 3D fills the box
// at the right aspect. PickingTracer puts the picking on the same 4:3 basis so clicks stay aligned.
void* OriginalPointer_IDirect3DDevice7_SetTransform = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3DDevice7_SetTransform)(IDirect3DDevice7* This, D3DTRANSFORMSTATETYPE dtstTransformStateType, LPD3DMATRIX lpD3DMatrix) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3DDevice7_SetTransform(IDirect3DDevice7* This, D3DTRANSFORMSTATETYPE dtstTransformStateType, LPD3DMATRIX lpD3DMatrix)
{
  if (gWidescreen && gPillarbox && lpD3DMatrix && dtstTransformStateType == D3DTRANSFORMSTATE_PROJECTION
      && gScreenHeight > 0 && tracerPillarboxWanted())
  {
    DWORD w43 = (DWORD)(gScreenHeight * 4 / 3);
    if (w43 > 0 && w43 < (DWORD)gScreenWidth)
    {
      D3DMATRIX m = *lpD3DMatrix;
      m._11 *= (float)gScreenWidth / (float)w43; // un-squash horizontal to fill the centred 4:3 box
      return Real_IDirect3DDevice7_SetTransform(This, dtstTransformStateType, &m);
    }
  }
  return Real_IDirect3DDevice7_SetTransform(This, dtstTransformStateType, lpD3DMatrix);
}


// Cutscene 4:3: the Bink movie ends up on screen via a DirectDraw Blt that stretches its surface across
// the whole framebuffer. All IDirectDrawSurface7 instances share one vtable, so hooking Blt on any
// surface catches that blit. DIAGNOSTIC for now: while a movie is on screen (movieIsPlaying), log the
// destination rect so we can confirm this is the seam and read the geometry, then rewrite it to a
// centred 4:3 box. Outside a movie the hook is a single GetTickCount compare, so it is effectively free.
void* OriginalPointer_DDS_Blt = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_DDS_Blt)(IDirectDrawSurface7* This, LPRECT lpDestRect, IDirectDrawSurface7* lpDDSrc, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpFx) = nullptr;
HRESULT STDMETHODCALLTYPE My_DDS_Blt(IDirectDrawSurface7* This, LPRECT lpDestRect, IDirectDrawSurface7* lpDDSrc, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpFx)
{
  // Upscale present: the source is the render target (captured at CreateDevice), so this blit is the
  // backbuffer -> primary present. The window is sized to the whole desktop, so rewrite the dest rect to
  // the full screen, stretching the low-res frame up to fill it. A plain pointer compare, no per-blit
  // GetSurfaceDesc, so the cost on every other blit is one branch.
  if (gUpscale && lpDDSrc && lpDDSrc == gRenderTarget)
  {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (screenW > 0 && screenH > 0)
    {
      static bool logged = false;
      if (!logged)
      {
        logged = true;
        RECT* d = lpDestRect;
        Log("UpscaleFix: present blit (render %dx%d) -> full screen %dx%d, game dest {%ld,%ld,%ld,%ld}\n",
            gScreenWidth, gScreenHeight, screenW, screenH,
            d ? d->left : 0, d ? d->top : 0, d ? d->right : 0, d ? d->bottom : 0);
      }
      RECT full = { 0, 0, screenW, screenH };
      return Real_DDS_Blt(This, &full, lpDDSrc, lpSrcRect, dwFlags, lpFx);
    }
  }

  // The Bink cutscene reaches the screen as: 640x480 movie surface --(NULL dest rect)--> the full-screen
  // surface, so DirectDraw stretches the 4:3 movie across the whole 16:9 target. When the option is on and
  // a movie is playing, redirect that one blit into a centred 4:3 box and black-fill the side bars. The
  // later full-screen 1:1 copy (src == dest size) is left alone, so the bars carry through to the screen.
  if (gWidescreen && gCutscene43 && lpDDSrc && lpDestRect == nullptr && gScreenHeight > 0 && movieIsPlaying())
  {
    DDSURFACEDESC2 td = {}; td.dwSize = sizeof(td);
    DDSURFACEDESC2 sd = {}; sd.dwSize = sizeof(sd);
    This->lpVtbl->GetSurfaceDesc(This, &td);
    lpDDSrc->lpVtbl->GetSurfaceDesc(lpDDSrc, &sd);
    if ((int)td.dwWidth == gScreenWidth && (int)td.dwHeight == gScreenHeight && sd.dwWidth < td.dwWidth)
    {
      int w43 = gScreenHeight * 4 / 3;
      if (w43 > 0 && w43 < gScreenWidth)
      {
        int xoff = (gScreenWidth - w43) / 2;
        static bool logged = false;
        if (!logged)
        {
          logged = true;
          Log("MovieFix: pillarbox movie %lux%lu -> 4:3 box {%d,0,%d,%d} on %dx%d\n",
              sd.dwWidth, sd.dwHeight, xoff, xoff + w43, gScreenHeight, gScreenWidth, gScreenHeight);
        }
        DDBLTFX fx = {}; fx.dwSize = sizeof(fx); fx.dwFillColor = 0;
        Real_DDS_Blt(This, nullptr, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
        RECT box = { xoff, 0, xoff + w43, gScreenHeight };
        return Real_DDS_Blt(This, &box, lpDDSrc, lpSrcRect, dwFlags, lpFx);
      }
    }
  }
  return Real_DDS_Blt(This, lpDestRect, lpDDSrc, lpSrcRect, dwFlags, lpFx);
}


void* OriginalPointer_IDirect3D7_CreateDevice = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3D7_CreateDevice)(IDirect3D7* This, REFCLSID rclsid, LPDIRECTDRAWSURFACE7 lpDDS, LPDIRECT3DDEVICE7* lplpD3DDevice) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3D7_CreateDevice(IDirect3D7* This, REFCLSID rclsid, LPDIRECTDRAWSURFACE7 lpDDS, LPDIRECT3DDEVICE7* lplpD3DDevice)
{
  IDirect3DDevice7* device = *lplpD3DDevice;
  HRESULT result = Real_IDirect3D7_CreateDevice(This, rclsid, lpDDS, &device);
  *lplpD3DDevice = device;

  if (OriginalPointer_IDirect3DDevice7_EndScene)
    release_assert(device->lpVtbl->EndScene == OriginalPointer_IDirect3DDevice7_EndScene);

  if (device && Real_IDirect3DDevice7_EndScene == nullptr)
  {
    timeBeginPeriod(1);
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&lastFrameTime);

    Real_IDirect3DDevice7_EndScene = device->lpVtbl->EndScene;
    OriginalPointer_IDirect3DDevice7_EndScene = device->lpVtbl->EndScene;
    Real_IDirect3DDevice7_SetViewport = device->lpVtbl->SetViewport;
    OriginalPointer_IDirect3DDevice7_SetViewport = device->lpVtbl->SetViewport;
    Real_IDirect3DDevice7_SetTransform = device->lpVtbl->SetTransform;
    OriginalPointer_IDirect3DDevice7_SetTransform = device->lpVtbl->SetTransform;

    // Hook Blt on the surface vtable (shared by every surface) to reach the movie blit.
    if (lpDDS && Real_DDS_Blt == nullptr)
    {
      Real_DDS_Blt = lpDDS->lpVtbl->Blt;
      OriginalPointer_DDS_Blt = lpDDS->lpVtbl->Blt;
    }

    // Remember the render target so the upscale present blit can be identified by its source surface.
    gRenderTarget = lpDDS;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)Real_IDirect3DDevice7_EndScene, My_IDirect3DDevice7_EndScene);
    if (Real_DDS_Blt)
      DetourAttach(&(PVOID&)Real_DDS_Blt, My_DDS_Blt);
    // No viewport-narrow / pillarbox: full widescreen everywhere. The 4:3-pinned front-end clicks are
    // fixed by Moro's 2D-UI patch (PickingTracer), the 3D by the dist-scale. My_SetViewport/SetTransform
    // stay defined but unattached (tracerPillarboxWanted() is always false now).
    DetourTransactionCommit();
  }

  return result;
}


HRESULT(WINAPI* RealDirectDrawCreateEx)(GUID FAR* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown FAR* pUnkOuter) = nullptr;
HRESULT WINAPI MyDirectDrawCreateEx(GUID FAR* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown FAR* pUnkOuter)
{
  IDirectDraw7* directDraw = *((IDirectDraw7**)lplpDD);
  HRESULT result = RealDirectDrawCreateEx(lpGuid, (void**)&directDraw, iid, pUnkOuter);
  *((IDirectDraw7**)lplpDD) = directDraw;

  if (directDraw)
  {
    IDirect3D7* direct3D = nullptr;
    directDraw->lpVtbl->QueryInterface(directDraw, IID_IDirect3D7, (void**)&direct3D);

    if (OriginalPointer_IDirect3D7_CreateDevice)
      release_assert(direct3D->lpVtbl->CreateDevice == OriginalPointer_IDirect3D7_CreateDevice);

    if (direct3D && Real_IDirect3D7_CreateDevice == nullptr)
    {
      Real_IDirect3D7_CreateDevice = direct3D->lpVtbl->CreateDevice;
      OriginalPointer_IDirect3D7_CreateDevice = direct3D->lpVtbl->CreateDevice;

      DetourTransactionBegin();
      DetourUpdateThread(GetCurrentThread());
      DetourAttach(&(PVOID&)Real_IDirect3D7_CreateDevice, My_IDirect3D7_CreateDevice);
      DetourTransactionCommit();

      direct3D->lpVtbl->Release(direct3D);
    }
  }

  return result;
}


void HookD3D7(bool widescreen, bool pillarbox, bool cutscene43, bool upscaleToDesktop, int screenWidth, int screenHeight)
{
  gWidescreen = widescreen;
  gPillarbox = pillarbox;
  gCutscene43 = cutscene43;
  gUpscale = upscaleToDesktop;
  gScreenWidth = screenWidth;
  gScreenHeight = screenHeight;

  // For some reason, this doesn't work if I use normal dynamic linking to fetch the original function pointer
  HMODULE ddraw = LoadLibraryA("DDRAW.DLL");
  RealDirectDrawCreateEx = (HRESULT(WINAPI*)(GUID FAR*, LPVOID*, REFIID, IUnknown FAR*)) GetProcAddress(ddraw, "DirectDrawCreateEx");

  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach(&(PVOID&)RealDirectDrawCreateEx, MyDirectDrawCreateEx);
  DetourTransactionCommit();
}

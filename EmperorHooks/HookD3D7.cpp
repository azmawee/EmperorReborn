#define INITGUID
#define CINTERFACE
#include <windows.h>
#include "dx7sdk_include/ddraw.h"
#include "dx7sdk_include/d3d.h"
#include <cstdio>
#include <detours.h>
#include <timeapi.h>
#include <profileapi.h>
#include <cstdint>
#include <mutex>
#include <intrin.h>
#include "Error.hpp"
#include "Log.hpp"
#include "PickingTracer.hpp"


LARGE_INTEGER lastFrameTime = {};
LARGE_INTEGER qpcFreq = {};
DWORD targetFps = 60;

// Widescreen projection correction. Set by HookD3D7() from the launcher settings.
static bool gWidescreen = false;
static int gScreenWidth = 0;
static int gScreenHeight = 0;

// Hor+ screen-offset scale factors (vs the game's native projection), computed each frame in the
// projection hook and reused by the picking fix so mouse hit-testing matches what is rendered.
static volatile float gPickXFactor = 1.0f;
static volatile float gPickYFactor = 1.0f;


// Correct the 3D projection so the battlefield renders at the real screen aspect instead of
// stretching the original 4:3 image. The game builds its perspective matrix assuming a 4:3
// backbuffer; when we render that into a 16:9 backbuffer everything looks horizontally fat.
// We shrink the x-axis scale (_11) by (gameAspect / screenAspect). This both removes the
// distortion AND reveals more of the world horizontally (Hor+ widescreen), which is what we
// want for single-player. Only perspective matrices are touched (_44 == 0 && _34 != 0); the
// game's 2D / orthographic passes (HUD, menus) have _44 == 1 and are left untouched.
void* OriginalPointer_IDirect3DDevice7_SetTransform = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3DDevice7_SetTransform)(IDirect3DDevice7* This, D3DTRANSFORMSTATETYPE dtstType, LPD3DMATRIX lpD3DMatrix) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3DDevice7_SetTransform(IDirect3DDevice7* This, D3DTRANSFORMSTATETYPE dtstType, LPD3DMATRIX lpD3DMatrix)
{
  void* caller = _ReturnAddress();

  if (dtstType == D3DTRANSFORMSTATE_PROJECTION && lpD3DMatrix)
  {
    // Log the Game.exe call site that sets the projection (once), so we can locate the projection
    // computation in the binary and patch it directly. Module base lets us turn this into a static
    // address (Game.exe ImageBase is 0x400000).
    static bool loggedCaller = false;
    if (!loggedCaller)
    {
      Log("PROJECTION SetTransform caller=%p gameExeBase=%p\n", caller, (void*)GetModuleHandleA(nullptr));
      loggedCaller = true;
    }

    const D3DMATRIX& m = *lpD3DMatrix;
    bool isPerspective = (m._44 == 0.0f && m._34 != 0.0f);

    // Spike logging only: dump the first handful of projection sets so we can confirm the
    // rendering model without flooding emperor.txt (this is called every frame). The correction
    // below always runs regardless of logging.
    static int projLogCount = 0;
    if (projLogCount < 30)
    {
      Log("SetTransform PROJECTION (%s) _11=%f _22=%f _34=%f _44=%f\n",
        isPerspective ? "perspective" : "ortho/2D", m._11, m._22, m._34, m._44);
      projLogCount++;
    }

    // NOTE: widescreen is now done by scaling the camera focal "distance" (see PickingTracer.cpp's
    // unified dist-scale), which makes BOTH the GPU render and mouse picking Hor+ consistently. The
    // old approach of editing the projection matrix here only affected the GPU side and desynced the
    // clicks, so it is disabled. (void) the factors to keep them referenced.
    (void)isPerspective;
    (void)gPickXFactor;
    (void)gPickYFactor;
  }

  return Real_IDirect3DDevice7_SetTransform(This, dtstType, lpD3DMatrix);
}


// Experiment: if the game's mouse picking reads the projection back via GetTransform, force it to
// see the same widescreen matrix we render with, so click targets line up with the visuals.
void* OriginalPointer_IDirect3DDevice7_GetTransform = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3DDevice7_GetTransform)(IDirect3DDevice7* This, D3DTRANSFORMSTATETYPE dtstType, LPD3DMATRIX lpD3DMatrix) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3DDevice7_GetTransform(IDirect3DDevice7* This, D3DTRANSFORMSTATETYPE dtstType, LPD3DMATRIX lpD3DMatrix)
{
  HRESULT result = Real_IDirect3DDevice7_GetTransform(This, dtstType, lpD3DMatrix);

  if (dtstType == D3DTRANSFORMSTATE_PROJECTION && lpD3DMatrix && gWidescreen
    && gScreenWidth > 0 && gScreenHeight > 0 && lpD3DMatrix->_11 != 0.0f)
  {
    static bool loggedGet = false;
    if (!loggedGet)
    {
      Log("GetTransform PROJECTION called by caller=%p (picking may use this)\n", _ReturnAddress());
      loggedGet = true;
    }

    float screenAspect = float(gScreenWidth) / float(gScreenHeight);
    float verticalScale = lpD3DMatrix->_11 * (4.0f / 3.0f);
    lpD3DMatrix->_22 = verticalScale;
    lpD3DMatrix->_11 = verticalScale / screenAspect;
  }

  return result;
}


// Spike-only logging hook: confirm how/when the game sets its viewport at 16:9.
void* OriginalPointer_IDirect3DDevice7_SetViewport = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3DDevice7_SetViewport)(IDirect3DDevice7* This, LPD3DVIEWPORT7 lpViewport) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3DDevice7_SetViewport(IDirect3DDevice7* This, LPD3DVIEWPORT7 lpViewport)
{
  static int vpLogCount = 0;
  if (lpViewport && vpLogCount < 30)
  {
    Log("SetViewport x=%lu y=%lu w=%lu h=%lu\n", lpViewport->dwX, lpViewport->dwY, lpViewport->dwWidth, lpViewport->dwHeight);
    vpLogCount++;
  }

  return Real_IDirect3DDevice7_SetViewport(This, lpViewport);
}


// --- Spike: diagnose how the 2D UI (menus / HUD) is drawn -------------------------------------
// UI is almost certainly drawn with pre-transformed D3DFVF_XYZRHW vertices, whose x/y are screen
// pixels. Logging the bounding box of those draws tells us the coordinate space the UI assumes
// (e.g. does it span 0..1920 / scale to width / overflow the height) so we can plan the fix.
static DWORD fvfVertexSize(DWORD fvf)
{
  DWORD size = 0;
  switch (fvf & D3DFVF_POSITION_MASK)
  {
  case D3DFVF_XYZ:    size += 3 * sizeof(float); break;
  case D3DFVF_XYZRHW: size += 4 * sizeof(float); break;
  case D3DFVF_XYZB1:  size += 4 * sizeof(float); break;
  case D3DFVF_XYZB2:  size += 5 * sizeof(float); break;
  case D3DFVF_XYZB3:  size += 6 * sizeof(float); break;
  case D3DFVF_XYZB4:  size += 7 * sizeof(float); break;
  case D3DFVF_XYZB5:  size += 8 * sizeof(float); break;
  default: break;
  }
  if (fvf & D3DFVF_NORMAL)   size += 3 * sizeof(float);
  if (fvf & D3DFVF_DIFFUSE)  size += sizeof(DWORD);
  if (fvf & D3DFVF_SPECULAR) size += sizeof(DWORD);
  DWORD texCount = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
  size += texCount * 2 * sizeof(float); // assume the common 2 floats per texture coord set
  return size;
}

static void logXyzrhwBBox(const char* who, DWORD fvf, void* verts, DWORD count)
{
  static int drawLogCount = 0;
  if (drawLogCount >= 60)
    return;
  if ((fvf & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW || !verts || !count)
    return;

  DWORD stride = fvfVertexSize(fvf);
  if (!stride)
    return;

  float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
  const char* p = (const char*)verts;
  for (DWORD i = 0; i < count; i++)
  {
    const float* v = (const float*)(p + i * stride);
    if (v[0] < minX) minX = v[0];
    if (v[0] > maxX) maxX = v[0];
    if (v[1] < minY) minY = v[1];
    if (v[1] > maxY) maxY = v[1];
  }

  Log("%s XYZRHW fvf=0x%lx count=%lu x=[%.1f..%.1f] y=[%.1f..%.1f]\n", who, fvf, count, minX, maxX, minY, maxY);
  drawLogCount++;
}

void* OriginalPointer_IDirect3DDevice7_DrawPrimitive = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3DDevice7_DrawPrimitive)(IDirect3DDevice7* This, D3DPRIMITIVETYPE, DWORD, LPVOID, DWORD, DWORD) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3DDevice7_DrawPrimitive(IDirect3DDevice7* This, D3DPRIMITIVETYPE primType, DWORD fvf, LPVOID verts, DWORD count, DWORD flags)
{
  logXyzrhwBBox("DrawPrimitive", fvf, verts, count);
  return Real_IDirect3DDevice7_DrawPrimitive(This, primType, fvf, verts, count, flags);
}

void* OriginalPointer_IDirect3DDevice7_DrawIndexedPrimitive = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3DDevice7_DrawIndexedPrimitive)(IDirect3DDevice7* This, D3DPRIMITIVETYPE, DWORD, LPVOID, DWORD, LPWORD, DWORD, DWORD) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3DDevice7_DrawIndexedPrimitive(IDirect3DDevice7* This, D3DPRIMITIVETYPE primType, DWORD fvf, LPVOID verts, DWORD count, LPWORD indices, DWORD indexCount, DWORD flags)
{
  logXyzrhwBBox("DrawIndexedPrimitive", fvf, verts, count);
  return Real_IDirect3DDevice7_DrawIndexedPrimitive(This, primType, fvf, verts, count, indices, indexCount, flags);
}

// Limit FPS to 60, some effects in the game just don't work at high FPS
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

  QueryPerformanceCounter(&lastFrameTime);

  return result;
}


void* OriginalPointer_IDirect3D7_CreateDevice = nullptr;
HRESULT(STDMETHODCALLTYPE* Real_IDirect3D7_CreateDevice)(IDirect3D7* This, REFCLSID rclsid, LPDIRECTDRAWSURFACE7 lpDDS, LPDIRECT3DDEVICE7* lplpD3DDevice) = nullptr;
HRESULT STDMETHODCALLTYPE My_IDirect3D7_CreateDevice(IDirect3D7* This, REFCLSID rclsid, LPDIRECTDRAWSURFACE7 lpDDS, LPDIRECT3DDEVICE7* lplpD3DDevice)
{
  Log("!!!!!!!!!!!!!!!!! My_IDirect3D7_CreateDevice !!!!!!!!!!!!!!!!!!!\n");


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

    Real_IDirect3DDevice7_SetTransform = device->lpVtbl->SetTransform;
    OriginalPointer_IDirect3DDevice7_SetTransform = device->lpVtbl->SetTransform;

    Real_IDirect3DDevice7_SetViewport = device->lpVtbl->SetViewport;
    OriginalPointer_IDirect3DDevice7_SetViewport = device->lpVtbl->SetViewport;

    Real_IDirect3DDevice7_GetTransform = device->lpVtbl->GetTransform;
    OriginalPointer_IDirect3DDevice7_GetTransform = device->lpVtbl->GetTransform;

    Real_IDirect3DDevice7_DrawPrimitive = device->lpVtbl->DrawPrimitive;
    OriginalPointer_IDirect3DDevice7_DrawPrimitive = device->lpVtbl->DrawPrimitive;

    Real_IDirect3DDevice7_DrawIndexedPrimitive = device->lpVtbl->DrawIndexedPrimitive;
    OriginalPointer_IDirect3DDevice7_DrawIndexedPrimitive = device->lpVtbl->DrawIndexedPrimitive;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)Real_IDirect3DDevice7_EndScene, My_IDirect3DDevice7_EndScene);
    DetourAttach(&(PVOID&)Real_IDirect3DDevice7_SetTransform, My_IDirect3DDevice7_SetTransform);
    DetourAttach(&(PVOID&)Real_IDirect3DDevice7_SetViewport, My_IDirect3DDevice7_SetViewport);
    DetourAttach(&(PVOID&)Real_IDirect3DDevice7_GetTransform, My_IDirect3DDevice7_GetTransform);
    DetourAttach(&(PVOID&)Real_IDirect3DDevice7_DrawPrimitive, My_IDirect3DDevice7_DrawPrimitive);
    DetourAttach(&(PVOID&)Real_IDirect3DDevice7_DrawIndexedPrimitive, My_IDirect3DDevice7_DrawIndexedPrimitive);
    DetourTransactionCommit();
  }


  return result;
}


HRESULT(WINAPI* RealDirectDrawCreateEx)(GUID FAR* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown FAR* pUnkOuter) = nullptr;
HRESULT WINAPI MyDirectDrawCreateEx(GUID FAR* lpGuid, LPVOID* lplpDD, REFIID iid, IUnknown FAR* pUnkOuter)
{
  Log("!!!!!!!!!!!!!!!!! MyDirectDrawCreateEx !!!!!!!!!!!!!!!!!!!\n");

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


// The game's CPU world-to-screen projection (sub_421300 in Game.exe): thiscall, transforms a 3D
// point to screen pixels and writes them to an output struct (out[0]=screen X, out[1]=screen Y as
// floats). It uses the game's NATIVE projection, so with our Hor+ render the click targets land in
// the wrong place. We scale its output toward the screen centre by the same per-axis factors our
// projection hook applied, which keeps mouse picking (unit selection, HUD, etc.) aligned with what
// is drawn. (Found via the runtime mouse/view-matrix tracer; see WIDESCREEN-NOTES.md.)
typedef int(__fastcall* PFN_worldToScreen)(void* thisPtr, void* edxDummy, void* pointIn, float* pOut, int a3, int a4);
static PFN_worldToScreen Real_worldToScreen = (PFN_worldToScreen)0x00421300;
static int __fastcall My_worldToScreen(void* thisPtr, void* edxDummy, void* pointIn, float* pOut, int a3, int a4)
{
  int result = Real_worldToScreen(thisPtr, edxDummy, pointIn, pOut, a3, a4);

  if (gWidescreen && pOut && gScreenWidth > 0 && gScreenHeight > 0)
  {
    float cx = gScreenWidth * 0.5f;
    float cy = gScreenHeight * 0.5f;

    static int dbg = 0;
    if (dbg < 10)
    {
      Log("WORLD2SCREEN before x=%.1f y=%.1f  xf=%.3f yf=%.3f\n", pOut[0], pOut[1], gPickXFactor, gPickYFactor);
      dbg++;
    }

    pOut[0] = cx + (pOut[0] - cx) * gPickXFactor;
    pOut[1] = cy + (pOut[1] - cy) * gPickYFactor;
  }

  return result;
}


void HookD3D7(bool widescreen, int screenWidth, int screenHeight)
{
  gWidescreen = widescreen;
  gScreenWidth = screenWidth;
  gScreenHeight = screenHeight;

  // NOTE: sub_421300 turned out to be the world-to-screen used for DRAWING overlays (it projects
  // off-screen 3D geometry), not the click hit-test. Scaling its output moved overlays without
  // fixing clicks, so the detour is left detached. The click hit-test is a separate path (likely
  // screen-to-world / unproject) still to be located. See WIDESCREEN-NOTES.md.
  (void)My_worldToScreen;

  // For some reason, this doesn't work if I use normal dynamic linking to fetch the original function pointer
  HMODULE ddraw = LoadLibraryA("DDRAW.DLL");
  RealDirectDrawCreateEx = (HRESULT(WINAPI*)(GUID FAR*, LPVOID*, REFIID, IUnknown FAR*)) GetProcAddress(ddraw, "DirectDrawCreateEx");

  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach(&(PVOID&)RealDirectDrawCreateEx, MyDirectDrawCreateEx);
  DetourTransactionCommit();
}
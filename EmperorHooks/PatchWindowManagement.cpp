#include <windows.h>
#include <windowsx.h>
#include "GameExeImports.hpp"
#include <stdio.h>
#include <detours.h>
#include "Log.hpp"
#include <thread>

static volatile bool focus = true;
static bool emperorLauncherDoFullscreen = 1;
static volatile HWND backgroundWindowHandle = nullptr;
static volatile DWORD backgroundWindowThreadId = 0;

// Upscale mode: render at gRenderWidth/gRenderHeight but keep the desktop native and size the game window
// to the whole screen. The render target is stretched to full screen on present (see HookD3D7). Because the
// window is bigger than the game thinks it is, incoming mouse coords must be scaled back down to the render
// resolution or every click lands in the wrong place.
static bool emperorLauncherUpscale = false;
static int gRenderWidth = 0;
static int gRenderHeight = 0;


PFN_wndProcDuneIII wndProcDuneIIIOrig = wndProcDuneIII;
int __stdcall wndProcDuneIIIPatched(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  if (emperorLauncherDoFullscreen)
  {
    // The original wndProc has code that sets up + tears down the renderer on focus gain / loss
    // This is not necessary on modern windows. Also, this was causing problems with nesting the
    // game window inside the black background window, as the game window itself didn't get those
    // messages, so the renderer was never enabled.
    // This function force runs the setup code once, when it receives its first event. Theoretically,
    // we could also block forwarding WM_ACTIVATEAPP messages. That seems to almost work, but there
    // are some weird issues with input after alt-tabbing in and out. So, we just force the event once.
    // We also forward in WM_ACTIVATEAPP events from the background / parent window

    static bool doOnce = false;
    if (!doOnce)
    {
      wndProcDuneIIIOrig(hWnd, WM_ACTIVATEAPP, TRUE, 0);
      doOnce = true;
    }
  }

  // Something in WOLAPI likes to hang on exit, so let's just *really* make sure we exit
  if (Msg == WM_DESTROY)
    TerminateProcess(GetCurrentProcess(), 0);

  // Upscale mode: the window covers the whole (e.g. 4K) screen but the game thinks the client area is the
  // render resolution. Scale the mouse position in the message down from screen space to render space so the
  // cursor and clicks line up with the stretched-up image. WM_MOUSEWHEEL carries screen coords / a wheel
  // delta, not a client position, so it is left alone.
  if (emperorLauncherUpscale && gRenderWidth > 0 && gRenderHeight > 0)
  {
    bool isClientMouseMsg =
      Msg == WM_MOUSEMOVE ||
      Msg == WM_LBUTTONDOWN || Msg == WM_LBUTTONUP || Msg == WM_LBUTTONDBLCLK ||
      Msg == WM_RBUTTONDOWN || Msg == WM_RBUTTONUP || Msg == WM_RBUTTONDBLCLK ||
      Msg == WM_MBUTTONDOWN || Msg == WM_MBUTTONUP || Msg == WM_MBUTTONDBLCLK;

    if (isClientMouseMsg)
    {
      int screenW = GetSystemMetrics(SM_CXSCREEN);
      int screenH = GetSystemMetrics(SM_CYSCREEN);
      if (screenW > 0 && screenH > 0)
      {
        int x = GET_X_LPARAM(lParam) * gRenderWidth / screenW;
        int y = GET_Y_LPARAM(lParam) * gRenderHeight / screenH;
        lParam = MAKELPARAM(x, y);
      }
    }
  }

  return wndProcDuneIIIOrig(hWnd, Msg, wParam, lParam);
}

PFN_setWindowStyleAndDrainMessages setWindowStyleAndDrainMessagesOrig = setWindowStyleAndDrainMessages;
BOOL __cdecl setWindowStyleAndDrainMessagesPatched(HWND hWnd, int width, int height, char windowedMode)
{
  if (emperorLauncherDoFullscreen)
  {
    SetWindowLongA(hWnd, GWL_STYLE, WS_POPUP);
    SetParent(hWnd, backgroundWindowHandle);

    if (emperorLauncherUpscale)
    {
      // Cover the whole screen so the stretched render fills it; the desktop is left at its native res.
      width = GetSystemMetrics(SM_CXSCREEN);
      height = GetSystemMetrics(SM_CYSCREEN);
    }

    int left = GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2;
    SetWindowPos(hWnd, nullptr, left, 0, width, height, SWP_SHOWWINDOW);

    BOOL result;
    MSG msg;
    for (result = PeekMessageA(&msg, 0, 0, 0, 0); result; result = PeekMessageA(&msg, 0, 0, 0, 0))
    {
      GetMessageA(&msg, 0, 0, 0);
      if (msg.message == WM_QUIT && (msg.hwnd == mainWindowHandle || (HWND)msg.wParam == mainWindowHandle))
        gDoQuit = 1;
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    SetForegroundWindow(hWnd);

    return result;
  }
  else
  {
    BOOL retval = setWindowStyleAndDrainMessagesOrig(hWnd, width, height, windowedMode);

    int left = GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2;
    int top = GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2;
    SetWindowPos(hWnd, nullptr, left, top, width, height, SWP_SHOWWINDOW);

    return retval;
  }
}

LRESULT __stdcall backgroundWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  if (Msg == WM_ACTIVATEAPP)
  {
    // Only react when the activation state actually changes. During a fullscreen device reset (e.g.
    // aborting a mission) the topmost/focus dance against an always-on-top taskbar (Start11,
    // ExplorerPatcher) can spam duplicate WM_ACTIVATEAPP 0 events and spin into a focus loop that
    // hangs the game. Ignoring duplicates breaks that loop while a real change (alt-tab, 0<->1) still
    // gets handled.
    static int lastActivate = -1;
    if ((int)wParam == lastActivate)
      return DefWindowProcA(hWnd, Msg, wParam, lParam);
    lastActivate = (int)wParam;

    // Keep the borderless fullscreen window above an always-on-top third-party taskbar while we have
    // focus, but drop topmost when we lose focus so the user can still alt-tab away cleanly.
    SetWindowPos(hWnd, wParam ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // Forward to the game window or input gets screwy (see comment in wndProcDuneIIIPatched()).
    if (mainWindowHandle)
    {
      SetFocus(mainWindowHandle);
      SendMessageA(mainWindowHandle, Msg, wParam, lParam);
    }
  }

  return DefWindowProcA(hWnd, Msg, wParam, lParam);
}


void createBackgroundWindow()
{
  backgroundWindowThreadId = GetCurrentThreadId();

  const char CLASS_NAME[] = "DuneIII background";

  WNDCLASSA wc = { };
  wc.lpfnWndProc = backgroundWndProc;
  wc.hInstance = GetModuleHandleA(nullptr);
  wc.lpszClassName = CLASS_NAME;
  RegisterClassA(&wc);

  HWND temp = CreateWindowExA(
    0,//WS_EX_TOOLWINDOW,                              // Optional window styles.
    CLASS_NAME,                     // Window class
    nullptr,    // Window text
    WS_CLIPCHILDREN | WS_POPUP,            // Window style

    // Size and position
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

    NULL,       // Parent window
    NULL,       // Menu
    wc.hInstance,  // Instance handle
    NULL        // Additional application data
  );

  ShowWindow(temp, SW_MAXIMIZE);

  // Cover the entire monitor and sit above the taskbar. Some third-party taskbars (Start11,
  // ExplorerPatcher, etc.) force themselves always-on-top and would otherwise draw over our
  // borderless fullscreen window. backgroundWndProc drops topmost again on focus loss.
  SetWindowPos(temp, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_SHOWWINDOW);

  HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
  SetClassLongPtrA(temp, GCLP_HBRBACKGROUND, (LONG_PTR)black);
  InvalidateRect(temp, nullptr, 1);

  backgroundWindowHandle = temp;
}

void backgroundWindowLoop()
{
  BOOL bRet;

  MSG msg;
  while ((bRet = GetMessage(&msg, backgroundWindowHandle, 0, 0)) != 0)
  {
    if (bRet == -1)
    {
      break;
    }
    else
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
}

void captureCursorLoop()
{
  while (true)
  {
    Sleep(1000 * 1);

    if (!mainWindowHandle)
      continue;

    if (mainWindowHandle != GetForegroundWindow())
      continue;

    RECT rect = {};
    if (!GetClientRect(mainWindowHandle, &rect))
      continue;

    POINT topLeft = { 0, 0 };
    if (!ClientToScreen(mainWindowHandle, &topLeft))
      continue;

    rect.left += topLeft.x;
    rect.right += topLeft.x;
    rect.top += topLeft.y;
    rect.bottom += topLeft.y;

    rect.left += 10;
    rect.top += 10;
    rect.right -= 10;
    rect.bottom -= 10;

    if (!IsDebuggerPresent())
      ClipCursor(&rect);
  }
}

void setFullscreenDisplayMode(int width, int height)
{
  // Switch the desktop to the game's resolution so the GPU / monitor scales it up to fill the
  // whole physical screen, instead of drawing a small box in the middle of a huge (e.g. 4K) desktop.
  // The game still runs in its borderless window - we're only changing the screen resolution, not
  // using DirectDraw exclusive fullscreen (which is what the windowed hack exists to avoid).
  // CDS_FULLSCREEN makes the change temporary: Windows automatically restores the original
  // resolution when this process exits, so we don't need to clean it up ourselves.
  if (width <= 0 || height <= 0)
    return;

  DEVMODEA dm = {};
  dm.dmSize = sizeof(dm);
  dm.dmPelsWidth = width;
  dm.dmPelsHeight = height;
  dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
  LONG result = ChangeDisplaySettingsA(&dm, CDS_FULLSCREEN);
  Log("ChangeDisplaySettings to %dx%d returned %ld\n", width, height, result);
}

void patchWindowManagement(bool doFullscreen, bool doCursorCapture, bool upscaleToDesktop, int width, int height)
{
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach(&(PVOID&)wndProcDuneIIIOrig, wndProcDuneIIIPatched);
  DetourAttach(&(PVOID&)setWindowStyleAndDrainMessagesOrig, setWindowStyleAndDrainMessagesPatched);
  DetourTransactionCommit();

  emperorLauncherDoFullscreen = doFullscreen;
  emperorLauncherUpscale = upscaleToDesktop;
  gRenderWidth = width;
  gRenderHeight = height;

  if (emperorLauncherDoFullscreen)
  {
    // Upscale mode keeps the desktop at its native resolution and stretches the rendered frame up on
    // present instead, so skip the display-mode switch. Otherwise switch the desktop to the game's
    // resolution and let the monitor scale it. Must happen before creating the background window so it
    // maximizes to the right resolution.
    if (!emperorLauncherUpscale)
      setFullscreenDisplayMode(width, height);

    createBackgroundWindow();
    std::thread([]() { backgroundWindowLoop(); }).detach();
  }

  if (doCursorCapture)
    std::thread([]() { captureCursorLoop(); }).detach();
}
#pragma once
#include <string>
#include <optional>
#include <windows.h>
#include "Error.hpp"


class Settings
{
public:

  bool fullscreen = true;
  bool hostGame = true;
  std::string serverAddress = "host IP or hostname";

  bool pauseOnStartup = false;
  bool forceCursorVisible = false;
  bool disableCursorCapture = false;

  // Custom game resolution. Defaults to the lowest widescreen mode (1280x720) so the game
  // looks right out of the box on a modern display without the fonts getting tiny.
  int screenWidth = 1280;
  int screenHeight = 720;

  // Proper widescreen: correct the 3D projection so the battlefield renders at the real
  // screen aspect instead of stretching the original 4:3 image. On by default now that the
  // in-game HUD and menus line up properly at 16:9.
  bool widescreen = true;

public:
  void readSettings()
  {
    HKEY key = nullptr;
    HRESULT result = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings", 0, KEY_READ, &key);
    if (FAILED(result))
      return;

    std::optional<std::string> fullscreenTemp = regReadString(key, "DoFullscreen");
    if (fullscreenTemp)
      fullscreen = *fullscreenTemp == "1";

    std::optional<std::string> hostGameTemp = regReadString(key, "HostGame");
    if (hostGameTemp)
      hostGame = *hostGameTemp == "1";

    std::optional<std::string> serverAddressTemp = regReadString(key, "ServerAddress");
    if (serverAddressTemp)
      serverAddress = *serverAddressTemp;

    std::optional<std::string> pauseOnStartupTemp = regReadString(key, "PauseOnStartup");
    if (pauseOnStartupTemp)
      pauseOnStartup = *pauseOnStartupTemp == "1";

    std::optional<std::string> forceCursorVisibleTemp = regReadString(key, "ForceCursorVisible");
    if (forceCursorVisibleTemp)
      forceCursorVisible = *forceCursorVisibleTemp == "1";

    std::optional<std::string> disableCursorCaptureTemp = regReadString(key, "DisableCursorCapture");
    if (disableCursorCaptureTemp)
      disableCursorCapture = *disableCursorCaptureTemp == "1";

    std::optional<std::string> screenWidthTemp = regReadString(key, "ScreenWidth");
    if (screenWidthTemp)
    {
      try { screenWidth = std::stoi(*screenWidthTemp); }
      catch (...) {}
    }

    std::optional<std::string> screenHeightTemp = regReadString(key, "ScreenHeight");
    if (screenHeightTemp)
    {
      try { screenHeight = std::stoi(*screenHeightTemp); }
      catch (...) {}
    }

    std::optional<std::string> widescreenTemp = regReadString(key, "Widescreen");
    if (widescreenTemp)
      widescreen = *widescreenTemp == "1";

    RegCloseKey(key);
  }

  void writeSettings()
  {
    HKEY key = nullptr;
    HRESULT result = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\LauncherCustomSettings", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
    release_assert(result == S_OK && key);

    regWriteString(key, "DoFullscreen", fullscreen ? "1" : "0");
    regWriteString(key, "HostGame", hostGame ? "1" : "0");
    regWriteString(key, "ServerAddress", serverAddress);
    regWriteString(key, "PauseOnStartup", pauseOnStartup ? "1" : "0");
    regWriteString(key, "ForceCursorVisible", forceCursorVisible ? "1" : "0");
    regWriteString(key, "DisableCursorCapture", disableCursorCapture ? "1" : "0");
    regWriteString(key, "ScreenWidth", std::to_string(screenWidth));
    regWriteString(key, "ScreenHeight", std::to_string(screenHeight));
    regWriteString(key, "Widescreen", widescreen ? "1" : "0");

    RegCloseKey(key);
  }

private:
  std::optional<std::string> regReadString(HKEY key, std::string valueName)
  {
    DWORD type = 0;
    DWORD size = 0;
    LSTATUS status = RegGetValueA(key, nullptr, valueName.c_str(), RRF_RT_REG_SZ, &type, nullptr, &size);
    if (status != ERROR_SUCCESS)
      return std::nullopt;

    std::string value(size, '\0');
    status = RegGetValueA(key, nullptr, valueName.c_str(), RRF_RT_REG_SZ, &type, reinterpret_cast<PBYTE>(value.data()), &size);
    if (status != ERROR_SUCCESS)
      return std::nullopt;

    value.resize(strlen(value.c_str()));
    return value;
  }

  void regWriteString(HKEY key, std::string valueName, std::string value)
  {
    RegSetKeyValueA(key, nullptr, valueName.c_str(), REG_SZ, value.c_str(), DWORD(value.size()));
  }
};
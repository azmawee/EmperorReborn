#include "ResolveHost.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

bool resolveHostV4(const std::string& host, uint32_t& outAddrNetworkOrder)
{
  if (host.empty())
    return false;

  // Make sure Winsock is available. This is reference counted, so it is safe to
  // call alongside the game's own WSAStartup; we balance it with WSACleanup.
  WSADATA wsaData = {};
  bool started = WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;

  addrinfo hints = {};
  hints.ai_family = AF_INET;       // IPv4 only: the game speaks IPv4
  hints.ai_socktype = SOCK_DGRAM;

  addrinfo* result = nullptr;
  int err = getaddrinfo(host.c_str(), nullptr, &hints, &result);

  bool ok = false;
  if (err == 0 && result != nullptr)
  {
    outAddrNetworkOrder = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr.s_addr;
    ok = true;
  }

  if (result != nullptr)
    freeaddrinfo(result);
  if (started)
    WSACleanup();

  return ok;
}

#pragma once
#include <string>
#include <cstdint>

// Resolve a host string to an IPv4 address in network byte order.
//
// Accepts:
//   - an IPv4 literal, e.g. "203.0.113.7"
//   - a hostname or dynamic DNS (DDNS) name, e.g. "myhost.duckdns.org"
//
// The game itself only speaks IPv4, so we resolve to the A record. This lets a
// host with a changing home IP share a stable name instead of a raw address.
//
// Returns true and fills outAddrNetworkOrder on success; returns false if the
// name could not be resolved to an IPv4 address.
//
// Deliberately winsock-header-free so it can be included from translation units
// that use either <winsock.h> (the game side) or <winsock2.h>.
bool resolveHostV4(const std::string& host, uint32_t& outAddrNetworkOrder);

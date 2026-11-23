#pragma once

#include <string>
#include <string_view>

// SHA-256 and HMAC-SHA256 (FIPS 180-4, RFC 2104). Results are the raw 32 bytes.
namespace sha256 {

std::string digest(std::string_view data);
std::string hmac(std::string_view key, std::string_view message);
// Lowercase hexadecimal.
std::string hex(std::string_view bytes);
// Compares without stopping at the first difference, so timing reveals nothing.
bool sameBytes(std::string_view a, std::string_view b);

} // namespace sha256

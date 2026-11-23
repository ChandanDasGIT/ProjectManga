#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Reads gzip files (Project Gutenberg's catalogue), written here so the app needs no
// compression library.
namespace archive {

// Raw Deflate data (RFC 1951) to `out`. Returns an empty string or why it failed.
std::string inflate(const unsigned char* data, std::size_t size, std::string& out);

// A gzip file's content (one member, as `gzip` writes it), its CRC-32 and length checked.
// Returns an empty string or why it failed.
std::string gunzip(const std::string& gz, std::string& out);

std::uint32_t crc32(const unsigned char* data, std::size_t size, std::uint32_t crc = 0);

} // namespace archive

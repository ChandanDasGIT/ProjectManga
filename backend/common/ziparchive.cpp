#include "common/ziparchive.h"

#include <array>
#include <vector>

namespace archive {

namespace {

// ---- Deflate, after the structure of zlib's "puff" reference decoder ----

constexpr int maxBits = 15;

struct Huffman {
    std::array<short, maxBits + 1> count{};   // codes of each length
    std::vector<short> symbol;               // symbols in canonical order
};

struct BitReader {
    const unsigned char* data;
    std::size_t size;
    std::size_t pos = 0;
    unsigned bitBuffer = 0;
    int bitCount = 0;
    bool overrun = false;

    int bits(int need)
    {
        long value = bitBuffer;
        while (bitCount < need) {
            if (pos >= size) {
                overrun = true;
                return 0;
            }
            value |= static_cast<long>(data[pos++]) << bitCount;
            bitCount += 8;
        }
        bitBuffer = static_cast<unsigned>(value >> need);
        bitCount -= need;
        return static_cast<int>(value & ((1L << need) - 1));
    }
};

// Builds a decoding table from code lengths; false when the lengths are not a valid code.
bool build(Huffman& h, const short* lengths, int n)
{
    h.count.fill(0);
    h.symbol.assign(static_cast<std::size_t>(n), 0);
    for (int s = 0; s < n; ++s) ++h.count[static_cast<std::size_t>(lengths[s])];
    if (h.count[0] == n) return true;                 // no codes: fine, unused
    int left = 1;
    for (int len = 1; len <= maxBits; ++len) {
        left <<= 1;
        left -= h.count[static_cast<std::size_t>(len)];
        if (left < 0) return false;                   // over-subscribed
    }
    std::array<short, maxBits + 1> offs{};
    for (int len = 1; len < maxBits; ++len)
        offs[static_cast<std::size_t>(len + 1)] = static_cast<short>(offs[static_cast<std::size_t>(len)] + h.count[static_cast<std::size_t>(len)]);
    for (int s = 0; s < n; ++s)
        if (lengths[s] != 0) h.symbol[static_cast<std::size_t>(offs[static_cast<std::size_t>(lengths[s])]++)] = static_cast<short>(s);
    return true;
}

int decode(BitReader& r, const Huffman& h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= maxBits; ++len) {
        code |= r.bits(1);
        if (r.overrun) return -1;
        const int count = h.count[static_cast<std::size_t>(len)];
        if (code - count < first) return h.symbol[static_cast<std::size_t>(index + (code - first))];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

constexpr short lengthBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr short lengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr short distBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr short distExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

std::string codes(BitReader& r, std::string& out, const Huffman& lencode, const Huffman& distcode)
{
    while (true) {
        int symbol = decode(r, lencode);
        if (symbol < 0) return "damaged compressed data";
        if (symbol < 256) {
            out += static_cast<char>(symbol);
        } else if (symbol == 256) {
            return {};
        } else {
            symbol -= 257;
            if (symbol >= 29) return "damaged compressed data";
            const int length = lengthBase[symbol] + r.bits(lengthExtra[symbol]);
            const int dsym = decode(r, distcode);
            if (dsym < 0 || dsym >= 30) return "damaged compressed data";
            const std::size_t dist = static_cast<std::size_t>(distBase[dsym] + r.bits(distExtra[dsym]));
            if (r.overrun) return "compressed data ends early";
            if (dist > out.size()) return "damaged compressed data";
            const std::size_t from = out.size() - dist;
            for (int i = 0; i < length; ++i) out += out[from + static_cast<std::size_t>(i)];
        }
    }
}

std::string stored(BitReader& r, std::string& out)
{
    r.bitBuffer = 0;
    r.bitCount = 0;                                   // to a byte boundary
    if (r.pos + 4 > r.size) return "compressed data ends early";
    const unsigned len = r.data[r.pos] | (r.data[r.pos + 1] << 8);
    const unsigned nlen = r.data[r.pos + 2] | (r.data[r.pos + 3] << 8);
    r.pos += 4;
    if (len != (~nlen & 0xffff)) return "damaged stored block";
    if (r.pos + len > r.size) return "compressed data ends early";
    out.append(reinterpret_cast<const char*>(r.data + r.pos), len);
    r.pos += len;
    return {};
}

std::string fixedBlock(BitReader& r, std::string& out)
{
    static Huffman lencode, distcode;
    static bool ready = false;
    if (!ready) {
        short lengths[288];
        int s = 0;
        for (; s < 144; ++s) lengths[s] = 8;
        for (; s < 256; ++s) lengths[s] = 9;
        for (; s < 280; ++s) lengths[s] = 7;
        for (; s < 288; ++s) lengths[s] = 8;
        build(lencode, lengths, 288);
        for (s = 0; s < 30; ++s) lengths[s] = 5;
        build(distcode, lengths, 30);
        ready = true;
    }
    return codes(r, out, lencode, distcode);
}

std::string dynamicBlock(BitReader& r, std::string& out)
{
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int nlen = r.bits(5) + 257;
    const int ndist = r.bits(5) + 1;
    const int ncode = r.bits(4) + 4;
    if (nlen > 286 || ndist > 30) return "damaged compressed data";
    short lengths[320] = {};
    for (int i = 0; i < ncode; ++i) lengths[order[i]] = static_cast<short>(r.bits(3));
    Huffman lencode, distcode;
    if (!build(lencode, lengths, 19)) return "damaged compressed data";
    int index = 0;
    while (index < nlen + ndist) {
        int symbol = decode(r, lencode);
        if (symbol < 0) return "damaged compressed data";
        if (symbol < 16) {
            lengths[index++] = static_cast<short>(symbol);
        } else {
            short len = 0;
            if (symbol == 16) {
                if (index == 0) return "damaged compressed data";
                len = lengths[index - 1];
                symbol = 3 + r.bits(2);
            } else if (symbol == 17) {
                symbol = 3 + r.bits(3);
            } else {
                symbol = 11 + r.bits(7);
            }
            if (index + symbol > nlen + ndist) return "damaged compressed data";
            while (symbol--) lengths[index++] = len;
        }
    }
    if (lengths[256] == 0) return "damaged compressed data";
    if (!build(lencode, lengths, nlen) || !build(distcode, lengths + nlen, ndist)) return "damaged compressed data";
    return codes(r, out, lencode, distcode);
}

std::uint32_t read16(const unsigned char* p)
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8);
}

std::uint32_t read32(const unsigned char* p)
{
    return read16(p) | (read16(p + 2) << 16);
}

} // namespace

std::uint32_t crc32(const unsigned char* data, std::size_t size, std::uint32_t crc)
{
    static std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    return ~crc;
}

std::string inflate(const unsigned char* data, std::size_t size, std::string& out)
{
    BitReader r{data, size};
    int last = 0;
    do {
        last = r.bits(1);
        const int type = r.bits(2);
        if (r.overrun) return "compressed data ends early";
        std::string error = type == 0 ? stored(r, out) : type == 1 ? fixedBlock(r, out) : type == 2 ? dynamicBlock(r, out)
                                                                                                       : std::string("damaged compressed data");
        if (!error.empty()) return error;
    } while (!last);
    return {};
}

std::string gunzip(const std::string& gz, std::string& out)
{
    const auto* d = reinterpret_cast<const unsigned char*>(gz.data());
    const std::size_t n = gz.size();
    if (n < 18 || d[0] != 0x1f || d[1] != 0x8b) return "not a gzip file";
    if (d[2] != 8) return "unsupported gzip compression";
    const unsigned flags = d[3];
    std::size_t at = 10;
    if (flags & 4) {                                   // FEXTRA
        if (at + 2 > n) return "damaged gzip header";
        at += 2 + (d[at] | (d[at + 1] << 8));
    }
    if (flags & 8) while (at < n && d[at++] != 0) {}   // FNAME
    if (flags & 16) while (at < n && d[at++] != 0) {}  // FCOMMENT
    if (flags & 2) at += 2;                            // FHCRC
    if (at + 8 > n) return "damaged gzip header";
    out.clear();
    if (const std::string error = inflate(d + at, n - at - 8, out); !error.empty()) return error;
    const std::uint32_t crc = read32(d + n - 8);
    const std::uint32_t size = read32(d + n - 4);
    if (static_cast<std::uint32_t>(out.size()) != size ||
        crc32(reinterpret_cast<const unsigned char*>(out.data()), out.size()) != crc)
        return "damaged gzip file (checksum)";
    return {};
}

} // namespace archive

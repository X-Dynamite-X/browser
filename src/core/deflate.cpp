#include "deflate.hpp"

#include <array>
#include <cstring>

namespace litebrowser {
namespace compression {

namespace {

struct BitReader {
    const uint8_t* data;
    size_t size;
    size_t bytePos = 0;
    uint32_t bitBuf = 0;
    int bitCount = 0;

    BitReader(const uint8_t* d, size_t s) : data(d), size(s) {}

    bool ensureBits(int n) {
        while (bitCount < n) {
            if (bytePos >= size) return false;
            bitBuf |= static_cast<uint32_t>(data[bytePos++]) << bitCount;
            bitCount += 8;
        }
        return true;
    }

    bool readBits(int n, uint32_t& out) {
        if (n == 0) {
            out = 0;
            return true;
        }
        if (!ensureBits(n)) return false;
        out = bitBuf & ((1u << n) - 1u);
        bitBuf >>= n;
        bitCount -= n;
        return true;
    }

    void alignToByte() {
        int discard = bitCount & 7;
        bitBuf >>= discard;
        bitCount -= discard;
    }

    bool readAlignedBytes(uint8_t* dst, size_t count) {
        alignToByte();
        size_t i = 0;
        while (bitCount >= 8 && i < count) {
            dst[i++] = static_cast<uint8_t>(bitBuf & 0xFFu);
            bitBuf >>= 8;
            bitCount -= 8;
        }
        if (i < count) {
            size_t rem = count - i;
            if (bytePos + rem > size) return false;
            std::memcpy(dst + i, data + bytePos, rem);
            bytePos += rem;
        }
        return true;
    }
};

struct HuffmanTree {
    static constexpr int kMaxBits = 15;
    std::array<int, kMaxBits + 1> counts{};
    std::vector<int> symbols;

    bool build(const int* lengths, int numSymbols) {
        counts.fill(0);
        int maxLen = 0;
        for (int i = 0; i < numSymbols; ++i) {
            int l = lengths[i];
            if (l < 0 || l > kMaxBits) return false;
            if (l > 0) {
                counts[l]++;
                if (l > maxLen) maxLen = l;
            }
        }
        if (maxLen == 0) {
            symbols.clear();
            return true;
        }

        // Check for over-subscribed tree
        int left = 1;
        for (int bits = 1; bits <= kMaxBits; ++bits) {
            left <<= 1;
            left -= counts[bits];
            if (left < 0) return false;
        }

        std::array<int, kMaxBits + 1> offsets{};
        int total = 0;
        for (int bits = 1; bits <= kMaxBits; ++bits) {
            offsets[bits] = total;
            total += counts[bits];
        }
        symbols.assign(static_cast<size_t>(total), 0);
        for (int sym = 0; sym < numSymbols; ++sym) {
            int l = lengths[sym];
            if (l > 0) {
                symbols[static_cast<size_t>(offsets[l]++)] = sym;
            }
        }
        return true;
    }

    bool decode(BitReader& br, int& outSym) const {
        if (symbols.empty()) return false;
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len <= kMaxBits; ++len) {
            uint32_t bit = 0;
            if (!br.readBits(1, bit)) return false;
            code |= static_cast<int>(bit);
            int count = counts[len];
            if (code - count < first) {
                outSym = symbols[static_cast<size_t>(index + (code - first))];
                return true;
            }
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return false;
    }
};

static const int kCodeLengthOrder[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static const int kLenBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

static const int kLenExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

static const int kDistBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

static const int kDistExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

bool decodeBlockBody(BitReader& br, const HuffmanTree& litLenTree, const HuffmanTree& distTree,
                     std::vector<uint8_t>& out, size_t maxOutputBytes) {
    while (true) {
        int sym = 0;
        if (!litLenTree.decode(br, sym)) return false;
        if (sym < 256) {
            if (out.size() >= maxOutputBytes) return false;
            out.push_back(static_cast<uint8_t>(sym));
        } else if (sym == 256) {
            return true;
        } else {
            int lenIdx = sym - 257;
            if (lenIdx < 0 || lenIdx >= 29) return false;
            uint32_t extra = 0;
            if (!br.readBits(kLenExtra[lenIdx], extra)) return false;
            size_t matchLen = static_cast<size_t>(kLenBase[lenIdx]) + extra;

            int distSym = 0;
            if (!distTree.decode(br, distSym)) return false;
            if (distSym < 0 || distSym >= 30) return false;
            uint32_t distExtra = 0;
            if (!br.readBits(kDistExtra[distSym], distExtra)) return false;
            size_t dist = static_cast<size_t>(kDistBase[distSym]) + distExtra;

            if (dist == 0 || dist > out.size()) return false;
            if (out.size() + matchLen > maxOutputBytes) return false;

            size_t startIdx = out.size() - dist;
            for (size_t i = 0; i < matchLen; ++i) {
                out.push_back(out[startIdx + i]);
            }
        }
    }
}

}  // namespace

bool inflateRaw(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t maxOutputBytes) {
    out.clear();
    if (!src || srcLen == 0) return false;

    BitReader br(src, srcLen);
    uint32_t bfinal = 0;
    do {
        if (!br.readBits(1, bfinal)) return false;
        uint32_t btype = 0;
        if (!br.readBits(2, btype)) return false;

        if (btype == 0) {
            uint8_t hdr[4] = {0};
            if (!br.readAlignedBytes(hdr, 4)) return false;
            uint16_t len = static_cast<uint16_t>(hdr[0] | (hdr[1] << 8));
            uint16_t nlen = static_cast<uint16_t>(hdr[2] | (hdr[3] << 8));
            if (static_cast<uint16_t>(~nlen) != len) return false;
            if (out.size() + len > maxOutputBytes) return false;
            size_t oldSz = out.size();
            out.resize(oldSz + len);
            if (len > 0 && !br.readAlignedBytes(out.data() + oldSz, len)) return false;
        } else if (btype == 1) {
            int litLengths[288];
            for (int i = 0; i <= 143; ++i) litLengths[i] = 8;
            for (int i = 144; i <= 255; ++i) litLengths[i] = 9;
            for (int i = 256; i <= 279; ++i) litLengths[i] = 7;
            for (int i = 280; i <= 287; ++i) litLengths[i] = 8;
            int distLengths[32];
            for (int i = 0; i < 32; ++i) distLengths[i] = 5;

            HuffmanTree litTree, distTree;
            if (!litTree.build(litLengths, 288) || !distTree.build(distLengths, 32)) return false;
            if (!decodeBlockBody(br, litTree, distTree, out, maxOutputBytes)) return false;
        } else if (btype == 2) {
            uint32_t hlit = 0, hdist = 0, hclen = 0;
            if (!br.readBits(5, hlit) || !br.readBits(5, hdist) || !br.readBits(4, hclen)) return false;
            int numLit = static_cast<int>(hlit) + 257;
            int numDist = static_cast<int>(hdist) + 1;
            int numCodeLen = static_cast<int>(hclen) + 4;
            if (numLit > 286 || numDist > 30) return false;

            int codeLenLengths[19] = {0};
            for (int i = 0; i < numCodeLen; ++i) {
                uint32_t cl = 0;
                if (!br.readBits(3, cl)) return false;
                codeLenLengths[kCodeLengthOrder[i]] = static_cast<int>(cl);
            }
            HuffmanTree codeLenTree;
            if (!codeLenTree.build(codeLenLengths, 19)) return false;

            std::vector<int> allLengths(static_cast<size_t>(numLit + numDist), 0);
            int idx = 0;
            int totalSyms = numLit + numDist;
            while (idx < totalSyms) {
                int sym = 0;
                if (!codeLenTree.decode(br, sym)) return false;
                if (sym < 16) {
                    allLengths[static_cast<size_t>(idx++)] = sym;
                } else if (sym == 16) {
                    if (idx == 0) return false;
                    uint32_t rep = 0;
                    if (!br.readBits(2, rep)) return false;
                    int prev = allLengths[static_cast<size_t>(idx - 1)];
                    int count = static_cast<int>(rep) + 3;
                    if (idx + count > totalSyms) return false;
                    for (int k = 0; k < count; ++k) allLengths[static_cast<size_t>(idx++)] = prev;
                } else if (sym == 17) {
                    uint32_t rep = 0;
                    if (!br.readBits(3, rep)) return false;
                    int count = static_cast<int>(rep) + 3;
                    if (idx + count > totalSyms) return false;
                    for (int k = 0; k < count; ++k) allLengths[static_cast<size_t>(idx++)] = 0;
                } else if (sym == 18) {
                    uint32_t rep = 0;
                    if (!br.readBits(7, rep)) return false;
                    int count = static_cast<int>(rep) + 11;
                    if (idx + count > totalSyms) return false;
                    for (int k = 0; k < count; ++k) allLengths[static_cast<size_t>(idx++)] = 0;
                } else {
                    return false;
                }
            }

            HuffmanTree litTree, distTree;
            if (!litTree.build(allLengths.data(), numLit)) return false;
            if (!distTree.build(allLengths.data() + numLit, numDist)) return false;
            if (!decodeBlockBody(br, litTree, distTree, out, maxOutputBytes)) return false;
        } else {
            return false;
        }
    } while (bfinal == 0);

    return true;
}

bool inflateZlibOrRaw(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t maxOutputBytes) {
    if (!src || srcLen == 0) return false;
    if (srcLen >= 2) {
        uint8_t cmf = src[0];
        uint8_t flg = src[1];
        if ((cmf & 0x0F) == 8 && ((static_cast<uint16_t>(cmf) * 256u + flg) % 31u == 0)) {
            size_t headerLen = (flg & 0x20) ? 6 : 2;
            if (srcLen > headerLen) {
                if (inflateRaw(src + headerLen, srcLen - headerLen, out, maxOutputBytes)) {
                    return true;
                }
            }
        }
    }
    return inflateRaw(src, srcLen, out, maxOutputBytes);
}

}  // namespace compression
}  // namespace litebrowser


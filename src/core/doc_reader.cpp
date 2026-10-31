#include "doc_reader.hpp"

#include "docx_reader.hpp"
#include "fs_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace litebrowser {

namespace {

uint16_t u16le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t u32le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

uint32_t cp1252ToUnicode(uint8_t b) {
    static const uint16_t kWin1252_80_9F[32] = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
    };
    if (b >= 0x80 && b <= 0x9F) return kWin1252_80_9F[b - 0x80];
    return b;
}

size_t countWords(const std::string& s) {
    size_t c = 0;
    bool inW = false;
    for (unsigned char ch : s) {
        if (ch <= 32) inW = false;
        else if (!inW) { inW = true; ++c; }
    }
    return c;
}

DocxParseResult buildResultFromPlainText(const std::string& title, const std::string& rawText) {
    DocxParseResult res;
    res.ok = true;
    res.title = title;

    std::string para;
    auto flushPara = [&]() {
        std::string t = fs::trim(para);
        para.clear();
        if (t.empty()) return;
        res.paragraphCount++;
        res.plainText += t + "\n";
        bool rtl = fs::containsArabicText(t);
        res.bodyHtml += std::string("<p dir=\"") + (rtl ? "rtl" : "ltr") + "\"" +
                        (rtl ? " style=\"text-align:right;\"" : "") + ">" +
                        fs::htmlEscape(t) + "</p>\n";
    };

    for (size_t i = 0; i < rawText.size(); ++i) {
        char c = rawText[i];
        if (c == '\r' || c == '\n' || c == 0x07) {
            flushPara();
            if (c == '\r' && i + 1 < rawText.size() && rawText[i + 1] == '\n') ++i;
        } else if (static_cast<unsigned char>(c) >= 0x20 || c == '\t') {
            para.push_back(c);
        }
    }
    flushPara();
    res.wordCount = countWords(res.plainText);
    return res;
}

DocxParseResult parseRtfContent(const std::string& title, const std::string& rtf) {
    std::string out;
    out.reserve(rtf.size() / 2);
    int depth = 0;
    int skipDepth = -1;
    int ucSkip = 1;

    size_t i = 0, n = rtf.size();
    while (i < n) {
        char c = rtf[i];
        if (c == '{') {
            depth++;
            ++i;
            if (i < n && rtf.compare(i, 2, "\\*") == 0 && skipDepth < 0) {
                skipDepth = depth;
            }
            continue;
        }
        if (c == '}') {
            if (depth == skipDepth) skipDepth = -1;
            depth = std::max(0, depth - 1);
            ++i;
            continue;
        }
        if (c == '\r' || c == '\n') {
            ++i;
            continue;
        }
        if (c == '\\') {
            ++i;
            if (i >= n) break;
            if (rtf[i] == '\\' || rtf[i] == '{' || rtf[i] == '}') {
                if (skipDepth < 0) out.push_back(rtf[i]);
                ++i;
                continue;
            }
            if (rtf[i] == '\'') {
                if (i + 2 < n) {
                    char hex[3] = {rtf[i + 1], rtf[i + 2], 0};
                    uint8_t b = static_cast<uint8_t>(std::strtoul(hex, nullptr, 16));
                    if (skipDepth < 0) appendUtf8(out, cp1252ToUnicode(b));
                    i += 3;
                    continue;
                }
            }
            size_t wStart = i;
            while (i < n && std::isalpha(static_cast<unsigned char>(rtf[i]))) ++i;
            std::string word = rtf.substr(wStart, i - wStart);
            bool hasNum = false;
            int num = 0;
            if (i < n && (rtf[i] == '-' || std::isdigit(static_cast<unsigned char>(rtf[i])))) {
                hasNum = true;
                size_t numStart = i;
                if (rtf[i] == '-') ++i;
                while (i < n && std::isdigit(static_cast<unsigned char>(rtf[i]))) ++i;
                num = std::atoi(rtf.substr(numStart, i - numStart).c_str());
            }
            if (i < n && rtf[i] == ' ') ++i;

            if ((word == "fonttbl" || word == "colortbl" || word == "stylesheet" ||
                 word == "info" || word == "pict" || word == "header" || word == "footer") && skipDepth < 0) {
                skipDepth = depth;
            } else if (skipDepth < 0) {
                if (word == "par" || word == "line" || word == "row") {
                    out.push_back('\n');
                } else if (word == "tab" || word == "cell") {
                    out.push_back('\t');
                } else if (word == "uc" && hasNum && num >= 0 && num <= 8) {
                    ucSkip = num;
                } else if (word == "u" && hasNum) {
                    int32_t code = num;
                    if (code < 0) code += 65536;
                    appendUtf8(out, static_cast<uint32_t>(code));
                    // Skip fallback ANSI characters
                    for (int s = 0; s < ucSkip && i < n; ++s) {
                        if (rtf.compare(i, 2, "\\'") == 0 && i + 4 <= n) i += 4;
                        else if (rtf[i] != '{' && rtf[i] != '}') ++i;
                    }
                }
            }
            continue;
        }
        if (skipDepth < 0) {
            out.push_back(c);
        }
        ++i;
    }

    return buildResultFromPlainText(title, out);
}

struct OleDirEntry {
    std::string name;
    uint8_t type = 0;
    uint32_t startSect = 0;
    uint32_t size = 0;
};

bool extractOle2WordText(const std::vector<uint8_t>& fileBytes, std::string& outUtf8) {
    static const uint8_t kOleMagic[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    if (fileBytes.size() < 512 || std::memcmp(fileBytes.data(), kOleMagic, 8) != 0) {
        return false;
    }

    uint16_t sectorShift = u16le(fileBytes.data() + 0x1E);
    uint16_t miniShift = u16le(fileBytes.data() + 0x20);
    if (sectorShift < 7 || sectorShift > 16 || miniShift < 4 || miniShift > sectorShift) return false;
    const size_t sectorSize = static_cast<size_t>(1u) << sectorShift;
    const size_t miniSectorSize = static_cast<size_t>(1u) << miniShift;

    uint32_t numFatSectors = u32le(fileBytes.data() + 0x2C);
    uint32_t firstDirSect = u32le(fileBytes.data() + 0x30);
    uint32_t miniCutoff = u32le(fileBytes.data() + 0x38);
    uint32_t firstMiniFatSect = u32le(fileBytes.data() + 0x3C);
    uint32_t numMiniFatSectors = u32le(fileBytes.data() + 0x40);
    uint32_t firstDifatSect = u32le(fileBytes.data() + 0x44);
    uint32_t numDifatSectors = u32le(fileBytes.data() + 0x48);

    auto sectorOffset = [&](uint32_t sect) -> size_t {
        return 512 + static_cast<size_t>(sect) * sectorSize;
    };

    std::vector<uint32_t> fatSectors;
    for (int i = 0; i < 109 && fatSectors.size() < numFatSectors; ++i) {
        uint32_t s = u32le(fileBytes.data() + 0x4C + i * 4);
        if (s < 0xFFFFFFFCu) fatSectors.push_back(s);
    }
    uint32_t difatSect = firstDifatSect;
    for (uint32_t d = 0; d < numDifatSectors && difatSect < 0xFFFFFFFCu; ++d) {
        size_t off = sectorOffset(difatSect);
        if (off + sectorSize > fileBytes.size()) break;
        size_t entriesPerSect = (sectorSize / 4) - 1;
        for (size_t k = 0; k < entriesPerSect && fatSectors.size() < numFatSectors; ++k) {
            uint32_t s = u32le(fileBytes.data() + off + k * 4);
            if (s < 0xFFFFFFFCu) fatSectors.push_back(s);
        }
        difatSect = u32le(fileBytes.data() + off + entriesPerSect * 4);
    }

    std::vector<uint32_t> fat;
    fat.reserve(fatSectors.size() * (sectorSize / 4));
    for (uint32_t fs : fatSectors) {
        size_t off = sectorOffset(fs);
        if (off + sectorSize > fileBytes.size()) break;
        for (size_t k = 0; k < sectorSize; k += 4) {
            fat.push_back(u32le(fileBytes.data() + off + k));
        }
    }
    if (fat.empty()) return false;

    auto readChain = [&](uint32_t startSect, size_t maxBytes) -> std::vector<uint8_t> {
        std::vector<uint8_t> out;
        uint32_t cur = startSect;
        size_t guard = 0;
        while (cur < fat.size() && cur < 0xFFFFFFFCu && guard < fat.size() + 1) {
            size_t off = sectorOffset(cur);
            if (off + sectorSize > fileBytes.size()) break;
            size_t toCopy = sectorSize;
            if (maxBytes > 0 && out.size() + toCopy > maxBytes) {
                toCopy = maxBytes - out.size();
            }
            out.insert(out.end(), fileBytes.begin() + static_cast<std::ptrdiff_t>(off),
                       fileBytes.begin() + static_cast<std::ptrdiff_t>(off + toCopy));
            if (maxBytes > 0 && out.size() >= maxBytes) break;
            cur = fat[cur];
            ++guard;
        }
        return out;
    };

    std::vector<uint8_t> dirData = readChain(firstDirSect, 0);
    if (dirData.size() < 128) return false;

    std::vector<OleDirEntry> dirs;
    for (size_t off = 0; off + 128 <= dirData.size(); off += 128) {
        uint8_t type = dirData[off + 0x42];
        uint16_t cbName = u16le(dirData.data() + off + 0x40);
        if (type == 0 || cbName < 2 || cbName > 64) continue;
        std::string name;
        for (uint16_t b = 0; b + 1 < cbName - 2; b += 2) {
            uint16_t ch = u16le(dirData.data() + off + b);
            if (ch == 0) break;
            appendUtf8(name, ch);
        }
        OleDirEntry e;
        e.name = name;
        e.type = type;
        e.startSect = u32le(dirData.data() + off + 0x74);
        e.size = u32le(dirData.data() + off + 0x78);
        dirs.push_back(e);
    }

    // Build MiniStream & MiniFAT for streams smaller than miniCutoff (typically 4096 bytes, e.g. small 1Table!)
    std::vector<uint8_t> miniStream;
    std::vector<uint32_t> miniFat;
    for (const auto& d : dirs) {
        if (d.type == 5 && d.size > 0) {
            miniStream = readChain(d.startSect, d.size);
            break;
        }
    }
    if (numMiniFatSectors > 0 && firstMiniFatSect < 0xFFFFFFFCu) {
        std::vector<uint8_t> mfBytes = readChain(firstMiniFatSect, numMiniFatSectors * sectorSize);
        for (size_t k = 0; k + 4 <= mfBytes.size(); k += 4) {
            miniFat.push_back(u32le(mfBytes.data() + k));
        }
    }

    auto readStreamByName = [&](const std::string& targetName) -> std::vector<uint8_t> {
        for (const auto& d : dirs) {
            if (d.type == 2 && d.name == targetName) {
                if (d.size < miniCutoff && !miniStream.empty() && !miniFat.empty()) {
                    std::vector<uint8_t> out;
                    uint32_t cur = d.startSect;
                    size_t guard = 0;
                    while (cur < miniFat.size() && cur < 0xFFFFFFFCu && out.size() < d.size && guard < miniFat.size() + 1) {
                        size_t off = static_cast<size_t>(cur) * miniSectorSize;
                        if (off + miniSectorSize > miniStream.size()) break;
                        size_t toCopy = std::min(miniSectorSize, static_cast<size_t>(d.size) - out.size());
                        out.insert(out.end(), miniStream.begin() + static_cast<std::ptrdiff_t>(off),
                                   miniStream.begin() + static_cast<std::ptrdiff_t>(off + toCopy));
                        cur = miniFat[cur];
                        ++guard;
                    }
                    return out;
                }
                return readChain(d.startSect, d.size);
            }
        }
        return {};
    };

    std::vector<uint8_t> wordDoc = readStreamByName("WordDocument");
    if (wordDoc.size() < 68) return false;

    uint16_t flags = u16le(wordDoc.data() + 0x0A);
    std::string tableName = (flags & 0x0200) ? "1Table" : "0Table";
    std::vector<uint8_t> tableStream = readStreamByName(tableName);

    // Navigate FIB dynamically: FibBase (32B) -> csw/FibRgW -> cslw/FibRgLw -> cbRgFcLcb/FibRgFcLcbBlob[33] (Clx)
    size_t pos = 0x20;
    if (pos + 2 <= wordDoc.size() && !tableStream.empty()) {
        uint16_t csw = u16le(wordDoc.data() + pos);
        pos += 2 + static_cast<size_t>(csw) * 2;
        if (pos + 2 <= wordDoc.size()) {
            uint16_t cslw = u16le(wordDoc.data() + pos);
            uint32_t ccpText = (cslw >= 4 && pos + 2 + 16 <= wordDoc.size()) ? u32le(wordDoc.data() + pos + 2 + 12) : 0;
            pos += 2 + static_cast<size_t>(cslw) * 4;
            if (pos + 2 <= wordDoc.size()) {
                uint16_t cbRgFcLcb = u16le(wordDoc.data() + pos);
                size_t blobStart = pos + 2;
                if (cbRgFcLcb > 33 && blobStart + 34 * 8 <= wordDoc.size()) {
                    uint32_t fcClx = u32le(wordDoc.data() + blobStart + 33 * 8);
                    uint32_t lcbClx = u32le(wordDoc.data() + blobStart + 33 * 8 + 4);
                    if (lcbClx > 0 && static_cast<uint64_t>(fcClx) + lcbClx <= tableStream.size()) {
                        size_t clxPos = fcClx;
                        size_t clxEnd = fcClx + lcbClx;
                        while (clxPos < clxEnd && tableStream[clxPos] == 0x01) {
                            if (clxPos + 3 > clxEnd) break;
                            uint16_t cbGrpprl = u16le(tableStream.data() + clxPos + 1);
                            clxPos += 3 + cbGrpprl;
                        }
                        if (clxPos + 5 <= clxEnd && tableStream[clxPos] == 0x02) {
                            uint32_t pcdtLen = u32le(tableStream.data() + clxPos + 1);
                            size_t plcPos = clxPos + 5;
                            if (pcdtLen >= 16 && plcPos + pcdtLen <= clxEnd && ((pcdtLen - 4) % 12 == 0)) {
                                uint32_t numPieces = (pcdtLen - 4) / 12;
                                size_t pcdArrayStart = plcPos + static_cast<size_t>(numPieces + 1) * 4;
                                uint32_t charsCollected = 0;
                                for (uint32_t p = 0; p < numPieces; ++p) {
                                    uint32_t cpStart = u32le(tableStream.data() + plcPos + p * 4);
                                    uint32_t cpEnd = u32le(tableStream.data() + plcPos + (p + 1) * 4);
                                    if (cpEnd <= cpStart) continue;
                                    uint32_t charCount = cpEnd - cpStart;
                                    if (ccpText > 0 && charsCollected >= ccpText) break;
                                    if (ccpText > 0 && charsCollected + charCount > ccpText) {
                                        charCount = ccpText - charsCollected;
                                    }

                                    uint32_t fcComp = u32le(tableStream.data() + pcdArrayStart + p * 8 + 2);
                                    bool isAnsi = (fcComp & 0x40000000u) != 0;
                                    uint32_t fc = fcComp & 0x3FFFFFFFu;
                                    if (isAnsi) {
                                        size_t byteOff = fc / 2;
                                        for (uint32_t k = 0; k < charCount && byteOff + k < wordDoc.size(); ++k) {
                                            appendUtf8(outUtf8, cp1252ToUnicode(wordDoc[byteOff + k]));
                                        }
                                    } else {
                                        size_t byteOff = fc;
                                        for (uint32_t k = 0; k < charCount && byteOff + k * 2 + 1 < wordDoc.size(); ++k) {
                                            uint16_t ch = u16le(wordDoc.data() + byteOff + k * 2);
                                            if (ch >= 0xD800 && ch <= 0xDBFF && k + 1 < charCount &&
                                                byteOff + (k + 1) * 2 + 1 < wordDoc.size()) {
                                                uint16_t lo = u16le(wordDoc.data() + byteOff + (k + 1) * 2);
                                                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                                    uint32_t cp = 0x10000u + (((ch - 0xD800u) << 10) | (lo - 0xDC00u));
                                                    appendUtf8(outUtf8, cp);
                                                    ++k;
                                                    continue;
                                                }
                                            }
                                            appendUtf8(outUtf8, ch);
                                        }
                                    }
                                    charsCollected += charCount;
                                }
                                if (!outUtf8.empty()) return true;
                            }
                        }
                    }
                }
            }
        }
    }

    // Fallback: extract readable UTF-16LE / ASCII runs from WordDocument stream after FIB header
    size_t startScan = std::min<size_t>(0x800, wordDoc.size());
    for (size_t i = startScan; i + 1 < wordDoc.size(); i += 2) {
        uint16_t ch = u16le(wordDoc.data() + i);
        if (ch == 0x0D || ch == 0x0A || ch == 0x09 ||
            (ch >= 0x20 && ch <= 0x7E) || (ch >= 0x0600 && ch <= 0x06FF)) {
            appendUtf8(outUtf8, ch);
        }
    }
    return !fs::trim(outUtf8).empty();
}

}  // namespace

DocxParseResult parseLegacyDocOrRtfFile(const std::string& filePathUtf8) {
    std::string fileName = fs::getFileName(filePathUtf8);
    std::vector<uint8_t> bytes;
    if (!fs::readFileBytes(filePathUtf8, bytes) || bytes.empty()) {
        DocxParseResult err;
        err.errorMessage = "تعذّر قراءة الملف من القرص.";
        return err;
    }

    // Check if the file is actually a ZIP/.docx saved with .doc extension
    if (bytes.size() >= 4 && bytes[0] == 'P' && bytes[1] == 'K' && bytes[2] == 0x03 && bytes[3] == 0x04) {
        return parseDocxFile(filePathUtf8);
    }

    // Check if the file is RTF (starts with {\rtf)
    if (bytes.size() >= 5 && std::memcmp(bytes.data(), "{\\rtf", 5) == 0) {
        std::string rtfStr(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return parseRtfContent(fileName, rtfStr);
    }

    // Check OLE2 binary Word 97-2003 (.doc)
    std::string extracted;
    if (extractOle2WordText(bytes, extracted)) {
        return buildResultFromPlainText(fileName, extracted);
    }

    // Plain UTF-8 fallback if user renamed a text file to .doc
    std::string fallback(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return buildResultFromPlainText(fileName, fallback);
}

}  // namespace litebrowser


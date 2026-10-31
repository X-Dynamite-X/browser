#include "pdf_reader.hpp"

#include "deflate.hpp"
#include "fs_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#ifdef __linux__
#include <dlfcn.h>
#endif

namespace litebrowser {

namespace {

void appendUtf8Cp(std::string& out, uint32_t cp) {
    if (cp == 0) return;
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

int hexNib(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::vector<uint8_t> decodeHexBytes(const std::string& hex) {
    std::vector<uint8_t> out;
    int hi = -1;
    for (char c : hex) {
        int v = hexNib(c);
        if (v < 0) continue;
        if (hi < 0) {
            hi = v;
        } else {
            out.push_back(static_cast<uint8_t>((hi << 4) | v));
            hi = -1;
        }
    }
    if (hi >= 0) out.push_back(static_cast<uint8_t>(hi << 4));
    return out;
}

std::string utf16BeBytesToUtf8(const std::vector<uint8_t>& b) {
    std::string out;
    size_t i = 0;
    if (b.size() >= 2 && b[0] == 0xFE && b[1] == 0xFF) i = 2;
    for (; i + 1 < b.size(); i += 2) {
        uint16_t w1 = static_cast<uint16_t>((b[i] << 8) | b[i + 1]);
        if (w1 >= 0xD800 && w1 <= 0xDBFF && i + 3 < b.size()) {
            uint16_t w2 = static_cast<uint16_t>((b[i + 2] << 8) | b[i + 3]);
            if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
                uint32_t cp = 0x10000u + (((w1 - 0xD800u) << 10) | (w2 - 0xDC00u));
                appendUtf8Cp(out, cp);
                i += 2;
                continue;
            }
        }
        appendUtf8Cp(out, w1);
    }
    return out;
}

struct CMapTable {
    std::unordered_map<uint32_t, std::string> map1Byte;
    std::unordered_map<uint32_t, std::string> map2Byte;

    void parseCMapStream(const std::string& cmap) {
        // Parse beginbfchar ... endbfchar
        size_t pos = 0;
        while ((pos = cmap.find("beginbfchar", pos)) != std::string::npos) {
            pos += 11;
            size_t end = cmap.find("endbfchar", pos);
            if (end == std::string::npos) break;
            std::string sec = cmap.substr(pos, end - pos);
            size_t k = 0;
            while (k < sec.size()) {
                size_t a1 = sec.find('<', k);
                if (a1 == std::string::npos) break;
                size_t a2 = sec.find('>', a1 + 1);
                if (a2 == std::string::npos) break;
                size_t b1 = sec.find('<', a2 + 1);
                if (b1 == std::string::npos) break;
                size_t b2 = sec.find('>', b1 + 1);
                if (b2 == std::string::npos) break;

                auto srcBytes = decodeHexBytes(sec.substr(a1 + 1, a2 - a1 - 1));
                auto dstBytes = decodeHexBytes(sec.substr(b1 + 1, b2 - b1 - 1));
                std::string u = utf16BeBytesToUtf8(dstBytes);
                if (!u.empty()) {
                    if (srcBytes.size() == 1) {
                        map1Byte[srcBytes[0]] = u;
                    } else if (srcBytes.size() == 2) {
                        uint32_t code = (static_cast<uint32_t>(srcBytes[0]) << 8) | srcBytes[1];
                        map2Byte[code] = u;
                    }
                }
                k = b2 + 1;
            }
            pos = end + 9;
        }

        // Parse beginbfrange ... endbfrange
        pos = 0;
        while ((pos = cmap.find("beginbfrange", pos)) != std::string::npos) {
            pos += 12;
            size_t end = cmap.find("endbfrange", pos);
            if (end == std::string::npos) break;
            std::string sec = cmap.substr(pos, end - pos);
            size_t k = 0;
            while (k < sec.size()) {
                size_t a1 = sec.find('<', k);
                if (a1 == std::string::npos) break;
                size_t a2 = sec.find('>', a1 + 1);
                if (a2 == std::string::npos) break;
                size_t b1 = sec.find('<', a2 + 1);
                if (b1 == std::string::npos) break;
                size_t b2 = sec.find('>', b1 + 1);
                if (b2 == std::string::npos) break;

                auto startB = decodeHexBytes(sec.substr(a1 + 1, a2 - a1 - 1));
                auto endB = decodeHexBytes(sec.substr(b1 + 1, b2 - b1 - 1));

                size_t next = b2 + 1;
                while (next < sec.size() && std::isspace(static_cast<unsigned char>(sec[next]))) ++next;
                if (next < sec.size() && sec[next] == '<') {
                    size_t c2 = sec.find('>', next + 1);
                    if (c2 == std::string::npos) break;
                    auto dstB = decodeHexBytes(sec.substr(next + 1, c2 - next - 1));
                    if (startB.size() == endB.size() && (startB.size() == 1 || startB.size() == 2) && dstB.size() >= 2) {
                        uint32_t sCode = (startB.size() == 1) ? startB[0] : ((static_cast<uint32_t>(startB[0]) << 8) | startB[1]);
                        uint32_t eCode = (endB.size() == 1) ? endB[0] : ((static_cast<uint32_t>(endB[0]) << 8) | endB[1]);
                        uint32_t uBase = (static_cast<uint32_t>(dstB[0]) << 8) | dstB[1];
                        if (eCode >= sCode && eCode - sCode <= 4096) {
                            for (uint32_t code = sCode; code <= eCode; ++code) {
                                std::string u;
                                appendUtf8Cp(u, uBase + (code - sCode));
                                if (startB.size() == 1) map1Byte[code] = u;
                                else map2Byte[code] = u;
                            }
                        }
                    }
                    k = c2 + 1;
                } else if (next < sec.size() && sec[next] == '[') {
                    size_t closeBr = sec.find(']', next + 1);
                    if (closeBr == std::string::npos) break;
                    if (startB.size() == 1 || startB.size() == 2) {
                        uint32_t curCode = (startB.size() == 1) ? startB[0] : ((static_cast<uint32_t>(startB[0]) << 8) | startB[1]);
                        size_t m = next + 1;
                        while (m < closeBr) {
                            size_t x1 = sec.find('<', m);
                            if (x1 == std::string::npos || x1 >= closeBr) break;
                            size_t x2 = sec.find('>', x1 + 1);
                            if (x2 == std::string::npos || x2 >= closeBr) break;
                            auto dstB = decodeHexBytes(sec.substr(x1 + 1, x2 - x1 - 1));
                            std::string u = utf16BeBytesToUtf8(dstB);
                            if (startB.size() == 1) map1Byte[curCode] = u;
                            else map2Byte[curCode] = u;
                            curCode++;
                            m = x2 + 1;
                        }
                    }
                    k = closeBr + 1;
                } else {
                    k = next + 1;
                }
            }
            pos = end + 10;
        }
    }
};

std::string decodePdfLiteralString(const std::string& s, const CMapTable& cmap) {
    std::vector<uint8_t> raw;
    raw.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            if (n == 'n') raw.push_back('\n');
            else if (n == 'r') raw.push_back('\r');
            else if (n == 't') raw.push_back('\t');
            else if (n == 'b') raw.push_back('\b');
            else if (n == 'f') raw.push_back('\f');
            else if (n == '(' || n == ')' || n == '\\') raw.push_back(static_cast<uint8_t>(n));
            else if (n >= '0' && n <= '7') {
                int oct = n - '0';
                for (int d = 0; d < 2 && i + 1 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '7'; ++d) {
                    oct = (oct * 8) + (s[++i] - '0');
                }
                raw.push_back(static_cast<uint8_t>(oct & 0xFF));
            } else {
                raw.push_back(static_cast<uint8_t>(n));
            }
        } else {
            raw.push_back(static_cast<uint8_t>(s[i]));
        }
    }

    if (raw.size() >= 2 && raw[0] == 0xFE && raw[1] == 0xFF) {
        return utf16BeBytesToUtf8(raw);
    }

    std::string out;
    for (uint8_t b : raw) {
        auto it = cmap.map1Byte.find(b);
        if (it != cmap.map1Byte.end()) {
            out += it->second;
        } else {
            appendUtf8Cp(out, b);
        }
    }
    return out;
}

std::string decodePdfHexString(const std::string& hex, const CMapTable& cmap) {
    auto bytes = decodeHexBytes(hex);
    if (bytes.empty()) return "";
    if (bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        return utf16BeBytesToUtf8(bytes);
    }
    if (!cmap.map2Byte.empty() && (bytes.size() % 2 == 0)) {
        std::string mapped;
        bool anyMapped = false;
        for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
            uint32_t code = (static_cast<uint32_t>(bytes[i]) << 8) | bytes[i + 1];
            auto it = cmap.map2Byte.find(code);
            if (it != cmap.map2Byte.end()) {
                mapped += it->second;
                anyMapped = true;
            } else {
                appendUtf8Cp(mapped, code);
            }
        }
        if (anyMapped) return mapped;
    }
    if (!cmap.map1Byte.empty()) {
        std::string mapped;
        for (uint8_t b : bytes) {
            auto it = cmap.map1Byte.find(b);
            if (it != cmap.map1Byte.end()) mapped += it->second;
            else appendUtf8Cp(mapped, b);
        }
        return mapped;
    }
    if (bytes.size() % 2 == 0) {
        return utf16BeBytesToUtf8(bytes);
    }
    std::string out;
    for (uint8_t b : bytes) appendUtf8Cp(out, b);
    return out;
}

std::string extractTextFromContentStream(const std::string& content, const CMapTable& cmap) {
    std::string out;
    size_t pos = 0;
    size_t n = content.size();

    while (pos < n) {
        size_t bt = content.find("BT", pos);
        if (bt == std::string::npos) break;
        if (bt > 0 && std::isalpha(static_cast<unsigned char>(content[bt - 1]))) {
            pos = bt + 2;
            continue;
        }
        if (bt + 2 < n && std::isalpha(static_cast<unsigned char>(content[bt + 2]))) {
            pos = bt + 2;
            continue;
        }
        size_t et = content.find("ET", bt + 2);
        if (et == std::string::npos) et = n;

        std::string block = content.substr(bt + 2, et - (bt + 2));
        size_t i = 0, bn = block.size();
        std::string pendingStr;

        while (i < bn) {
            char c = block[i];
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++i;
                continue;
            }
            if (c == '(') {
                ++i;
                int depth = 1;
                std::string lit;
                while (i < bn && depth > 0) {
                    if (block[i] == '\\' && i + 1 < bn) {
                        lit.push_back(block[i]);
                        lit.push_back(block[i + 1]);
                        i += 2;
                        continue;
                    }
                    if (block[i] == '(') ++depth;
                    else if (block[i] == ')') {
                        --depth;
                        if (depth == 0) { ++i; break; }
                    }
                    lit.push_back(block[i++]);
                }
                pendingStr += decodePdfLiteralString(lit, cmap);
                continue;
            }
            if (c == '<' && (i + 1 >= bn || block[i + 1] != '<')) {
                size_t close = block.find('>', i + 1);
                if (close == std::string::npos) break;
                pendingStr += decodePdfHexString(block.substr(i + 1, close - i - 1), cmap);
                i = close + 1;
                continue;
            }
            if (c == '[') {
                ++i;
                std::string arrText;
                while (i < bn && block[i] != ']') {
                    if (block[i] == '(') {
                        ++i;
                        int depth = 1;
                        std::string lit;
                        while (i < bn && depth > 0) {
                            if (block[i] == '\\' && i + 1 < bn) {
                                lit.push_back(block[i]);
                                lit.push_back(block[i + 1]);
                                i += 2;
                                continue;
                            }
                            if (block[i] == '(') ++depth;
                            else if (block[i] == ')') {
                                --depth;
                                if (depth == 0) { ++i; break; }
                            }
                            lit.push_back(block[i++]);
                        }
                        arrText += decodePdfLiteralString(lit, cmap);
                    } else if (block[i] == '<') {
                        size_t close = block.find('>', i + 1);
                        if (close == std::string::npos) break;
                        arrText += decodePdfHexString(block.substr(i + 1, close - i - 1), cmap);
                        i = close + 1;
                    } else if (block[i] == '-' || std::isdigit(static_cast<unsigned char>(block[i]))) {
                        size_t numStart = i++;
                        while (i < bn && (std::isdigit(static_cast<unsigned char>(block[i])) || block[i] == '.')) ++i;
                        double kern = std::atof(block.substr(numStart, i - numStart).c_str());
                        if (kern < -180.0 && !arrText.empty() && arrText.back() != ' ') {
                            arrText.push_back(' ');
                        }
                    } else {
                        ++i;
                    }
                }
                if (i < bn && block[i] == ']') ++i;
                pendingStr += arrText;
                continue;
            }

            // Read operator or number token
            size_t tokStart = i;
            while (i < bn && !std::isspace(static_cast<unsigned char>(block[i])) &&
                   block[i] != '(' && block[i] != '<' && block[i] != '[' && block[i] != '/') {
                ++i;
            }
            if (i == tokStart) { ++i; continue; }
            std::string op = block.substr(tokStart, i - tokStart);
            if (op == "Tj" || op == "TJ") {
                out += pendingStr;
                pendingStr.clear();
            } else if (op == "'" || op == "\"") {
                out.push_back('\n');
                out += pendingStr;
                pendingStr.clear();
            } else if (op == "T*" || op == "TD" || op == "Td") {
                if (!out.empty() && out.back() != '\n') out.push_back('\n');
                pendingStr.clear();
            } else if (op == "Tf" || op == "Tm" || op == "Tc" || op == "Tw") {
                pendingStr.clear();
            }
        }
        if (!out.empty() && out.back() != '\n') out.push_back('\n');
        pos = (et < n) ? et + 2 : n;
    }

    return out;
}

#ifdef __linux__
typedef int cairo_status_t;
typedef cairo_status_t (*cairo_write_func_t)(void* closure, const unsigned char* data, unsigned int length);

cairo_status_t cairoPngWriteCb(void* closure, const unsigned char* data, unsigned int length) {
    auto* vec = static_cast<std::vector<uint8_t>*>(closure);
    vec->insert(vec->end(), data, data + length);
    return 0;  // CAIRO_STATUS_SUCCESS
}

bool tryRenderPdfWithPopplerLinux(const std::string& filePathUtf8, int maxVisualPages, PdfParseResult& out) {
    void* hPoppler = dlopen("libpoppler-glib.so.8", RTLD_LAZY | RTLD_LOCAL);
    void* hCairo = dlopen("libcairo.so.2", RTLD_LAZY | RTLD_LOCAL);
    void* hGObj = dlopen("libgobject-2.0.so.0", RTLD_LAZY | RTLD_LOCAL);
    void* hGlib = dlopen("libglib-2.0.so.0", RTLD_LAZY | RTLD_LOCAL);
    if (!hPoppler || !hCairo || !hGObj || !hGlib) {
        if (hPoppler) dlclose(hPoppler);
        if (hCairo) dlclose(hCairo);
        if (hGObj) dlclose(hGObj);
        if (hGlib) dlclose(hGlib);
        return false;
    }

    using DocNewFn = void* (*)(const char*, const char*, void**);
    using DocNPagesFn = int (*)(void*);
    using DocTitleFn = char* (*)(void*);
    using DocGetPageFn = void* (*)(void*, int);
    using PageSizeFn = void (*)(void*, double*, double*);
    using PageRenderFn = void (*)(void*, void*);
    using PageTextFn = char* (*)(void*);

    using SurfCreateFn = void* (*)(int, int, int);
    using SurfDestroyFn = void (*)(void*);
    using SurfWritePngFn = cairo_status_t (*)(void*, cairo_write_func_t, void*);
    using CairoCreateFn = void* (*)(void*);
    using CairoDestroyFn = void (*)(void*);
    using CairoScaleFn = void (*)(void*, double, double);
    using CairoSetRgbFn = void (*)(void*, double, double, double);
    using CairoPaintFn = void (*)(void*);
    using GUnrefFn = void (*)(void*);
    using GFreeFn = void (*)(void*);

    auto docNew = reinterpret_cast<DocNewFn>(dlsym(hPoppler, "poppler_document_new_from_file"));
    auto docNPages = reinterpret_cast<DocNPagesFn>(dlsym(hPoppler, "poppler_document_get_n_pages"));
    auto docTitle = reinterpret_cast<DocTitleFn>(dlsym(hPoppler, "poppler_document_get_title"));
    auto docGetPage = reinterpret_cast<DocGetPageFn>(dlsym(hPoppler, "poppler_document_get_page"));
    auto pageSize = reinterpret_cast<PageSizeFn>(dlsym(hPoppler, "poppler_page_get_size"));
    auto pageRender = reinterpret_cast<PageRenderFn>(dlsym(hPoppler, "poppler_page_render"));
    auto pageText = reinterpret_cast<PageTextFn>(dlsym(hPoppler, "poppler_page_get_text"));

    auto surfCreate = reinterpret_cast<SurfCreateFn>(dlsym(hCairo, "cairo_image_surface_create"));
    auto surfDestroy = reinterpret_cast<SurfDestroyFn>(dlsym(hCairo, "cairo_surface_destroy"));
    auto surfWritePng = reinterpret_cast<SurfWritePngFn>(dlsym(hCairo, "cairo_surface_write_to_png_stream"));
    auto crCreate = reinterpret_cast<CairoCreateFn>(dlsym(hCairo, "cairo_create"));
    auto crDestroy = reinterpret_cast<CairoDestroyFn>(dlsym(hCairo, "cairo_destroy"));
    auto crScale = reinterpret_cast<CairoScaleFn>(dlsym(hCairo, "cairo_scale"));
    auto crSetRgb = reinterpret_cast<CairoSetRgbFn>(dlsym(hCairo, "cairo_set_source_rgb"));
    auto crPaint = reinterpret_cast<CairoPaintFn>(dlsym(hCairo, "cairo_paint"));
    auto gUnref = reinterpret_cast<GUnrefFn>(dlsym(hGObj, "g_object_unref"));
    auto gFree = reinterpret_cast<GFreeFn>(dlsym(hGlib, "g_free"));

    if (!docNew || !docNPages || !docGetPage || !pageSize || !pageRender || !pageText ||
        !surfCreate || !surfDestroy || !surfWritePng || !crCreate || !crDestroy ||
        !crScale || !crSetRgb || !crPaint || !gUnref || !gFree) {
        return false;
    }

    std::string uri = fs::pathToFileUrl(filePathUtf8);
    void* gerr = nullptr;
    void* doc = docNew(uri.c_str(), nullptr, &gerr);
    if (!doc) return false;

    out.title = fs::getFileName(filePathUtf8);
    if (docTitle) {
        char* t = docTitle(doc);
        if (t) {
            if (*t) out.title = t;
            gFree(t);
        }
    }

    int nPages = docNPages(doc);
    out.totalPages = nPages;
    out.pages.clear();

    for (int i = 0; i < nPages; ++i) {
        void* page = docGetPage(doc, i);
        if (!page) continue;

        PdfPageContent pc;
        pc.pageNumber = i + 1;

        char* txt = pageText(page);
        if (txt) {
            pc.textUtf8 = txt;
            gFree(txt);
        }

        if (i < maxVisualPages) {
            double w = 595.0, h = 842.0;
            pageSize(page, &w, &h);
            const double scale = 1.65;
            int imgW = std::max(1, static_cast<int>(w * scale));
            int imgH = std::max(1, static_cast<int>(h * scale));
            // CAIRO_FORMAT_ARGB32 == 0
            void* surf = surfCreate(0, imgW, imgH);
            if (surf) {
                void* cr = crCreate(surf);
                if (cr) {
                    crSetRgb(cr, 1.0, 1.0, 1.0);
                    crPaint(cr);
                    crScale(cr, scale, scale);
                    pageRender(page, cr);
                    crDestroy(cr);

                    std::vector<uint8_t> pngBytes;
                    pngBytes.reserve(65536);
                    if (surfWritePng(surf, &cairoPngWriteCb, &pngBytes) == 0 && !pngBytes.empty()) {
                        pc.pngBase64 = fs::base64Encode(pngBytes.data(), pngBytes.size());
                        out.hasRenderedImages = true;
                    }
                }
                surfDestroy(surf);
            }
        }

        out.pages.push_back(std::move(pc));
        gUnref(page);
    }

    gUnref(doc);
    out.ok = true;
    return true;
}
#endif

}  // namespace

PdfParseResult parsePdfPortable(const std::string& filePathUtf8) {
    PdfParseResult res;
    res.title = fs::getFileName(filePathUtf8);

    std::vector<uint8_t> bytes;
    if (!fs::readFileBytes(filePathUtf8, bytes) || bytes.size() < 8) {
        res.errorMessage = "تعذّر قراءة ملف PDF من القرص.";
        return res;
    }

    std::string raw(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (raw.find("%PDF-") == std::string::npos) {
        res.errorMessage = "الملف المختار ليس ملف PDF صالحاً.";
        return res;
    }

    struct RawStream {
        std::string dictHeader;
        std::vector<uint8_t> decoded;
        bool isJpeg = false;
        std::vector<uint8_t> rawBytes;
    };

    std::vector<RawStream> streams;
    CMapTable cmap;

    // Count /Type /Page (not /Pages)
    int pageTypeCount = 0;
    {
        size_t p = 0;
        while ((p = raw.find("/Type", p)) != std::string::npos) {
            size_t q = p + 5;
            while (q < raw.size() && std::isspace(static_cast<unsigned char>(raw[q]))) ++q;
            if (raw.compare(q, 5, "/Page") == 0 &&
                (q + 5 >= raw.size() || !std::isalpha(static_cast<unsigned char>(raw[q + 5])))) {
                ++pageTypeCount;
            }
            p = q;
        }
    }

    // Extract streams
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t sIdx = raw.find("stream", pos);
        if (sIdx == std::string::npos) break;
        if (sIdx >= 3 && raw.compare(sIdx - 3, 9, "endstream") == 0) {
            pos = sIdx + 6;
            continue;
        }
        size_t dataStart = sIdx + 6;
        if (dataStart < raw.size() && raw[dataStart] == '\r') ++dataStart;
        if (dataStart < raw.size() && raw[dataStart] == '\n') ++dataStart;
        else if (dataStart == sIdx + 6) {
            pos = sIdx + 6;
            continue;
        }

        size_t endIdx = raw.find("endstream", dataStart);
        if (endIdx == std::string::npos) break;

        size_t dictStart = (sIdx > 512) ? (sIdx - 512) : 0;
        size_t dOpen = raw.rfind("<<", sIdx);
        if (dOpen != std::string::npos && dOpen >= dictStart) dictStart = dOpen;
        std::string dict = raw.substr(dictStart, sIdx - dictStart);

        size_t streamLen = endIdx - dataStart;
        while (streamLen > 0 && (bytes[dataStart + streamLen - 1] == '\n' || bytes[dataStart + streamLen - 1] == '\r')) {
            --streamLen;
        }

        RawStream rs;
        rs.dictHeader = dict;
        if (dict.find("/DCTDecode") != std::string::npos && streamLen <= 4 * 1024 * 1024) {
            rs.isJpeg = true;
            rs.rawBytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(dataStart),
                               bytes.begin() + static_cast<std::ptrdiff_t>(dataStart + streamLen));
            streams.push_back(std::move(rs));
        } else if (dict.find("/FlateDecode") != std::string::npos) {
            std::vector<uint8_t> inflated;
            if (compression::inflateZlibOrRaw(bytes.data() + dataStart, streamLen, inflated, 32 * 1024 * 1024)) {
                std::string asStr(reinterpret_cast<const char*>(inflated.data()), inflated.size());
                if (asStr.find("beginbfchar") != std::string::npos || asStr.find("beginbfrange") != std::string::npos) {
                    cmap.parseCMapStream(asStr);
                } else {
                    rs.decoded = std::move(inflated);
                    streams.push_back(std::move(rs));
                }
            }
        } else if (dict.find("/Filter") == std::string::npos && streamLen < 4 * 1024 * 1024) {
            std::string asStr(reinterpret_cast<const char*>(bytes.data() + dataStart), streamLen);
            if (asStr.find("beginbfchar") != std::string::npos || asStr.find("beginbfrange") != std::string::npos) {
                cmap.parseCMapStream(asStr);
            } else if (asStr.find("BT") != std::string::npos) {
                rs.decoded.assign(bytes.begin() + static_cast<std::ptrdiff_t>(dataStart),
                                  bytes.begin() + static_cast<std::ptrdiff_t>(dataStart + streamLen));
                streams.push_back(std::move(rs));
            }
        }

        pos = endIdx + 9;
    }

    int pageIdx = 1;
    std::vector<std::string> pendingJpegs;
    for (const auto& st : streams) {
        if (st.isJpeg && !st.rawBytes.empty()) {
            pendingJpegs.push_back(fs::base64Encode(st.rawBytes.data(), st.rawBytes.size()));
            continue;
        }
        if (st.decoded.empty()) continue;
        std::string content(reinterpret_cast<const char*>(st.decoded.data()), st.decoded.size());
        if (content.find("BT") == std::string::npos) continue;

        std::string txt = fs::trim(extractTextFromContentStream(content, cmap));
        if (!txt.empty() || !pendingJpegs.empty()) {
            PdfPageContent pc;
            pc.pageNumber = pageIdx++;
            pc.textUtf8 = txt;
            pc.jpegImagesBase64 = std::move(pendingJpegs);
            pendingJpegs.clear();
            res.pages.push_back(std::move(pc));
        }
    }

    if (!pendingJpegs.empty()) {
        PdfPageContent pc;
        pc.pageNumber = pageIdx++;
        pc.jpegImagesBase64 = std::move(pendingJpegs);
        res.pages.push_back(std::move(pc));
    }

    if (res.pages.empty()) {
        PdfPageContent emptyPage;
        emptyPage.pageNumber = 1;
        res.pages.push_back(emptyPage);
    }

    res.totalPages = std::max(pageTypeCount, static_cast<int>(res.pages.size()));
    res.ok = true;
    return res;
}

PdfParseResult parsePdfFile(const std::string& filePathUtf8, int maxVisualPages) {
#ifdef __linux__
    PdfParseResult popplerRes;
    if (tryRenderPdfWithPopplerLinux(filePathUtf8, maxVisualPages, popplerRes)) {
        return popplerRes;
    }
#else
    (void)maxVisualPages;
#endif
    return parsePdfPortable(filePathUtf8);
}

}  // namespace litebrowser


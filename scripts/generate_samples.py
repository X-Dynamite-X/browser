#!/usr/bin/env python3
"""Generates sample PDF, DOCX, legacy DOC (OLE2), and RTF documents in samples/ for testing LiteBrowser."""

import os
import struct
import zipfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAMPLES_DIR = os.path.join(ROOT, "samples")


def make_tiny_png() -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", 16, 16, 8, 2, 0, 0, 0)
    rows = b""
    for y in range(16):
        rows += b"\x00" + (b"\x02\x84\xc7" * 16 if y < 8 else b"\x10\xb9\x81" * 16)
    idat = zlib.compress(rows)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")


def create_sample_docx(path: str) -> None:
    content_types = """<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Default Extension="png" ContentType="image/png"/>
  <Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
  <Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>
</Types>"""

    root_rels = """<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml"/>
</Relationships>"""

    core_xml = """<?xml version="1.0" encoding="UTF-8"?>
<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties"
                   xmlns:dc="http://purl.org/dc/elements/1.1/">
  <dc:title>دليل متصفح النور الخفيف</dc:title>
</cp:coreProperties>"""

    doc_rels = """<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rIdImg1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="media/banner.png"/>
  <Relationship Id="rIdLink1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink" Target="https://ar.wikipedia.org" TargetMode="External"/>
</Relationships>"""

    document_xml = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"
            xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"
            xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">
  <w:body>
    <w:p>
      <w:pPr><w:pStyle w:val="Heading1"/><w:bidi/></w:pPr>
      <w:r><w:rPr><w:b/><w:color w:val="0284C7"/></w:rPr><w:t>مرحباً بك في القارئ المدمج لملفات Word</w:t></w:r>
    </w:p>
    <w:p>
      <w:pPr><w:bidi/></w:pPr>
      <w:r><w:t>هذا مستند تجريبي بصيغة </w:t></w:r>
      <w:r><w:rPr><w:b/></w:rPr><w:t>DOCX</w:t></w:r>
      <w:r><w:t> يحتوي على نصوص عربية وإنجليزية، جداول، روابط، وصور مضمّنة تعمل بالكامل بدون برامج خارجية.</w:t></w:r>
    </w:p>
    <w:p>
      <w:pPr><w:numPr><w:ilvl w:val="0"/></w:numPr><w:bidi/></w:pPr>
      <w:r><w:t>يعمل بسرعة عالية على Linux و Windows XP و Windows 7/8/10.</w:t></w:r>
    </w:p>
    <w:p>
      <w:r><a:blip r:embed="rIdImg1"/></w:r>
    </w:p>
    <w:tbl>
      <w:tr>
        <w:tc><w:p><w:r><w:rPr><w:b/></w:rPr><w:t>النظام</w:t></w:r></w:p></w:tc>
        <w:tc><w:p><w:r><w:rPr><w:b/></w:rPr><w:t>الحالة</w:t></w:r></w:p></w:tc>
      </w:tr>
      <w:tr>
        <w:tc><w:p><w:r><w:t>Windows XP / 7 / 8 / 10</w:t></w:r></w:p></w:tc>
        <w:tc><w:p><w:r><w:t>مدعوم بالكامل</w:t></w:r></w:p></w:tc>
      </w:tr>
      <w:tr>
        <w:tc><w:p><w:r><w:t>Linux (GTK3 + WebKit2)</w:t></w:r></w:p></w:tc>
        <w:tc><w:p><w:r><w:t>مدعوم بالكامل</w:t></w:r></w:p></w:tc>
      </w:tr>
    </w:tbl>
    <w:p>
      <w:pPr><w:bidi/></w:pPr>
      <w:hyperlink r:id="rIdLink1"><w:r><w:rPr><w:u w:val="single"/></w:rPr><w:t>زيارة ويكيبيديا العربية</w:t></w:r></w:hyperlink>
    </w:p>
  </w:body>
</w:document>"""

    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("[Content_Types].xml", content_types)
        zf.writestr("_rels/.rels", root_rels)
        zf.writestr("docProps/core.xml", core_xml)
        zf.writestr("word/_rels/document.xml.rels", doc_rels)
        zf.writestr("word/document.xml", document_xml)
        zf.writestr("word/media/banner.png", make_tiny_png())


def create_sample_legacy_doc(path: str) -> None:
    """Builds a minimal valid OLE2 Compound File Binary (.doc) with FIB + 1Table Piece Table."""
    text = "مستند Word 97-2003 تجريبي لنظام Windows XP\rيدعم قراءة النصوص العربية والإنجليزية (Legacy OLE2 DOC).\r"
    text_utf16 = text.encode("utf-16le")
    num_chars = len(text_utf16) // 2

    # Build WordDocument stream (FIB + text at offset 0x800, >= 4096 bytes so stored in regular FAT sectors)
    fib = bytearray(0x800)
    struct.pack_into("<HH", fib, 0x00, 0xA5EC, 0x00C1)  # wIdent, nFib (Word 97+)
    struct.pack_into("<H", fib, 0x0A, 0x0200)           # fWhichTblStm = 1 -> "1Table"
    pos = 0x20
    csw = 14
    struct.pack_into("<H", fib, pos, csw)
    pos += 2 + csw * 2
    cslw = 22
    struct.pack_into("<H", fib, pos, cslw)
    struct.pack_into("<I", fib, pos + 2 + 12, num_chars)  # ccpText
    pos += 2 + cslw * 4
    cb_rg = 93
    struct.pack_into("<H", fib, pos, cb_rg)
    blob_start = pos + 2

    # Build CLX in 1Table stream (>= 4096 bytes so also in regular FAT sectors)
    # Pcdt: clxt=0x02, lcb=16 (2 CPs * 4B + 1 Pcd * 8B = 16)
    clx = struct.pack("<BI II HIH", 0x02, 16, 0, num_chars, 0, 0x800, 0)
    struct.pack_into("<II", fib, blob_start + 33 * 8, 0, len(clx))  # fcClx=0, lcbClx=len(clx)

    word_stream = bytes(fib) + text_utf16
    if len(word_stream) < 4096:
        word_stream += b"\x00" * (4096 - len(word_stream))
    word_stream += b"\x00" * ((-len(word_stream)) % 512)

    table_stream = clx + b"\x00" * (4096 - len(clx))
    table_stream += b"\x00" * ((-len(table_stream)) % 512)

    ws_sects = len(word_stream) // 512
    ts_sects = len(table_stream) // 512
    # Layout sectors:
    # 0 .. ws_sects-1: WordDocument
    # ws_sects .. ws_sects+ts_sects-1: 1Table
    # dir_sect = ws_sects + ts_sects (1 sector)
    # fat_sect = dir_sect + 1 (1 sector)
    dir_sect = ws_sects + ts_sects
    fat_sect = dir_sect + 1

    def make_dir_entry(name: str, entry_type: int, start_sect: int, size: int, left=0xFFFFFFFF, right=0xFFFFFFFF, child=0xFFFFFFFF) -> bytes:
        ent = bytearray(128)
        name_u16 = name.encode("utf-16le") + b"\x00\x00"
        ent[0:len(name_u16)] = name_u16
        struct.pack_into("<HBBIII", ent, 0x40, len(name_u16), entry_type, 1, left, right, child)
        struct.pack_into("<II", ent, 0x74, start_sect, size)
        return bytes(ent)

    dir_data = (
        make_dir_entry("Root Entry", 5, 0xFFFFFFFE, 0, child=1) +
        make_dir_entry("WordDocument", 2, 0, len(word_stream), right=2) +
        make_dir_entry("1Table", 2, ws_sects, len(table_stream)) +
        bytes(128)
    )

    fat_entries = []
    for i in range(ws_sects):
        fat_entries.append(i + 1 if i + 1 < ws_sects else 0xFFFFFFFE)
    for i in range(ts_sects):
        s = ws_sects + i
        fat_entries.append(s + 1 if i + 1 < ts_sects else 0xFFFFFFFE)
    fat_entries.append(0xFFFFFFFE)  # dir_sect
    fat_entries.append(0xFFFFFFFD)  # fat_sect (FATSECT)
    while len(fat_entries) < 128:
        fat_entries.append(0xFFFFFFFF)
    fat_sector = struct.pack("<128I", *fat_entries)

    header = bytearray(512)
    header[0:8] = b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1"
    struct.pack_into("<HH", header, 0x18, 0x003E, 0x0003)
    struct.pack_into("<HH", header, 0x1C, 0xFFFE, 9)   # uSectorShift = 9 (512B)
    struct.pack_into("<H", header, 0x20, 6)            # uMiniSectorShift = 6 (64B)
    struct.pack_into("<III", header, 0x2C, 1, dir_sect, 0)
    struct.pack_into("<IIIII", header, 0x38, 4096, 0xFFFFFFFE, 0, 0xFFFFFFFE, 0)
    for k in range(109):
        struct.pack_into("<I", header, 0x4C + k * 4, fat_sect if k == 0 else 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(header)
        f.write(word_stream)
        f.write(table_stream)
        f.write(dir_data)
        f.write(fat_sector)


def create_sample_rtf(path: str) -> None:
    ar_line = "مرحباً من ملف RTF الخفيف!"
    u_escapes = "".join(f"\\u{ord(c)}?" for c in ar_line)
    rtf = "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Tahoma;}}\\uc1\\pard " + u_escapes + "\\par Sample RTF Document ready.\\par}"
    with open(path, "wb") as f:
        f.write(rtf.encode("ascii"))


def create_sample_pdf(path: str) -> None:
    content_p1 = b"BT /F1 18 Tf 50 760 Td (LiteBrowser Built-in PDF Reader) Tj 0 -30 Td (Fast lightweight C++ browser for Linux & Windows XP/7/8/10) Tj ET"
    comp_p1 = zlib.compress(content_p1)

    # Page 2 uses UTF-16BE hex string for Arabic text + English text
    ar_utf16be_hex = (b"\xfe\xff" + "قراءة ملفات PDF باللغة العربية بوضوح وسرعة".encode("utf-16-be")).hex().upper().encode("ascii")
    content_p2 = b"BT /F1 16 Tf 50 760 Td <" + ar_utf16be_hex + b"> Tj 0 -28 Td (Page 2 - Offline & Portable PDF Engine) Tj ET"
    comp_p2 = zlib.compress(content_p2)

    objs = [
        b"1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n",
        b"2 0 obj\n<< /Type /Pages /Kids [3 0 R 5 0 R] /Count 2 >>\nendobj\n",
        b"3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 7 0 R >> >> /Contents 4 0 R >>\nendobj\n",
        f"4 0 obj\n<< /Length {len(comp_p1)} /Filter /FlateDecode >>\nstream\n".encode("ascii") + comp_p1 + b"\nendstream\nendobj\n",
        b"5 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 7 0 R >> >> /Contents 6 0 R >>\nendobj\n",
        f"6 0 obj\n<< /Length {len(comp_p2)} /Filter /FlateDecode >>\nstream\n".encode("ascii") + comp_p2 + b"\nendstream\nendobj\n",
        b"7 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n",
    ]

    pdf = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for obj in objs:
        offsets.append(len(pdf))
        pdf.extend(obj)

    xref_pos = len(pdf)
    pdf.extend(f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode("ascii"))
    for off in offsets[1:]:
        pdf.extend(f"{off:010d} 00000 n \n".encode("ascii"))
    pdf.extend(
        f"trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref_pos}\n%%EOF\n".encode("ascii")
    )

    with open(path, "wb") as f:
        f.write(pdf)


def main() -> None:
    os.makedirs(SAMPLES_DIR, exist_ok=True)
    create_sample_docx(os.path.join(SAMPLES_DIR, "arabic_guide.docx"))
    create_sample_legacy_doc(os.path.join(SAMPLES_DIR, "legacy_xp_note.doc"))
    create_sample_rtf(os.path.join(SAMPLES_DIR, "sample_article.rtf"))
    create_sample_pdf(os.path.join(SAMPLES_DIR, "sample_handbook.pdf"))
    print("Generated sample files in:", SAMPLES_DIR)


if __name__ == "__main__":
    main()


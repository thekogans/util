// Copyright 2011 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_util.
//
// libthekogans_util is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_util is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_util. If not, see <http://www.gnu.org/licenses/>.

#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        namespace {
            struct RawHeader {
                ui32 magic = 0;
                Flags32 flags = 0;
                TransactedFile::SizeType size = 0;
            };

            bool PeekRawHeader (
                    TransactedFile *file,
                    TransactedFile::AddressType off,
                    RawHeader &out) noexcept {
                try {
                    const TransactedFile::AddressType fileSize = file->GetSizeEx ();
                    if (off + TransactedFile::Block::HEADER_SIZE > fileSize) {
                        return false;
                    }
                    TransactedFile::Range range (file, off, TransactedFile::Block::HEADER_SIZE);
                    range >> out.magic >> out.flags >> out.size;
                    return out.magic == MAGIC32;
                }
                catch (...) {
                    return false;
                }
            }

            void DumpSpan (
                    TransactedFile *file,
                    TransactedFile::AddressType start,
                    TransactedFile::AddressType length,
                    std::size_t maxBytes,
                std::string &out) noexcept {
                try {
                    const TransactedFile::AddressType fileSize = file->GetSizeEx ();
                    if (start >= fileSize || length == 0) {
                        return;
                    }
                    length = std::min (length, fileSize - start);
                    if (length <= maxBytes) {
                        TransactedFile::Range range (file, start, length);
                        if (range.GetDataAvailable () > 0) {
                            out += FormatString ("@ 0x" THEKOGANS_UTIL_UI64_HEX_FORMAT ":\n", start);
                            out += HexFormatBuffer (range.GetDataPtr (), range.GetDataAvailable (), false);
                        }
                    }
                    else {
                        const std::size_t half = maxBytes / 2;
                        {
                            TransactedFile::Range head (file, start, half);
                            if (head.GetDataAvailable () > 0) {
                                out += FormatString ("@ 0x" THEKOGANS_UTIL_UI64_HEX_FORMAT " (head):\n", start);
                                out += HexFormatBuffer (head.GetDataPtr (), head.GetDataAvailable (), false);
                            }
                        }
                        out += FormatString (
                            "... (" THEKOGANS_UTIL_UI64_FORMAT " bytes omitted) ...\n",
                            length - maxBytes);
                        {
                            const TransactedFile::AddressType tailStart = start + length - half;
                            TransactedFile::Range tail (file, tailStart, half);
                            if (tail.GetDataAvailable () > 0) {
                                out += FormatString ("@ 0x" THEKOGANS_UTIL_UI64_HEX_FORMAT " (tail):\n", tailStart);
                                out += HexFormatBuffer (tail.GetDataPtr (), tail.GetDataAvailable (), false);
                            }
                        }
                    }
                }
                catch (...) {
                    out += "<truncated/unreadable range>\n";
                }
            }

            std::string BuildCorruptionContext (
                    TransactedFile *file,
                    TransactedFile::AddressType offset,
                    bool isHeader) noexcept {
                constexpr std::size_t HSIZE = TransactedFile::Block::HEADER_SIZE;
                constexpr std::size_t MAX_BLOCK_DUMP = 512; // 256 head + 256 tail when clamped
                constexpr std::size_t FALLBACK_WINDOW = 64;

                std::string out;
                try {
                    const TransactedFile::AddressType fileSize = file->GetSizeEx ();
                    const TransactedFile::AddressType heapStart =
                        TransactedFile::Header::BLOCK_OFFSET - HSIZE;

                    out += FormatString (
                        "\n[FileSize: " THEKOGANS_UTIL_UI64_FORMAT
                        ", Phase: %s, CorruptBookend: %s @ 0x" THEKOGANS_UTIL_UI64_HEX_FORMAT "]\n",
                        fileSize,
                        TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()],
                        isHeader ? "Header" : "Footer",
                        offset);

                    if (!isHeader) {
                        out += "--- CURRENT BLOCK (Footer corrupt) ---\n";
                        const TransactedFile::AddressType scanStart =
                            offset > MAX_BLOCK_DUMP ? std::max (heapStart, offset - MAX_BLOCK_DUMP) : heapStart;
                        DumpSpan (file, scanStart, (offset + HSIZE) - scanStart, MAX_BLOCK_DUMP, out);

                        const TransactedFile::AddressType nextLead = offset + HSIZE;
                        RawHeader nextHdr;
                        if (PeekRawHeader (file, nextLead, nextHdr)) {
                            out += FormatString (
                                "--- NEXT BLOCK (flags: 0x%x, size: " THEKOGANS_UTIL_UI64_FORMAT ") ---\n",
                                nextHdr.flags.flags, nextHdr.size);
                            DumpSpan (file, nextLead, nextHdr.size + 2 * HSIZE, MAX_BLOCK_DUMP, out);
                        }
                        else {
                            out += "--- NEXT BLOCK (Unreadable/Corrupt) ---\n";
                            DumpSpan (file, nextLead, FALLBACK_WINDOW, FALLBACK_WINDOW, out);
                        }
                        return out;
                    }

                    // isHeader == true: offset is the corrupt leading header of CURRENT
                    out += "--- PREV BLOCK ---\n";
                    RawHeader prevFoot;
                    if (offset >= heapStart + 2 * HSIZE && PeekRawHeader (file, offset - HSIZE, prevFoot)) {
                        const TransactedFile::AddressType prevTotal = prevFoot.size + 2 * HSIZE;
                        if (offset >= heapStart + prevTotal) {
                            const TransactedFile::AddressType prevLead = offset - prevTotal;
                            out += FormatString (
                                "(flags: 0x%x, size: " THEKOGANS_UTIL_UI64_FORMAT ")\n",
                                prevFoot.flags.flags, prevFoot.size);
                            DumpSpan (file, prevLead, prevTotal, MAX_BLOCK_DUMP, out);
                        }
                        else {
                            DumpSpan (file, heapStart, offset - heapStart, FALLBACK_WINDOW, out);
                        }
                    }
                    else {
                        const TransactedFile::AddressType rawStart =
                            offset > FALLBACK_WINDOW ? std::max (heapStart, offset - FALLBACK_WINDOW) : heapStart;
                        DumpSpan (file, rawStart, offset - rawStart, FALLBACK_WINDOW, out);
                    }

                    out += "--- CURRENT BLOCK (Header corrupt) ---\n";
                    DumpSpan (file, offset, MAX_BLOCK_DUMP, MAX_BLOCK_DUMP, out);

                    // Probe forward for the next surviving SRAF bookend
                    const TransactedFile::AddressType scanLimit =
                        std::min (fileSize, offset + MAX_BLOCK_DUMP);
                    for (TransactedFile::AddressType probe = offset + HSIZE;
                         probe + HSIZE <= scanLimit;
                         ++probe) {
                        RawHeader probeHdr;
                        if (PeekRawHeader (file, probe, probeHdr)) {
                            out += FormatString (
                                "--- NEXT VALID BOOKEND @ 0x" THEKOGANS_UTIL_UI64_HEX_FORMAT
                                " (flags: 0x%x, size: " THEKOGANS_UTIL_UI64_FORMAT ") ---\n",
                                probe, probeHdr.flags.flags, probeHdr.size);
                            DumpSpan (file, probe, probeHdr.size + 2 * HSIZE, MAX_BLOCK_DUMP, out);
                            break;
                        }
                    }
                }
                catch (...) {
                    out += "<exception while formatting corruption context>\n";
                }
                return out;
            }
        }

        void TransactedFile::Block::Header::Read (
                TransactedFile *file,
                AddressType offset,
                bool header,
                ui32 expectedMagic) {
            Range range (file, offset, SIZE);
            ui32 magic;
            range >> magic;
            if (THEKOGANS_UTIL_LIKELY (magic == expectedMagic)) {
                range >> flags >> size;
            }
            else {
                std::cout << BuildCorruptionContext (file, offset, header);
                THEKOGANS_UTIL_DEBUG_BREAK
                throw Exception (
                    header ? Exception::CORRUPT_HEADER : Exception::CORRUPT_FOOTER,
                    FormatString (
                        "Corrupt TransactedFile::Block::%s @" THEKOGANS_UTIL_UI64_FORMAT,
                        header ? "Header" : "Footer",
                        offset).c_str (),
                    offset,
                    magic);
            }
        }

        void TransactedFile::Block::Header::Write (
                TransactedFile *file,
                AddressType offset,
                ui32 magic) const {
            Range range (file, offset, SIZE, false);
            range << magic << flags << size;
        }

        inline bool operator != (
                const TransactedFile::Block::Header &header,
                const TransactedFile::Block::Header &footer) {
            return header.flags != footer.flags || header.size != footer.size;
        }

        ui64 TransactedFile::Block::GetSize (
                TransactedFile *file,
                AddressType offset) {
            Block block (file, offset);
            block.Read ();
            return block.GetSize ();
        }

        bool TransactedFile::Block::Prev (
                Block &prev,
                ui32 expectedMagic) const {
            if (THEKOGANS_UTIL_LIKELY (!IsFirst ())) {
                Header footer;
                footer.Read (file, offset - SIZE, false, expectedMagic);
                prev.offset = offset - SIZE - footer.size;
                prev.header.Read (file, prev.offset - HEADER_SIZE, true, expectedMagic);
                if (THEKOGANS_UTIL_UNLIKELY (prev.header != footer)) {
                    throw Exception (
                        Exception::HEADER_FOOTER_MISMATCH,
                        FormatString (
                            "Corrupt TransactedFile::Block @" THEKOGANS_UTIL_UI64_FORMAT "\n"
                            " prev.header.flags: %x prev.header.size: " THEKOGANS_UTIL_UI64_FORMAT "\n"
                            " prev.footer.flags: %x prev.footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                            prev.offset,
                            prev.header.flags.flags, prev.header.size,
                            footer.flags.flags, footer.size).c_str (),
                        offset, prev.header, footer);
                }
                return true;
            }
            return false;
        }

        bool TransactedFile::Block::Next (
                Block &next,
                ui32 expectedMagic) const {
            if (THEKOGANS_UTIL_LIKELY (!IsLast ())) {
                next.offset = offset + header.size + SIZE;
                next.Read (expectedMagic);
                return true;
            }
            return false;
        }

        void TransactedFile::Block::Read (ui32 expectedMagic) {
            header.Read (file, offset - HEADER_SIZE, true, expectedMagic);
            Header footer;
            footer.Read (file, offset + header.size, false, expectedMagic);
            if (THEKOGANS_UTIL_UNLIKELY (header != footer)) {
                throw Exception (
                    Exception::HEADER_FOOTER_MISMATCH,
                    FormatString (
                        "Corrupt TransactedFile::Block @" THEKOGANS_UTIL_UI64_FORMAT "\n"
                        " header.flags: %x header.size: " THEKOGANS_UTIL_UI64_FORMAT "\n"
                        " footer.flags: %x footer.size: " THEKOGANS_UTIL_UI64_FORMAT,
                        offset,
                        header.flags.flags, header.size,
                        footer.flags.flags, footer.size).c_str (),
                    offset, header, footer);
            }
        }

        void TransactedFile::Block::Write (ui32 magic) const {
            header.Write (file, offset - HEADER_SIZE);
            header.Write (file, offset + header.size);
        }

        void TransactedFile::Block::Clear (char zero) const {
            {
                Range range (file, offset - HEADER_SIZE, HEADER_SIZE, false);
                range.Seek (SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable (), zero), SEEK_CUR);
            }
            {
                Range range (file, offset + header.size, HEADER_SIZE, false);
                range.Seek (SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable (), zero), SEEK_CUR);
            }
        }

    } // namespace util
} // namespace thekogans

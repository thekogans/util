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

#if defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)
    /// \struct TransactedFile::Stats TransactedFileRange.h thekogans/util/TransactedFileRange.h
    ///
    /// \brief
    /// Stats should be used during system integration and tuning. Every time
    /// a \see{Range} is created it bumps up the appropriate (based on reading)
    /// counter in it's ctor. It also bumps up an appropriate *Owner* counter if
    /// it happens to straddle a \see{TransactedFile::Buffer} boundary. If the
    /// ratio of *Owner* counter and range counter approaches 1 then you have some
    /// tuning to do.
    /// In an ideal world, no range would ever cross a buffer boundary and you would
    /// always have the best performing reads and writes. When a range does cross a
    /// buffer boundary it needs to allocate a local buffer to satisfy the fact
    /// range reads and writes do no boundary checking (hence the performance boost).
    /// If a large percentage of your ranges have to allocate the buffer it means
    /// that \see{TransactedFile::Buffer::SIZE} size is not properly tuned for your
    /// application. Follow the instructions in \see{TransactedFile::Buffer::SIZE}
    /// to change the page size.
    struct Stats {
        /// \brief
        /// A count of \see{Range} (reading == true) that have been created for this file.
        std::atomic<ui64> readingRanges;
        /// \brief
        /// A count of the above ranges that needed to allocate their own buffer.
        std::atomic<ui64> ownerReadingRanges;
        /// \brief
        /// A count of \see{Range} (reading == false) that have been created for this file.
        std::atomic<ui64> writingRanges;
        /// \brief
        /// A count of the above ranges that needed to allocate their own buffer.
        std::atomic<ui64> ownerWritingRanges;

        /// \brief
        /// ctor.
        Stats () :
            readingRanges (0),
            ownerReadingRanges (0),
            writingRanges (0),
            ownerWritingRanges (0) {}
    } stats;
#endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)

/// \struct TransactedFile::Range TransactedFileRange.h thekogans/util/TransactedFileRange.h
///
/// \brief
/// Range provides direct access to TransactedFile's underlying \see{Buffer}.
/// By pairing it with \see{RandomSeekSerializer}, Range provides serialization/
/// deserialization capabilities without the need to copy chunks of data
/// in to and out of the buffers resulting in better performance. Because
/// the file's 64 bit address space is chunked in to hierarchical pages,
/// if the requested range straddles a page boundary, a range buffer is
/// allocated to gurantee sequential access. Use \see{Stats} to tune the
/// \see{Buffer::SIZE} and \see{Buffer::SHIFT_COUNT}. Because range maintains
/// it's own set of state variables in to the file, if you create nonoveralapping
/// ranges, you can access the file from multiple threads without the need
/// for synchronization.
/// IMPORTANT: Range does no bounds checking on it's inputs. That's what
/// \see{SafeRange} (below) is for.
struct _LIB_THEKOGANS_UTIL_DECL Range : public RandomSeekSerializer {
protected:
    /// \brief
    /// \see{TransactedFile} the range is referring too.
    TransactedFile &file;
    /// \brief
    /// Begining of range.
    ui64 offset;
    /// \brief
    /// Length of range.
    std::size_t length;
    /// \brief
    /// true == range is for reading, false == range is for writing.
    bool reading;
    /// \brief
    /// If the provided range straddles a page boundary
    /// use this allocator to allocate a range buffer.
    util::Allocator::SharedPtr allocator;
    /// \brief
    /// Either a pointer in to \see{TransactedFile::Buffer::data} or self
    /// allocated range buffer.
    ui8 *data;
    /// \brief
    /// Mainains current position in the range.
    std::size_t position;
    /// \brief
    /// \see{TransactedFile::Buffer} associated with this range.
    TransactedFile::Buffer::SharedPtr buffer;
    /// \brief
    /// true == We straddle a \see{TransactedFile::Buffer} page boundary.
    /// We allocated data and need to copy and free it in
    /// the dtor.
    bool owner;

public:
    /// \brief
    /// ctor.
    /// \param[in] file_ Range \see{TransactedFile}.
    /// \param[in] offset_ Range start.
    /// \param[in] length_ Range length.
    /// \param[in] reading_ true == range is for reading, false == range is for writing.
    /// \param[in] allocator_ \see{util::Allocator} if we need to allocate.
    Range (
        TransactedFile &file_,
        ui64 offset_,
        std::size_t length_,
        bool reading_ = true,
        util::Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ());
    /// \brief
    /// dtor.
    virtual ~Range ();

    /// \brief
    /// Return the next location we will read/write from/to.
    /// \return data at the next location we will read/write from/to.
    inline ui8 *GetDataPtr () const {
        return data + position;
    }
    /// \brief
    /// Reurn the number of bytes till the end of the range.
    /// \reurn Number of bytes till the end of the range.
    inline std::size_t GetDataAvailable () const {
        return length - position;
    }

    // Serializer
    /// \brief
    /// Read raw bytes.
    /// \param[out] buffer Where to place the bytes.
    /// \param[in] count Number of bytes to read.
    /// \return count.
    virtual std::size_t Read (
        void *buffer,
        std::size_t count) override;
    /// \brief
    /// Write raw bytes.
    /// \param[in] buffer Bytes to write.
    /// \param[in] count Number of bytes to write.
    /// \return count.
    virtual std::size_t Write (
        const void *buffer,
        std::size_t count) override;

    // RandomSeekSerializer
    /// \brief
    /// Return the range position pointer.
    /// \return The range position pointer.
    virtual i64 Tell () const override {
        return position;
    }
    /// \brief
    /// Reposition the range position pointer.
    /// \param[in] offset Offset to move relative to fromWhere.
    /// \param[in] fromWhere SEEK_SET, SEEK_CUR or SEEK_END.
    /// \return The new range position pointer.
    virtual i64 Seek (
        i64 offset,
        i32 fromWhere) override;
};

/// \struct TransactedFile::SafeRange TransactedFileRange.h thekogans/util/TransactedFileRange.h
///
/// \brief
/// SafeRange layers bounds checking on top of \see{Range}.
struct _LIB_THEKOGANS_UTIL_DECL SafeRange : public Range {
    /// \brief
    /// Safe ctor. Clamp the range to file bounds.
    /// \param[in] file Range \see{TransactedFile}.
    /// \param[in] offset Range start.
    /// \param[in] length Range length.
    /// \param[in] reading true == range is for reading, false == range is for writing.
    /// \param[in] allocator \see{util::Allocator} if we need to allocate.
    SafeRange (
        TransactedFile &file,
        ui64 offset,
        std::size_t length,
        bool reading = true,
        util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
        Range (
            file,
            offset,
            MIN (length, file.GetSize () - offset),
            reading,
            allocator) {}

    // Serializer
    /// \brief
    /// Perform a safe read. Check buffer for null and
    /// clamp count to the length of the range.
    /// \param[out] buffer Where to place the bytes.
    /// \param[in] count Number of bytes to read.
    /// \return Number of bytes actually read.
    virtual std::size_t Read (
        void *buffer,
        std::size_t count) override;
    /// \brief
    /// Perform a safe write. Check buffer for null and
    /// clamp count to the length of the range.
    /// \param[in] buffer Bytes to write.
    /// \param[in] count Number of bytes to write.
    /// \return Number of bytes actually written.
    virtual std::size_t Write (
        const void *buffer,
        std::size_t count) override;

    // RandomSeekSerializer
    /// \brief
    /// Safe reposition the range pointer. Perform bounds checks on inputs.
    /// \param[in] offset Offset to move relative to fromWhere.
    /// \param[in] fromWhere SEEK_SET, SEEK_CUR or SEEK_END.
    /// \return The new range pointer.
    virtual i64 Seek (
        i64 offset,
        i32 fromWhere) override;
};

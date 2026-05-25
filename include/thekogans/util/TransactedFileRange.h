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
    /// \struct TransactedFile::Stats TransactedFile.h thekogans/util/TransactedFile.h
    ///
    /// \brief
    /// Stats should be used during system integration and tuning. Every time
    /// a \see{Range} is created (\see{ReadOnlyRange} and \see{WriteOnlyRange})
    /// it bumps up the appropriate counter in it's ctor. It also bumps up an
    /// appropriate *Owner* counter if it happens to straddle a \see{Buffer} boundary.
    /// If the ratio of *Owner* counter and range counter approaches 1 then you
    /// have some tuning to do.
    /// In an ideal world, no range would ever cross a buffer boundary and you would
    /// always have the best performing reads and writes. When a range does cross a
    /// buffer boundary it needs to allocate a local buffer to satisfy the fact
    /// range reads and writes do no boundary checking (hence the performance boost).
    /// If a large percentage of your ranges have to allocate the buffer it means
    /// that \see{Buffer::SIZE} size is not properly tuned for your application.
    /// Follow the instructions in \see{Buffer::SIZE} to change the page size.
    struct Stats {
        /// \brief
        /// A count of \see{ReadOnlyRange} that have been created for this file.
        std::atomic<ui64> readOnlyRanges;
        /// \brief
        /// A count of the above ranges that needed to allocate their own buffer.
        std::atomic<ui64> readOnlyOwnerRanges;
        /// \brief
        /// A count of \see{WriteOnlyRange} that have been created for this file.
        std::atomic<ui64> writeOnlyRanges;
        /// \brief
        /// A count of the above ranges that needed to allocate their own buffer.
        std::atomic<ui64> writeOnlyOwnerRanges;

        /// \brief
        /// ctor.
        Stats () :
            readOnlyRanges (0),
            readOnlyOwnerRanges (0),
            writeOnlyRanges (0),
            writeOnlyOwnerRanges (0) {}
    } stats;
#endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_RANGE_GET_STATS)

private:
    /// \struct TransactedFile::Range TransactedFile.h thekogans/util/TransactedFile.h
    ///
    /// \brief
    /// Range provides direct access to TransactedFile's underlying \see{Buffer}.
    /// Range is an abstract base (that's why it's private). Ranges two direct
    /// discendents are \see{ReadOnlyRange} and \see{WriteOnlyRange} (below).
    /// By pairing it with \see{Serializer}, Range provides serialization/
    /// deserialization capabilities without the need to copy chunks of data
    /// in to and out of the buffers resulting in better performance. Because
    /// the file's 64 bit address space is chunked in to hierarchical pages,
    /// if the requested range straddles a page boundary, a range buffer is
    /// allocated to gurantee sequential access. Use \see{Stats} to tune the
    /// \see{Buffer::SIZE} and \see{Buffer::SHIFT_COUNT}. Because range maintains
    /// it's own set of state variables in to the file, if you create nonoveralapping
    /// ranges, you can access the file from multiple threads without the need
    /// for synchronization.
    struct _LIB_THEKOGANS_UTIL_DECL Range : public Serializer {
    protected:
        /// \brief
        /// TransactedFile the range is referring too.
        TransactedFile &file;
        /// \brief
        /// Begining of range.
        ui64 offset;
        /// \brief
        /// Length of range.
        std::size_t length;
        /// \brief
        /// If the provided range straddles a page boundary
        /// use this allocaor to allocate a range buffer.
        util::Allocator::SharedPtr allocator;
        /// \brief
        /// Either a pointer in to \see{Buffer::data} or self
        /// allocated range buffer.
        ui8 *data;
        /// \brief
        /// Mainains current position in the range.
        std::size_t position;
        /// \brief
        /// TransactedFile::Buffer associated with this range.
        TransactedFile::Buffer::SharedPtr buffer;
        /// \brief
        /// true == We straddle a \see{Buffer} page boundary.
        /// We allocated data and need to copy and free it in
        /// the dtor.
        bool owner;

    public:
        /// \brief
        /// ctor.
        /// \param[in] file_ \see{TransactedFile} to buffer.
        /// \param[in] offset_ File offset.
        /// \param[in] length_ How much of the file we want access too.
        /// \param[in] allocator_ \see{util::Allocator} if we need to allocate.
        Range (
            TransactedFile &file_,
            ui64 offset_,
            std::size_t length_,
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
        /// \brief
        /// Advance position a given amount.
        /// \param[in] advance Amount to advance (no backing up).
        /// \return New position.
        std::size_t Advance (std::size_t advance);

        virtual std::size_t Read (
            void * /*buffer*/,
            std::size_t /*count*/) override;
        virtual std::size_t Write (
            const void * /*buffer*/,
            std::size_t /*count*/) override;
    };

public:
    /// \struct TransactedFile::UnsafeReadOnlyRange TransactedFile.h thekogans/util/TransactedFile.h
    ///
    /// \brief
    /// UnsafeReadOnlyRange provides one directional extraction from a file.
    /// It's unsafe because it does absolutely no bounds checking.
    struct _LIB_THEKOGANS_UTIL_DECL UnsafeReadOnlyRange : public Range {
        /// \brief
        /// ctor.
        /// \param[in] file \see{TransactedFile} to buffer.
        /// \param[in] offset File offset.
        /// \param[in] length How much of the file we want access too.
        /// \param[in] allocator \see{util::Allocator} if we need to allocate.
        UnsafeReadOnlyRange (
            TransactedFile &file,
            ui64 offset,
            std::size_t length,
            util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

        virtual std::size_t Read (
            void *buffer,
            std::size_t count) override;
    };

    /// \struct TransactedFile::ReadOnlyRange TransactedFile.h thekogans/util/TransactedFile.h
    ///
    /// \brief
    /// ReadOnlyRange provides one directional extraction from a file.
    /// It layers bounds checking on top of UnsafeReadOnlyRange ctor and Read.
    struct _LIB_THEKOGANS_UTIL_DECL ReadOnlyRange : public UnsafeReadOnlyRange {
        /// \brief
        /// ctor.
        /// \param[in] file \see{TransactedFile} to buffer.
        /// \param[in] offset File offset.
        /// \param[in] length How much of the file we want access too.
        /// \param[in] allocator \see{util::Allocator} if we need to allocate.
        ReadOnlyRange (
            TransactedFile &file,
            ui64 offset,
            std::size_t length,
            util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
            UnsafeReadOnlyRange (file, offset, MIN (length, file.GetSize () - offset), allocator) {}

        /// \brief
        /// Perform a safe read. Clamp count to the length of the range.
        virtual std::size_t Read (
            void *buffer,
            std::size_t count) override;
    };

    /// \struct TransactedFile::UnsafeWriteOnlyRange TransactedFile.h thekogans/util/TransactedFile.h
    ///
    /// \brief
    /// UnsafeWriteOnlyRange provides one directional insertion in to a file.
    /// It's unsafe because it does absolutely no bounds checking.
    struct _LIB_THEKOGANS_UTIL_DECL UnsafeWriteOnlyRange : public Range {
        /// \brief
        /// ctor.
        /// \param[in] file \see{TransactedFile} to buffer.
        /// \param[in] offset File offset.
        /// \param[in] length How much of the file we want access too.
        /// \param[in] allocator \see{util::Allocator} if we need to allocate.
        UnsafeWriteOnlyRange (
            TransactedFile &file,
            ui64 offset,
            std::size_t length,
            util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
        /// \brief
        /// dtor.
        virtual ~UnsafeWriteOnlyRange ();

        virtual std::size_t Write (
            const void *buffer,
            std::size_t count) override;
    };

    /// \struct TransactedFile::WriteOnlyRange TransactedFile.h thekogans/util/TransactedFile.h
    ///
    /// \brief
    /// WriteOnlyRange provides one directional insertion in to a file.
    /// It layers bounds checking on top of UnsafeWriteOnlyRange::Write.
    struct _LIB_THEKOGANS_UTIL_DECL WriteOnlyRange : public UnsafeWriteOnlyRange {
        /// \brief
        /// ctor.
        /// \param[in] file \see{TransactedFile} to buffer.
        /// \param[in] offset File offset.
        /// \param[in] length How much of the file we want access too.
        /// \param[in] allocator \see{util::Allocator} if we need to allocate.
        WriteOnlyRange (
            TransactedFile &file,
            ui64 offset,
            std::size_t length,
            util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
            UnsafeWriteOnlyRange (file, offset, length, allocator) {}

        /// \brief
        /// Write a buffer to the file cache. This method
        /// does bounds checking on the input before calling
        /// UnsafeWriteOnlyRange::Write.
        /// \param[in] buffer Buffer of bytes to write.
        /// \param[in] count Buffer length.
        virtual std::size_t Write (
            const void *buffer,
            std::size_t count) override;
    };

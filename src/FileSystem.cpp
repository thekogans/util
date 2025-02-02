// This file is a part of the B&F Software Solutions
// MFC 3D Modeling EXtensions (MEX) C++ Class library.
// Copyright (C) 1996 B&F Software Solutions Corporation
// All rights reserved.

#include "libMEX.h"
#include <io.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>

namespace MEX {

/**********************************************************************************/
/*********************************** Public ***************************************/
/**********************************************************************************/

File::File (const char *path, bool iscreate)
{
	if (!path)
		throw FileEx (FSL_CONSTRUCTOR, FSX_PARAM_ERROR);

	cwd[0] = '\\';
	cwd[1] = '\0';
	ofi_head =
	ofi_tail = 0;
    file_info.seek = 0;
    file_info.len = 0;
    file_info.buf = 0;
    file_info.isdirty = false;

	if (iscreate) {
		fh = _open (path, _O_BINARY | _O_CREAT | _O_TRUNC | _O_RDWR, _S_IREAD | _S_IWRITE);
		if (fh == -1) {
			throw FileEx (FSL_CONSTRUCTOR, FSX_OPEN_ERROR);
        }
		fshdr.magic = FILE_MAGIC;
		fshdr.root = HEADER_SIZE;
		fshdr.first_free_block = -1;
		fshdr.size = BLOCK_SIZE;
		Block root;
		root.hdr.curr = HEADER_SIZE;
		root.hdr.prev = -1;
		root.hdr.next = -1;
		root.hdr.seek_offs = 0;
		memset (&root.data, 0, BLOCK_DATA_SIZE);
		WriteHdr ();
		WriteBlock (0, BLOCK_SIZE, &root, &root.hdr);
	}
	else {
		fh = _open (path, _O_BINARY | _O_RDWR, _S_IREAD | _S_IWRITE);
		if (fh == -1) {
			throw FileEx (FSL_CONSTRUCTOR, FSX_OPEN_ERROR);
        }
        file_info.len = _filelength (fh);
        file_info.buf = (char*)malloc (file_info.len);
		if (!file_info.buf) {
			throw FileEx (FSL_CONSTRUCTOR, FSX_READ_ERROR);
        }
        _read (fh, file_info.buf, file_info.len);
		if (Read (&fshdr, HEADER_SIZE) != HEADER_SIZE) {
			throw FileEx (FSL_CONSTRUCTOR, FSX_READ_ERROR);
        }
		if (fshdr.magic != FILE_MAGIC) {
			throw FileEx (FSL_CONSTRUCTOR, FSX_NOT_FS_ERROR);
        }
	}
}

File::~File ()
{
    if (file_info.buf) {
        if (file_info.isdirty) {
            _lseek (fh, 0, SEEK_SET);
            _write (fh, file_info.buf, file_info.len);
        }
        free (file_info.buf);
    }

}

bool File::IsFile (const char *name)
{
	OpenFileInfo ofi;
	char path[MAX_PATH_LEN];
    GetAbsPath (name, path);
    FindFileDesc (path, FILE_FLAG_FILE, &ofi);
}

bool File::IsDir (const char *name)
{
	OpenFileInfo ofi;
	char path[MAX_PATH_LEN];
    GetAbsPath (name, path);
    FindFileDesc (path, FILE_FLAG_DIR, &ofi);
}

void File::SplitPathName (const char *path_name, char *path, char *name)
{
	if (!path && !name)
		return;

	if (!ValidatePath (path_name))
		throw FileEx (FSL_SPLITPATHNAME, FSX_BAD_PATH_ERROR);

	const char *ptr1 = path_name;
	const char *ptr2 = path_name;

	while (ptr2) {
		ptr2 = strchr (ptr1 + 1, '\\');
		if (ptr2)
			ptr1 = ptr2;
	}

	if (path) {
		while (*path_name && path_name != ptr1)
			*path++ = *path_name++;
		*path = '\0';
	}

	if (name) {
		if (*ptr1 == '\\')
			++ptr1;
		strcpy (name, ptr1);
	}
}

void File::MakePathName (char *path_name, const char *path, const char *name)
{
	strcpy (path_name, path);
	ui32 l = strlen (path_name);
	if (l&&path_name[l - 1] != '\\')
		path_name[l++] = '\\';
	strcpy (&path_name[l], name);
}

OpenFileInfo *File::OpenFile (const char *name, ui32 flags)
{
	OpenFileInfo *ofi = 0;
	char path[MAX_PATH_LEN];
    GetAbsPath (name, path);
    ofi = (OpenFileInfo *)ofia.Alloc ();
    if (!ofi)
        throw FileEx (FSL_OPENFILE, FSX_MEM_ERROR);
    if (flags & OPEN_CREATE) {
        CreateFileDesc (path, FILE_FLAG_FILE, ofi, true);
        TruncateFile (ofi);
    }
    else
        FindFileDesc (path, FILE_FLAG_FILE, ofi);
    ofi->next = 0;
    if (!ofi_head) {
        ofi->prev = 0;
        ofi_head = ofi_tail = ofi;
    }
    else {
        ofi->prev = ofi_tail;
        ofi_tail = ofi_tail->next = ofi;
    }
    ofi->flags = flags;
    return ofi;
}

void File::CloseFile (OpenFileInfo *ofi)
{
	if (!ofia.IsPtr (ofi))
		throw FileEx (FSL_CLOSEFILE, FSX_PARAM_ERROR);

	if (ofi->prev && ofi->next) {
		ofi->prev->next = ofi->next;
		ofi->next->prev = ofi->prev;
	}
	else if (ofi->prev) {
		ofi_tail = ofi->prev;
		ofi_tail->next = 0;
	}
	else if (ofi->next) {
		ofi_head = ofi->next;
		ofi_head->prev = 0;
	}
	else
		ofi_head = ofi_tail = 0;

	ofia.Free (ofi);
}

i32 File::ReadFile (
        OpenFileInfo *ofi,
        void *buf,
        i32 size) {
	if (!size) {
		return 0;
    }
	if (!ofi || !buf || size < 0) {
		throw FileEx (FSL_READFILE, FSX_PARAM_ERROR);
    }
	BlockHdr hdr;
	i32 len = 0;
	char *ptr = (char *)buf;
	if (GetFileBlockHdr (ofi, &hdr)) {
		while (1) {
			i32 offs = ofi->seek_offs - hdr.seek_offs;
			i32 lenAvailable = __min (size, (i32)BLOCK_DATA_SIZE - offs);
			ReadBlock (hdr.curr, (ui16)(BLOCK_HDR_SIZE + offs), (ui16)lenAvailable, ptr);
			size -= lenAvailable;
			len += lenAvailable;
			ofi->seek_offs += lenAvailable;
			if (!size || hdr.next == -1) {
				break;
            }
			else {
				ReadBlockHdr (hdr.next, &hdr);
            }
			ptr += lenAvailable;
		}
    }
	return len;
}

i32 File::WriteFile (
        OpenFileInfo *ofi,
        const void *buf,
        i32 size) {
	if (!size) {
		return 0;
    }
	if (!ofi || !buf || size < 0) {
		throw FileEx (FSL_WRITEFILE, FSX_PARAM_ERROR);
    }
	BlockHdr hdr;
	i32 len = 0;
	char *ptr = (char *)buf;
	bool update = false;
    if (size && (GetFileBlockHdr (ofi, &hdr) || AddFileBlock (ofi, &hdr))) {
		while (1) {
			i32 offs = ofi->seek_offs - hdr.seek_offs;
			i32 lenAvailable = __min (size, (i32)BLOCK_DATA_SIZE - offs);
			WriteBlock ((ui16)(BLOCK_HDR_SIZE + offs), (ui16)lenAvailable, ptr, &hdr);
			size -= lenAvailable;
			len += lenAvailable;
			ofi->seek_offs += lenAvailable;
			if (ofi->seek_offs > ofi->fd.size) {
				ofi->fd.size = ofi->seek_offs;
				update = true;
			}
			if (!size) {
				break;
            }
			if (hdr.next == -1) {
				AddFileBlock (ofi, &hdr);
            }
			else {
				ReadBlockHdr (hdr.next, &hdr);
            }
			ptr += lenAvailable;
		}
		if (update) {
			WriteFileDesc (ofi->dir, ofi->idx, &ofi->fd);
        }
	}

	return len;
}

i32 File::SeekFile (
        OpenFileInfo *ofi,
        i32 offs,
        i32 from_where) {
    i32 seek;
    switch (from_where) {
		case SEEK_FIRST:
			seek = offs;
			break;
		case SEEK_LAST:
			seek = ofi->fd.size + offs;
			break;
		case SEEK_CURR:
			seek = ofi->seek_offs + offs;
			break;
		default:
			return -1;
	}
    if (seek < 0) {
        return -1;
    }
    ofi->seek_offs = seek;
    if (ofi->seek_offs > ofi->fd.size) {
        ofi->fd.size = ofi->seek_offs;
    	BlockHdr hdr;
        AddFileBlock (ofi, &hdr);
        WriteFileDesc (ofi->dir, ofi->idx, &ofi->fd);
    }
    return seek;
}

i32 File::TellFile (OpenFileInfo *ofi)
{
	return ofi->seek_offs;
}

void File::TruncateFile (OpenFileInfo *ofi)
{
	BlockHdr hdr, tmp;
    if (GetFileBlockHdr (ofi, &hdr)) {
        if (ofi->seek_offs == hdr.seek_offs) {
            if (hdr.prev != -1) {
                ReadBlockHdr (hdr.prev, &tmp);
                tmp.next = -1;
                WriteBlockHdr (&tmp);
            }
            RemoveFileBlock (ofi, &hdr);
        }
        else if (hdr.next != -1) {
            ReadBlockHdr (hdr.next, &tmp);
            hdr.next = -1;
            WriteBlockHdr (&hdr);
            RemoveFileBlock (ofi, &tmp);
        }
        if (!ofi->seek_offs) {
            ofi->fd.first_block = -1;
            ofi->fd.last_block = -1;
        }
        else
            ofi->fd.last_block = hdr.curr;
        ofi->fd.size = ofi->seek_offs;
        WriteFileDesc (ofi->dir, ofi->idx, &ofi->fd);
    }
}

void File::DeleteFile (const char *name)
{
	OpenFileInfo *ofi;
    ofi = OpenFile (name, 0);
    TruncateFile (ofi);
    DeleteFileDesc (ofi);
    CloseFile (ofi);
}

void File::CreateDir (const char *name)
{
	BlockHdr hdr;
	OpenFileInfo ofi;
	char path[MAX_PATH_LEN];

    GetAbsPath (name, path);
    CreateFileDesc (path, FILE_FLAG_DIR, &ofi);
    AddFileBlock (&ofi, &hdr);
    ofi.fd.size = BLOCK_DATA_SIZE;
    WriteFileDesc (ofi.dir, ofi.idx, &ofi.fd);
    ClearBlock (&hdr);
}

static const char *MakePathName (const char *path, const char *name)
{
	static char path_name[MAX_PATH_LEN];
	strncpy (path_name, path, MAX_PATH_LEN);
	ui32 len = strlen (path_name);
	if (path_name[len - 1] != '\\') {
		path_name[len] = '\\';
		path_name[len + 1] = '\0';
	}
	strcat (path_name, name);
	return path_name;
}

void File::DeleteDir (const char *name)
{
	ui16 i;
	char path[MAX_PATH_LEN];
	OpenFileInfo ofi;
	Block dir;

    GetAbsPath (name, path);
    if (!path[1])	/*can't delete root*/
        throw FileEx (FSL_DELETEDIR, FSX_BAD_PATH_ERROR);
    FindFileDesc (path, FILE_FLAG_DIR, &ofi);
    ReadBlock (ofi.fd.first_block, 0, BLOCK_SIZE, &dir);
    for (i = 0; i < BLOCK_NUM_FILES; ++i) {
        if (dir.data.fda[i].name[0]) {
            if (dir.data.fda[i].flags & FILE_FLAG_DIR)
                DeleteDir (MEX::MakePathName (dir.data.fda[i].name, path));
            else if (dir.data.fda[i].flags & FILE_FLAG_FILE)
                DeleteFile2 (MEX::MakePathName (dir.data.fda[i].name, path));
            else
                throw FileEx (FSL_DELETEDIR, FSX_DIR_NOT_EMPTY_ERROR);
        }
    }
    while (dir.hdr.next != -1) {
        ReadBlock (dir.hdr.next, 0, BLOCK_SIZE, &dir);
        for (i = 0; i < BLOCK_NUM_FILES; ++i) {
            if (dir.data.fda[i].name[0]) {
                if (dir.data.fda[i].flags & FILE_FLAG_DIR)
                    DeleteDir (MEX::MakePathName (dir.data.fda[i].name, path));
                else if (dir.data.fda[i].flags & FILE_FLAG_FILE)
                    DeleteFile2 (MEX::MakePathName (dir.data.fda[i].name, path));
                else
                    throw FileEx (FSL_DELETEDIR, FSX_DIR_NOT_EMPTY_ERROR);
            }
        }
    }
    TruncateFile (&ofi);
    DeleteFileDesc (&ofi);
}

void File::ChangeDir (const char *name)
{
	OpenFileInfo ofi;
	char path[MAX_PATH_LEN];

	try {
		GetAbsPath (name, path);
		// short circuit.
		if (_strnicmp (cwd, path, MAX_PATH_LEN)) {
			// root has no file descriptor
			if (path[0])
				FindFileDesc (path, FILE_FLAG_DIR, &ofi);
			strncpy (cwd, path, MAX_PATH_LEN);
			strcat (cwd, "\\");
		}
	}
	catch (FileEx &fsx) {
		throw FileEx (FSL_CHANGEDIR, fsx.WhatsWrong ());
	}
}

void File::GetCurrDir (char *path)
{
	strncpy (path, cwd, MAX_PATH_LEN);
}

bool File::FindFirst (const char *name, ui32 flags, FindFileInfo *ffi)
{
	char path[MAX_PATH_LEN];
    GetAbsPath (name, path, true);
	OpenFileInfo ofi;
    FindFileDesc (path, flags, &ofi);
    strncpy (ffi->search_name, name, MAX_FILE_NAME_LEN2);
    ffi->search_flags = flags;
    strncpy (ffi->name, ofi.fd.name, MAX_FILE_NAME_LEN2);
    ffi->flags = ofi.fd.flags;
    ffi->dir = ofi.dir;
    ffi->idx = ofi.idx;
    return true;
}

bool File::FindNext (FindFileInfo *ffi)
{
	ui16 i;
	Block dir;

    ReadBlock (ffi->dir, 0, BLOCK_SIZE, &dir);
    for (i = ffi->idx + 1; i < BLOCK_NUM_FILES; ++i) {
        if (!CompareNames (ffi->search_name, dir.data.fda[i].name, MAX_FILE_NAME_LEN2) &&
            (ffi->search_flags & dir.data.fda[i].flags)) {
            ffi->idx = i;
            strncpy (ffi->name, dir.data.fda[i].name, MAX_FILE_NAME_LEN2);
            ffi->flags = dir.data.fda[i].flags;
            return true;
        }
    }
    while (dir.hdr.next != -1) {
        ReadBlock (dir.hdr.next, 0, BLOCK_SIZE, &dir);
        for (i = 0; i < BLOCK_NUM_FILES; ++i) {
            if (!CompareNames (ffi->search_name, dir.data.fda[i].name, MAX_FILE_NAME_LEN2) &&
                (ffi->search_flags & dir.data.fda[i].flags)) {
                ffi->dir = dir.hdr.curr;
                ffi->idx = i;
                strncpy (ffi->name, dir.data.fda[i].name, MAX_FILE_NAME_LEN2);
                ffi->flags = dir.data.fda[i].flags;
                return true;
            }
        }
    }
    return false;
}

/**********************************************************************************/
/*********************************** Private **************************************/
/**********************************************************************************/

/********************************* Misc ************************************/
void File::GrowFile (i32 len)
{
    Seek (len - 1, SEEK_END);
    Write ("", 1);
}

i32 File::CompareNames (const char *name1, const char *name2, ui16 len)
{
	i32 char1, char2;

	for (ui16 i = 0; i < len; ++i) {
		if (!name1[i] && !name2[i] || name1[i] == '*' || name2[i] == '*')
			return 0;

		if (name1[i] == '?' || name2[i] == '?')
			continue;

		char1 = toupper (name1[i]);
		char2 = toupper (name2[i]);

		if (char1 != char2)
			return char1 > char2 ? 1 : -1;
	}

	return 0;
}
/***************************************************************************/

/********************************* Path ************************************/
bool File::ValidatePath (const char *path, bool wild)
{
	while (*path) {
		if (!IsFileChar (*path, wild))
			return false;
		path++;
	}

	return true;
}

void File::GetAbsPath (const char *name, char *path, bool wild)
{
	ui16 idx;
	const char *ptr1, *ptr2;

	if (!ValidatePath (name, wild))
		throw FileEx (FSL_GETABSPATH, FSX_BAD_PATH_ERROR);

	if (name[0] == '\\') {
		path[0] = '\\';
		ptr1 = &name[1];
		idx = 1;
	}
	else {
		idx = 0;
		while (path[idx] = cwd[idx])
			++idx;
		ptr1 = &name[0];
	}

	while (*ptr1)
		if (*ptr1 == '\\')
			throw FileEx (FSL_GETABSPATH, FSX_BAD_PATH_ERROR);
		else if (*ptr1 == '.') {
			++ptr1;
			if (*ptr1 == '.') {
				++ptr1;
				// are we trying to get the parent of root?
				if (idx == 1)
					throw FileEx (FSL_GETABSPATH, FSX_BAD_PATH_ERROR);
				// back up one level
				idx -= 2;
				while (path[idx] != '\\')
					--idx;
				++idx;
			}
			if (*ptr1 && *ptr1 != '\\')
				throw FileEx (FSL_GETABSPATH, FSX_BAD_PATH_ERROR);
			++ptr1;
		}
		else {
			ptr2 = strchr (ptr1, '\\');
			// have we reached the filename?
			if (!ptr2)
				break;
			// append the sub-directory
			while (ptr1 <= ptr2) {
				if (idx == _MAX_PATH)
					throw FileEx (FSL_GETABSPATH, FSX_BAD_PATH_ERROR);
				path[idx++] = *ptr1++;
			}
		}

	// append filename
	while (path[idx++] = *ptr1++);

	assert (idx > 1);

	// strip the last backslash
	if (path[idx - 2] == '\\')
		path[idx - 2] = '\0';
}
/***************************************************************************/

/******************************** Header ***********************************/
void File::ReadHdr ()
{
	Seek (0, SEEK_SET);
    Read (&fshdr, HEADER_SIZE);
}

void File::WriteHdr ()
{
	Seek (0, SEEK_SET);
	Write (&fshdr, HEADER_SIZE);
}
/***************************************************************************/

/*************************** File Descriptor *******************************/
// given a file name, scan the directory block to locate it's file descriptor
// input:
//		len - length of file name
//		name - file name
//		flags - file flags to match (used to distinguish between
//					files and directories with the same name)
//		dir - directory block to scan
// output:
//		ofi->dir - directory to which this descriptor beli32s
//		ofi->idx - directory index of the descriptor
//		ofi->fd - copy of the file descriptor if found
// return:
//		false - not found
//		true - found
bool File::ScanFileDesc (ui16 len, const char *name, ui32 flags, Block *dir, OpenFileInfo *ofi)
{
	for (ui16 i = 0; i < BLOCK_NUM_FILES; ++i) {
		// If were looking for a free slot,
		// just check for blank file name.
		if (!len && !dir->data.fda[i].name[0] ||
                // Else match names and flags.
                len && !CompareNames (name, dir->data.fda[i].name, len) &&
                (dir->data.fda[i].flags & flags)) {
			ofi->dir = dir->hdr.curr;
			ofi->idx = i;
			memcpy (&ofi->fd, &dir->data.fda[i], FILE_DESC_SIZE);
			ofi->seek_offs = 0;
			ofi->flags = flags;
			return true;
		}
	}
	return false;
}

// given a file name, scan the chain of directory blocks starting
// with dir to locate it's file descriptor
// input:
//		len - length of file name
//		name - file name
//		flags - file flags to match (used to distinguish between
//					files and directories with the same name)
//		dir - directory block to start the scan from
// output:
//		see ScanFileDesc
// return:
//		see ScanFileDesc
bool File::GetFileDesc (ui16 len, const char *name, ui32 flags, Block *dir, OpenFileInfo *ofi)
{
    if (ScanFileDesc (len, name, flags, dir, ofi))
        return true;

    while (dir->hdr.next != -1) {
        ReadBlock (dir->hdr.next, 0, BLOCK_SIZE, dir);
        if (ScanFileDesc (len, name, flags, dir, ofi))
            return true;
    }

    return false;
}

void File::FindFileDesc (const char *path, ui32 flags, OpenFileInfo *ofi)
{
	const char *ptr1, *ptr2;
	Block dir;
    ptr1 = &path[1];
    ReadBlock (fshdr.root, 0, BLOCK_SIZE, &dir);
    while (*ptr1) {
        ptr2 = strchr (ptr1, '\\');
        if (ptr2) {
            if (!GetFileDesc ((ui16)(ptr2 - ptr1), ptr1, FILE_FLAG_DIR, &dir, ofi))
                throw FileEx (FSL_FINDFILEDESC, FSX_NO_FILE_DESC_ERROR);
            ReadBlock (ofi->fd.first_block, 0, BLOCK_SIZE, &dir);
            ptr1 = &ptr2[1];
        }
        else {
            if (!GetFileDesc (MAX_FILE_NAME_LEN2, ptr1, flags, &dir, ofi))
                throw FileEx (FSL_FINDFILEDESC, FSX_NO_FILE_DESC_ERROR);
            strncpy (ofi->path, path, MAX_PATH_LEN);
            return;
        }
    }
}

void File::CreateFileDesc (const char *path, ui32 flags, OpenFileInfo *ofi, bool scan)
{
	bool sub = false;
    const char *ptr1 = &path[1];
    Block dir;
    ReadBlock (fshdr.root, 0, BLOCK_SIZE, &dir);
    while (*ptr1) {
        const char *ptr2 = strchr (ptr1, '\\');
        if (ptr2) {
            if (!GetFileDesc ((ui16)(ptr2 - ptr1), ptr1, FILE_FLAG_DIR, &dir, ofi)) {
                throw FileEx (FSL_CREATEFILEDESC, FSX_BAD_PATH_ERROR);
            }
            ReadBlock (ofi->fd.first_block, 0, BLOCK_SIZE, &dir);
            sub = true;
            ptr1 = &ptr2[1];
        }
        else {
            if (GetFileDesc (MAX_FILE_NAME_LEN2, ptr1, flags, &dir, ofi)) {
                if (!scan)
                    throw FileEx (FSL_CREATEFILEDESC, FSX_DUP_DESC_ERROR);
                else {
                    strncpy (ofi->path, path, MAX_PATH_LEN);
                    return;
                }
            }
            // Expand the directory if it has no empty slots.
            if (!GetFileDesc (0, "", 0, &dir, ofi)) {
                BlockHdr hdr;
                // Sub-directories have file descriptors,
                // we must use AddFileBlock to wire in a
                // new block.
                if (sub) {
                    // Set seek_offs to the end of the file,
                    // so that the new block goes at the end.
                    ofi->seek_offs = ofi->fd.size;
                    AddFileBlock (ofi, &hdr);
                    // Bump up the file (directory) size.
                    ofi->fd.size += BLOCK_DATA_SIZE;
                    // Update the descriptor.
                    WriteFileDesc (ofi->dir, ofi->idx, &ofi->fd);
                }
                // Root has no file descriptor,
                // just wire in a new block.
                else {
                    AllocBlock (&hdr);
                    hdr.prev = dir.hdr.curr;
                    hdr.next = -1;
                    WriteBlockHdr (&hdr);
                    dir.hdr.next = hdr.curr;
                    WriteBlockHdr (&dir.hdr);
                }
                // Start with an empty directory.
                ClearBlock (&hdr);
                ofi->prev = 0;
                ofi->next = 0;
                strncpy (ofi->path, path, MAX_PATH_LEN);
                ofi->dir = hdr.curr;
                ofi->idx = 0;
                ofi->seek_offs = 0;
                ofi->flags = 0;
            }
            strncpy (ofi->fd.name, ptr1, MAX_FILE_NAME_LEN2);
            ofi->fd.first_block = -1;
            ofi->fd.last_block = -1;
            ofi->fd.size = 0;
            ofi->fd.flags = flags;
            WriteFileDesc (ofi->dir, ofi->idx, &ofi->fd);
            return;
        }
    }
}

void File::DeleteFileDesc (OpenFileInfo *ofi)
{
    memset (&ofi->fd, 0, FILE_DESC_SIZE);
    WriteFileDesc (ofi->dir, ofi->idx, &ofi->fd);
}

void File::ReadFileDesc (i32 blk, ui16 idx, FileDesc *fd)
{
    ReadBlock (blk, FILE_DESC_OFFS (idx), FILE_DESC_SIZE, fd);
}

void File::WriteFileDesc (i32 blk, ui16 idx, FileDesc *fd)
{
    WriteBlock (blk, FILE_DESC_OFFS (idx), FILE_DESC_SIZE, fd);
}
/***************************************************************************/

/******************************** Block ************************************/
void File::AllocBlock (BlockHdr *hdr)
{
    // Is there a free block chain
    if (fshdr.first_free_block != -1) {
        // read the block header
        ReadBlockHdr (fshdr.first_free_block, hdr);
        // update the header
        fshdr.first_free_block = hdr->next;
        WriteHdr ();
    }
    // expand the file system file
    else {
        // update the block
        hdr->curr = file_info.len;
        // grow the file
        GrowFile (BLOCK_SIZE);
    }
}

void File::FreeBlock (BlockHdr *hdr)
{
    // link this block in to the free block chain
    hdr->prev = -1;
    hdr->next = fshdr.first_free_block;
    fshdr.first_free_block = hdr->curr;
    // write header and block header
    WriteHdr ();
    WriteBlockHdr (hdr);
}

bool File::AddFileBlock (
        OpenFileInfo *ofi,
        BlockHdr *hdr) {
	i32 sb = ofi->fd.size / BLOCK_DATA_SIZE;
    i32 eb = ofi->seek_offs / BLOCK_DATA_SIZE + 1;
    for (i32 i = sb; i < eb; ++i) {
        AllocBlock (hdr);
        hdr->seek_offs = i * BLOCK_DATA_SIZE;
        // first block
        if (ofi->fd.first_block == -1) {
            hdr->prev = hdr->next = -1;
            ofi->fd.first_block = ofi->fd.last_block = hdr->curr;
        }
        // last block
        else {
            hdr->prev = ofi->fd.last_block;
            hdr->next = -1;
            BlockHdr prev;
            ReadBlockHdr (ofi->fd.last_block, &prev);
            prev.next = hdr->curr;
            WriteBlockHdr (&prev);
            ofi->fd.last_block = hdr->curr;
        }
        WriteBlockHdr (hdr);
    }
	return true;
}

void File::RemoveFileBlock (OpenFileInfo *ofi, BlockHdr *hdr)
{
	BlockHdr tmp;
    hdr->prev = -1;
    WriteBlockHdr (hdr);

    if (fshdr.first_free_block != -1) {
        ReadBlockHdr (fshdr.first_free_block, &tmp);
        tmp.prev = ofi->fd.last_block;
        WriteBlockHdr (&tmp);
    }

    ReadBlockHdr (ofi->fd.last_block, &tmp);
    tmp.next = fshdr.first_free_block;
    WriteBlockHdr (&tmp);

    fshdr.first_free_block = hdr->curr;
    WriteHdr ();
}

bool File::GetFileBlockHdr (OpenFileInfo *ofi, BlockHdr *hdr)
{
	if (ofi->fd.first_block != -1) {
		ReadBlockHdr (ofi->fd.first_block, hdr);
		if (hdr->seek_offs <= ofi->seek_offs && hdr->seek_offs + (i32)BLOCK_DATA_SIZE > ofi->seek_offs) {
			return true;
        }
		while (hdr->next != -1) {
			ReadBlockHdr (hdr->next, hdr);
			if (hdr->seek_offs <= ofi->seek_offs && hdr->seek_offs + (i32)BLOCK_DATA_SIZE > ofi->seek_offs) {
				return true;
            }
		}
	}

	return false;
}

bool File::GetFileBlock (OpenFileInfo *ofi, Block *blk)
{
	if (GetFileBlockHdr (ofi, &blk->hdr)) {
		ReadBlockData (blk->hdr.curr, &blk->data);
		return true;
	}

	return false;
}

i32 File::Seek (
        i32 offs,
        i32 org) {
    i32 seek;
    switch (org) {
        case SEEK_SET:
			seek = offs;
			break;
		case SEEK_END:
			seek = file_info.len + offs;
			break;
		case SEEK_CUR:
			seek = file_info.seek + offs;
			break;
		default:
            return -1;
	}
    if (seek < 0) {
        return -1;
    }
    if (seek > file_info.len) {
        char *buf = (char *)realloc (file_info.buf, seek);
        if (!buf) {
            return -1;
        }
        file_info.buf = buf;
        file_info.len = seek;
    }
    file_info.seek = seek;
    return seek;
}

i32 File::Read (
        void *buf,
        i32 count) {
    i32 read_count = MIN (count, file_info.len - file_info.seek);
    memcpy (buf, &file_info.buf[file_info.seek], read_count);
    file_info.seek += read_count;
    return read_count;
}

i32 File::Write (
        void *buf,
        i32 count) {
    i32 buf_size = file_info.len - file_info.seek;
    if (count > buf_size) {
        char *buf = (char *)realloc (file_info.buf, file_info.len + count - buf_size);
        if (!buf) {
            return -1;
        }
        file_info.buf = buf;
        file_info.len = file_info.len + count - buf_size;
    }
    memcpy (&file_info.buf[file_info.seek], buf, count);
    file_info.seek += count;
    file_info.isdirty = true;
    return count;
}

void File::ReadBlock (i32 blk, ui16 offs, ui16 len, void *buf)
{
	Seek (blk + offs, SEEK_SET);
	Read (buf, len);
}

void File::ReadBlockHdr (i32 blk, BlockHdr *hdr)
{
    ReadBlock (blk, 0, BLOCK_HDR_SIZE, hdr);
}

void File::ReadBlockData (i32 blk, BlockData *data)
{
    ReadBlock (blk, BLOCK_HDR_SIZE, BLOCK_DATA_SIZE, data);
}

void File::WriteBlock (i32 blk, ui16 offs, ui16 len, void *buf)
{
    Seek (blk + offs, SEEK_SET);
    Write (buf, len);
}

void File::WriteBlockHdr (BlockHdr *hdr)
{
    WriteBlock (0, BLOCK_HDR_SIZE, hdr, hdr);
}

void File::WriteBlockData (BlockData *data, BlockHdr *hdr)
{
    WriteBlock (BLOCK_HDR_SIZE, BLOCK_DATA_SIZE, data, hdr);
}

void File::ClearBlock (BlockHdr *hdr)
{
	BlockData data;
    memset (&data, 0, BLOCK_DATA_SIZE);
    WriteBlockData (&data, hdr);
}

} // namespace MEX

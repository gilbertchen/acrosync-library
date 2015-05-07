// Copyright (C) 2015 Acrosync LLC
//
// Unless explicitly acquired and licensed from Licensor under another
// license, the contents of this file are subject to the Reciprocal Public
// License ("RPL") Version 1.5, or subsequent versions as allowed by the RPL,
// and You may not copy or use this file in either source code or executable
// form, except in compliance with the terms and conditions of the RPL.
//
// All software distributed under the RPL is provided strictly on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, AND
// LICENSOR HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
// LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, QUIET ENJOYMENT, OR NON-INFRINGEMENT. See the RPL for specific
// language governing rights and limitations under the RPL. 

#include <rsync/rsync_client.h>

#include <rsync/rsync_entry.h>
#include <rsync/rsync_file.h>
#include <rsync/rsync_log.h>
#include <rsync/rsync_pathutil.h>
#include <rsync/rsync_socketio.h>
#include <rsync/rsync_sshio.h>
#include <rsync/rsync_timeutil.h>
#include <rsync/rsync_util.h>

#include <openssl/evp.h>

#include <set>
#include <algorithm>
#include <sstream>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <qi/qi_build.h>

namespace rsync
{

namespace
{

// Flags for sending entries.  Defined by rsync.
enum {
    XFLAGS_TOP_DIR          = 0x01,
    XFLAGS_SAME_MODE        = 0x02,
    XFLAGS_EXTENDED_FLAGS   = 0x04,
    XFLAGS_SAME_UID         = 0x08,
    XFLAGS_SAME_GID         = 0x10,
    XFLAGS_SAME_NAME        = 0x20,
    XFLAGS_LONG_NAME        = 0x40,
    XFLAGS_SAME_TIME        = 0x80,
    XFLAGS_NO_CONTENT_DIR   = 1 << 8,
    XFLAGS_IO_ERROR_ENDLIST = 1 << 12,
};

// The initial chunk size
const int DefaultChunkSize = 64 * 1024;

const int NumberOfHashBuckets = 65536;

// The rsync rolling checksum
inline void getRollingChecksum(const char *chunk, int size,
                               uint32_t &s1, uint32_t &s2)
{
    s1 = s2 = 0;
    for (int i = 0; i < size; ++i) {
        s1 += static_cast<int32_t>(chunk[i]);
        s2 += s1;
    }
}

// The hash for making the checksum hash table
inline int getChecksumHash(uint32_t checksum)
{
    return ((checksum & 0xffff) + (checksum >> 16)) & 0xffff;
}

// A convenient method for calculating MD4/MD5 checksums
void getMDDigest(int protocol, const char *data, int size, int32_t seed, char digest[16])
{
    Util::md_struct context;
    Util::md_init(protocol, &context);
    Util::md_update(protocol, &context, data, size);
    Util::md_update(protocol, &context, reinterpret_cast<char *>(&seed), sizeof seed);
    Util::md_final(protocol, &context, digest);
}
 
// Given a file size, return a good block length.
int getBlockLength(int64_t size)
{
    const unsigned int minimumBlockLength = 700;
    const unsigned int maxBlockLength = 0x20000;
    if (size < minimumBlockLength * minimumBlockLength) {
        return minimumBlockLength;
    }

    uint32_t g = 0x80000000;
    uint32_t c = 0x80000000;

    while (c) {
        if (static_cast<int64_t>(g) * g > size) {
            g ^= c;
        }
        c >>= 1;
        g |= c;
    }
    return (g > maxBlockLength) ? maxBlockLength : (g >> 3) * 8;
}

// Used to save the partial file.
class PartialFileKeeper
{
public:

    // 'source' is usually the intermediate file during downloading; 'destination' is the file to be renamed to once
    // the download is complete.
    PartialFileKeeper(const char *source, const char *destination, uint32_t mode)
        : d_source(source)
        , d_destination(destination)
        , d_mode(mode)
        , d_modifiedTime(-1)
        , d_startTime(::time(0))
    {
    }
    
    ~PartialFileKeeper()
    {
        // Keep the file only if downloading completed or lasted a while (we dont' want to waste the bytes that have
        // been downloaded)
        if (d_modifiedTime >= 0 || (::time(0) - d_startTime) > 10) {
            PathUtil::rename(d_source.c_str(), d_destination.c_str(), false);
            PathUtil::setModifiedTime(d_destination.c_str(), d_modifiedTime);
            PathUtil::setMode(d_destination.c_str(), d_mode);
        } else {
            PathUtil::remove(d_source.c_str(), false);
        }
    }

    // Set the modified time to indicate to indicate that the download is complete
    void setModifiedTime(int64_t modifiedTime)
    {
        d_modifiedTime = modifiedTime;
    }

private:
    //NOT IMPLEMENTED
    PartialFileKeeper(const PartialFileKeeper&);
    PartialFileKeeper& operator=(const PartialFileKeeper&);
    
    std::string d_source;
    std::string d_destination;
    uint32_t d_mode;
    int64_t d_modifiedTime;
    time_t d_startTime;
};

// Valid characters for paths.
uint8_t validatePathCharacterTable[] =
{
//  Invalid on Windows:
//      <   >   :   "   /   \   |   ?   * 
//      3C  3E  3A  22  2F  5C  7C  3F  2A
//
//---------------------------------------------------------
//  0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,   // 0
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,   // 1
    1, 1, 0, 1, 1, 1, 1, 1,  1, 1, 0, 1, 1, 1, 1, 1,   // 2
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 0, 1, 0, 1, 0, 0,   // 3

    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,   // 4
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 0, 1, 1, 1,   // 5
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,   // 6
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 0, 1, 1, 1,   // 7

    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,   // 8
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,   // 9
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,   // A
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,   // B

    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,   // C
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,   // D
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,   // E
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,   // F
};

// Determine if a given path contains all valid characters.
bool validatePathCharacters(const char *path)
{
    const unsigned char *p = reinterpret_cast<const unsigned char*>(path);
    while (*p) {
        if (!validatePathCharacterTable[*p]) {
            LOG_INFO(RSYNC_INVALID_PATH) << "Skip file '" << path << "' as its name contains illegal characters" << LOG_END
            return false;
        }
        ++p;
    }
    return true;
}

} // unnamed namespace

Client::Client(IO*io, const char *rsyncCommand, int preferredProtocol, int *cancelFlagAddress)
    : d_usingSSH(dynamic_cast<SSHIO*>(io))
    , d_io(io)
    , d_stream(new Stream(d_io, cancelFlagAddress))
    , d_rsyncCommand(rsyncCommand)
    , d_downloadLimit(0)
    , d_deletionEnabled(false)
    , d_backupPaths()
    , d_protocol(preferredProtocol)
    , d_checksumSeed(0)
    , d_lastEntryTime(0)
    , d_lastEntryMode(0)
    , d_lastEntryPath()
    , d_chunkSize(DefaultChunkSize)
    , d_numberOfChecksums(256)
    , d_hashBuckets(new int[NumberOfHashBuckets])
    , d_checksums(new Checksum[d_numberOfChecksums])
    , d_chunk(new char[DefaultChunkSize])
    , d_totalBytes(&d_dummyCounter)
    , d_physicalBytes(&d_dummyCounter)
    , d_logicalBytes(&d_dummyCounter)
    , d_skippedBytes(&d_dummyCounter)
    , d_dummyCounter(0)
    , d_deletedFiles()
    , d_updatedFiles()
{
}

Client::~Client()
{
    // Leave d_io alone as it may be resued for next sync.

    delete [] d_hashBuckets;
    delete [] d_checksums;
    delete [] d_chunk;

    delete d_stream;
}

void Client::setSpeedLimits(int downloadLimit, int uploadLimit)
{
    // Download limit is controlled by the remote rsync.
    d_downloadLimit = downloadLimit;
    d_stream->setUploadLimit(uploadLimit);
}
    
void Client::setDeletionEnabled(bool deletionEnabled)
{
    d_deletionEnabled = deletionEnabled;
}
    
void Client::addBackupPath(const char *backupPath)
{
    d_backupPaths.push_back(std::string(backupPath));
}

void Client::clearBackupPaths()
{
    d_backupPaths.clear();
}


void Client::setStatsAddresses(int64_t *totalBytes, int64_t *physicalBytes, int64_t *logicalBytes, int64_t *skippedBytes)
{
    d_totalBytes = totalBytes;
    d_physicalBytes = physicalBytes;
    d_logicalBytes = logicalBytes;
    d_skippedBytes = skippedBytes;
}

int Client::readIndex()
{
    return d_protocol < 30 ? d_stream->readInt32() : d_stream->readIndex();
}

void Client::writeIndex(int index)
{
    if (d_protocol < 30) {
        d_stream->writeInt32(index);
    } else {
        d_stream->writeIndex(index);
    }
}

void Client::resizeChunk(int size)
{
    // Increase the chunk size if needed
    if (size > d_chunkSize) {
        d_chunkSize = size;
        delete [] d_chunk;
        d_chunk = new char[d_chunkSize];
    }

}

bool Client::receiveEntry(std::string *path, bool *isDir, int64_t *size, int64_t *time, uint32_t *mode, std::string *symlink)
{
    // Read the one-byte transfer flags
    int xflags = d_stream->readUInt8();

    if (xflags == 0) {
        return false;
    }

    // The flag could be more than one byte
    if (xflags & XFLAGS_EXTENDED_FLAGS) {
        xflags |= (d_stream->readUInt8() << 8); 
        if (xflags & XFLAGS_IO_ERROR_ENDLIST) {
            int io_error = d_stream->readInt32();
            LOG_ERROR(RSYNC_ENDLIST) << "remote rsync encountered a file list error: " << io_error << LOG_END
        }
    }

    int l1 = 0;    // the length of the common prefix of this path and the path of last path
    int l2 = 0;    // the remaining length
    if (xflags & XFLAGS_SAME_NAME) {
        l1 = d_stream->readUInt8();
    }
    if (xflags & XFLAGS_LONG_NAME) {
        if (d_protocol < 30) {
            l2 = d_stream->readInt32();
        } else {
            l2 = d_stream->readVariableInt32();
        }
    } else {
        l2 = d_stream->readUInt8();
    }

    path->clear();
    path->reserve(l1 + l2);

    // Get the first 'l1' bytes from 'd_lastEntryPath'
    if (l1 > 0) {
        path->assign(d_lastEntryPath.substr(0, l1));
    }

    // Read the rest 'l2' bytes from the stream
    char buffer[256];
    while (l2 > 0) {
        int bytes = d_stream->read(buffer, (l2 > static_cast<int>(sizeof(buffer))) ? sizeof(buffer) : l2);
        path->append(buffer, bytes);
        l2 -= bytes;
    }

    // Read the file size from the stream
    if (d_protocol < 30) {
        *size = d_stream->readInt64();
    } else {
        *size = d_stream->readVariableInt64(3);
    }

    // Read the modification time from the stream, or reuse the last modification time
    *time = d_lastEntryTime;
    if (!(xflags & XFLAGS_SAME_TIME)) {
        if (d_protocol < 30) {
            *time = d_stream->readInt32();      // send_file_entry: write_int(f, modtime) 
        } else {
            *time = d_stream->readVariableInt64(4); // send_file_entry: write_varlong(...)
        }
    }

    // Same for the mode
    *mode = d_lastEntryMode;
    if (!(xflags & XFLAGS_SAME_MODE)) {
        *mode = d_stream->readInt32();           // send_file_entry: write_int(f, to_wire_mode(mode))
    }
    *isDir = (*mode & Entry::IS_DIR) && !(*mode & Entry::IS_FILE);

    // Handle symbolic links
    if (*mode & Entry::IS_FILE && *mode & Entry::IS_LINK) {
        int length;
        if (d_protocol < 30) {
            length = d_stream->readInt32();
        } else {
            length = d_stream->readVariableInt32();
        }
        char buffer[1024];
        int read = 0;
        *symlink = "";
        while (read < length) {
            int left = length - read;
            if (left > sizeof(buffer)) {
                left = sizeof(buffer);
            }
            int bytes = d_stream->read(buffer, left);
            symlink->append(buffer, bytes);
            read += bytes;
        }
    }
    
    d_lastEntryPath = *path;
    d_lastEntryMode = *mode;
    d_lastEntryTime = *time;

    return true;
}

void Client::sendChecksum(int index, const char *oldFile)
{
    File f;

    // Send the index and the flag
    writeIndex(index);
    d_stream->writeUInt16(0x8000); 

    // The size of the old file
    int64_t oldFileSize = 0;
    if (oldFile) {
        oldFileSize = PathUtil::getSize(oldFile);
    }

    // If we can open the old file then send all zeroes
    if (!oldFile || !f.open(oldFile, false, false)) {
        d_stream->writeInt32(0);
        d_stream->writeInt32(0);
        d_stream->writeInt32(0);
        d_stream->writeInt32(0);
        return;
    }

    // The old file will be divided into 'count' blocks each of which has a length of 'blockLength'.
    int32_t blockLength = getBlockLength(oldFileSize);
    int32_t md5Length = 16;
    int32_t count = (oldFileSize - 1) / blockLength + 1;
    int32_t remainder = oldFileSize - (count - 1) * blockLength;

    resizeChunk(blockLength);

    // Send the header
    d_stream->writeInt32(count);
    d_stream->writeInt32(blockLength);
    d_stream->writeInt32(md5Length);
    d_stream->writeInt32(remainder);

    // Send the actualy checksum
    int bytes;
    uint32_t s1, s2;
    char digest[16];
    for (int i = 0; i < count; ++i) {
        d_stream->checkCancelFlag();
        bytes = f.read(d_chunk, blockLength);
        getRollingChecksum(d_chunk, bytes, s1, s2);
        d_stream->writeInt32((s1 & 0xffff) | (s2 << 16));
        getMDDigest(d_protocol, d_chunk, bytes, d_checksumSeed, digest);
        d_stream->write(digest, sizeof(digest));
    }
}

bool Client::receiveFile(const char *remotePath, const char *newFilePath, const char *oldFilePath, int64_t *fileSize)
{
    // Receive the iflags
    uint32_t iflags = d_stream->readUInt16();
    if (!(iflags & 0x8000)) {
        LOG_FATAL(RSYNC_IFLAGS) << "File '" << remotePath << "' not transmitted as iflags is "
                                << iflags << LOG_END 
        return false;
    }
 
    int32_t count = d_stream->readInt32(); 
    int32_t blockLength = d_stream->readInt32();
    d_stream->readInt32();   // We don't need md5Length and the remainder length for receiving the file content
    d_stream->readInt32(); 

    resizeChunk(blockLength);

    // Try to open the file
    File newFile(newFilePath, true, true);
    if (!newFile.isValid()) {
        LOG_FATAL(RSYNC_OPEN) << "Abort operation due to an open error" << LOG_END
        return false;
    }

    // If checksums have been received (count > 0), we must open the old file to retrieve chunks that have not
    // been modified
    File oldFile;
    if (count) {
        if (!oldFilePath || !oldFile.open(oldFilePath, false, false)) {
            LOG_FATAL(RSYNC_BASE) << "Local file disappeared when transferring '" 
                                  << remotePath << "'" << LOG_END
        }
    }

    int token;
    *fileSize = 0;
    int64_t physicalBytes = 16;
    int64_t logicalBytes = 0;
    int previousToken = 0;

    Util::md_struct md_context;
    Util::md_init(d_protocol, &md_context);
    if (d_protocol < 30) {
        Util::md_update(d_protocol, &md_context, reinterpret_cast<char *>(&d_checksumSeed),
                  sizeof d_checksumSeed);  
    }

    while ((token = d_stream->readInt32()) != 0) {
        if (token > 0) {
            // A positive token means a chunk is going to be sent over the wire, and the token is actually the
            // length of the chunk
            resizeChunk(token);
            d_stream->read(d_chunk, token);
            newFile.write(d_chunk, token);
            Util::md_update(d_protocol, &md_context, d_chunk, token);
            *fileSize += token; 
            physicalBytes += 4 + token;
            *d_physicalBytes += 4 + token;
            logicalBytes += token;
            *d_logicalBytes += token;
        } else if (oldFile.isValid()) {
            // Otherwise, the token indicate a chunk in the old file
            token = -token - 1;
            if (previousToken == 0 || previousToken != token) {
                oldFile.seek(static_cast<int64_t>(token) * blockLength, File::SEEK_FROM_BEGIN);
            }
            int bytes = oldFile.read(d_chunk, blockLength);
            newFile.write(d_chunk, bytes);
            Util::md_update(d_protocol, &md_context, d_chunk, bytes); 
            previousToken = token + 1;
            *fileSize += bytes;
            physicalBytes += 4;
            *d_physicalBytes += 4;
            logicalBytes += bytes;
            *d_logicalBytes += bytes;
        }
    }

    // The MD5 checksum of the entire file is transmitted at the end, so we can make sure that we've got the
    // right file.  This means that if there is any error we'll be able to tell and retry later.
    char localDigest[16], remoteDigest[16];
    Util::md_final(d_protocol, &md_context, localDigest);
    d_stream->read(remoteDigest, sizeof(remoteDigest));
    physicalBytes += 16;
    *d_physicalBytes += 16;

    if (::strncmp(remoteDigest, reinterpret_cast<const char*>(localDigest), sizeof(remoteDigest))) {
        LOG_ERROR(RSYNC_CHECKSUM) << "Failed to download '" << remotePath << "': checksum mismatch" << LOG_END
        return false;
    } else {
        LOG_INFO(RSYNC_DOWNLOAD) << "Downloaded " << remotePath
                                 << " (" << logicalBytes << "/" << physicalBytes << ")" << LOG_END
        return true;
    }
}

// For directories, 'localTop' and 'remoteTop' must end with '/'.
int Client::download(const char *localTop, const char *remoteTop, const char *temporaryFile,
                     const std::set<std::string> *includeFiles)
{
    *d_totalBytes = 0;
    *d_physicalBytes = 0;
    *d_logicalBytes = 0;
    *d_skippedBytes = 0;

    std::string remotePath = remoteTop;
    std::string localPath = localTop;

    // We can actually handle the case where 'localTop' points to a single file.
    bool singleFile = remotePath[remotePath.size() - 1] != '/';
    while (localPath[localPath.size() - 1] == '/') {
        localPath = localPath.substr(0, localPath.size() - 1);
    }

    if (!singleFile && !PathUtil::exists(localPath.c_str())) {
        PathUtil::createDirectory(localPath.c_str());
    }

    start(remotePath.c_str(), /*downloading=*/true, /*recursive=*/true, /*deleting=*/false);

    std::vector<Entry*> remoteFiles;
    Util::EntryListReleaser remoteFilesReleaser(&remoteFiles);

    Entry *entry;
    std::string path;
    bool isDir;
    int64_t size, time;
    uint32_t mode;
    std::string symlink;
    
    if (statusOut.isConnected()) {
        statusOut((std::string("Indexing remote directory ") + remoteTop).c_str());
    }

    // First receive the list of file entries from the server.
    while ((receiveEntry(&path, &isDir, &size, &time, &mode, &symlink)) != 0) {
        if (!singleFile && remoteFiles.empty() && path != ".") {
            remoteFiles.push_back(new Entry("./", true, 0, 0, 0));
        }

        entry = new Entry(path.c_str(), isDir, size, time, mode);
        if (entry->isLink()) {
            entry->setSymlink(symlink);
        } else {
            *d_totalBytes += size;
        }
        entry->normalizePath();
        remoteFiles.push_back(entry);
    }

    // Ignore the 'io_error' flag.
    if (d_protocol < 30) {
        d_stream->readInt32();
    }

    if (remoteFiles.empty()) {
        return 0;
    }

    if (statusOut.isConnected()) {
        statusOut((std::string("Indexing local directory ") + localTop).c_str());
    }

    // Usually the remote file list should be already sorted, but it doesn't hurt to sort it again.
    std::vector<Entry*>::iterator start = remoteFiles.begin();
    std::sort(++start, remoteFiles.end(), Entry::compareGlobally);

    // Now let's construct the local file list.
    std::vector<Entry*> localFiles, localDirectories;
    Util::EntryListReleaser localFilesReleaser(&localFiles);

    if (!singleFile) {
    localFiles.push_back(new Entry("./", true, 0, 0, 0));
    }

    // If 'includeFiles' is speicified, we'll create entries one by one from those included in 'includeFiles'.
    // Otherwise, we'll enumerate the entire directory recursively.
    if (includeFiles) {
    for (std::set<std::string>::const_iterator iter = includeFiles->begin();
            iter != includeFiles->end(); ++iter) {
        Entry *entry = PathUtil::createEntry(localPath.c_str(), iter->c_str());
        if (entry) {
            entry->normalizePath();
        localFiles.push_back(entry);
            }
        }

        std::sort(localFiles.begin() + 1, localFiles.end(), Entry::compareGlobally);
    } else {
        PathUtil::listDirectory(localPath.c_str(), "", &localFiles, &localDirectories, d_protocol > 29);
        while (localDirectories.size()) {
            entry = localDirectories.back();
            localDirectories.pop_back();
            localFiles.push_back(entry);
            PathUtil::listDirectory(localPath.c_str(), entry->getPath(), &localFiles, &localDirectories,
                                    d_protocol > 29);

            d_stream->checkCancelFlag();
    }
    }

    if (statusOut.isConnected()) {
        statusOut("Download starting...");
    }

    std::vector<int> queue;         // store the indices of files needed to be downloaded

    int begin = singleFile ? 0 : 1;
    int i = begin;
    for (int index = begin; index < remoteFiles.size(); ++index) {

        while (i < localFiles.size() && Entry::compareGlobally(localFiles[i], remoteFiles[index])) {
            ++i;
        }
        std::string path = PathUtil::join(localPath.c_str(), remoteFiles[index]->getPath());
        if (i >= localFiles.size() || Entry::compareGlobally(remoteFiles[index], localFiles[i])) {
            // Local file/dir doesn't exist
            if (remoteFiles[index]->isDirectory()) {
                PathUtil::createDirectory(path.c_str());
                *d_skippedBytes += remoteFiles[index]->getSize();
            } else if (remoteFiles[index]->isLink()) {
                PathUtil::createSymlink(path.c_str(), remoteFiles[index]->getSymlink(), remoteFiles[index]->isDirectory());
            } else if (!remoteFiles[index]->isRegular()) {
                LOG_INFO(RSYNC_SKIP) << "Skip non-regular file '" << remoteFiles[index]->getPath() << "'" << LOG_END
            } else {
                // The rmeote file must be downloaded unless the path is invalid
                const char *path = remoteFiles[index]->getPath();
                if (validatePathCharacters(remoteFiles[index]->getPath()) && (*path != '.' || ::strncmp(path, ".acrosync/", 10))) {
                    queue.push_back(index);
                }
            }
        } else if (remoteFiles[index]->isLink()) {
            PathUtil::remove(path.c_str());
            PathUtil::createSymlink(path.c_str(), remoteFiles[index]->getSymlink(), remoteFiles[index]->isDirectory());
        } else if (remoteFiles[index]->isDirectory()) {
            // It is a directory on the remote side.
            if (!localFiles[i]->isDirectory()) {
                PathUtil::remove(path.c_str());
                PathUtil::createDirectory(path.c_str());
                PathUtil::setMode(path.c_str(), remoteFiles[index]->getMode());
            } else if (localFiles[i]->getMode() != remoteFiles[index]->getMode()) {
                PathUtil::setMode(path.c_str(), remoteFiles[index]->getMode());
            }
        } else {
            // It is a file on the remote side.
            if (localFiles[i]->isDirectory()) {
                PathUtil::remove(path.c_str());
                queue.push_back(index);
            } else if (!remoteFiles[index]->isRegular()) {
                // Skipping non-regular file silently
            } else if (localFiles[i]->isOlderThan(*remoteFiles[index])) {
                if (validatePathCharacters(remoteFiles[index]->getPath())) {
                    queue.push_back(index);
                }
            } else {
                *d_skippedBytes += remoteFiles[index]->getSize();
                if (localFiles[i]->getMode() != remoteFiles[index]->getMode()) {
                    PathUtil::setMode(path.c_str(), remoteFiles[index]->getMode());
                }
            }
        }
    }

    // Disable automatic flushing since we want to control when to send data.  Otherwise it may deadlock as at the
    // same time we're reading file content from the server.
    d_stream->disableAutomaticFlush();

    std::vector<int> retries;     // Store indices of files that must be retrasmitted due to errors.

    int phase = 0;
    int updated = 0;
    while (phase < 2 && queue.size()) {

        int i = 0;

        bool bufferFlushed = true;
        while (true) {

            // If buffer hasn't been flushed, don't add anything to it, as the flag head may have
            // already been sent which includes the data length.
            if (bufferFlushed && i < queue.size()) {

                if (!remoteFiles[queue[i]]->isReadable()) {
                    LOG_INFO(RSYNC_SKIP) << "Skip unreadable file '" << remoteFiles[queue[i]]->getPath() << "'" << LOG_END
                } else {
                    std::string localFile = PathUtil::join(localPath.c_str(), remoteFiles[queue[i]]->getPath()); 
                    const char *oldFile = 0;
                    if (phase == 0 && PathUtil::exists(localFile.c_str())) {
                        oldFile = localFile.c_str();
                    }

                    // Send the checksums for each file to be downloaded.
                    sendChecksum(queue[i], oldFile);
                }
                ++i;

                if (i == queue.size()) {
                    writeIndex(Stream::INDEX_DONE);
                }
            }

            // Try to flush the write buffer.  This is 'all or none' operation, meaning either it completes with a
            // success or no bytes have been sent.
            bufferFlushed = d_stream->tryFlushWriteBuffer();

            // If any data is available for reading, we'll start receiving the file content
            if (d_stream->isDataAvailable()) {
                int index = readIndex();
                if (index == Stream::INDEX_DONE) {
                    break;
                }

                if (index < 0 || index >= remoteFiles.size()) {
                    LOG_FATAL(RSYNC_INDEX) << "Received an out-of-bound index: " << index << LOG_END;
                }

                if (phase == 1) {
                    LOG_INFO(RSYNC_RETRY) << "Attempting to download '" << remoteFiles[index]->getPath() << "' again" << LOG_END
                }

                std::string oldFile = localPath.c_str();
                if (!singleFile ) {
                    oldFile = PathUtil::join(localPath.c_str(), remoteFiles[index]->getPath());
                }

                // Use the PartialFileKeeper class to keep the partially downloaded file if an error occurs
                PartialFileKeeper keeper(temporaryFile, oldFile.c_str(), remoteFiles[index]->getMode());
                int64_t fileSize = 0;
                int64_t currentLogicalBytes = *d_logicalBytes;
                if (!receiveFile(remoteFiles[index]->getPath(), temporaryFile, oldFile.c_str(), &fileSize)) {
                    *d_logicalBytes = currentLogicalBytes;
                    retries.push_back(index);
                } else {
                    keeper.setModifiedTime(remoteFiles[index]->getTime());
                    ++updated;
                    d_updatedFiles.push_back(oldFile);
                }
            } 
        }

        queue.clear();
        queue.swap(retries);
        ++phase;
    }
    
    writeIndex(Stream::INDEX_DONE);
    writeIndex(Stream::INDEX_DONE);
    writeIndex(Stream::INDEX_DONE);
    writeIndex(Stream::INDEX_DONE);
    d_stream->flushWriteBuffer();
    d_stream->flush();

    // The following code removes local files that do not exist on the remote side.
    if (d_deletionEnabled) {
        for (size_t i = localFiles.size() - 1, index = remoteFiles.size() - 1; i > 0; --i) {
            while (index > 0 && Entry::compareGlobally(localFiles[i], remoteFiles[index])) {
                --index;
            }
            if (index <= 0 || Entry::compareGlobally(remoteFiles[index], localFiles[i])) {
                LOG_INFO(RSYN_DELETE) << "Deleted " << localFiles[i]->getPath() << LOG_END
                std::string path = PathUtil::join(localTop, localFiles[i]->getPath());
                PathUtil::remove(path.c_str());
                d_deletedFiles.push_back(localFiles[i]->getPath());
            }
        }
    }
    
    // Update all the directory entries recursively, in a depth first order, to calculate
    // the total size of all the files under each direcotry.
    std::vector<Entry*> directoryStack;
    directoryStack.push_back(remoteFiles[0]);
    for (int index = 1; index < remoteFiles.size(); ++index) {
        entry = remoteFiles[index];
        while (directoryStack.size() > 1 && !directoryStack.back()->contains(entry)) {
            directoryStack.pop_back();
        }
        if (entry->isDirectory()) {
            directoryStack.push_back(entry);
        } else {
            for (int i = 0; i < directoryStack.size(); ++i) {
                directoryStack[i]->addSize(entry->getSize());
            }
        }
    }

    // Send the directory info out if needed.
    if (entryOut.isConnected()) {
        for (int index = 0; index < remoteFiles.size(); ++index) {
            entry = remoteFiles[index];
            entryOut(entry->getPath(), entry->isDirectory(), entry->getSize(), entry->getTime(),
                     entry->isLink() ? entry->getSymlink() : 0);
        }
    }

    LOG_DEBUG(RSYNC_DOWNLOAD) << "Downloaded " << updated << " files; "
                                << "total " << *d_totalBytes << " bytes, skipped " << *d_skippedBytes
                                << " bytes, received " << *d_logicalBytes << "/" << *d_physicalBytes
                                << " bytes" << LOG_END
    
    return updated;
}

// List the remote directory.  Works the same way as download() untile all remote entries have been received,
// and then terminate the operation.
bool Client::list(const char *path)
{    
    start(path, /*downloading=*/true, /*recursive=*/false, /*deleting=*/false);

    std::string pathStr;
    bool isDir;
    int64_t time, size;
    uint32_t mode;
    std::string symlink;
    
    std::vector<Entry*> remoteFiles;
    Util::EntryListReleaser localFilesReleaser(&remoteFiles);

    while ((receiveEntry(&pathStr, &isDir, &size, &time, &mode, &symlink)) != 0) {
        Entry *entry = new Entry(pathStr.c_str(), isDir, size, time, mode);
        if (entry->isLink()) {
            entry->setSymlink(symlink);
        }
        remoteFiles.push_back(entry);
    }

    std::sort(remoteFiles.begin(), remoteFiles.end(), Entry::compareLocally);

    for (int i = 0; i < 2; ++i) {
        for (unsigned int j = 0; j < remoteFiles.size(); ++j) {
            Entry *entry = remoteFiles[j];
            if ((entry->isDirectory() && (i == 0)) || (!entry->isDirectory() && (i == 1))) {
                entryOut(entry->getPath(), entry->isDirectory(), entry->getSize(), entry->getTime(),
                         entry->isLink() ? entry->getSymlink() : 0);
            }
        }
    }

    // Ignore the 'io_error' flag.
    if (d_protocol < 30) {
        d_stream->readInt32();  // send_file_list: write_int(f, ignore_errors...)
    }

    writeIndex(Stream::INDEX_DONE);
    writeIndex(Stream::INDEX_DONE);
    d_stream->flushWriteBuffer();

    return true;
}

// Send an entry to the remote server.  It is the exact opposite of 'receiveEntry()' so refer to that method if you need
// to understand the details.
void Client::sendEntry(Entry *entry, bool isTop, bool noDirContent)
{
    int xflags = XFLAGS_SAME_UID | XFLAGS_SAME_GID;

    if (entry->getTime() == d_lastEntryTime) {
        xflags |= XFLAGS_SAME_TIME;
    }

    if (isTop) {
        xflags |= XFLAGS_TOP_DIR;
    }
    
    uint32_t fileMode = entry->getMode();

    if (d_lastEntryMode == fileMode) {
        xflags |= XFLAGS_SAME_MODE;
    }

    int l1 = 0, l2;
    int filePathLength = ::strlen(entry->getPath());
    
    if (noDirContent && entry->isDirectory()) {
        if (d_protocol >= 30) {
            xflags |= XFLAGS_NO_CONTENT_DIR;
        } else {
            --filePathLength;
        }
    }
    
    if (d_lastEntryPath.size()) {
        for (; l1 < static_cast<int>(d_lastEntryPath.size()) && l1 < 255; ++l1) {
            if (entry->getPath()[l1] != d_lastEntryPath[l1]) {
                break;
            }
        }
        l2 = filePathLength - l1;
    } else {
        l2 = filePathLength;
    }

    if (l1 > 0) {
        xflags |= XFLAGS_SAME_NAME;
    }

    if (l2 > 255) {
        xflags |= XFLAGS_LONG_NAME;
    }

    if (xflags == 0 && !entry->isDirectory()) {
        xflags |= XFLAGS_TOP_DIR;
    }
    if (xflags == 0 || (xflags & 0xFF00)) {
        xflags |= XFLAGS_EXTENDED_FLAGS;
        d_stream->writeUInt16(xflags);
    } else {
        d_stream->writeUInt8(xflags);
    }

    if (xflags & XFLAGS_SAME_NAME) {
        d_stream->writeUInt8(l1);
    }
    if (xflags & XFLAGS_LONG_NAME) {
        if (d_protocol < 30) {
            d_stream->writeInt32(l2);
        } else {
            d_stream->writeVariableInt32(l2);
        }
    } else {
        d_stream->writeUInt8(l2);
    }
    d_stream->write(entry->getPath() + l1, l2);

    int64_t size = entry->getSize();

    if (d_protocol < 30) {
        d_stream->writeInt64(size);
    } else {
        d_stream->writeVariableInt64(size, 3);
    }

    if (!(xflags & XFLAGS_SAME_TIME)) {
        if (d_protocol < 30) {
            d_stream->writeInt64(entry->getTime());
        } else {
            d_stream->writeVariableInt64(entry->getTime(), 4);
        }
    }

    if (!(xflags & XFLAGS_SAME_MODE)) {
        d_stream->writeInt32(fileMode);
    }

    if (entry->isLink()) {
        if (d_protocol < 30) {
            d_stream->writeInt32(entry->getSymlinkLength());
        } else {
            d_stream->writeVariableInt32(entry->getSymlinkLength());
        }
        d_stream->write(entry->getSymlink(), entry->getSymlinkLength());
    }

    d_lastEntryPath = entry->getPath();
    d_lastEntryMode = fileMode;
    d_lastEntryTime = entry->getTime();
}

// Send a file to the remote server
bool Client::sendFile(int index, const char *remotePath, const char *localPath)
{
    // First we receive the 'iflags' from the generator on the remove server.  If the generator doesn't want the
    // file we simply forward the 'iflags' and that is for this file.
    uint16_t iflags = d_stream->readUInt16();
    if (!(iflags & 0x8000)) {
        writeIndex(index);
        d_stream->writeUInt16(iflags);
        return false;
    }

    // This is one byte for fnamecmp_type; ignore its value, just forward it 
    uint8_t fnamecmp_type = 0;
    if (iflags & 0x0800) {
        fnamecmp_type = d_stream->readUInt8();
    }

    if (iflags & 0x1000) {
        LOG_FATAL("RSYNC_BUG") << "ITEM_XNAME_FOLLOWS flag not handled" << LOG_END
        return false;
    }

    // Read the checksum header
    int32_t count = d_stream->readInt32();
    int32_t blockLength = d_stream->readInt32();
    int32_t md5Length = d_stream->readInt32();
    int32_t remainder = d_stream->readInt32();

    Util::md_struct md_context;
    Util::md_init(d_protocol, &md_context);
    if (d_protocol < 30) {
        Util::md_update(d_protocol, &md_context, reinterpret_cast<char *>(&d_checksumSeed),
                  sizeof d_checksumSeed);
    }

    int64_t size = 0;
    int64_t physicalBytes = 0;
    int64_t logicalBytes = 0;
    
    // Try to open the local file.
    File f;
    
    f.open(localPath, false, false);
    if (!f.isValid()) {
        // Ignore the checksums
        char unused[16];
        for (int i = 0; i < count; ++i) {
            d_stream->readInt32();
            d_stream->read(unused, md5Length);
        }
            
        // Note that here we don't forward the index to the remote receiver.
            
        LOG_INFO(RSYNC_UPLOAD) << "Ignore '" << remotePath << "' due to open error" << LOG_END
        return false;
    }
    
    // Forward the index and the 'iflags' to the remote receiver.
    writeIndex(index);
    d_stream->writeUInt16(iflags);
    if (iflags &0x0800) {
        d_stream->writeUInt8(fnamecmp_type);
    }
    d_stream->writeInt32(count);
    d_stream->writeInt32(blockLength);
    d_stream->writeInt32(md5Length);
    d_stream->writeInt32(remainder);

    if (count == 0) {
        // No checksums received from the generator, so we just send the file content one chunk at a time.
        while (true) {
            int bytes = f.read(d_chunk, d_chunkSize);
            size += bytes;
            if (bytes) {
                d_stream->writeInt32(bytes);
                d_stream->write(d_chunk, bytes);
                Util::md_update(d_protocol, &md_context, d_chunk, bytes);
                physicalBytes += bytes + 4;
                *d_physicalBytes += bytes + 4;
                logicalBytes += bytes;
                *d_logicalBytes += bytes;
            } else {
                break;
            }
        }
    } else {
        // The following implements the rsync diff algorithm.

        // Allocate a new array of checksums if necessary.
        if (count > d_numberOfChecksums) {
            delete [] d_checksums;
            d_numberOfChecksums = count;
            d_checksums = new Checksum[count];
        }

        // Allocate a new chunk if necessary.  The chunk is used as the buffer to read the file.
        int chunkSize = blockLength * 2;
        resizeChunk(chunkSize);

        // Prepare the hash table that use the rolling checksum as the key.
        for (int i = 0; i < NumberOfHashBuckets; ++i) {
            d_hashBuckets[i] = -1;
        }
        for (int i = 0; i < count; ++i) {
            d_checksums[i].d_sum1 = d_stream->readInt32();
            d_stream->read(d_checksums[i].d_sum2, md5Length);
            int bucket = getChecksumHash(d_checksums[i].d_sum1);
            d_checksums[i].d_next = d_hashBuckets[bucket];
            d_hashBuckets[bucket] = i;
        }

        // Read the first data into the chunk
        int n = f.read(d_chunk, chunkSize);
        size += n;
        Util::md_update(d_protocol, &md_context, d_chunk, n);
        int i = blockLength;
        if (n < blockLength) {
            // Not enough data to perform the rolling checksum.  Just send the raw data and done.
            if (n > 0) {
                d_stream->writeInt32(n);
                d_stream->write(d_chunk, n);
                physicalBytes += n + 4;
                *d_physicalBytes += n + 4;
                logicalBytes += n;
                *d_logicalBytes += n;
            }
        } else {
            // 's' is the rolling checksum at the core of rdiff.  's1' and 's2' are just its high
            // and low 16 bits.
            uint32_t s, s1, s2;
            getRollingChecksum(d_chunk, blockLength, s1, s2);

            // 'n' is the number of valid bytes in d_chunk; 'i' points to the next byte to be
            // counted in 's1' and 's2'.  All bytes before 'i' (and after i - blockLength if 
            // i > blockLength) should have been counted.
            while (true) {
                s = (s1 & 0xffff) | (s2 << 16);
                int bucket = d_hashBuckets[getChecksumHash(s)];
                bool matched = false;
                char digest[16];
                bool digestCalculated = false;
                while (bucket != -1) {
                    if (d_checksums[bucket].d_sum1 == s) {
                        // Potential match.  Must compute the MD5 checksum to confirm.
                        if (!digestCalculated) {
                            digestCalculated = true;
                            getMDDigest(d_protocol, d_chunk + i - blockLength, blockLength, d_checksumSeed, digest);
                        }
                        if (::memcmp(digest, d_checksums[bucket].d_sum2, md5Length) == 0) {
                            matched = true;
                            break;
                        }
                    }
                    bucket = d_checksums[bucket].d_next;
                }
                if (matched) {
                    if (i > blockLength) {
                        // The data in the buffer is longer than 'blockLength'.  Send the first 'blockLength - i' bytes
                        d_stream->writeInt32(i - blockLength);
                        d_stream->write(d_chunk, i - blockLength);
                        physicalBytes += i - blockLength + 4;
                        *d_physicalBytes += i - blockLength + 4;
                        logicalBytes += i - blockLength;
                        *d_logicalBytes += i - blockLength;
                    }
                    // Instead of sending the plain chunk data, just send the index as a negative token.  This is the
                    // heart of the rsync algorithm
                    d_stream->writeInt32(-(bucket + 1));
                    physicalBytes += 4;
                    *d_physicalBytes += 4;
                    logicalBytes += blockLength;
                    *d_logicalBytes += blockLength;
                    ::memmove(d_chunk, d_chunk + i, n - i);
                    n -= i;
                    i = 0;
                } else if (i >= chunkSize) {
                    // We are at the end of the buffer.  Will send out the first 'i - blockLength' bytes and hope
                    // a match will be found on the remaining 'blockLength' bytes and more.
                    assert(n == chunkSize);
                    int bytes = i - blockLength;
                    d_stream->writeInt32(bytes);
                    d_stream->write(d_chunk, bytes);
                    physicalBytes += 4 + bytes;
                    *d_physicalBytes += 4 + bytes;
                    logicalBytes += bytes;
                    *d_logicalBytes += bytes;
                    ::memmove(d_chunk, d_chunk + bytes, n - bytes);
                    n -= bytes;
                    i -= bytes;
                }

                if (i >= n || (i == 0 && n < blockLength)) {
                    // Read in more bytes.
                    int bytes = f.read(d_chunk + n, chunkSize - n);
                    size += bytes;
                    Util::md_update(d_protocol, &md_context, d_chunk + n, bytes);
                    n += bytes;
                    if (i >= n || (i == 0 && n < blockLength)) {
                        // No more bytes from the file.  Must decide what to do with the bytes in the buffer.
                        if (n == 0) {
                            break;
                        }
                        if (n == remainder) {
                            // Special case, the remaining bytes in the buffer is of the same length as the
                            // last chunk of the remote file.  Calculate the checksum to find out if they are
                            // actually identical.
                            getRollingChecksum(d_chunk, remainder, s1, s2);
                            s = (s1 & 0xffff) | (s2 << 16);
                            if (s == d_checksums[count - 1].d_sum1) {
                                char digest[16];
                                getMDDigest(d_protocol, d_chunk, n, d_checksumSeed, digest);
                                if (::memcmp(digest, d_checksums[count - 1].d_sum2, md5Length) == 0) {
                                    d_stream->writeInt32(-count);
                                    physicalBytes += 4;
                                    *d_physicalBytes += 4;
                                    logicalBytes += n;
                                    *d_logicalBytes += n;
                                    break;
                                }
                            }
                        }

                        // Send whatever in the buffer as it is.
                        d_stream->writeInt32(n);
                        d_stream->write(d_chunk, n);
                        physicalBytes += 4 + n;
                        *d_physicalBytes += 4 + n;
                        logicalBytes += n;
                        *d_logicalBytes += n;
                        break;
                    }
                }

                if (i == 0) {
                    getRollingChecksum(d_chunk, blockLength, s1, s2);
                    i = blockLength;
                    assert(i <= n);
                    continue;
                }

                // Update the rolling checkum by one byte.
                assert(i < n);
                assert(i >= blockLength);
                s1 -= d_chunk[i - blockLength];
                s2 -= blockLength * d_chunk[i - blockLength];
                s1 += d_chunk[i];
                s2 += s1;
                ++i;
            }
        }
    }

    // Send '0' to indicate that no more chunk will be sent.
    d_stream->writeInt32(0);

    // Send the MD5 checksum of the whole file.
    char localDigest[16];
    Util::md_final(d_protocol, &md_context, localDigest);
    d_stream->write(reinterpret_cast<char*>(localDigest), sizeof(localDigest));
    physicalBytes += 16;
    *d_physicalBytes += 16;
    d_stream->flushWriteBuffer();

    LOG_INFO(RSYNC_UPLOAD) << "Uploaded " << remotePath
                           << " (" << logicalBytes << "/" << physicalBytes << ")" << LOG_END

    return true;
}

// For directories, 'localTop' and 'remoteTop' must end with '/'.
int Client::upload(const char *localTop, const char *remoteTop, const std::set<std::string> *includeFiles)
{
    std::vector<Entry*> localFiles, directories;
    Util::EntryListReleaser localFileReleaser(&localFiles);
    Util::EntryListReleaser directoriesReleaser(&directories);

    std::string localPath = localTop;
    while (localPath[localPath.size() - 1] == '/') {
        localPath = localPath.substr(0, localPath.size() - 1);
    }

    if (statusOut.isConnected()) {
        statusOut((std::string("Indexing local directory ") + localTop).c_str());
    }

    if (PathUtil::isDirectory(localPath.c_str())) {

        // This top entry is required to enable remote deletion under the top directory.
        localFiles.push_back(new Entry("./", true, 0, ::time(0),
                             Entry::IS_DIR | Entry::IS_ALL_READABLE | Entry::IS_WRITABLE | Entry::IS_EXECUTABLE));
        
        if (includeFiles) {
            // We must include parent directories of every file.
            std::set<std::string> entries;
            for (std::set<std::string>::const_iterator iter = includeFiles->begin();
                 iter != includeFiles->end(); ++iter) {
                const char *path = iter->c_str();
                if (*path == '/') {
                    ++path;
                }

                entries.insert(path);
                for (const char *p = path; *p; ++p) {
                    if (*p == '/' && *(p + 1) != 0) {
                        std::string dir = std::string(path, p);
                        entries.insert(dir);
                    }
                }
            }

            // Now create entries.
            for (std::set<std::string>::const_iterator iter = entries.begin();
                iter != entries.end(); ++iter) {
                Entry *entry = PathUtil::createEntry(localPath.c_str(), iter->c_str());
                if (entry) {
                    entry->normalizePath();
                    localFiles.push_back(entry);
                }
            }

            std::sort(localFiles.begin() + 1, localFiles.end(), Entry::compareGlobally);

        } else {

            // Populate 'localFiles' by iterating through the local directory recursively.
            PathUtil::listDirectory(localPath.c_str(), "", &localFiles, &directories, d_protocol > 29);

            while (directories.size()) {
                Entry *entry = directories.back();
                directories.pop_back();
                localFiles.push_back(entry);
                PathUtil::listDirectory(localPath.c_str(), entry->getPath(), &localFiles, &directories,
                                        d_protocol > 29);
                d_stream->checkCancelFlag();
            }
        }
    } else {
        // 'localPath' points to a single file.
        localPath = PathUtil::getDirectory(localTop);
        std::string base = PathUtil::getBase(localTop);
        Entry *entry = PathUtil::createEntry(localPath.c_str(), base.c_str());
        if (!entry) {
            return 0;
        }
        localFiles.push_back(entry);
    }
   
 
    *d_totalBytes = 0;
    *d_physicalBytes = 0;
    *d_logicalBytes = 0;
    *d_skippedBytes = 0;
    
    start(remoteTop, /*downloading=*/false, /*isRecursive=*/true, /*isDeleting=*/d_deletionEnabled);
    
    for (unsigned int i = 0; i < localFiles.size(); ++i) {
        sendEntry(localFiles[i], i == 0);
        *d_totalBytes += localFiles[i]->getSize();
    }
    
    if (statusOut.isConnected()) {
        statusOut("Upload starting...");
    }

    d_stream->writeUInt8(0);

    if (d_protocol < 30) {
        d_stream->writeInt32(0); 
    }

    d_stream->flushWriteBuffer();

    int updated = 0;
    int phase = 0;
    int lastIndex = -1;
    while (phase < 2) {
        int index = readIndex();
        if (index == Stream::INDEX_DONE) {
            writeIndex(index);
            d_stream->flushWriteBuffer();

            // The remote generator will send the indices in order; so if there is a gap then it menas some files are not needed.
            for (++lastIndex; lastIndex < localFiles.size(); ++lastIndex) {
                *d_skippedBytes += localFiles[lastIndex]->getSize();
            }

            ++phase;
            continue;
        }

        if (index < 0 || index >= localFiles.size()) {
            LOG_FATAL(RSYNC_INDEX) << "Received an out-of-bound index: " << index << LOG_END
        }

        if (phase == 1) {
            LOG_INFO(RSYNC_RETRY) << "Attempting to upload '" << localFiles[index]->getPath() << "' again" << LOG_END
            *d_logicalBytes -= localFiles[index]->getSize();
        }
        
        for (int i = lastIndex + 1; i < index; ++i) {
            *d_skippedBytes += localFiles[i]->getSize();
        }
        lastIndex = index;
        
        std::string localFile = PathUtil::join(localPath.c_str(), localFiles[index]->getPath());
        if (sendFile(index, localFiles[index]->getPath(), localFile.c_str())) {
            ++updated;
            d_updatedFiles.push_back(localFile);
        }        
    }

    for (int i = 0; i < 2; ++i) {
        int index = readIndex();
        writeIndex(index);
        if (index != Stream::INDEX_DONE) {
            uint16_t iflags = d_stream->readUInt16();
            d_stream->writeUInt16(iflags);
        }
        d_stream->flushWriteBuffer();
    }

    if (d_deletionEnabled) {
        const std::vector<std::string> &deletedFiles = d_stream->getDeletedFiles();
        for (unsigned int i = 0; i < deletedFiles.size(); ++i) {
            LOG_INFO(RSYN_DELETE) << "Deleted " << deletedFiles[i] << " remotely" << LOG_END
            this->d_deletedFiles.push_back(deletedFiles[i].c_str());
        }
    }

    LOG_DEBUG(RSYNC_UPLOAD) << "Uploaded " << updated << " files; "
                            << "total " << *d_totalBytes << " bytes, skipped " << *d_skippedBytes
                            << " bytes, sent " << *d_logicalBytes << "/" << *d_physicalBytes
                            << " bytes" << LOG_END
    return updated;
}

// To remove a remote file or directory, send a file entry list containing only './', and set the include/exclude
// patterns to exclude everything but the file to be deleted.  
void Client::remove(const char *remoteFile)
{
    std::string remotePath = remoteFile;
    if (remotePath[remotePath.size() - 1] == '/') {
        remotePath = remotePath.substr(0, remotePath.size() - 1);
    }
    std::string remoteDir = PathUtil::getDirectory(remotePath.c_str());

    start(remoteDir.c_str(), /*downloading=*/false, /*recursive=*/false, /*isDeleting=*/true);

    std::string includeFilter("+ /");
    includeFilter += PathUtil::getBase(remotePath.c_str());
    d_stream->writeInt32(includeFilter.size());
    d_stream->write(includeFilter.c_str(), includeFilter.size());

    includeFilter += "/";
    d_stream->writeInt32(includeFilter.size());
    d_stream->write(includeFilter.c_str(), includeFilter.size());

    includeFilter += "**";
    d_stream->writeInt32(includeFilter.size());
    d_stream->write(includeFilter.c_str(), includeFilter.size());

    std::string excludeFilter("- *");
    d_stream->writeInt32(excludeFilter.size());
    d_stream->write(excludeFilter.c_str(), excludeFilter.size());

    d_stream->writeInt32(0); 
    d_stream->flushWriteBuffer();

    Entry entry("./", true, 0, ::time(0), 0);
    sendEntry(&entry, true);
    d_stream->writeUInt8(0); // no more file to send; end of list
    if (d_protocol < 30) {
        d_stream->writeInt32(0); 
    }
    d_stream->flushWriteBuffer();

    // Send INDEX_DONE 4 times to terminate the transmission properly;
    for (int i = 0; i < 4; ++i) {
        int index = readIndex();
        writeIndex(index);
        if (index != Stream::INDEX_DONE) {
            uint16_t iflags = d_stream->readUInt16();
            d_stream->writeUInt16(iflags);
        }
        d_stream->flushWriteBuffer();
    }
    return;
}

// To create a new directory, send a file entry list that contains only that directory.
void Client::mkdir(const char *path)
{
    std::string remotePath = path;
    while (remotePath.back() == '/') {
        remotePath.erase(remotePath.end() - 1);
    }
    std::string remoteDir = PathUtil::getDirectory(remotePath.c_str());

    if (remoteDir.size() == 0) {
        remoteDir = "~";
    }
    
    start(remoteDir.c_str(), /*downloading=*/false, /*recursive=*/false, /*isDeleting=*/false);

    Entry entry((PathUtil::getBase(remotePath.c_str()) + "/").c_str(), true, 0, ::time(0), Entry::IS_ALL_READABLE | Entry::IS_WRITABLE | Entry::IS_EXECUTABLE);
    sendEntry(&entry, true);

    d_stream->writeUInt8(0); // no more file to send; end of list
    if (d_protocol < 30) {
        d_stream->writeInt32(0); 
    }
    d_stream->flushWriteBuffer();

    // Send INDEX_DONE 4 times to terminate the transmission properly;
    for (int i = 0; i < 4; ++i) {
        int index = readIndex();
        writeIndex(index);
        if (index != Stream::INDEX_DONE) {
            uint16_t iflags = d_stream->readUInt16();
            d_stream->writeUInt16(iflags);
        }
        d_stream->flushWriteBuffer();
    }
    return;
}

// Create a symbolic link on the remote side
void Client::link(const char *remotePath, const char *link)
{
    std::string remoteDir = PathUtil::getDirectory(remotePath);

    start(remoteDir.c_str(), /*downloading=*/false, /*recursive=*/false, /*isDeleting=*/false);

    Entry entry(PathUtil::getBase(remotePath).c_str(), false, 0, ::time(0),
        Entry::IS_FILE | Entry::IS_LINK | Entry::IS_ALL_READABLE | Entry::IS_WRITABLE);
    entry.setSymlink(link);
    sendEntry(&entry, false);

    d_stream->writeUInt8(0); // no more file to send; end of list
    if (d_protocol < 30) {
        d_stream->writeInt32(0); 
    }
    d_stream->flushWriteBuffer();
    d_stream->flush();

    for (int i = 0; i < 4; ++i) {
        readIndex();
        writeIndex(Stream::INDEX_DONE);
        d_stream->flushWriteBuffer();
    }
    return;
}

// List all modules.  Only valid under the daemon mode
void Client::listModules()
{
    d_stream->reset();
    std::string command = d_rsyncCommand + " --server --sender -tude. . /";
    d_io->createChannel(command.c_str(), &d_protocol);
    LOG_DEBUG(RSYNC_COMMAND) << "rsync command: " << command << LOG_END

    std::vector<std::string> modules;
    d_stream->login(command.c_str(), &d_protocol, &modules);

    for (unsigned int i = 0; i < modules.size(); ++i) {
        std::string module = modules[i];
        size_t found = module.find_first_of(" \t\r\n");
        if (found != std::string::npos) {
            module.erase(found);
        }
        entryOut(module.c_str(), true, 0, 0, 0);
    }

    return;
}

// Start an rsync session
void Client::start(const char *remotePath, bool isDownloading, bool recursive, bool isDeleting)
{
    d_stream->reset();

    // Options for the remote server (note that these are completely different from the command line options)
    //   --copy_dirlinks: treat symlinked dir as real dir
    //   -t: preserve times
    //   -u: skip files that are newer on the receiver
    //   -d: transfer directories without recursing
    //   -e.:  server protocol options not used
    std::string command = d_rsyncCommand + " --server --modify-window=2 ";
    if (isDownloading) {
        command += "--sender ";
        if (d_downloadLimit > 0) {
            std::stringstream out;
            out << "--bwlimit=" << d_downloadLimit << " ";
            command += out.str();
        }
    }
    
    // Uncomment these if you want logging on the server.  Note that '--log-file' isn't a valid option under the daemon
    // mode.
    //command += "-vvvv ";
    //command += "--log-file=rsync.log ";

    // For receiving the list of deleted remote files properly.
    command += "--out-format=%n ";
    
    command += "--links ";

    if (recursive) {
        command += "--recursive ";
    }
    
    if (isDeleting) {
        command += "--delete-during ";
    }

    for (unsigned int i = 0; i < d_backupPaths.size(); ++i) {
        command += "--link-dest=";
        command += d_backupPaths[i];
        command += " ";
    }
    
    command += "-tude. . ";

    if (*remotePath == 0) {
        command += "\"\"";
    } else if (::strchr(remotePath, ' ') || ::strchr(remotePath, '(') || ::strchr(remotePath, ')')) {
        command += "\"";
        command += remotePath;
        command += "\"";
    } else {
        command += remotePath;
    }

    d_io->createChannel(command.c_str(), &d_protocol);
    
    LOG_DEBUG(RSYNC_COMMAND) << "rsync command: " << command << LOG_END

    if (d_usingSSH) {
        // Negotiate the protocol version
        d_stream->writeInt32(d_protocol);
        int protocol = d_stream->readInt32();      
        // If 'd_protocol' isn't supported by the server then use what it wants.
        if (protocol < d_protocol) {
            d_protocol = protocol;
        }

        if (d_protocol != 30 && d_protocol != 29) {
            LOG_FATAL(RSYNC_VER) << "Server only supports protocol " << d_protocol << LOG_END
        }
    } else {
        d_stream->login(command.c_str(), &d_protocol, 0);
    }
    
    if (d_protocol >= 30) {
        uint8_t compatibilityFlag = d_stream->readUInt8(); 
        if (compatibilityFlag & 0x1) {
            LOG_FATAL(RSYNC_COMPAT) << "Server demands incremental directory recursion" << LOG_END
        }

        /*if (compatibilityFlag & 0x8) {
            LOG_FATAL(RSYNC_SAFE_LIST) << "Server demands safe file lists" << LOG_END
        }*/
    }
    
    d_checksumSeed = d_stream->readInt32();

    d_stream->enableBuffer();
    if (d_protocol >= 30) {
        d_stream->enableWriteMultiplex();
    }

    d_lastEntryPath = "";
    d_lastEntryMode = 0;
    d_lastEntryTime = 0;
    
    if (isDownloading || d_deletionEnabled) {
        d_stream->writeInt32(0);
        d_stream->flushWriteBuffer();
    }
}

void Client::stop()
{
    d_io->closeChannel();
}

const std::vector<std::string> &Client::getDeletedFiles()
{
    return d_deletedFiles;
}
    
const std::vector<std::string> &Client::getUpdatedFiles()
{
    return d_updatedFiles;
}


} // namespace rsync

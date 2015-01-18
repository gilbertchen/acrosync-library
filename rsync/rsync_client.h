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

#ifndef INCLUDED_RSYNC_CLIENT_H
#define INCLUDED_RSYNC_CLIENT_H

#include <rsync/rsync_stream.h>
#include <rsync/rsync_io.h>
#include <rsync/rsync_file.h>

#include <block/block_out.h>

#include <string>
#include <vector>
#include <set>

namespace rsync
{

class IO;
class Entry;

class Client
{
public:
    // Create an rsync client on top of the io channel 'io' that has already been connected to the rsync server.
    // 'rsyncCommand' is the path of the rsync executable on the server.  'preferredProtocol' is either 29 or 30.
    // 'd_cancelFlagAddress' points to flag whose value change will abort the sync operation immediately.
    Client(IO*io, const char *rsyncCommand, int preferredProtocol, int *cancelFlagAddress);
    
    // Destructor.  Does not delete the io channel so it can be reused.
    ~Client();

    // List the remote directory 'path' (not recursively).  Each item will be returned by invoking the callback
    // 'entryOut' defined below.
    bool list(const char *path);

    // Perform a full recursive sync (if 'includedFiles' is 0), or a selective sync (of files specified by
    // 'includedFiles'), from the remote directory 'remoteTop' to the local directory 'localTop'.  For each file to
    // be downloaded, the file will be written to 'temporaryFile' first and then moved to its destination after
    // each file transfer.  The names of all downloaded and deleted files can be retrieved by calling
    // 'getUpdatedFiles()' and 'getDeletedFiles()'.
    int download(const char *localTop, const char *remoteTop, const char *temporaryFile,
                 const std::set<std::string> *includeFiles = 0);

    // Perform a sync, recursive or non-recursive, from the local directory 'localPath' to the remote directory
    // 'remoteTop'.  If 'includeFiles' is specified, sync only those files contained in 'includedFiles'.
    int upload(const char *localPath, const char *remoteTop, const std::set<std::string> *includeFiles = 0);

    // Remove the file or directory specified by 'remotePath' on the server.
    void remove(const char *remotePath);

    // Create a new directory 'remotePath' on the server.
    void mkdir(const char *remotePath);

    // Create a symbolic link with the name 'link' that points to 'remotePath' under the same directory.
    void link(const char *remotePath, const char *link);

    // List all modules provided by the rsync daemon server.  Used only in rsync daemon mode.
    void listModules();

    // Add a remote directory 'backupPath' to be used as the parameter for the '--link-dest' option.  Multiple
    // backup paths are allowed.
    void addBackupPath(const char *backupPath);

    // Remove all backup paths.
    void clearBackupPaths();

    // Set the download and upload limits.  The unit is Kilobytes per second.
    void setSpeedLimits(int downloadLimit, int uploadLimit);

    // If 'deletionEnabled' is true, files or dirs that do not exist on the other side will be removed.
    void setDeletionEnabled(bool deletionEnabled);

    // Statistics that will be updated while the sync is in progress.
    // '*totalBytes': the total bytes of all bytes in the source directory
    // '*physicalBytes': the number of bytes that have been transmitted by the network
    // '*logicalBytes': the number of bytes that have been synced.  This value may be greater than '*physicalBytes'
    //                  when the rdiff algorithm is in effect.
    // '*skippedBytes': the bytes of files that do not need to be synced.
    void setStatsAddresses(int64_t *totalBytes, int64_t *physicalBytes, int64_t *logicalBytes, int64_t *skippedBytes);
    
    // Think this as a callback.  The function connected to it will be called when the info about an file/dir entry
    // is about to be sent (for instance, during the 'list' call)
    block::out<void(const char * /*path*/, bool /*isDir*/, int64_t /*size*/, int64_t /*time*/,
                    const char * /*symlink*/)> entryOut;
   
    // Used to output a status message to indicate the sync progress.
    block::out<void(const char * status)> statusOut;
 
    // Return the files that have been created or modified after the sync is complete.  Does not include any
    // directories.
    const std::vector<std::string> &getUpdatedFiles();

    // Return the files that have been deleted by the sync operation.  Does not include any directories.
    const std::vector<std::string> &getDeletedFiles();

private:
    // NOT IMPLEMENTED
    Client(const Client&);
    Client& operator=(const Client&);

    // Resize the size of 'd_chunk'.
    void resizeChunk(int size);

    // Send a series of checksums calcuated from the base file 'oldFile' for the file with the specifed 'index'.
    void sendChecksum(int index, const char *oldFile);

    // Send an entry to the remote server.
    void sendEntry(Entry *entry, bool isTop, bool noDirContent = false);

    // Send a file to the remote server.
    bool sendFile(int index, const char *remotePath, const char *localPath);

    // Receive an entry from the remote server.
    bool receiveEntry(std::string *path, bool *isDir, int64_t *size, int64_t *time, uint32_t *mode,
                      std::string *symlink);

    // Receive a file from the remote server.
    bool receiveFile(const char *remotePath, const char *newFile, const char *oldFile, int64_t *fileSize);

    // Start a new rsync session. 
    void start(const char *remotePath, bool isDownloading, bool recursive, bool isDeleting);

    // Close the rsync session.
    void stop();

    // Read an entry index.
    int readIndex();

    // Write an entry index.
    void writeIndex(int index);

    bool d_usingSSH;               // whether the remote server is an ssh server or an rsync daemon
    IO *d_io;                      // io channel to the remote server
    Stream *d_stream;              // data stream on top of 'd_io'

    std::string d_rsyncCommand;    // the path to the rsync executable on the server

    int d_downloadLimit;           // maximum download speed in kiloBytes/sec
    bool d_deletionEnabled;        // whether to propagate deletions

    std::vector<std::string> d_includePatterns;    // include patterns
    std::vector<std::string> d_excludePatterns;    // exclude patterns

    std::vector<std::string> d_backupPaths;        // paths of previous backups; used by the '--link-desk' option
    
    int32_t d_protocol;            // rsync protocol version
    int32_t d_checksumSeed;        // the seed for checksum calculation
    int64_t d_lastEntryTime;       // the modified time of the last entry transmitted
    uint32_t d_lastEntryMode;      // the file mode of the last entry transmitted
    std::string d_lastEntryPath;   // the path of the last entry transmitted

    struct Checksum
    {
        uint32_t d_sum1;           // the rolling checksum of each block
        uint32_t d_next;           // the next pointer for making a hash table of checksum
        char d_sum2[16];           // the MD5 checksum of of each block
    };

    int d_numberOfChecksums;       // the number of checksums for the current file transfer

    int* d_hashBuckets;            // the entries to the checksum hash table
    Checksum *d_checksums;         // store all the checksum

    char *d_chunk;                 // a chunk buffer used to send or receive file content
    int d_chunkSize;               // the size of 'd_chunk'
    
    int64_t *d_totalBytes;         // the total bytes of all bytes in the source directory
    int64_t *d_physicalBytes;      // the number of bytes that have been transmitted by the network
    int64_t *d_logicalBytes;       // the number of bytes that have been synced
    int64_t *d_skippedBytes;       // the number of bytes that are the same on the both sides.

    int64_t d_dummyCounter;        // used to initialize the above stats counter if they are not being used.
    
    std::vector<std::string> d_deletedFiles;  // files deleted by the current sync
    std::vector<std::string> d_updatedFiles;  // files created or modified by the current sync
    
};


} // namespace rsync

#endif // INCLUDED_RSYNC_CLIENT_H

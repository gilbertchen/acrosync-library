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

#ifndef INCLUDED_RSYNC_STREAM_H
#define INCLUDED_RSYNC_STREAM_H

#include <rsync/rsync_io.h>

#include <string>
#include <vector>

#include <ctime>
#include <cstdint>

namespace rsync
{


// IMPORTANT NOTE:
//   To save developement time, this class only works on little-endian machines, as the rsync protocol transmits
// integers in little-endian format.  Conversion must be added if you want to port this class to big-endian machines.
//
// Blocking model:
// 
// - all reads and writes are all blocking
// - writeAll will try to fill the read buffer when it can't write
// - read will read from the read buffer first, and then from the stream until d_readDataLength is 0
// - write will write to the write buffer first.  If d_automaticFlush is on, it will call flushWriteBuffer when the
//   buffer is full.  Otherwise, it will never block and tryFlushWriteBuffer must be called frequently.
// - flushWriteBuffer is blocking
// - tryFlushWriteBuffer is non-blocking
//
// Potential deadlock scenario:
//
//   read is trying to complete, but the remote side has nothing to send because
//   data is still in the local write buffer and not yet flushed.  This can only happen
//   in download mode (in upload mode the remote side starts the stream).  So make sure
//   not to call read() unless the write buffer has been flushed.


class Stream
{
public:

    // Create a stream on top of 'io'.  If 'cancelFlag' is provide, whenever '*cancelFlag' becomes non-zero, any
    // operation, even a blocking one, will be terminated immediately.
    Stream(IO *io, int *cancelFlag = 0);
    ~Stream();

    // Reset the steam to the intial state.
    void reset();

    enum {INDEX_DONE = -1 };

    // Read 'size' bytes to 'buffer'.  Return 'size' until all bytes have been read.  Note that a write can be
    // buffered or unbuffered
    int read(char *buffer, int size);

    // Wrtie 'size' bytes from 'buffer'.  Return 'size' until all bytes have been written.  Note that a write can be
    // buffered or unbuffered
    int write(const char *buffer, int size);

    // Send out the content in the write buffer.
    void flush();

    // Check if '*d_cancelFlag' has been set.
    void checkCancelFlag() const;

    // A special method for loggin into an rsync daemon.  
    void login(const char *remoteCommand, int *protocol, std::vector<std::string> *modules);
    
    // Turn on read and write buffering.
    void enableBuffer();

    // Turn on write multiplexing, which means it can send not just data, but also other kinds of messages, like
    // logging messages.  Multiplexing is always on for reads.
    void enableWriteMultiplex();

    // Flush the write buffer.  Can take additional data for convenience.
    void flushWriteBuffer(const char *additionalData = 0, int additionalLength = 0);

    // Attempt to flush the write buffer.  Once the flush starts it won't return until done.
    bool tryFlushWriteBuffer();

    // Normally when the write buffer is full it will flush by itself.  This method is to disable this behavior.
    void disableAutomaticFlush();

    // If there is any data available for reading.
    bool isDataAvailable();

    // Limit how fast to send out data.
    void setUploadLimit(int uploadLimit);
    
    // Read a unsigned 8-bit integer
    uint8_t readUInt8();

    // Methods for receiving integers or strings.  
    uint16_t readUInt16();
    int32_t readInt32();
    int64_t readInt64();
    int32_t readVariableInt32();                             // in variable-length integer format
    int64_t readVariableInt64(int minimumBytes);             // in variable-length integer format
    int32_t readIndex();
    std::string readLine();

    // Methods for sending integers or strings
    void writeUInt8(uint8_t);
    void writeUInt16(uint16_t);
    void writeInt32(int32_t);
    void writeInt64(int64_t);
    void writeVariableInt32(int32_t);                        // in variable-length integer format
    void writeVariableInt64(int64_t, int minimumBytes);      // in variable-length integer format
    void writeIndex(int32_t index);
    void writeLine(std::string line, bool appendNewLine);

    // Files deleted on the remote server are sent as DELETION messages.
    const std::vector<std::string> &getDeletedFiles() const
    {
        return d_deletedFiles;
    }
    
private:
    // NOT IMPLEMENTED
    Stream(const Stream&);
    Stream& operator=(const Stream&);

    // Read 'size' bytes into 'buffer'.  Will throw an exception on timeout.
    int readAll(char *buffer, int size);

    // Write 'size' bytes from 'buffer'.  Will throw an exception on timeout.  May also attempt to fill the read buffer
    // if there is any data.
    int writeAll(const char *buffer, int size);

    // Check if the operation timeouts.
    void timedWait(bool isReading, const char *location);

    // Read the message flag (which indicates whether it is a data message or other kinds).  This is a non-blocking call.
    bool readMessageFlag(uint32_t *flag);

    // Read the message content.  This is a blocking call.
    void readMessageContent(uint32_t flag);

    // Convert to a hexical string for easy display.
    std::string toHex(const char *buffer, int size);

    IO *d_io;                           // The io channel
    
    int *d_cancelFlag;                  // The pointer to the cancellation flag

    bool d_isReadBuffered;              // If the read buffer is being used
    bool d_isWriteBuffered;             // If the write buffer is being used

    bool d_isWriteMultiplexed;          // If writing is multiplexed

    int d_readDataLength;               // How many bytes are available for read after the data flag

    char *d_writeBuffer;                // All writes will go to this buffer first in the buffered mode
    int d_writeBufferSize;              // Size of the write buffer
    int d_writeBufferPosition;          // The start of empty space in the writer buffer

    char *d_readBuffer;                 // This buffer is used to store data received during write operations
    int d_readBufferSize;               // Size of the read buffer
    int d_readBufferPosition;           // The start of new data in the read buffer
    int d_readBufferEnd;                // The end of data in the read buffer

    int d_flushStart;                   // Data before this position have been flushed
    bool d_automaticFlush;              // If the write buffer will be automatically flushed

    int d_lastPositiveIndexRead;        // There variables are used by readIndex() and writeIndex();
    int d_lastNegativeIndexRead;
    int d_lastPositiveIndexWritten;
    int d_lastNegativeIndexWritten;

    time_t d_blockedTime;               // The first moment when read/write becomes blcoked

    int d_uploadLimit;                  // How fast to limit sending
    std::vector<int> d_uploadBuckets;   // For speed limiting; each bucket contains the number of bytes sent
                                        // during a period of 1 second
    int64_t d_bucketStartTime;          // The time of the earliest counting bucket
    
    std::vector<std::string> d_deletedFiles;  // Stores files deleted on the remote server

    int d_messageFlag;                  // The message flag; currently only used for tracing
};

} // namespace rsync

#endif // INCLUDED_RSYNC_STREAM_H

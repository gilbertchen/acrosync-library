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

#ifndef INCLUDED_RSYNC_LOG_H
#define INCLUDED_RSYNC_LOG_H

#include <block/block_out.h>

#include <string>
#include <sstream>

namespace rsync {

// The exception to throw if an error has occurred and the operation can't continue.
class Exception
{
public:
    Exception(const char *id, int level, const char *message)
        : d_id(id)
        , d_level(level)
        , d_message(message)
    {
    }

    Exception(const Exception &other)
        : d_id(other.d_id)
        , d_level(other.d_level)
        , d_message(other.d_message)
    {
    }

    ~Exception()
    {
    }

    const char *getID() const
    {
        return d_id;
    }

    int getLevel() const
    {
        return d_level;
    }

    const std::string& getMessage() const
    {
        return d_message;
    }

private:
    // NOT IMPLEMENTED
    const Exception& operator=(const Exception&);

    const char *d_id;          // id of the log message
    int d_level;               // severity level
    std::string d_message;     // the actual log message
};

struct Log
{
public:
    enum Level {
        Debug,
        Trace,
        Info,
        Warning,
        Error,
        Fatal,
        Assert
    };

    // Accessors for 's_level'
    static void setLevel(int);
    static int getLevel();

    // Basically the string name of the level
    static const char *getLiteral(int level);

    // Any log message with this level or above will be reported.  Others will be ignored.
    static int s_level;

    // A callback for replacing the default logging handler.
    static block::out<void (const char *id, int level, const char *message)> out;
};

// A helper object that will generate a log message on destruction.
class LogRecord
{
public:

    LogRecord(const char *id, int level);

    ~LogRecord() throw (Exception);

    LogRecord& operator<<(const char * message)
    {
        d_message += message;
        return *this;
    }

    LogRecord& operator<<(const std::string& message)
    {
        d_message += message;
        return *this;
    }

    template <class T> LogRecord& operator<<(T n)
    {
        std::stringstream stream;
        stream << n;
        d_message += stream.str();
        return *this;
    }

private:
    // NOT IMPLEMENTED
    LogRecord(const LogRecord&);
    LogRecord& operator=(const LogRecord&);

    const char *d_id;
    int d_level;
    std::string d_message;
};

#define RSYNC_LOG(ID, LEVEL) \
    if (LEVEL >= rsync::Log::getLevel()) { \
        rsync::LogRecord(ID, LEVEL)

// Use these macros for creating log messages.
#define LOG_STATUS(ID) RSYNC_LOG(#ID, rsync::Log::Status)
#define LOG_DEBUG(ID) RSYNC_LOG(#ID, rsync::Log::Debug)
#define LOG_TRACE(ID) RSYNC_LOG(#ID, rsync::Log::Trace)
#define LOG_INFO(ID) RSYNC_LOG(#ID, rsync::Log::Info)
#define LOG_WARNING(ID) RSYNC_LOG(#ID, rsync::Log::Warning)
#define LOG_ERROR(ID) RSYNC_LOG(#ID, rsync::Log::Error)
#define LOG_FATAL(ID) RSYNC_LOG(#ID, rsync::Log::Fatal)
#define LOG_ASSERT(ID) RSYNC_LOG(#ID, rsync::Log::Assert)

#define LOG_END ""; }


}

#endif // INCLUDED_RSYNC_LOG_H

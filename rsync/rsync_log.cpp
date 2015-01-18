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

#include <rsync/rsync_log.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include <qi/qi_build.h>

namespace rsync
{

int Log::s_level = Log::Info;

block::out<void (const char *id, int level, const char *message)> Log::out;

void Log::setLevel(int level)
{
    s_level = level;
}

int Log::getLevel()
{
    return s_level;
}

const char *Log::getLiteral(int level)
{
    switch (level) {
        case Log::Debug: return "DEBUG";
        case Log::Trace: return "TRACE";
        case Log::Info: return "INFO";
        case Log::Warning: return "WARNING";
        case Log::Error: return "ERROR";
        case Log::Fatal: return "FATAL";
        case Log::Assert: return "ASSERT";
        default: return "UNDEFINED";
    }
}

LogRecord::LogRecord(const char *id, int level)
    : d_id(id)
    , d_level(level)
    , d_message()
{
}

LogRecord::~LogRecord() throw (Exception)
{
    // If a custom logging handler was installed, call that handler
    if (Log::out.isConnected()) {
        Log::out(d_id, d_level, d_message.c_str());
        return;
    }

    // Otherwise send the log message to stdout, and throw an exception for an error.
    std::string message = Log::getLiteral(d_level);
    message += " ";

    time_t t = ::time(0);
    struct tm *timeInfo = localtime(&t);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%m/%d/%y %H:%M:%S", timeInfo);
    message += buffer;
    message += " ";
    message += d_id;
    message += " ";
    message += d_message;

    ::printf("%s\n", message.c_str());
    if (d_level > Log::Error) {
        throw Exception(d_id, d_level, message.c_str());
    }
}

} // namespace rsync

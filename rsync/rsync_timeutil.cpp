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

#include <rsync/rsync_timeutil.h>

#include <qi/qi_build.h>


#if defined(WIN32) || defined(__MINGW32__)

#include <windows.h>

namespace rsync
{

int64_t TimeUtil::getUnixTime(uint32_t windowsTimeHigh, uint32_t windowsTimeLow)
{
    return ((uint64_t(windowsTimeHigh) << 32) | windowsTimeLow) / 10000000 - 11644473600ll;
}

void TimeUtil::getWindowsTime(int64_t unixTime, uint32_t *windowsTimeHigh, uint32_t *windowsTimeLow)
{
    unixTime += 11644473600ll;
    unixTime *= 10000000;
    *windowsTimeHigh = unixTime >> 32;
    *windowsTimeLow = unixTime & 0xffffffff;
}

int64_t TimeUtil::getTimeOfDay()
{
    FILETIME windowTime;
    GetSystemTimeAsFileTime(&windowTime);

    uint64_t unixTime = ((uint64_t(windowTime.dwHighDateTime) << 32) | windowTime.dwLowDateTime) - 116444736000000000ULL;
    unixTime /= 10;
    return unixTime;
}

void TimeUtil::sleep(int milliseconds)
{
    Sleep(milliseconds);
}

} // close namespace rsync

#else // non-windows

#include <time.h>
#include <sys/time.h>

namespace rsync {

int64_t TimeUtil::getTimeOfDay()
{
    struct timeval t;
    gettimeofday(&t, 0);
    int64_t value = static_cast<int64_t>(t.tv_sec);
    value = value * 1000000 + t.tv_usec;
    return value;   
}

void TimeUtil::sleep(int milliseconds)
{
    struct timespec delay;
    delay.tv_sec = milliseconds / 1000;
    delay.tv_nsec = 1000000 * (milliseconds % 1000);
    nanosleep(&delay, 0);
}

} // close namesapce rsync

#endif




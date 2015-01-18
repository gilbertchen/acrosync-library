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

#ifndef INCLUDED_RSYNC_TIMEUTIL_H
#define INCLUDED_RSYNC_TIMEUTIL_H

#include <cstdint>

#include <time.h>

namespace rsync
{

struct TimeUtil
{
#if defined(WIN32) || defined(__MINGW32__)
    // Convert a Windows time into a Unix time.
    static int64_t getUnixTime(uint32_t windowsTimeHigh, uint32_t windowsTimeLow);

    // Convert a Unix time to a Windows time.
    static void getWindowsTime(int64_t unixTime, uint32_t *windowsTimeHigh, uint32_t *windowsTimeLow);
#endif
	
	// Return current time in microseconds.
    static int64_t getTimeOfDay();

    // Sleep for the specified time.
    static void sleep(int milliseconds);
};

} // namespace rsync
#endif //INCLUDED_RSYNC_TIMEUTIL_H

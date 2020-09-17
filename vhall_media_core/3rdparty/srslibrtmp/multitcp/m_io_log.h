/*
 *  Copyright (C) 2008-2009 Andrej Stepanchuk
 *  Copyright (C) 2009-2010 Howard Chu
 *
 *  This file is part of libM_IO.
 *
 *  libM_IO is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1,
 *  or (at your option) any later version.
 *
 *  libM_IO is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libM_IO see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/lgpl.html
 */

#ifndef __M_IO_LOG_H__
#define __M_IO_LOG_H__
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
    /* Enable this to get full debugging output */
    /* #define _DEBUG */

#ifdef _DEBUG
#undef NODEBUG
#endif
	char* GetFormatDate();
    typedef enum
    {
        M_IO_LOGCRIT=0, M_IO_LOGERROR, M_IO_LOGWARNING, M_IO_LOGINFO,
        M_IO_LOGDEBUG, M_IO_LOGDEBUG2, M_IO_LOGALL
    }
    M_IO_LogLevel;

    extern M_IO_LogLevel M_IO_debuglevel;

    typedef void (M_IO_LogCallback)(int level, const char *fmt, va_list);
    void M_IO_LogSetCallback(M_IO_LogCallback *cb);
    void M_IO_LogSetOutput(FILE *file);
#ifdef __GNUC__
    void M_IO_LogPrintf(const char *format, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
    void M_IO_LogStatus(const char *format, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
    void M_IO_Log(int level, const char *format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));
#else
    void M_IO_LogPrintf(const char *format, ...);
    void M_IO_LogStatus(const char *format, ...);
    void M_IO_Log(int level, const char *format, ...);
#endif
    void M_IO_LogHex(int level, const uint8_t *data, unsigned long len);
    void M_IO_LogHexString(int level, const uint8_t *data, unsigned long len);
    void M_IO_LogSetLevel(M_IO_LogLevel lvl);
    M_IO_LogLevel M_IO_LogGetLevel(void);

#ifdef __cplusplus
}
#endif

#endif

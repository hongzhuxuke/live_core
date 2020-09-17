#include "m_io_log.h"
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#include <sys/time.h>
#endif
#include <stdio.h>
#include <stdlib.h>

char TimeBuf[255];
#ifdef _MSC_VER	/* MSVC */
#define snprintf _snprintf
#endif

char* GetFormatDate(){
	memset(TimeBuf, 0, 255);
#ifdef WIN32
	SYSTEMTIME sys;
	GetLocalTime(&sys);
	snprintf(TimeBuf, 255, "%d-%02d-%02d %02d:%02d:%02d.%03d",
		sys.wYear, sys.wMonth, sys.wDay,
		sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds);
#else
	struct timeval  tv;
	struct tm       *p;
	gettimeofday(&tv, NULL);
	p = localtime(&tv.tv_sec);
	snprintf(TimeBuf, 255, "%d-%02d-%02d %02d:%02d:%02d.%03d",
		1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday,
		p->tm_hour, p->tm_min, p->tm_sec, (int)(tv.tv_usec / 1000));
#endif
	return TimeBuf;
}

#define MAX_PRINT_LEN	2048

M_IO_LogLevel M_IO_debuglevel = M_IO_LOGERROR;

static int neednl;

static FILE *fmsg;

static M_IO_LogCallback m_io_log_default, *cb = m_io_log_default;

static const char *levels[] =
{
    "CRIT", "ERROR", "WARNING", "INFO",
    "DEBUG", "DEBUG2"
};

static void m_io_log_default(int level, const char *format, va_list vl)
{
    char str[MAX_PRINT_LEN]="";

    vsnprintf(str, MAX_PRINT_LEN-1, format, vl);

    /* Filter out 'no-name' */
    if ( M_IO_debuglevel<M_IO_LOGALL && strstr(str, "no-name" ) != NULL )
        return;

    if ( !fmsg ) fmsg = stderr;

    if ( level <= M_IO_debuglevel )
    {
        if (neednl)
        {
            putc('\n', fmsg);
            neednl = 0;
        }
		fprintf(fmsg, "%s: [%s] %s\n", GetFormatDate(), levels[level], str);
#ifdef _DEBUG
        fflush(fmsg);
#endif
    }
}

void M_IO_LogSetOutput(FILE *file)
{
    fmsg = file;
}

void M_IO_LogSetLevel(M_IO_LogLevel level)
{
    M_IO_debuglevel = level;
}

void M_IO_LogSetCallback(M_IO_LogCallback *cbp)
{
    cb = cbp;
}

M_IO_LogLevel M_IO_LogGetLevel()
{
    return M_IO_debuglevel;
}

void M_IO_Log(int level, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    cb(level, format, args);
    va_end(args);
}

static const char hexdig[] = "0123456789abcdef";

void M_IO_LogHex(int level, const uint8_t *data, unsigned long len)
{
    unsigned long i;
    char line[50], *ptr;

    if ( level > M_IO_debuglevel )
        return;

    ptr = line;

    for(i=0; i<len; i++)
    {
        *ptr++ = hexdig[0x0f & (data[i] >> 4)];
        *ptr++ = hexdig[0x0f & data[i]];
        if ((i & 0x0f) == 0x0f)
        {
            *ptr = '\0';
            ptr = line;
            M_IO_Log(level, "%s", line);
        }
        else
        {
            *ptr++ = ' ';
        }
    }
    if (i & 0x0f)
    {
        *ptr = '\0';
        M_IO_Log(level, "%s", line);
    }
}

void M_IO_LogHexString(int level, const uint8_t *data, unsigned long len)
{
#define BP_OFFSET 9
#define BP_GRAPH 60
#define BP_LEN	80
    char	line[BP_LEN];
    unsigned long i;

    if ( !data || level > M_IO_debuglevel )
        return;

    /* in case len is zero */
    line[0] = '\0';

    for ( i = 0 ; i < len ; i++ )
    {
        int n = i % 16;
        unsigned off;

        if( !n )
        {
            if( i ) M_IO_Log( level, "%s", line );
            memset( line, ' ', sizeof(line)-2 );
            line[sizeof(line)-2] = '\0';

            off = i % 0x0ffffU;

            line[2] = hexdig[0x0f & (off >> 12)];
            line[3] = hexdig[0x0f & (off >>  8)];
            line[4] = hexdig[0x0f & (off >>  4)];
            line[5] = hexdig[0x0f & off];
            line[6] = ':';
        }

        off = BP_OFFSET + n*3 + ((n >= 8)?1:0);
        line[off] = hexdig[0x0f & ( data[i] >> 4 )];
        line[off+1] = hexdig[0x0f & data[i]];

        off = BP_GRAPH + n + ((n >= 8)?1:0);

        if (isprint( data[i] ))
        {
            line[BP_GRAPH + n] = data[i];
        }
        else
        {
            line[BP_GRAPH + n] = '.';
        }
    }

    M_IO_Log( level, "%s", line );
}

/* These should only be used by apps, never by the library itself */
void M_IO_LogPrintf(const char *format, ...)
{
    char str[MAX_PRINT_LEN]="";
    int len;
    va_list args;
    va_start(args, format);
    len = vsnprintf(str, MAX_PRINT_LEN-1, format, args);
    va_end(args);

    if ( M_IO_debuglevel==M_IO_LOGCRIT )
        return;

    if ( !fmsg ) fmsg = stderr;

    if (neednl)
    {
        putc('\n', fmsg);
        neednl = 0;
    }

    if (len > MAX_PRINT_LEN-1)
        len = MAX_PRINT_LEN-1;
    fprintf(fmsg, "%s", str);
    if (str[len-1] == '\n')
        fflush(fmsg);
}

void M_IO_LogStatus(const char *format, ...)
{
    char str[MAX_PRINT_LEN]="";
    va_list args;
    va_start(args, format);
    vsnprintf(str, MAX_PRINT_LEN-1, format, args);
    va_end(args);

    if ( M_IO_debuglevel==M_IO_LOGCRIT )
        return;

    if ( !fmsg ) fmsg = stderr;

    fprintf(fmsg, "%s", str);
    fflush(fmsg);
    neednl = 1;
}

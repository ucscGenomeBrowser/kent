/* log.c - logging for servers, can log to a file and/or syslog.  Compile with
 * -DNO_SYSLOG for systems without syslog. */

#include "common.h"
#include "errabort.h"
#include "log.h"
#ifndef NO_SYSLOG
#define SYSLOG_NAMES  /* this gets the facilitynames compiled in */
#include <syslog.h>
#endif
#include <time.h>

static char *gProgram = "unknown";  /* name of program */
static boolean gSysLogOn = FALSE;   /* syslog logging enabled? */
static FILE *gLogFh = NULL;         /* logging file */

static void logWarnHander(char *format, va_list args)
/* Warn handler that logs message. */
{
/* use logError, since errAbort and warn all print through warn handler */
logErrorVa(format, args);
}

static void logAbortHandler()
/* abort handler that logs this fact and exits. */
{
logError("%s aborted", gProgram);
fprintf(stderr, "aborted");
exit(1);
}

static void setProgram(char* program)
/* set the program name, removing leading directories from file */
{
char name[128], ext[64];
int len;
splitPath(program, NULL, name, ext);
len = strlen(name) + strlen(ext) + 1;
gProgram = needMem(len);
strcpy(gProgram, name);
if (ext[0] != '\0')
    strcat(gProgram, ext); /* includes dot */
}

#ifndef NO_SYSLOG
static int parseFacility(char *facility)
/* parse a facility name into a number, or use default if NULL. */
{
int i;
/* it appears that defining SYSLOG_NAMES before including syslog.h gets the
 * facilitynames compiled in on most (all?) implementations.  if this doesn't
 * prove portable, will have have a fixed list here */
if (facility == NULL)
    return LOG_LOCAL0;
for (i = 0; facilitynames[i].c_name != NULL; i++)
    {
    if (sameString(facilitynames[i].c_name, facility))
        return facilitynames[i].c_val;
    }
errAbort("invalid log facility: %s", facility);
return 0; /* never reached */
}
#endif

void logOpenSyslog(char* program, char *facility)
/* Initialize syslog using the specified facility.  Facility is the syslog
 * facility as specified in syslog.conf.  If facility is NULL, local0 is used.
 * This adds a warn and errAbort handlers that do logging.  If custom handlers
 * are added, they should call logErrorVa().
 */
{
#ifndef NO_SYSLOG
setProgram(program);
openlog(program, LOG_PID, parseFacility(facility));
pushWarnHandler(logWarnHander);
pushAbortHandler(logAbortHandler);
gSysLogOn = TRUE;
#else
errAbort("syslog support was not compiled into %s", __FILE__);
#endif
}

void logOpenFile(char* program, char *logFile)
/* Initialize logging to the specified file.  Append to the file if it exists.
 * This adds a warn and errAbort handlers that do logging.  If custom handlers
 * are added, they should call logErrorVa().
 */
{
setProgram(program);
gLogFh = mustOpen(logFile, "a");
pushWarnHandler(logWarnHander);
pushAbortHandler(logAbortHandler);
}

FILE *logGetFile()
/* Returns the log FILE object if file logging is enabled, or NULL if it
 * isn't. This is useful for logging debugging data that doesn't fit the log
 * message paradigm, For example, logging fasta records. */
{
return gLogFh;
}

static void logFilePrint(char* level, char *format, va_list args)
/* write a message to the log file */
{
static char *timeFmt = "%Y/%m/%d %H:%M:%S";
char timeBuf[128];
time_t curTime = time(NULL);
strftime(timeBuf, sizeof(timeBuf), timeFmt, localtime(&curTime));
fprintf(gLogFh, "%s: %s: ", timeBuf, level);
vfprintf(gLogFh, format, args);
fputc('\n', gLogFh);
fflush(gLogFh);
}

void logErrorVa(char *format, va_list args)
/* Variable args logError. */
{
#ifndef NO_SYSLOG
if (gSysLogOn)
    vsyslog(LOG_ERR, format, args);
#endif
if (gLogFh != NULL)
    logFilePrint("error", format, args);
}

void logError(char *format, ...)
/* Log an error message. */
{
va_list args;
va_start(args, format);
logErrorVa(format, args);
va_end(args);
}

void logWarnVa(char *format, va_list args)
/* Variable args logWarn. */
{
#ifndef NO_SYSLOG
if (gSysLogOn)
    vsyslog(LOG_WARNING, format, args);
#endif
if (gLogFh != NULL)
    logFilePrint("warn", format, args);
}

void logWarn(char *format, ...)
/* Log a warn message. */
{
va_list args;
va_start(args, format);
logWarnVa(format, args);
va_end(args);
}

void logInfoVa(char *format, va_list args)
/* Variable args logInfo. */
{
#ifndef NO_SYSLOG
if (gSysLogOn)
    vsyslog(LOG_INFO, format, args);
#endif
if (gLogFh != NULL)
    logFilePrint("info", format, args);
}

void logInfo(char *format, ...)
/* Log an info message. */
{
va_list args;
va_start(args, format);
logInfoVa(format, args);
va_end(args);
}

void logDebugVa(char *format, va_list args)
/* Variable args logDebug. */
{
#ifndef NO_SYSLOG
if (gSysLogOn)
    vsyslog(LOG_DEBUG, format, args);
#endif
if (gLogFh != NULL)
    logFilePrint("debug", format, args);
}

void logDebug(char *format, ...)
/* Log a debug message. */
{
va_list args;
va_start(args, format);
logDebugVa(format, args);
va_end(args);
}

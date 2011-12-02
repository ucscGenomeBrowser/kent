/* log.c - logging for servers, can log to a file and/or syslog.  Compile with
 * -DNO_SYSLOG for systems without syslog. */

#include "common.h"
#include "log.h"
#include "errabort.h"
#include "dystring.h"
#include "options.h"
#include "portable.h"

#ifndef NO_SYSLOG
#include <syslog.h>
#endif
#include <time.h>


static char *gProgram = "unknown";  /* name of program */
static boolean gSysLogOn = FALSE;   /* syslog logging enabled? */
static FILE *gLogFh = NULL;         /* logging file */

struct nameVal
/* pair of string name and integer value */
{
    char *name;
    int val;
};

#ifndef NO_SYSLOG

static struct nameVal facilityNameTbl[] =
/* not all version of syslog have the facilitynames table, so  define our own */
{
    {"auth",         LOG_AUTH},
#ifdef LOG_AUTHPRIV
    {"authpriv",     LOG_AUTHPRIV},
#endif
    {"cron",         LOG_CRON},
    {"daemon",       LOG_DAEMON},
#ifdef LOG_FTP
    {"ftp",          LOG_FTP},
#endif
    {"kern",         LOG_KERN},
    {"lpr",          LOG_LPR},
    {"mail",         LOG_MAIL},
    {"news",         LOG_NEWS},
    {"syslog",       LOG_SYSLOG},
    {"user",         LOG_USER},
#ifdef LOG_UUCP
    {"uucp",         LOG_UUCP},
#endif
    {"local0",       LOG_LOCAL0},
    {"local1",       LOG_LOCAL1},
    {"local2",       LOG_LOCAL2},
    {"local3",       LOG_LOCAL3},
    {"local4",       LOG_LOCAL4},
    {"local5",       LOG_LOCAL5},
    {"local6",       LOG_LOCAL6},
    {"local7",       LOG_LOCAL7},
    {NULL,           0}
};
#endif

/* Priority numbers and names used for setting minimum priority to log.  This
 * is kept independent of syslog, so it works on file logging too.  */
#define	PRI_EMERG	0
#define	PRI_ALERT	1
#define	PRI_CRIT	2
#define	PRI_ERR		3
#define	PRI_WARNING	4
#define	PRI_NOTICE	5
#define	PRI_INFO	6
#define	PRI_DEBUG	7

static struct nameVal priorityNameTbl[] = {
    {"panic", PRI_EMERG},
    {"emerg", PRI_EMERG},
    {"alert", PRI_ALERT},
    {"crit", PRI_CRIT},
    {"err", PRI_ERR},
    {"error", PRI_ERR},
    {"warn", PRI_WARNING},
    {"warning", PRI_WARNING},
    {"notice", PRI_NOTICE},
    {"info", PRI_INFO},
    {"debug", PRI_DEBUG},
    {NULL, -1}
    };

static int gMinPriority = PRI_INFO;  // minimum priority to log (reverse numbering)

static int nameValTblFind(struct nameVal *tbl, char *name)
/* search a nameVal table, return -1 if not found */
{
int i;
for (i = 0; tbl[i].name != NULL; i++)
    {
    if (sameString(tbl[i].name, name))
        return tbl[i].val;
    }
return -1;
}

static char *nameValTblMsg(struct nameVal *tbl)
/* generate a message for values in table */
{
struct dyString *msg = dyStringNew(256);
int i;
for (i = 0; tbl[i].name != NULL; i++)
    {
    if (i > 0)
        dyStringAppend(msg, ", ");
    dyStringAppend(msg, tbl[i].name);
    }
return dyStringCannibalize(&msg);
}

static void logWarnHandler(char *format, va_list args)
/* Warn handler that logs message. */
{
if (isErrAbortInProgress())
    logErrorVa(format, args);
else
    logWarnVa(format, args);
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
if (facility == NULL)
    return LOG_LOCAL0;
int val = nameValTblFind(facilityNameTbl, facility);
if (val < 0)
    errAbort("invalid log facility: %s, expected one of: %s", facility, nameValTblMsg(facilityNameTbl));
return val;
}
#endif

static int parsePriority(char *pri)
/* parse a priority name into a number, or use default if NULL. */
{
if (pri == NULL)
    return PRI_INFO;
int val = nameValTblFind(priorityNameTbl, pri);
if (val < 0)
    errAbort("invalid log priority: %s, expected one of: %s", pri, nameValTblMsg(priorityNameTbl));
return val;
}

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
pushWarnHandler(logWarnHandler);
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
pushWarnHandler(logWarnHandler);
pushAbortHandler(logAbortHandler);
}

void logSetMinPriority(char *minPriority)
/* set minimum priority to log, which is one of the syslog priority names,
 * even when logging to a file */
{
gMinPriority = parsePriority(minPriority);
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
if (gMinPriority >= PRI_ERR)
    {
#ifndef NO_SYSLOG
    if (gSysLogOn)
        vsyslog(LOG_ERR, format, args);
#endif
    if (gLogFh != NULL)
        logFilePrint("error", format, args);
    }
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
if (gMinPriority >= PRI_WARNING)
    {
#ifndef NO_SYSLOG
    if (gSysLogOn)
        vsyslog(LOG_WARNING, format, args);
#endif
    if (gLogFh != NULL)
        logFilePrint("warn", format, args);
    }
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
if (gMinPriority >= PRI_INFO)
    {
#ifndef NO_SYSLOG
    if (gSysLogOn)
        vsyslog(LOG_INFO, format, args);
#endif
    if (gLogFh != NULL)
        logFilePrint("info", format, args);
    }
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
if (gMinPriority >= PRI_DEBUG)
    {
#ifndef NO_SYSLOG
    if (gSysLogOn)
        vsyslog(LOG_DEBUG, format, args);
#endif
    if (gLogFh != NULL)
        logFilePrint("debug", format, args);
    }
}

void logDebug(char *format, ...)
/* Log a debug message. */
{
va_list args;
va_start(args, format);
logDebugVa(format, args);
va_end(args);
}

void logDaemonize(char *progName)
/* daemonize parasol server process, closing open file descriptors and
 * starting logging based on the -logFacility and -log command line options .
 * if -debug is supplied , don't fork. */
{
if (!optionExists("debug"))
    {
    int i, maxFiles = getdtablesize();
    if (mustFork() != 0)
        exit(0);  /* parent goes away */

    /* Close all open files first (before logging) */
    for (i = 0; i < maxFiles; i++)
        close(i);
    }

/* Set up log handler. */
if (optionExists("log"))
    logOpenFile(progName, optionVal("log", NULL));
else    
    logOpenSyslog(progName, optionVal("logFacility", NULL));
}


#include "paraCommon.h"
#include <sys/utsname.h>

#define SYSLOG_NAMES  /* this gets the facilitynames compiled in */
#include <syslog.h>

#include "options.h"
#include "errabort.h"
#include "net.h"
#include "paraLib.h"

static char *gProgram = "unknown";  /* name of program if logging */

time_t now;	/* Time when started processing current message */

void findNow()
/* Just set now to current time. */
{
now = time(NULL);
}

char paraSig[17] = "0d2f070562685f29";  /* Mild security measure. */
int paraSigSize = 16;

int paraHubPort = 0x46DC;		      /* Our hub port */
int paraNodePort = 0x46DD;		      /* Our node port */

char *getMachine()
/* Return host name. */
{
static char *host = NULL;
if (host == NULL)
    {
    host = getenv("HOST");
    if (host == NULL)
	{
	struct utsname unamebuf;
	if (uname(&unamebuf) < 0)
	    errAbort("Couldn't find HOST environment variable or good uname");
	host = unamebuf.nodename;
	}
    host = cloneString(host);
    }
return host;
}

int forkOrDie()
/* Fork, aborting if it fails. */
{
int childId = fork();
if (childId == -1)
    errnoAbort("Unable to fork");
return childId;
}

boolean parseRunJobMessage(char *line, struct runJobMessage *rjm)
/* Parse runJobMessage as paraNodes sees it. */
{
bool ret;
rjm->jobIdString = nextWord(&line);
rjm->reserved = nextWord(&line);
rjm->user = nextWord(&line);
rjm->dir = nextWord(&line);
rjm->in = nextWord(&line);
rjm->out = nextWord(&line);
rjm->err = nextWord(&line);
rjm->command = line = skipLeadingSpaces(line);
ret = (line != NULL && line[0] != 0);
if (!ret)
    warn("Badly formatted jobMessage");
return ret;
}

void fillInErrFile(char errFile[512], int jobId, char *tempDir )
/* Fill in error file name */
{
sprintf(errFile, "%s/para%d.err", tempDir, jobId);
}

void vLogDebug(char *format, va_list args)
/* Variable args logDebug. */
{
vsyslog(LOG_DEBUG, format, args);
}

void logDebug(char *format, ...)
/* Log a debug message. */
{
va_list args;
va_start(args, format);
vLogDebug(format, args);
va_end(args);
}

void vLogInfo(char *format, va_list args)
/* Variable args logInfo. */
{
vsyslog(LOG_INFO, format, args);
}

void logInfo(char *format, ...)
/* Log a info message. */
{
va_list args;
va_start(args, format);
vLogInfo(format, args);
va_end(args);
}

static void warnToLog(char *format, va_list args)
/* Warn handler that prints to log file. */
{
vsyslog(LOG_WARNING, format, args);
}

static void logAndExit()
/* abort handler that logs this fact and exits. */
{
vsyslog(LOG_ERR, "%s aborted", gProgram);
exit(1);
}

static int parseFacility(char *facility)
/* parse a facility name into a number, or use default if NULL. */
{
int i;
/* it appears that defining SYSLOG_NAMES before including syslog.h gets the
 * facilitynames compiled in on most (all?) implementations.  if this doesn't
 * prove protable, we will have code a fixed list here */
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

void logOpen(char *program, char *facility)
/* Setup logging.  This initializes syslog using the specified facility.
 * Facility is the syslog facility as specified in syslog.conf.  If facility
 * is NULL, local0 is used.  This adds a warn handlers that logs at level
 * error for warn() and errAbort() calls. Use logInfo and logDebug for info
 * and debug messages. */
{
gProgram = cloneString(program);
openlog(program, LOG_PID, parseFacility(facility));
pushWarnHandler(warnToLog);
pushAbortHandler(logAndExit);
}

#include <sys/utsname.h>
#include "common.h"
#include "errabort.h"
#include "net.h"
#include "paraLib.h"


char paraSig[17] = "0d2f070562685f29";  /* Mild security measure. */
int paraSigSize = 16;
int paraPort = 0x46DC;		      /* Our port */

boolean sendWithSig(int fd, char *string)
/* Send a string with the signature prepended.  Warn 
 * but don't abort if there's a problem.  Return TRUE
 * on success. */
{
if (write(fd, paraSig, paraSigSize) < paraSigSize)
    {
    warn("Couldn't write signature to socket");
    return FALSE;
    }
return netSendLongString(fd, string);
}

void mustSendWithSig(int fd, char *string)
/* Send a string with the signature prepended. 
 * Abort on failure. */
{
if (!sendWithSig(fd, string))
    noWarnAbort();
}

char *getHost()
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

static FILE *logFile = NULL;  /* Log file - if NULL no logging. */

void vLogIt(char *format, va_list args)
/* Variable args logit. */
{
if (logFile != NULL)
    {
    vfprintf(logFile, format, args);
    fflush(logFile);
    }
}

void logIt(char *format, ...)
/* Print message to log file. */
{
if (logFile != NULL)
    {
    va_list args;
    va_start(args, format);
    vLogIt(format, args);
    va_end(args);
    }
}

static void warnToLog(char *format, va_list args)
/* Warn handler that prints to log file. */
{
if (logFile != NULL)
    {
    fputs("warn: ", logFile);
    vfprintf(logFile, format, args);
    fputs("\n", logFile);
    fflush(logFile);
    }
}

void setupDaemonLog(char *fileName)
/* Setup log file, and warning handler that goes to this
 * file.  If fileName is NULL then no log, and warning
 * messages go into the bit bucket. */
{
if (fileName != NULL)
    logFile = mustOpen(fileName, "w");
pushWarnHandler(warnToLog);
}

void logClose()
/* Close log file. */
{
carefulClose(&logFile);
}

/* verbose.c - write out status messages according to the
 * current verbosity level.  These messages go to stderr. */

#include "common.h"
#include "verbose.h"

static char const rcsid[] = "$Id: verbose.c,v 1.1 2004/02/23 09:07:24 kent Exp $";

static int logVerbosity = 1;	/* The level of log verbosity.  0 is silent. */
static FILE *logFile;	/* File to log to. */

void verboseVa(int verbosity, char *format, va_list args)
/* Log with at given verbosity vprintf formatted args. */
{
if (verbosity <= logVerbosity)
    {
    if (logFile == NULL)
        logFile = stderr;
    vfprintf(logFile, format, args);
    fflush(logFile);
    }
}

void verbose(int verbosity, char *format, ...)
/* Write printf formatted message to log (which by
 * default is stderr) if global verbose variable
 * is set to verbosity or higher. */
{
va_list args;
va_start(args, format);
verboseVa(verbosity, format, args);
va_end(args);
}

void verboseDot()
/* Write I'm alive dot (at verbosity level 1) */
{
verbose(1, ".");
}

void verboseSetLevel(int verbosity)
/* Set verbosity level in log.  0 for no logging,
 * higher number for increasing verbosity. */
{
logVerbosity = verbosity;
}

int verboseLevel()
/* Get verbosity level. */
{
return logVerbosity;
}




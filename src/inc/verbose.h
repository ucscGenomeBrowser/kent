/* verbose.h - write out status messages according to the
 * current verbosity level.  These messages go to stderr. */

#ifndef VERBOSE_H
#define VERBOSE_H

void verbose(int verbosity, char *format, ...)
/* Write printf formatted message to log (which by
 * default is stderr) if global verbose variable
 * is set to verbosity or higher. */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
    ;

void verboseVa(int verbosity, char *format, va_list args);
/* Log with at given verbosity vprintf formatted args. */

void verboseTimeInit(void);
/* Initialize or reinitialize the previous time for use by verboseTime. */

void verboseTime(int verbosity, char *label, ...)
/* Print label and how long it's been since last call.  Start time can be
 * initialized with verboseTimeInit, otherwise the elapsed time will be
 * zero. */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
    ;

void verboseDot();
/* Write I'm alive dot (at verbosity level 1) */

boolean verboseDotsEnabled();
/* check if outputting of happy dots are enabled.  They will be enabled if the
 * verbosity is > 0, stderr is a tty and we don't appear to be running an
 * emacs shell. */

int verboseLevel(void);
/* Get verbosity level. */

void verboseSetLevel(int verbosity);
/* Set verbosity level in log.  0 for no logging,
 * higher number for increasing verbosity. */

void verboseSetLogFile(char *name);
/* Set logFile for verbose messages overrides stderr. */

FILE *verboseLogFile();
/* Get the verbose log file. */

#endif /* VERBOSE_H */


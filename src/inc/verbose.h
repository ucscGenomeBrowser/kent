/* verbose.h - write out status messages according to the
 * current verbosity level.  These messages go to stderr. */

#ifndef VERBOSE_H
#define VERBOSE_H

void verbose(int verbosity, char *format, ...)
/* Write printf formatted message to log (which by
 * default is stderr) if global verbose variable
 * is set to verbosity or higher. */
#if defined(__GNUC__) && defined(JK_WARN)
__attribute__((format(printf, 2, 3)))
#endif
    ;

void verboseVa(int verbosity, char *format, va_list args);
/* Log with at given verbosity vprintf formatted args. */

void verboseDot();
/* Write I'm alive dot (at verbosity level 1) */

int verboseLevel();
/* Get verbosity level. */

void verboseSetLevel(int verbosity);
/* Set verbosity level in log.  0 for no logging,
 * higher number for increasing verbosity. */

#endif /* VERBOSE_H */


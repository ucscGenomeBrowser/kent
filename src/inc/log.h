/* log.h - write out log messages according to the
 * current verbosity level.  By default these messages
 * go to stderr, but they can end up going to a
 * file (and maybe eventually the system logging facility)
 * instead. */

#ifndef LOG_H
#define LOG_H

void logPrintf(int verbosity, char *format, ...)
/* Write printf formatted message to log (which by
 * default is stderr) if global verbose variable
 * is set to verbosity or higher. */
#if defined(__GNUC__) && defined(JK_WARN)
__attribute__((format(printf, 2, 3)))
#endif
    ;

void logVaPrintf(int verbosity, char *format, va_list args);
/* Log with at given verbosity vprintf formatted args. */

void logDot();
/* Write I'm alive dot (at verbosity level 1) */

void logToFile(char *fileName);
/* Set logging to named file rather than stderr. */

void logSetVerbosity(int verbosity);
/* Set verbosity level in log.  0 for no logging,
 * higher number for increasing verbosity. */

#endif /* LOG_H */


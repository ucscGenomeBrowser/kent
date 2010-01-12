/* log.h - logging for servers, can log to a file and/or syslog.  Compile with
 * -DNO_SYSLOG for systems without syslog. */

#ifndef LOG_H
#define LOG_H

void logOpenSyslog(char* program, char *facility);
/* Initialize syslog using the specified facility.  Facility is the syslog
 * facility as specified in syslog.conf.  If facility is NULL, local0 is used.
 * This adds a warn and errAbort handlers that do logging.  If custom handlers
 * are added, they should call logErrorVa().
 */

void logOpenFile(char* program, char *logFile);
/* Initialize logging to the specified file.  Append to the file if it exists.
 * This adds a warn and errAbort handlers that do logging.  If custom handlers
 * are added, they should call logErrorVa(). 
 */

void logSetMinPriority(char *minPriority);
/* set minimum priority to log, which is one of the syslog priority names,
 * even when logging to a file */

FILE *logGetFile();
/* Returns the log FILE object if file logging is enabled, or NULL if it
 * isn't. This is useful for logging debugging data that doesn't fit the log
 * message paradigm, For example, logging fasta records. */

void logErrorVa(char *format, va_list args);
/* Variable args logError. */

void logError(char *format, ...)
/* Log an error message. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

void logWarnVa(char *format, va_list args);
/* Variable args logWarn. */

void logWarn(char *format, ...)
/* Log a warn message. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

void logInfoVa(char *format, va_list args);
/* Variable args logInfo. */

void logInfo(char *format, ...)
/* Log an info message. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

void logDebugVa(char *format, va_list args);
/* Variable args logDebug. */

void logDebug(char *format, ...)
/* Log a debug message. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

void logDaemonize(char *progName);
/* daemonize server process: closing open file descriptors and
 * starting logging based on the -logFacility and -log command line options .
 * if -debug is supplied , don't fork. */

#endif

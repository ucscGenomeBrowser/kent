/* paraLib - some misc. routines used by multiple parasol 
 * associated programs. */

#ifndef PARALIB_H
#define PARALIB_H

extern char *cuserid(char *__s);  /* Linux should define this but... */

extern int paraHubPort;		      /* Port for hub. */
extern int paraNodePort;	      /* Port for nodes. */

boolean sendWithSig(int fd, char *string);
/* Send a string with the signature prepended.  Warn 
 * but don't abort if there's a problem.  Return TRUE
 * on success. */

void mustSendWithSig(int fd, char *string);
/* Send a string with the signature prepended. 
 * Abort on failure. */

char *getMachine();
/* Return host machine name. */

char *getUser();
/* Get user name */

int forkOrDie();
/* Fork, aborting if it fails. */

void vLogDebug(char *format, va_list args);
/* Variable args logDebug. */

void logDebug(char *format, ...);
/* Log a debug message. */

void vLogInfo(char *format, va_list args);
/* Variable args logInfo. */

void logInfo(char *format, ...);
/* Log a info message. */

void logOpen(char *program, char *facility);
/* Setup logging.  This initializes syslog using the specified facility.
 * Facility is the syslog facility as specified in syslog.conf.  If facility
 * is NULL, local0 is used.  This adds a warn handlers that logs at level
 * error for warn() and errAbort() calls. Use logInfo and logDebug for info
 * and debug messages. */

struct runJobMessage
/* Parsed out runJob message as paraNode sees it. */
    {
    char *jobIdString;	   /* Unique ID - ascii number */
    char *reserved;	   /* Reserved for future expansion, always "0" */
    char *user;		   /* User associated with job. */
    char *dir;		   /* Directory to run job in. */
    char *in;              /* File for stdin. */
    char *out;		   /* File for stdout. */
    char *err;             /* File for stderr. */
    char *command;	   /* Command line for job. */
    };

boolean parseRunJobMessage(char *line, struct runJobMessage *rjm);
/* Parse runJobMessage as paraNodes sees it. */

void fillInErrFile(char errFile[512], int jobId, char *tempDir);
/* Fill in error file name */

extern time_t now;	/* Time when started processing current message */

void findNow();
/* Just set now to current time. */

#endif /* PARALIB_H */


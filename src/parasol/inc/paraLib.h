#ifndef PARALIB_H

extern char *cuserid(char *__s);  /* Linux should define this but... */

extern char paraSig[];  /* Mild security measure. */
extern int paraPort;		      /* Our port */

boolean sendWithSig(int fd, char *string);
/* Send a string with the signature prepended.  Warn 
 * but don't abort if there's a problem.  Return TRUE
 * on success. */

void mustSendWithSig(int fd, char *string);
/* Send a string with the signature prepended. 
 * Abort on failure. */

char *getMachine();
/* Return host machine name. */

void vLogIt(char *format, va_list args);
/* Variable args logit. */

void logIt(char *format, ...);
/* Print message to log file. */

void flushLog();
/* Flush log file. */

extern boolean logFlush; /* Set this to true to flush log after every logIt */

void setupDaemonLog(char *fileName);
/* Setup log file, and warning handler that goes to this
 * file.  If fileName is NULL then no log, and warning
 * messages go into the bit bucket. */

void logClose();
/* Close log file. */

struct runJobMessage
/* Parsed out runJob message as paraNode sees it. */
    {
    char *managingHost;	   /* Hub's machine. */
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


#ifndef PARALIB_H

extern char paraSig[];  /* Mild security measure. */
extern int paraPort;		      /* Our port */

boolean sendWithSig(int fd, char *string);
/* Send a string with the signature prepended.  Warn 
 * but don't abort if there's a problem.  Return TRUE
 * on success. */

void mustSendWithSig(int fd, char *string);
/* Send a string with the signature prepended. 
 * Abort on failure. */

char *getHost();
/* Return host name. */

void vLogIt(char *format, va_list args);
/* Variable args logit. */

void logIt(char *format, ...);
/* Print message to log file. */

void setupDaemonLog(char *fileName);
/* Setup log file, and warning handler that goes to this
 * file.  If fileName is NULL then no log, and warning
 * messages go into the bit bucket. */

void logClose();
/* Close log file. */

#endif /* PARALIB_H */


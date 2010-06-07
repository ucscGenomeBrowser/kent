/* chkCommon - common function used by various modules */
#ifndef CHKCOMMON_H
#define CHKCOMMON_H
extern int errorCnt;  /* count of errors */
extern boolean testMode; /* Ignore errors that occure in test db */

void gbErrorSetDb(char *database);
/* set database to use in error messages */

void gbError(char *format, ...)
/* print and count an error */
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

unsigned strToUnsigned(char* str, char* acc, char* useMsg,
                       boolean* gotError);
/* Parse a string into an unsigned. */

off_t strToOffset(char* str, char* acc, char* useMsg);
/* Parse a string into an offset_t. */


#endif

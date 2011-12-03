/* chkCommon - common function used by various modules */
#include "common.h"
#include "chkCommon.h"


int errorCnt = 0;  /* count of errors */
boolean testMode = FALSE; /* Ignore errors that occure in test db */

static char *gbDatabase = NULL;

void gbErrorSetDb(char *database)
/* set database to use in error messages */
{
gbDatabase = cloneString(database);
}

void gbError(char *format, ...)
/* print and count an error */
{
va_list args;
fprintf(stderr, "Error: %s: ", gbDatabase);
va_start(args, format);
vfprintf(stderr, format, args);
va_end(args);
fputc('\n', stderr);
errorCnt++;
}

unsigned strToUnsigned(char* str, char* acc, char* useMsg,
                       boolean* gotError)
/* Parse a string into an unsigned. */
{
char* stop;
unsigned num = 0;
num = strtoul(str, &stop, 10);
if ((*stop != '\0') || (stop == str))
    {
    gbError("%s: invalid unsigned \"%s\": %s ", acc, str, useMsg);
    if (gotError != NULL)
        *gotError = TRUE;
    }
else
    if (gotError != NULL)
        *gotError = FALSE;
return num;
}

off_t strToOffset(char* str, char* acc, char* useMsg)
/* Parse a string into an offset_t. */
{
char* stop;
off_t num = 0;
num = strtoull(str, &stop, 10);
if ((*stop != '\0') || (stop == str))
    gbError("%s: invalid offset \"%s\": %s ", acc, str, useMsg);
return num;
}


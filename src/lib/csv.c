/* csv - stuff to help process comma separated values.  Have to wrap quotes around
 * things with commas, and escape quotes with more quotes sometimes. */

#include "common.h"
#include "linefile.h"
#include "csv.h"

char *csvEscapeToDyString(struct dyString *dy, char *string)
/* Wrap string in quotes if it has any commas.  Anything already in quotes get s double-quoted 
 * Returns transformated result, which will be input string if it has no commas, otherwise
 * will be dy*/
{
/* If there are no commas just output it */
if (strchr(string, ',') == NULL)
    {
    return string;
    }
dyStringClear(dy);
dyStringAppendC(dy, '"');
char c;
while ((c = *string++) != 0)
    {
    if (c == '"')
        dyStringAppendC(dy, c);
    dyStringAppendC(dy, c);
    }
dyStringAppendC(dy, '"');
return dy->string;
}

void csvWriteVal(char *val, FILE *f)
/* Write val, which may have some quotes or commas in it, in a way to be compatable with
 * csv list representation */
{
/* If there are no commas just output it */
if (strchr(val, ',') == NULL)
    {
    fputs(val, f);
    return;
    }

/* Strip surrounding quotes if any */
val = trimSpaces(val);
int valLen = strlen(val);
if (valLen > 2 && val[0] == '"' && lastChar(val) == '"')
     {
     val[valLen-1] = 0;
     val += 1;
     }

/* Put quotes around it and output, escaping internal quotes with double quotes */
fputc('"', f);
char c;
while ((c = *val++) != 0)
    {
    if (c == '"')
	fputc('"', f);
    fputc(c, f);
    }
fputc('"', f);
}


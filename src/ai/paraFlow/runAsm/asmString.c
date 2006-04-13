/* asmString - interface between generated assembly code and
 * strings */

#include "common.h"
#include "../compiler/pfPreamble.h"

struct _pf_string *_pfaNewString(char *s)
{
uglyf("Theoretically getting new string for %p %s\n", s, s);
return _pf_string_from_const(s);
}

void _printString(struct _pf_string *string)
/* Print double, just for debugging really */
{
char *text = "(null)";
if (string != NULL)
    text = string->s;
printf("%s\n", text);
}

int _pfStringCmp(struct _pf_string *a, struct _pf_string *b)
/* Compare two strings */
{
int ret = 0;

if (a != b)
    {
    if (a == NULL)
        ret = -1;
    else if (b == NULL)
        ret = 1;
    else
        ret = strcmp(a->s, b->s);
    }
return ret;
}

extern void pf_print(_pf_Stack *stack);

void print(_pf_Stack stack)
/* Print out single variable where type is determined at run time. 
 * Add newline. */
{
pf_print(&stack);
}


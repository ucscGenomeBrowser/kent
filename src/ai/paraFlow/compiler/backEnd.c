/* backEnd.h - interface between compiler front and back ends.
 * The front end is responsible for parsing, type checking,
 * and creating intermediate code.  The back end is responsible
 * for creating assembly language. */

#include "common.h"
#include "backEnd.h"

extern struct pfBackEnd macPentiumBackEnd;

struct pfBackEnd *pfBackEndFind(char *name)
/* Find named back end. */
{
if (sameString(name, "mac-pentium"))
    return &macPentiumBackEnd;
else
    {
    errAbort("Can't find %s backEnd", name);
    return NULL;
    }
}

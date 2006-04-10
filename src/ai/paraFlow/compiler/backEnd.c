/* backEnd.h - interface between compiler front and back ends.
 * The front end is responsible for parsing, type checking,
 * and creating intermediate code.  The back end is responsible
 * for creating assembly language. */

#include "common.h"
#include "pfCompile.h"
#include "backEnd.h"

extern struct pfBackEnd macPentiumBackEnd;

struct pfBackEnd *backEndFind(char *name)
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

int backEndTempLabeledString(struct pfCompile *pfc, char *s, FILE *f)
/* Put out string, label it LNN for some number N, and return N. */
{
struct pfBackEnd *back = pfc->backEnd;
int id = ++pfc->isxLabelMaker;
char label[64];
back->stringSegment(back, f);
safef(label, sizeof(label), "L%d", id);
back->emitLabel(back, label, 1, FALSE, f);
back->emitAscii(back, s, strlen(s), f);
return id;
}


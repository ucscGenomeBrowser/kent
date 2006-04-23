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

void backEndLocalPointer(struct pfBackEnd *back, int id, FILE *f)
/* Emit code for pointer labeled LXX where XX is id. */
{
char label[64];
safef(label, sizeof(label), "L%d", id);
back->emitPointer(back, label, f);
}

static void emitLocalString(struct pfBackEnd *back, int id, char *s, FILE *f)
/* Emid string labeled by ID */
{
char label[64];
back->stringSegment(back, f);
safef(label, sizeof(label), "L%d", id);
back->emitLabel(back, label, 1, FALSE, f);
back->emitAscii(back, s, strlen(s), f);
}

int backEndTempLabeledString(struct pfCompile *pfc, char *s, FILE *f)
/* Put out string, label it LNN for some number N, and return N. */
{
int id = ++pfc->isxLabelMaker;
emitLocalString(pfc->backEnd, id, s, f);
return id;
}

int backEndStringAdd(struct pfCompile *pfc,
	struct backEndString **pList, char *string)
/* Add string and label to list.  Return string ID. */
{
struct backEndString *bes;
AllocVar(bes);
bes->string = string;
bes->id = ++pfc->isxLabelMaker;
slAddHead(pList, bes);
return bes->id;
}

void backEndStringEmitAll(struct pfBackEnd *back,
	struct backEndString *list, FILE *f)
/* Emit all labeled strings to file. */
{
char label[64];
struct backEndString *bes;
for (bes = list; bes != NULL; bes=bes->next)
    emitLocalString(back, bes->id, bes->string, f);
}


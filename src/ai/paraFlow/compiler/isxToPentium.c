/* isxToPentium - convert isx code to Pentium code. */

#include "common.h"
#include "dlist.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfCompile.h"
#include "isx.h"

struct regInfo
    {
    char *name;			/* Register name */
    enum isxValType *types;	/* Types it can handle */
    int typeCount;		/* Count of types it can handle */
    };

static enum isxValType abcdRegTypes[] = { ivInt, ivObject, };
static enum isxValType siDiRegTypes[] = {ivInt, ivObject, };
static enum isxValType stRegTypes[] = {ivFloat, ivDouble,};

struct regInfo regInfo[] = {
    { "eax", abcdRegTypes, ArraySize(abcdRegTypes),},
    { "ebx", abcdRegTypes, ArraySize(abcdRegTypes),},
    { "ecx", abcdRegTypes, ArraySize(abcdRegTypes),},
    { "edx", abcdRegTypes, ArraySize(abcdRegTypes),},
    { "esi", siDiRegTypes, ArraySize(siDiRegTypes),},
    { "edi", siDiRegTypes, ArraySize(siDiRegTypes),},
    { "st0", stRegTypes, ArraySize(stRegTypes),},
    { "st1", stRegTypes, ArraySize(stRegTypes),},
    { "st2", stRegTypes, ArraySize(stRegTypes),},
    { "st3", stRegTypes, ArraySize(stRegTypes),},
    { "st4", stRegTypes, ArraySize(stRegTypes),},
    { "st5", stRegTypes, ArraySize(stRegTypes),},
    { "st6", stRegTypes, ArraySize(stRegTypes),},
    { "st7", stRegTypes, ArraySize(stRegTypes),},
};

void isxToPentium(struct dlList *iList, FILE *f)
/* Convert isx code to pentium instructions in file. */
{
}


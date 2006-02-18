/* runErr - handle run-time errors. */

#include "common.h"
#include "hash.h"
#include "errAbort.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "print.h"

static void stackDump()
/* Print stack dump. */
{
struct _pf_activation *s;
struct hash *idHash = hashNew(0);
for (s = _pf_activation_stack; s != NULL; s = s->parent)
    {
    struct _pf_functionFixedInfo *ffi = s->fixed;
    struct _pf_type *funcType = _pf_type_table[ffi->typeId];
    struct _pf_type *classType = NULL;
    struct _pf_type *inputTypeTuple = funcType->children;
    char *data = s->data;
    int inputCount = slCount(inputTypeTuple->children);
    int i;
    struct _pf_localVarInfo *vars = ffi->vars;
    if (ffi->classType >= 0)
        classType = _pf_type_table[ffi->classType];
    if (classType != NULL)
        fprintf(stderr, "%s.", classType->base->name);
    fprintf(stderr, "%s(", ffi->name);
    for (i=0; i<inputCount; ++i)
	{
	struct _pf_localVarInfo *var = &vars[i];
	struct _pf_type *paramType = _pf_type_table[var->type];
	if (i != 0)
	    fprintf(stderr, ", ");
	fprintf(stderr, "%s %s = ", paramType->base->name, var->name);
	_pf_printField(stderr, data + var->offset, paramType->base, idHash);
	}
    fprintf(stderr, ")\n");
    }
hashFree(&idHash);
}

void _pf_run_err(char *format, ...)
/* Run time error.  Prints message, dumps stack, and aborts. */
{
va_list args;
va_start(args, format);

fprintf(stderr, "\n----------start stack dump---------\n");
stackDump();
fprintf(stderr, "-----------end stack dump----------\n");
vaErrAbort(format, args);

va_end(args);
}



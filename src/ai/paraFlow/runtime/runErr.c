/* runErr - handle run-time errors (and try/catch). */

#include "common.h"
#include "hash.h"
#include "errAbort.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "print.h"
#include "runErr.h"

static void stackDump(FILE *f)
/* Print stack dump. */
{
struct _pf_activation *s;
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
        fprintf(f, "%s.", classType->base->name);
    fprintf(f, "%s(", ffi->name);
    for (i=0; i<inputCount; ++i)
	{
	struct _pf_localVarInfo *var = &vars[i];
	struct _pf_type *paramType = _pf_type_table[var->type];
	if (i != 0)
	    fprintf(f, ", ");
	fprintf(f, "%s:", var->name);
	_pf_printField(f, data + var->offset, paramType->base, NULL);
	}
    fprintf(f, ")\n");
    }
}

void _pf_run_err(char *format, ...)
/* Run time error.  Prints message, dumps stack, and aborts. */
{
va_list args;
va_start(args, format);

fprintf(stderr, "\n----------start stack dump---------\n");
stackDump(stderr);
fprintf(stderr, "-----------end stack dump----------\n");
vaErrAbort(format, args);

va_end(args);
}

void _pf_err_check_level_and_unwind(_pf_Err_catch err, int level,
	struct _pf_activation *curActivation)
/* Check level against err->level.  If err is deeper then we
 * just throw to next level handler if any.  Otherwise we
 * unwind stack and return. */
{
if (err->level < level)	/* Can't catch.  Just throw it again. */
    {
    errAbort(err->message->string);
    }
else
    {
    uglyf("Theoretically unwinding stack here. \n");
#ifdef STACK_ALREADY_EATEN
    struct _pf_activation *s;
    for (s = _pf_activation_stack; s != curActivation; s = s->parent)
        {
	uglyf("s = %p\n", s);
	struct _pf_functionFixedInfo *ffi = s->fixed;
	uglyf("unwinding %s\n", ffi->name);
	}
#endif /* STACK_ALREADY_EATEN */
    _pf_activation_stack = curActivation;
    }
}

void pf_stackDump()
{
uglyf("_pf_activation_stack %p\n", _pf_activation_stack);
uglyf("_pf_activation_stack->parent %p\n", _pf_activation_stack->parent);
fprintf(uglyOut, "----------application requested stack dump---------\n");
stackDump(uglyOut);
fprintf(uglyOut, "----------end application user requesed stack dump----------\n");
}


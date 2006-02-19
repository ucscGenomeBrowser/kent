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

void pf_stackDump()
/* Handle user-requested stack dump. */
{
fprintf(stdout, "----------application requested stack dump---------\n");
stackDump(stdout);
fprintf(stdout, "----------end application user requesed stack dump----------\n");
}

static struct _pf_err_catch *errCatchStack = NULL;

struct _pf_err_catch *_pf_err_catch_new(struct _pf_activation *act, int level)
/* Create a new error catcher. */
{
struct _pf_err_catch *err;
AllocVar(err);
err->act = act;
err->level = level;
return err;
}

int _pf_err_catch_push(struct _pf_err_catch *err)
/* Pushes catcher on catch stack and returns TRUE. Not typically
 * used directly, but called vi _pf_err_catch_start macro. */
{
slAddHead(&errCatchStack, err);
return TRUE;
}

void _pf_err_catch_end(struct _pf_err_catch *err)
/* Pop error catcher off of stack.  Don't free it up yet, because
 * we want to examine gotErr still. */
{
errCatchStack = errCatchStack->next;
}

void _pf_err_catch_free(struct _pf_err_catch **pErr)
/* Free up memory associated with error catcher. */
{
freez(pErr);
}

struct _pf_punt_info
/* Information associated with a punt. */
    {
    int level;		/* Punt level.  0 normal, -1 program error */
    struct dyString *message;	/* Punt message. */
    struct dyString *source;	/* Punt source. */
    };
struct _pf_punt_info punter;

static void unwindStackTo(struct _pf_activation *act)
/* Call destructors on data in stack. */
{
struct _pf_activation *s;
for (s = _pf_activation_stack; s != act; s = s->parent)
    {
    struct _pf_functionFixedInfo *ffi = s->fixed;
    struct _pf_localVarInfo *vars = ffi->vars;
    char *data = s->data;
    int i;
    int varCount = ffi->varCount;
    if (ffi->classType >= 0)
	{
	/* For a class function (aka method) the self is
	 * always the last variable.  We don't want to
	 * clean it up, so take one off of varCount */
	varCount -= 1;
	}
    for (i=0; i<varCount; ++i)
        {
	struct _pf_localVarInfo *var = &vars[i];
	struct _pf_type *varType = _pf_type_table[var->type];
	if (varType->base->needsCleanup)
	    {
	    void *v = data + var->offset;
	    struct _pf_object **pObj = v;
	    struct _pf_object *obj = *pObj;
	    if (obj != NULL)
		{
		if (--obj->_pf_refCount == 0)
		    {
		    obj->_pf_cleanup(obj, var->type);
		    }
		}
	    }
	}
    }
_pf_activation_stack = act;
}

static void puntCatcher()
/* Try and catch the punt. */
{
struct _pf_err_catch *catcher;
for (catcher = errCatchStack; catcher != NULL; catcher = catcher->next)
    {
    if (catcher->level <= punter.level)
        break;
    }
if (catcher)
    {
    catcher->gotError = TRUE;
    catcher->message = punter.message->string;
    catcher->source = punter.source->string;
    unwindStackTo(catcher->act);
    errCatchStack = catcher;
    longjmp(catcher->jmpBuf, -1);
    }
else
    {
    fprintf(stderr, "\n----------start stack dump---------\n");
    stackDump(stderr);
    fprintf(stderr, "-----------end stack dump----------\n");
    fprintf(stderr, "%s\n", punter.message->string);
    exit(-1);
    }
}

static void puntAbortHandler()
/* Our handler for errAbort. */
{
puntCatcher();
}

static void puntWarnHandler(char *format, va_list args)
/* Our handler for warn - which would be coming from a library
 * function, not from ParaFlow application. */
{
dyStringClear(punter.message);
dyStringVaPrintf(punter.message, format, args);
punter.level = 0;
dyStringClear(punter.source);
dyStringAppend(punter.source, "lib");
}

void _pf_punt_init()
/* Initialize punt/catch system.  Mostly just redirects the
 * errAbort handler. */
{
punter.message = dyStringNew(0);
punter.source = dyStringNew(0);
pushWarnHandler(puntWarnHandler);
pushAbortHandler(puntAbortHandler);
}

void _pf_run_err(char *message)
/* Run time error.  Prints message, dumps stack, and aborts. */
{
dyStringClear(punter.message);
dyStringAppend(punter.message, message);
punter.level = -1;
dyStringClear(punter.source);
dyStringAppend(punter.source, "runtime");
puntCatcher();
}

void pf_punt(_pf_Stack *stack)
/* Print  message and die. */
{
_pf_String message = stack[0].String;
_pf_String source = stack[1].String;
_pf_Int level = stack[2].Int;

dyStringClear(punter.message);
dyStringAppend(punter.message, message->s);
dyStringClear(punter.source);
dyStringAppend(punter.source, source->s);
punter.level = level;
puntCatcher();
}


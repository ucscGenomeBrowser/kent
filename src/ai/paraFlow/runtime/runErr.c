/* runErr - handle run-time errors (and try/catch). */

#include "common.h"
#include "hash.h"
#include "errabort.h"
#include "dystring.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "print.h"
#include "runErr.h"

static void errCleanup(struct _pf_error *err, int typeId)
/* Clean up an error structure. */
{
_pf_class_cleanup((struct _pf_object *)err, typeId);
}

static struct _pf_error *errorNew(_pf_String message, _pf_String source, 
	int code)
/* Create a new generic error object. */
{
struct _pf_error *err;
AllocVar(err);
err->message = message;
err->source = source;
err->code = code;
err->_pf_refCount = 1;
err->_pf_cleanup = errCleanup;
return err;
}

static struct _pf_error *errorNewFromC(char *message, char *source, int code)
/* Create error object around C style strings. */
{
return errorNew(_pf_string_from_const(message), _pf_string_from_const(source),
    code);
}

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
}

static struct _pf_err_catch *errCatchStack = NULL;

struct _pf_err_catch *_pf_err_catch_new(struct _pf_activation *act,
	int errTypeId)
/* Create a new error catcher. */
{
struct _pf_err_catch *errCatch;
AllocVar(errCatch);
errCatch->act = act;
errCatch->catchType = errTypeId;
return errCatch;
}

int _pf_err_catch_push(struct _pf_err_catch *errCatch)
/* Pushes catcher on catch stack and returns TRUE. Not typically
 * used directly, but called vi _pf_err_catch_start macro. */
{
slAddHead(&errCatchStack, errCatch);
return TRUE;
}

void _pf_err_catch_end(struct _pf_err_catch *errCatch)
/* Pop error catcher off of stack.  Don't free it up yet, because
 * we want to examine gotErr still. */
{
errCatchStack = errCatchStack->next;
}

void _pf_err_catch_free(struct _pf_err_catch **pErrCatch)
/* Free up memory associated with error catcher. */
{
freez(pErrCatch);
}

struct _pf_punt_info
/* Information associated with a punt. */
    {
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
	struct _pf_type *classType = _pf_type_table[ffi->classType];
	/* For a class function (aka method) the self is
	 * always the last variable.  We don't want to
	 * clean it up, so take one off of varCount */
	varCount -= 1;
	if (classType->base->parent != NULL)
	    {
	    varCount -= 1;	/* Want to skip parent variable too. */
	    }
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

static void reportErrToFile(struct _pf_error *err, FILE *f)
/* Report error to file. */
{
while (err != NULL)
    {
    fprintf(f, "%s\n", err->message->s);
    err = err->next;
    }
}

static void puntCatcher(struct _pf_error *err, int typeId)
/* Try and catch the punt. */
{
struct _pf_err_catch *catcher;
struct _pf_type *errType = _pf_type_table[typeId];
struct _pf_base *errBase = errType->base;
for (catcher = errCatchStack; catcher != NULL; catcher = catcher->next)
    {
    struct _pf_type *catchType = _pf_type_table[catcher->catchType];
    struct _pf_base *catchBase = catchType->base;
    if (_pf_base_is_ancestor(errBase, catchBase))
        break;
    }
if (catcher)
    {
    catcher->err = err;
    unwindStackTo(catcher->act);
    errCatchStack = catcher;
    longjmp(catcher->jmpBuf, -1);
    }
else
    {
    if (_pf_activation_stack != NULL)
	{
	fprintf(stderr, "\n----------start stack dump---------\n");
	stackDump(stderr);
	}
    reportErrToFile(err, stderr);
    exit(-1);
    }
}

static void puntAbortHandler()
/* Our handler for errAbort. */
{
struct _pf_error *err = errorNewFromC(punter.message->string, 
	punter.source->string, errno);
int errTypeId = _pf_find_error_type_id();
puntCatcher(err, errTypeId);
}

static void puntWarnHandler(char *format, va_list args)
/* Our handler for warn - which would be coming from a library
 * function, not from ParaFlow application. */
{
dyStringClear(punter.message);
dyStringVaPrintf(punter.message, format, args);
dyStringClear(punter.source);
dyStringAppend(punter.source, "library");
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
struct _pf_error *err = errorNewFromC(message,  "runtime", -1);
int seriousErrTypeId = _pf_find_serious_error_type_id();
puntCatcher(err, seriousErrTypeId);
}

void pf_punt(_pf_Stack *stack)
/* Print  message and bail out. */
{
_pf_String message = stack[0].String;
_pf_String source = stack[1].String;
_pf_Int code = stack[2].Int;
int errTypeId = _pf_find_error_type_id();	
struct _pf_error *err = errorNew(message, source, code);
puntCatcher(err, errTypeId);
}

void pf_puntMore(_pf_Stack *stack)
/* Print  message and bail out. */
{
struct _pf_error *oldErr = stack[0].v;
_pf_String message = stack[1].String;
_pf_String source = stack[2].String;
_pf_Int code = stack[3].Int;
int errTypeId = _pf_find_error_type_id();	
struct _pf_error *err = errorNew(message, source, code);
err->next = oldErr;
puntCatcher(err, errTypeId);
}

void pf_throw(_pf_Stack *stack)
/* Throw an error, perhaps of a different type. */
{
struct _pf_error *err = stack[0].Var.val.v;
int typeId = stack[0].Var.typeId;
puntCatcher(err, typeId);
}

void pf_throwMore(_pf_Stack *stack)
/* Throw an error, perhaps of a different type. */
{
struct _pf_error *oldErr = stack[0].v;
struct _pf_error *newErr = stack[1].Var.val.v;
int typeId = stack[1].Var.typeId;
newErr->next = oldErr;
puntCatcher(newErr, typeId);
}


void pf_warn(_pf_Stack *stack)
/* Print warning message, but don't bail out. */
{
_pf_String message = stack[0].String;
_pf_nil_check(message);
warn(message->s);
}

void _pf_cm_seriousError_asString(_pf_Stack *stack)
/* Get error message. */
{
struct _pf_error *err = stack[0].v;
struct dyString *dy = dyStringNew(0);
while (err != NULL)
    {
    dyStringAppend(dy, err->message->s);
    err = err->next;
    if (err != NULL)
	dyStringAppendC(dy, '\n');
    }
stack[0].String = _pf_string_from_const(dy->string);
dyStringFree(&dy);
}


void _pf_cm_seriousError_report(_pf_Stack *stack)
/* Report an error. */
{
struct _pf_error *err = stack[0].v;
reportErrToFile(err, stderr);
}

void _pf_cm_seriousError_init(_pf_Stack *stack)
/* Initialize a serious error. */
{
struct _pf_error *err = stack[0].v;
_pf_String message = stack[1].String;
_pf_String source = stack[2].String;
_pf_Int code = stack[1].Int;
err->message = message;
err->source = source;
err->code = code;
}

void _pf_cm_error_init(_pf_Stack *stack)
/* Initialize an error. */
{
_pf_cm_seriousError_init(stack);
}


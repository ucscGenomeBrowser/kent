/* print - Implements print function for variable types. */
#include "common.h"
#include "dystring.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"

void extFunc(_pf_Stack *stack)
/* Just sum two numbers. */
{
stack[0].Int = stack[0].Int + stack[1].Int;
}

struct file
    {
    int _pf_refCount;			     	/* Number of references. */
    void (*_pf_cleanup)(struct file *obj, int id); /* Called when refCount <= 0 */
    struct _pf_string *name;		/* The only exported field. */
    FILE *f;				/* File handle. */
    struct dyString *dy;			/* Buffer for reading strings. */
    };

static void fileCleanup(struct file *file, int typeId)
/* Clean up string->s and string itself. */
{
carefulClose(&file->f);
dyStringFree(&file->dy);
_pf_class_cleanup((struct _pf_object *)file, typeId);
}

void fileOpen(_pf_Stack *stack)
/* paraFlow run time support routine to open file. */
{
struct _pf_string *name = stack[0].String;
struct _pf_string *mode = stack[1].String;
struct file *file;

AllocVar(file);
file->_pf_refCount = 1;
file->_pf_cleanup = fileCleanup;
file->name = stack[0].String;
file->f = mustOpen(name->s, mode->s);
file->dy = dyStringNew(0);

/* Remove mode reference. (We'll keep the name). */
if (--mode->_pf_refCount <= 0)
    mode->_pf_cleanup(mode, 0);

/* Save result on stack. */
stack[0].v = file;
}

void _pf_cm_file_close(_pf_Stack *stack)
/* Close file explicitly. */
{
struct file *file = stack[0].v;
carefulClose(&file->f);
}

void _pf_cm_file_writeString(_pf_Stack *stack)
/* paraFlow run time support routine to write string to file. */
{
struct file *file = stack[0].v;
struct _pf_string *string = stack[1].String;

mustWrite(file->f, string->s, string->size);

/* Clean up references on stack. */
if (--string->_pf_refCount <= 0)
    string->_pf_cleanup(string, 0);
}

void _pf_cm_file_readLine(_pf_Stack *stack)
/* Read next line from file. */
{
struct file *file = stack[0].v;
FILE *f = file->f;
struct dyString *dy = file->dy;
int c;

dyStringClear(dy);
for (;;)
    {
    c = fgetc(f);
    if (c == EOF)
        break;
    dyStringAppendC(dy, c);
    if (c == '\n')
        break;
    }
stack[0].String = _pf_string_from_const(dy->string);
}

void _pf_cm_file_readBytes(_pf_Stack *stack)
/* Read in a fixed number of bytes to string.
 * This will return a string of length zero at
 * EOF, and a string smaller than what is asked
 * for near EOF. */
{
struct file *file = stack[0].v;
_pf_Int count = stack[1].Int;
_pf_Int bytesRead;
struct _pf_string *string = _pf_string_new(needLargeMem(count+1), count);
bytesRead = fread(string->s, 1, count, file->f);
if (bytesRead <= 0)
    bytesRead = 0;
string->size = bytesRead;
string->s[bytesRead] = 0;
stack[0].String = string;
}


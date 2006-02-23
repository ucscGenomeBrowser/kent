/* print - Implements print function for variable types. */
#include "common.h"
#include "dystring.h"
#include "../compiler/pfPreamble.h"
#include "pfString.h"
#include "net.h"

void _pf_prin(FILE *f, _pf_Stack *stack, boolean quoteString);
/* Print out single variable where type is determined at run time. */

void _pf_getObj(FILE *f, _pf_Stack *stack);
/* Read in variable from file. */

struct file
    {
    int _pf_refCount;			     	/* Number of references. */
    void (*_pf_cleanup)(struct file *obj, int id); /* Called when refCount <= 0 */
    _pf_polyFunType *_pf_polyFun;	/* Polymorphic functions. */
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

static FILE *openUrlOrFile(char *spec, char *mode)
/* If it's a URL call the fancy net opener, otherwise
 * call the regular file opener. */
{
if (startsWith("http://", spec) || startsWith("ftp://", spec))
    {
    FILE *f;
    int fd;
    if (mode[0] != 'r')
        errAbort("Can't open %s in mode %s", spec, mode);
    fd = netUrlOpen(spec);
    f = fdopen(fd, mode);
    if (f == NULL)
        errnoAbort("Couldn't open %s", spec);
    return f;
    }
else
    return mustOpen(spec, mode);
}

void pf_fileOpen(_pf_Stack *stack)
/* paraFlow run time support routine to open file. */
{
struct _pf_string *name = stack[0].String;
struct _pf_string *mode = stack[1].String;
struct file *file;

_pf_nil_check(name);
_pf_nil_check(mode);
AllocVar(file);
file->_pf_refCount = 1;
file->_pf_cleanup = fileCleanup;
file->name = name;
file->f = openUrlOrFile(name->s, mode->s);
file->dy = dyStringNew(0);

/* Remove mode reference. (We'll keep the name). */
if (--mode->_pf_refCount <= 0)
    mode->_pf_cleanup(mode, 0);

/* Save result on stack. */
stack[0].v = file;
}

void _pf_cm1_file_close(_pf_Stack *stack)
/* Close file explicitly. */
{
struct file *file = stack[0].v;
carefulClose(&file->f);
}

void _pf_cm1_file_write(_pf_Stack *stack)
/* paraFlow run time support routine to write string to file. */
{
struct file *file = stack[0].v;
struct _pf_string *string = stack[1].String;

mustWrite(file->f, string->s, string->size);

/* Clean up references on stack. */
if (--string->_pf_refCount <= 0)
    string->_pf_cleanup(string, 0);
}


void _pf_cm1_file_put(_pf_Stack *stack)
/* Print to file with line feed. */
{
struct file *file = stack[0].v;
FILE *f = file->f;
_pf_prin(f, stack+1, TRUE);
fputc('\n', f);
}

void _pf_cm1_file_get(_pf_Stack *stack)
/* Read in variable from file, and then make sure have
 * a newline. */
{
int c;
struct file *file = stack[0].v;
_pf_getObj(file->f, stack);
c = getc(file->f);
if (c != '\n')
    warn("expecting newline, got %c", c);
}


void _pf_cm1_file_readLine(_pf_Stack *stack)
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

void _pf_cm1_file_read(_pf_Stack *stack)
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

void _pf_cm1_file_readAll(_pf_Stack *stack)
/* Read in whole file to string. */
{
struct file *file = stack[0].v;
char buf[4*1024];
int bytesRead;
struct _pf_string *string = _pf_string_new_empty(sizeof(buf));
for (;;)
    {
    bytesRead = fread(buf, 1, sizeof(buf), file->f);
    if (bytesRead <= 0)
        break;
    _pf_strcat(string, buf, bytesRead);
    }
stack[0].String = string;
}

void _pf_cm1_file_seek(_pf_Stack *stack)
/* Seek to a position. */
{
struct file *file = stack[0].v;
_pf_Long pos = stack[1].Long;
_pf_Bit fromEnd = stack[2].Bit;
int whence = (fromEnd ? SEEK_END : SEEK_SET);
if (fseek(file->f, pos, whence) < 0)
    errnoAbort("seek error on %s", file->name);
}

void _pf_cm1_file_skip(_pf_Stack *stack)
/* Seek relative to current position. */
{
struct file *file = stack[0].v;
_pf_Long pos = stack[1].Long;
if (fseek(file->f, pos, SEEK_CUR) < 0)
    errnoAbort("skip error on %s", file->name);
}

void _pf_cm1_file_tell(_pf_Stack *stack)
/* Return current file position. */
{
struct file *file = stack[0].v;
stack[0].Long = ftell(file->f);
}

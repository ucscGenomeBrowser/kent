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

static FILE *mustFdopen(int fd, char *name, char *mode)
/* fdopen a file (wrap FILE around it) or die trying */
{
FILE *f = fdopen(fd, mode);
if (f == NULL)
    errnoAbort("Couldn't fdopen %s", name);
return f;
}

static FILE *openUrlOrFile(char *spec, char *mode)
/* If it's a URL call the fancy net opener, otherwise
 * call the regular file opener. */
{
if (startsWith("http://", spec) || startsWith("ftp://", spec))
    {
    int fd;
    if (mode[0] != 'r')
        errAbort("Can't open %s in mode %s", spec, mode);
    fd = netUrlOpen(spec);
    return mustFdopen(fd, spec, mode);
    }
else
    return mustOpen(spec, mode);
}

static struct file *fileFromFILE(FILE *f, _pf_String name)
/* Convert a FILE to a ParaFlow file. */
{
struct file *file;
AllocVar(file);
file->_pf_refCount = 1;
file->_pf_cleanup = fileCleanup;
file->name = name;
file->f = f;
file->dy = dyStringNew(0);
return file;
}

void pf_fileOpen(_pf_Stack *stack)
/* paraFlow run time support routine to open file. */
{
struct _pf_string *name = stack[0].String;
struct _pf_string *mode = stack[1].String;
struct file *file;

_pf_nil_check(name);
_pf_nil_check(mode);
file = fileFromFILE(openUrlOrFile(name->s, mode->s), name);

/* Remove mode reference. (We'll keep the name). */
if (--mode->_pf_refCount <= 0)
    mode->_pf_cleanup(mode, 0);

/* Save result on stack. */
stack[0].v = file;
}

static _pf_String readAll(FILE *f)
/* Return a string that contains the whole file. */
{
_pf_String string;
char buf[4*1024];
int bytesRead;
string = _pf_string_new_empty(sizeof(buf));
for (;;)
    {
    bytesRead = fread(buf, 1, sizeof(buf), f);
    if (bytesRead <= 0)
        break;
    _pf_strcat(string, buf, bytesRead);
    }
return string;
}

void pf_fileReadAll(_pf_Stack *stack)
/* Read in whole file to string. */
{
_pf_String name = stack[0].String;
_pf_String string;
FILE *f;
_pf_nil_check(name);

f = openUrlOrFile(name->s, "r");
string = readAll(f);
carefulClose(&f);

stack[0].String = string;
if (--name->_pf_refCount <= 0)
    name->_pf_cleanup(name, 0);
}

void _pf_cm_file_readAll(_pf_Stack *stack)
/* Read all of file. */
{
struct file *file = stack[0].v;
_pf_nil_check(file);
stack[0].String = readAll(file->f);
}

void _pf_cm_file_close(_pf_Stack *stack)
/* Close file explicitly. */
{
struct file *file = stack[0].v;
carefulClose(&file->f);
}

void _pf_cm_file_write(_pf_Stack *stack)
/* paraFlow run time support routine to write string to file. */
{
struct file *file = stack[0].v;
struct _pf_string *string = stack[1].String;

mustWrite(file->f, string->s, string->size);

/* Clean up references on stack. */
if (--string->_pf_refCount <= 0)
    string->_pf_cleanup(string, 0);
}

void _pf_cm_file_writeByte(_pf_Stack *stack)
/* Write a Byte to file. */
{
struct file *file = stack[0].v;
mustWrite(file->f, &stack[1].Byte, sizeof(stack[1].Byte));
}

void _pf_cm_file_writeShort(_pf_Stack *stack)
/* Write a Short to file. */
{
struct file *file = stack[0].v;
mustWrite(file->f, &stack[1].Short, sizeof(stack[1].Short));
}

void _pf_cm_file_writeInt(_pf_Stack *stack)
/* Write a Int to file. */
{
struct file *file = stack[0].v;
mustWrite(file->f, &stack[1].Int, sizeof(stack[1].Int));
}

void _pf_cm_file_writeLong(_pf_Stack *stack)
/* Write a Long to file. */
{
struct file *file = stack[0].v;
mustWrite(file->f, &stack[1].Long, sizeof(stack[1].Long));
}

void _pf_cm_file_writeFloat(_pf_Stack *stack)
/* Write a Float to file. */
{
struct file *file = stack[0].v;
mustWrite(file->f, &stack[1].Float, sizeof(stack[1].Float));
}

void _pf_cm_file_writeDouble(_pf_Stack *stack)
/* Write a Double to file. */
{
struct file *file = stack[0].v;
mustWrite(file->f, &stack[1].Double, sizeof(stack[1].Double));
}

void _pf_cm_file_readByte(_pf_Stack *stack)
/* Read a Byte from file. */
{
struct file *file = stack[0].v;
mustRead(file->f, &stack[0].Byte, sizeof(stack[0].Byte));
}

void _pf_cm_file_readShort(_pf_Stack *stack)
/* Read a Short from file. */
{
struct file *file = stack[0].v;
mustRead(file->f, &stack[0].Short, sizeof(stack[0].Short));
}

void _pf_cm_file_readInt(_pf_Stack *stack)
/* Read a Int from file. */
{
struct file *file = stack[0].v;
mustRead(file->f, &stack[0].Int, sizeof(stack[0].Int));
}

void _pf_cm_file_readLong(_pf_Stack *stack)
/* Read a Long from file. */
{
struct file *file = stack[0].v;
mustRead(file->f, &stack[0].Long, sizeof(stack[0].Long));
}

void _pf_cm_file_readFloat(_pf_Stack *stack)
/* Read a Float from file. */
{
struct file *file = stack[0].v;
mustRead(file->f, &stack[0].Float, sizeof(stack[0].Float));
}

void _pf_cm_file_readDouble(_pf_Stack *stack)
/* Read a Double from file. */
{
struct file *file = stack[0].v;
mustRead(file->f, &stack[0].Double, sizeof(stack[0].Double));
}

static void mustFlush(struct file *file)
/* Flush file or die trying */
{
if (fflush(file->f) < 0)
    errnoAbort("Couldn't flush file %s.", file->name);
}

void _pf_cm_file_flush(_pf_Stack *stack)
/* Flush file */
{
struct file *file = stack[0].v;
_pf_nil_check(file);
mustFlush(file);
}

void _pf_cm_file_writeNow(_pf_Stack *stack)
/* paraFlow run time support routine to write string to file
 * and then flush. */
{
struct file *file = stack[0].v;
struct _pf_string *string = stack[1].String;

mustWrite(file->f, string->s, string->size);
mustFlush(file);

/* Clean up references on stack. */
if (--string->_pf_refCount <= 0)
    string->_pf_cleanup(string, 0);
}


void _pf_cm_file_put(_pf_Stack *stack)
/* Print to file with line feed. */
{
struct file *file = stack[0].v;
FILE *f = file->f;
_pf_prin(f, stack+1, TRUE);
fputc('\n', f);
}

void _pf_cm_file_get(_pf_Stack *stack)
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

void _pf_cm_file_read(_pf_Stack *stack)
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

void _pf_cm_file_seek(_pf_Stack *stack)
/* Seek to a position. */
{
struct file *file = stack[0].v;
_pf_Long pos = stack[1].Long;
_pf_Bit fromEnd = stack[2].Bit;
int whence = (fromEnd ? SEEK_END : SEEK_SET);
if (fseek(file->f, pos, whence) < 0)
    errnoAbort("seek error on %s", file->name);
}

void _pf_cm_file_skip(_pf_Stack *stack)
/* Seek relative to current position. */
{
struct file *file = stack[0].v;
_pf_Long pos = stack[1].Long;
if (fseek(file->f, pos, SEEK_CUR) < 0)
    errnoAbort("skip error on %s", file->name);
}

void _pf_cm_file_tell(_pf_Stack *stack)
/* Return current file position. */
{
struct file *file = stack[0].v;
stack[0].Long = ftell(file->f);
}

void pf_fileExists(_pf_Stack *stack)
/* Returns true if file exists. */
{
_pf_String string = stack[0].v;
_pf_nil_check(string);
stack[0].Bit = fileExists(string->s);
/* Clean up references on stack. */
if (--string->_pf_refCount <= 0)
    string->_pf_cleanup(string, 0);
}

void pf_fileRemove(_pf_Stack *stack)
/* Remove file. Punt if there's a problem. */
{
int err;
_pf_String string = stack[0].v;

_pf_nil_check(string);
err = remove(string->s);

/* Clean up references on stack. */
if (--string->_pf_refCount <= 0)
    string->_pf_cleanup(string, 0);

if (err < 0)
    errnoAbort("Couldn't remove file %s", string->s);
}

void pf_fileRename(_pf_Stack *stack)
/* Rename file. */
{
int err;
_pf_String oldName = stack[0].v;
_pf_String newName = stack[1].v;
_pf_nil_check(oldName);
_pf_nil_check(newName);

err = rename(oldName->s, newName->s);

/* Clean up references on stack. */
if (--oldName->_pf_refCount <= 0)
    oldName->_pf_cleanup(oldName, 0);
if (--newName->_pf_refCount <= 0)
    newName->_pf_cleanup(newName, 0);

if (err < 0)
    errnoAbort("Couldn't rename file %s to %s", oldName->s, newName->s);
}

void pf_httpConnect(_pf_Stack *stack)
/* Return an open HTTP connection in a file, with
 * most of the header sent.  Doesn't send the
 * last part though, so you can put out cookies. 
 * Implements:
 *   to httpConnect(string url, method, protocol, agent) into (file f) */
{
/* Fetch input from stack. */
_pf_String url = stack[0].String;
_pf_String method = stack[1].String;
_pf_String protocol = stack[2].String;
_pf_String agent = stack[3].String;
int fd;

/* The usual null checks on input. */
_pf_nil_check(url);
_pf_nil_check(method);
_pf_nil_check(protocol);
_pf_nil_check(agent);

/* Do the real work of opening network connection and saving
 * result on return stack. */
fd = netHttpConnect(url->s, method->s, agent->s, protocol->s);
stack[0].v = fileFromFILE(mustFdopen(fd, url->s, "r+"), url);

/* Clean up input, except for URL which got moved to file. */
if (--method->_pf_refCount <= 0)
    method->_pf_cleanup(method, 0);
if (--protocol->_pf_refCount <= 0)
    protocol->_pf_cleanup(protocol, 0);
if (--agent->_pf_refCount <= 0)
    agent->_pf_cleanup(agent, 0);
}


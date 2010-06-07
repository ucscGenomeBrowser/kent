/* argsEnv - set up command line argument array and
 * environment dir. */

#include "common.h"
#include "errabort.h"
#include "memalloc.h"
#include "dystring.h"
#include "portable.h"
#include "pthreadWrap.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "object.h"
#include "pfString.h"
#include "initVar.h"

static char **cl_env;

static struct memHandler *parentMem;
static pthread_mutex_t memMutex;

static void *threadSafeAlloc(size_t size)
/* Wrap mutex lock around allocation. */
{
void *result;
pthreadMutexLock(&memMutex);
result = parentMem->alloc(size);
pthreadMutexUnlock(&memMutex);
return result;
}

static void threadSafeFree(void *vpt)
/* Wrap mutex lock around free. */
{
pthreadMutexLock(&memMutex);
parentMem->free(vpt);
pthreadMutexUnlock(&memMutex);
}

static void *threadSafeRealloc(void *vpt, size_t size)
/* Wrap mutex lock around realloc. */
{
void *result;
pthreadMutexLock(&memMutex);
result = parentMem->realloc(vpt, size);
pthreadMutexUnlock(&memMutex);
return result;
}

static struct memHandler threadSafeMemHandler = 
/* Default memory handler. */
    {
    NULL,
    threadSafeAlloc,
    threadSafeFree,
    threadSafeRealloc,
    };


void _pf_init_mem()
/* Initialize thread safe memory handler. */
{
#ifdef UNNEEDED
pthreadMutexInit(&memMutex);
parentMem = pushMemHandler(&threadSafeMemHandler);
#endif /* UNNEEDED */
}

void *_pf_need_mem(int size)
/* Allocate memory, which is initialized to zero. */
{
return needMem(size);
}

void _pf_free_mem(void *pt)
/* Free memory. */
{
return freeMem(pt);
}

void _pf_clear_mem(void *pt, int size)
/* Clear memory. */
{
memset(pt, 0, size);
}

void _pf_init_args(int argc, char **argv, _pf_String *retProg, _pf_Array *retArgs, 
	char *environ[])
/* Set up command line arguments. */
{
_pf_Array args;
int stringTypeId = _pf_find_string_type_id();
int i;
*retProg = _pf_string_from_const(argv[0]);
struct _pf_string **a;
args = _pf_dim_array(argc-1, stringTypeId);
a = (struct _pf_string **)(args->elements);
for (i=1; i<argc; ++i)
    a[i-1] = _pf_string_from_const(argv[i]);
*retArgs = args;
cl_env = environ;
}

void pf_getEnvArray(_pf_Stack *stack)
/* Return an array of string containing the environment
 * int var=val format.   We'll leave it to paraFlow 
 * library to turn this into a hash. */
{
int stringTypeId = _pf_find_string_type_id();
int envSize = 0;
int i;
struct _pf_string **a;
_pf_Array envArray;
for (i=0; ; ++i)
    {
    if (cl_env[i] == NULL)
        break;
    envSize += 1;
    }
envArray = _pf_dim_array(envSize, stringTypeId);
a = (struct _pf_string **)(envArray->elements);

for (i=0; i<envSize; ++i)
    a[i] = _pf_string_from_const(cl_env[i]);

stack[0].Array = envArray;
}

void pf_keyIn(_pf_Stack *stack)
/* Get next key press. */
{
char buf[2];
buf[0] = rawKeyIn();
buf[1] = 0;
stack[0].String = _pf_string_from_const(buf);
}

void pf_lineIn(_pf_Stack *stack)
/* Get next line of input from stdin.  In general
 * you do not want to mix calls to keyIn and lineIn,
 * since keyIn is unbuffered and lineIn is buffered. 
 * Returns nil at EOF. */
{
char buf[256];
int i, c;
FILE *f = stdin;
for (i=0; i<sizeof(buf)-1;  ++i)
    {
    c = fgetc(f);
    if (c < 0)
	{
	if (i == 0)
	    stack[0].String = NULL;
	else
	    {
	    buf[i] = 0;
	    stack[0].String = _pf_string_from_const(buf);
	    }
	return;
	}
    buf[i] = c;
    if (c == '\n')
	{
	buf[i] = 0;
	stack[0].String = _pf_string_dupe(buf, i);
	return;
	}
    }
/* Well, looks like it's a pretty long string! 
 * Let's convert to dyString based capture rather
 * than using the fixed size buffer on stack. */
    {
    struct dyString *dy = dyStringNew(512);
    struct _pf_string *string;
    dyStringAppendN(dy, buf, i);
    for (;;)
        {
	c = fgetc(f);
	if (c < 0)
	    break;
	dyStringAppendC(dy, c);
	if (c == '\n')
	    break;
	}
    string = _pf_string_new(dy->string, dy->bufSize);
    string->size = dy->stringSize;
    dyStringCannibalize(&dy);
    stack[0].String = string;
    }
}

void pf_milliTicks(_pf_Stack *stack)
/* Return a long value that counts up pretty
 * quickly.  Units are milliseconds.   It may
 * either be starting since program startup or
 * relative to some other absolute number depending
 * on the system it is running on. */
{
stack[0].Long = clock1000();
}


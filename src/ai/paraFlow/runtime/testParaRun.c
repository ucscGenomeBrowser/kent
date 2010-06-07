
#include "common.h"
#include "../compiler/pfPreamble.h"
#include "paraRun.h"

struct _pf_activation *_pf_activation_stack;

struct myLocal
    {
    int x,y,z;
    char *name;
    };

void process(_pf_Stack *stack, char *key, void *item, void *localVars)
{
struct _pf_string **pString = item;
struct _pf_string *string = *pString;
struct myLocal *local = localVars;

printf(">>processing<< %p@%s, %d,%d,%d,%s\n", key,string->s, local->x,local->y,local->z,local->name);
}

struct _pf_array *fakeStringArray(int size)
{
struct _pf_array *array;
struct _pf_string *string, **strings;
char buf[256];
int i;
AllocVar(array);
array->elSize = sizeof(struct _pf_string*);
array->size = array->allocated = size;
array->elements = needMem(size*array->elSize);
strings = (struct _pf_string **)(array->elements);
for (i=0; i<size; ++i)
    {
    safef(buf, sizeof(buf), "string %d", i);
    strings[i] = _pf_string_from_const(buf);
    }
return array;
}

int main(int argc, char *argv[])
{
static struct myLocal local = {1,2,3,"localStuff"};
struct _pf_array *array = fakeStringArray(6);
int i;
_pf_paraRunInit();
for (i=1; i<=3; ++i)
   uglyf("Testing %d\n", i);

_pf_paraRunArray(array, &local, process, prtParaDo, 0);
// _pf_paraRunArray(array, &local, process, prtParaDo, 0);
return 0;
}

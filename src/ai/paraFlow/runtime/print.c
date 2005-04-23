/* print - Implements print function for variable types. */
#include "common.h"
#include "hash.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"


/* Stuff to keep track of object IDs. */
static int curObjId = 0;

static int idLookup(struct hash *hash, void *obj)
/* Look up object in hash.  Return 0 if can't find it.
 * Otherwise return object id. */
{
char buf[17];
safef(buf, sizeof(buf), "%p", obj);
return hashIntValDefault(hash, buf, 0);
}

static void idAdd(struct hash *hash, void *obj)
/* Add to object hash */
{
char buf[17];
safef(buf, sizeof(buf), "%p", obj);
hashAddInt(hash, buf, ++curObjId);
}

static boolean reuseObject(FILE *f, struct hash *idHash, void *obj)
/* Print code for reuse of object if possible.  Otherwise
 * print nothing, save object id and return FALSE. */
{
int id = idLookup(idHash, obj);
if (id != 0)
    {
    fprintf(f, "$%d", id);
    return TRUE;
    }
else
    {
    idAdd(idHash, obj);
    return FALSE;
    }
}

void printField(FILE *f, void *data, struct _pf_base *base, 
	struct hash *idHash);

void printClass(FILE *f, struct _pf_object *obj, struct _pf_base *base,
	struct hash *idHash)
/* Print out each field of class. */
{
struct _pf_type *field;
if (obj == NULL)
    fprintf(f, "()");
else
    {
    if (!reuseObject(f, idHash, obj))
	{
	char *s = (char *)obj;
	fprintf(f, "(");
	for (field = base->fields; field != NULL; field = field->next)
	    {
	    printField(f, s+field->offset, field->base, idHash);
	    if (field->next != NULL)
		 fprintf(f, ",");
	    }
	fprintf(f, ")");
	}
    }
}

void printString(FILE *f, struct _pf_string *string)
/* Print out string. */
{
if (string != NULL)
    fprintf(f, "%s", string->s);
}


void printArray(FILE *f, struct _pf_array *array, struct _pf_base *base,
	struct hash *idHash)
/* Print out each element of Array. */
{
if (array == NULL)
    fprintf(f, "()");
else
    {
    if (!reuseObject(f, idHash, array))
	{
	struct _pf_type *elType = array->elType;
	int i;
	boolean needsComma = FALSE;
	char *s = array->elements;
	fprintf(f, "(");
	for (i=0; i<array->size; ++i)
	    {
	    if (needsComma)
		 fprintf(f, ",");
	    needsComma = TRUE;
	    printField(f, s, elType->base, idHash);
	    s += array->elSize;
	    }
	fprintf(f, ")");
	}
    }
}

void printDir(FILE *f, struct _pf_dir *dir, struct _pf_base *base,
	struct hash *idHash)
/* Print out each key/val pair in dir. */
{
if (dir == NULL)
    fprintf(f, "()");
else
    {
    if (!reuseObject(f, idHash, dir))
	{
	struct hash *hash = dir->hash;
	struct hashEl *hel, *helList = hashElListHash(hash);
	struct _pf_base *base = dir->elType->base;
	slSort(&helList, hashElCmp);
	fprintf(f, "(");
	for (hel = helList; hel != NULL; hel = hel->next)
	    {
	    fprintf(f, "\"%s\" to ", hel->name);
	    if (base->needsCleanup)
		printField(f, &hel->val, base, idHash);
	    else
		printField(f, hel->val, base, idHash);
	    if (hel->next != NULL)
		 fprintf(f, ",");
	    }
	hashElFreeList(&helList);
	fprintf(f, ")");
	}
    }
}


void printField(FILE *f, void *data, struct _pf_base *base, struct hash *idHash)
/* Print out a data from a single field of given type. */
{
switch (base->singleType)
    {
    case pf_stBit:
	{
	_pf_Bit *p = data;
        fprintf(f, "%d", *p);
	break;
	}
    case pf_stByte:
	{
        _pf_Byte *p = data;
	_pf_Byte b = *p;
	if (isascii(b))
	    fprintf(f, "%c", b);
	else
	    fprintf(f, "0x%x", b);
	break;
	}
    case pf_stShort:
	{
        _pf_Short *p = data;
	fprintf(f, "%d", *p);
	break;
	}
    case pf_stInt:
	{
        _pf_Int *p = data;
	fprintf(f, "%d", *p);
	break;
	}
    case pf_stLong:
	{
        _pf_Long *p = data;
	fprintf(f, "%lld", *p);
	break;
	}
    case pf_stFloat:
	{
        _pf_Float *p = data;
	fprintf(f, "%f", *p);
	break;
	}
    case pf_stDouble:
	{
        _pf_Double *p = data;
	fprintf(f, "%f", *p);
	break;
	}
    case pf_stString:
	{
        _pf_String *p = data;
	fputc('"', f);
	printString(f, *p);
	fputc('"', f);
	break;
	}
    case pf_stClass:
	{
	struct _pf_object **p = data;
        printClass(f, *p, base, idHash);
	break;
	}
    case pf_stArray:
        {
	struct _pf_array **p = data;
        printArray(f, *p, base, idHash);
	break;
	}
    case pf_stDir:
        {
	struct _pf_dir **p = data;
        printDir(f, *p, base, idHash);
	break;
	}
    case pf_stVar:
        {
	struct _pf_var *var = data;
	struct _pf_type *type = _pf_type_table[var->typeId];
	printField(f, &var->val, type->base, idHash);
	break;
	}
    default:
	internalErr();
	break;
    }
}

void _pf_prin(FILE *f, _pf_Stack *stack)
/* Print out single variable where type is determined at run time. */
{
struct _pf_type *type = _pf_type_table[stack->Var.typeId];
struct _pf_base *base = type->base;
union _pf_varless val = stack->Var.val;
struct hash *idHash = NULL;
curObjId = 0;
switch (base->singleType)
    {
    case pf_stBit:
        fprintf(f, "%d", val.Bit);
	break;
    case pf_stByte:
	{
	_pf_Byte b = val.Byte;
	if (isascii(b))
	    fprintf(f, "%c", b);
	else
	    fprintf(f, "0x%x", b);
	break;
	}
    case pf_stShort:
        fprintf(f, "%d", val.Short);
	break;
    case pf_stInt:
        fprintf(f, "%d", val.Int);
	break;
    case pf_stLong:
        fprintf(f, "%lld", val.Long);
	break;
    case pf_stFloat:
        fprintf(f, "%f", val.Float);
	break;
    case pf_stDouble:
        fprintf(f, "%f", val.Double);
	break;
    case pf_stString:
	printString(f, val.String);
	break;
    case pf_stClass:
	idHash = newHash(18);
	printClass(f, val.Obj, base, idHash);
	break;
    case pf_stArray:
	idHash = newHash(18);
	printArray(f, val.Array, base, idHash);
	break;
    case pf_stDir:
	idHash = newHash(18);
        printDir(f, val.Dir, base, idHash);
	break;
    default:
        fprintf(f, "<type %d>\n", base->singleType);
	break;
    }
if (base->needsCleanup)
    {
    if (val.Obj != NULL && --val.Obj->_pf_refCount <= 0)
	val.Obj->_pf_cleanup(val.Obj, stack->Var.typeId);
    }
freeHash(&idHash);
}

void prin(_pf_Stack *stack)
/* Print out single variable where type is determined at run time. */
{
_pf_prin(stdout, stack);
}

void print(_pf_Stack *stack)
/* Print out single variable where type is determined at run time. 
 * Add newline. */
{
prin(stack);
fputc('\n', stdout);
}

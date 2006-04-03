/* print - Implements print function for variable types. */
#include "common.h"
#include "hash.h"
#include "../compiler/pfPreamble.h"
#include "runType.h"
#include "print.h"

static void printEscapedString(FILE *f, char *s)
/* Print string in such a way that C can use it. */
{
char c;
fputc('\'', f);
while ((c = *s++) != 0)
    {
    switch (c)
        {
	case '\n':
	    fputs("\\n", f);
	    break;
	case '\r':
	    fputs("\\r", f);
	    break;
	case '\\':
	    fputs("\\\\", f);
	    break;
	case '\'':
	    fputs("\\'", f);
	    break;
	default:
	    fputc(c, f);
	    break;
	}
    }
fputc('\'', f);
}

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
hashAddInt(hash, buf, hash->elCount+1);
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

static void printClass(FILE *f, struct _pf_object *obj, struct _pf_base *base,
	struct hash *idHash)
/* Print out each field of class. */
{
struct _pf_type *field;
if (obj == NULL)
    fprintf(f, "nil");
else
    {
    if (idHash == NULL || !reuseObject(f, idHash, obj))
	{
	char *s = (char *)obj;
	fprintf(f, "(");
	for (field = base->fields; field != NULL; field = field->next)
	    {
	    if (idHash != NULL || field->base->fields == NULL)
		_pf_printField(f, s+field->offset, field->base, idHash);
	    else
	        fprintf(f, "<%s>", field->base->name);
	    if (field->next != NULL)
		 fprintf(f, ",");
	    }
	fprintf(f, ")");
	}
    }
}

static void printString(FILE *f, struct _pf_string *string)
/* Print out string. */
{
if (string != NULL)
    {
    // fprintf(f, "[ugly:%d]", string->_pf_refCount);
    printEscapedString(f, string->s);
    }
else
    fprintf(f, "nil");
}


static void printArray(FILE *f, struct _pf_array *array, struct _pf_base *base,
	struct hash *idHash)
/* Print out each element of Array. */
{
if (array == NULL)
    fprintf(f, "nil");
else
    {
    if (idHash)
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
		_pf_printField(f, s, elType->base, idHash);
		s += array->elSize;
		}
	    fprintf(f, ")");
	    }
	}
    else
        {
	fprintf(f, "<array of %d>", array->size);
	}
    }
}

static void printDir(FILE *f, struct _pf_dir *dir, struct _pf_base *base,
	struct hash *idHash)
/* Print out each key/val pair in dir. */
{
if (dir == NULL)
    fprintf(f, "()");
else
    {
    if (idHash)
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
		fprintf(f, "'%s'@", hel->name);
		if (base->needsCleanup)
		    _pf_printField(f, &hel->val, base, idHash);
		else
		    _pf_printField(f, hel->val, base, idHash);
		if (hel->next != NULL)
		     fprintf(f, ",");
		}
	    hashElFreeList(&helList);
	    fprintf(f, ")");
	    }
	}
    else
        {
	fprintf(f, "<dir of %d>", dir->hash->elCount);
	}
    }
}


void _pf_printField(FILE *f, void *data, struct _pf_base *base, 
	struct hash *idHash)
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
    case pf_stChar:
	{
        _pf_Char *p = data;
	_pf_Char b = *p;
	fprintf(f, "%c", b);
	break;
	}
    case pf_stByte:
	{
        _pf_Byte *p = data;
	_pf_Byte b = *p;
	fprintf(f, "%d", b);
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
	printString(f, *p);
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
    case pf_stToPt:
	{
	_pf_FunctionPt **p = data;
	fprintf(f, "<toPt %p>", *p);
        break;
	}
    case pf_stFlowPt:
	{
	_pf_FunctionPt **p = data;
	fprintf(f, "<flowPt %p>", *p);
        break;
	}
    case pf_stVar:
        {
	struct _pf_var *var = data;
	struct _pf_type *type = _pf_type_table[var->typeId];
	_pf_printField(f, &var->val, type->base, idHash);
	break;
	}
    default:
	internalErr();
	break;
    }
}

void _pf_prin(FILE *f, _pf_Stack *stack, boolean formal)
/* Print out single variable where type is determined at run time. */
{
struct _pf_type *type = _pf_type_table[stack->Var.typeId];
struct _pf_base *base = type->base;
union _pf_varless val = stack->Var.val;
struct hash *idHash = NULL;
switch (base->singleType)
    {
    case pf_stBit:
        fprintf(f, "%d", val.Bit);
	break;
    case pf_stByte:
	fprintf(f, "%d", val.Byte);
	break;
    case pf_stChar:
	fprintf(f, "%c", val.Char);
	break;
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
	if (formal)
	    fprintf(f, "%f", val.Float);
	else
	    fprintf(f, "%0.2f", val.Float);
	break;
    case pf_stDouble:
	if (formal)
	    fprintf(f, "%f", val.Double);
	else
	    fprintf(f, "%0.2f", val.Double);
	break;
    case pf_stString:
	if (formal || val.String == NULL)
	    printString(f, val.String);
	else
	    fprintf(f, "%s", val.String->s);
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
    case pf_stToPt:
	fprintf(f, "<toPt %p>\n", val.FunctionPt);
        break;
    case pf_stFlowPt:
	fprintf(f, "<flowPt %p>\n", val.FunctionPt);
        break;
    default:
        fprintf(f, "<type %d>\n", base->singleType);
	internalErr();
	break;
    }
if (base->needsCleanup)
    {
    if (val.Obj != NULL && --val.Obj->_pf_refCount <= 0)
	val.Obj->_pf_cleanup(val.Obj, stack->Var.typeId);
    }
freeHash(&idHash);
}

void pf_prin(_pf_Stack *stack)
/* Print out single variable where type is determined at run time. */
{
_pf_prin(stdout, stack, FALSE);
}

void pf_print(_pf_Stack *stack)
/* Print out single variable where type is determined at run time. 
 * Add newline. */
{
pf_prin(stack);
fputc('\n', stdout);
}

void pf_ugly(_pf_Stack *stack)
/* This is just a synonym for print to distinguish debugging statement
 * from real prints. */
{
pf_print(stack);
}

void pf__printByte(_pf_Stack *stack)
/* Print integer, just for debugging really */
{
printf("%d\n", stack->Byte);
}

void pf__printShort(_pf_Stack *stack)
/* Print integer, just for debugging really */
{
printf("%d\n", stack->Short);
}

void pf__printInt(_pf_Stack *stack)
/* Print integer, just for debugging really */
{
printf("%d\n", stack->Int);
}

void pf__printLong(_pf_Stack *stack)
/* Print long, just for debugging really */
{
printf("%lld\n", stack->Long);
}

void pf__printFloat(_pf_Stack *stack)
/* Print double, just for debugging really */
{
printf("%f\n", stack->Float);
}

void pf__printDouble(_pf_Stack *stack)
/* Print double, just for debugging really */
{
printf("%f\n", stack->Double);
}

void pf__printString(_pf_Stack *stack)
/* Print double, just for debugging really */
{
struct _pf_string *string = stack->String;
char *text = "(null)";
if (string != NULL)
    text = string->s;
printf("%s\n", text);
}


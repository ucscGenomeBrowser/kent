/* getObj - get an object out of a file in the same format
 * it was put into the file with (parents, commas, quotes etc.).
 * This was first called 'scan' and most of the local names
 * are still that way.  The other side of this lives in the
 * print module. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "../compiler/pfPreamble.h"

static void scanField(FILE *f, void *data, struct _pf_type *type, 
	struct hash *idHash);
/* Print out a data from a single field of given type. */

static _pf_Int  scanInt(FILE *f);
/* Read integer or die trying */

static void *idLookup(struct hash *idHash, int id)
/* Look up object in hash.  */
{
char buf[17];
struct _pf_object *obj;
safef(buf, sizeof(buf), "%X", id);
obj = hashFindVal(idHash, buf);
if (obj == NULL)
    errAbort("no such ID");
obj->_pf_refCount += 1;
return obj;
}

static void idAdd(struct hash *idHash, void *v)
/* Add to object hash */
{
char buf[17];
safef(buf, sizeof(buf), "%X", idHash->elCount+1);
hashAdd(idHash, buf, v);
}

static void *idUse(struct hash *idHash, FILE *f)
/* This will look at next character.  If it's a parenthesis
 * it will eat it and return NULL.  If it's  a $ then it
 * will read the next number and then return the associated
 * object. */
{
int c = fgetc(f);
if (c == '$')
    {
    int id  = scanInt(f);
    return idLookup(idHash, id);
    }
else if (c != '(')
    errAbort("Expecting parenthesis");
return NULL;
}

_pf_Int  scanInt(FILE *f)
/* Read integer or die trying */
{
_pf_Int  i;
if (fscanf(f, "%d", &i) != 1)
    errAbort("Not an int");
return i;
}

_pf_Long scanLong(FILE *f)
/* Read integer or die trying */
{
_pf_Long i;
if (fscanf(f, "%lld", &i) != 1)
    errAbort("Not a long");
return i;
}

_pf_Double scanDouble(FILE *f)
/* Read double or die trying */
{
_pf_Double i;
if (fscanf(f, "%lf", &i) != 1)
    errAbort("Not a double");
return i;
}

_pf_Char scanChar(FILE *f)
/* Read char or die trying */
{
_pf_Char i;
if (fscanf(f, "%c", &i) != 1)
    errAbort("Not a char");
return i;
}


static void scanDyString(FILE *f, struct dyString *dy)
/* Scan between quotes into dy. */
{
boolean isEscaped = FALSE;
int c = fgetc(f);
if (c != '"')
   errAbort("Not a string");
for (;;)
   {
   c = fgetc(f);
   if (c == EOF)
       break;
   if (isEscaped)
	{
	isEscaped = FALSE;
	switch (c)
	    {
	    case 'n':
		c = '\n';
		break;
	    }
	dyStringAppendC(dy, c);
	}
   else
	{
	if (c == '\\')
	    isEscaped = TRUE;
	else if (c == '"')
	    break;
	else
	    dyStringAppendC(dy, c);
	}
   }
}

static struct _pf_string *scanString(FILE *f)
/* Read quoted string from file. */
{
struct dyString *dy = dyStringNew(0);
struct _pf_string *s;
scanDyString(f, dy);
s = _pf_string_from_const(dy->string);
dyStringFree(&dy);
return s;
}

static boolean gotNil(FILE *f)
/* Return TRUE if next characters are 'nil'  
 * and eat the nil. */
{
int c = getc(f);
if (c == 'n')
    {
    int c1 = getc(f);
    int c2 = getc(f);
    if (c1 != 'i' || c2 != 'l')
        errAbort("expecting nil");
    return TRUE;
    }
else
    {
    ungetc(c, f);
    return FALSE;
    }
}

static struct _pf_object *scanClass(FILE *f, struct _pf_type *type, 
	struct hash *idHash)
/* Read in a class. */
{
if (gotNil(f))
    return NULL;
else
    {
    struct _pf_base *base = type->base;
    struct _pf_object *obj = idUse(idHash, f);
    if (obj == NULL)
	{
	int c;
	struct _pf_type *field;
	char *s;
	obj = needMem(base->objSize);
	s = (char *)(obj);
	idAdd(idHash, obj);
	obj->_pf_refCount = 1;
	obj->_pf_cleanup = _pf_class_cleanup;
	obj->_pf_polyTable = base->polyTable;
	for (field = base->fields; field != NULL; field = field->next)
	    {
	    scanField(f, s+field->offset, field, idHash);
	    if (field->next != NULL)
		{
		c = fgetc(f);
		if (c != ',')
		    errAbort("Expecting comma");
		}
	    }
	c = fgetc(f);
	if (c != ')')
	    errAbort("Expecting )");
	}
    return obj;
    }
}

static struct _pf_array *scanArray(FILE *f, struct _pf_type *type, 
	struct hash *idHash)
/* Read in a array. */
{
struct _pf_array *array = NULL;
if (!gotNil(f))
    {
    array = idUse(idHash, f);
    if (array == NULL)
	{
	struct _pf_type *elType = type->children;
	struct _pf_base *elBase = elType->base;
	int size = 0, offset = 0;
	boolean needComma = FALSE;
	int c;
	array = _pf_dim_array(16, elType->typeId);
	idAdd(idHash, array);
	for (;;)
	    {
	    c = fgetc(f);
	    if (c == ')')
		break;
	    if (needComma)
		{
		if (c != ',')
		    errAbort("Expecting comma");
		}
	    else
		{
		needComma = TRUE;
		ungetc(c, f);
		}
	    if (size >= array->allocated)
		{
		int toAdd = size;
		if (toAdd > 64*1024)
		    toAdd = 64*1024;
		array->allocated += toAdd;
		array->elements = needLargeMemResize(array->elements, 
					array->allocated * array->elSize);
		}
	    scanField(f, array->elements + offset, elType, idHash);
	    offset += array->elSize;
	    size += 1;
	    }
	array->size = size;
	}
    }
return array;
}

static struct _pf_dir *scanDir(FILE *f, struct _pf_type *type, 
	struct hash *idHash)
/* Read in a dir. */
{
struct _pf_dir *dir = NULL;
if (!gotNil(f))
    {
    dir = idUse(idHash, f);
    if (dir == NULL)
	{
	struct dyString *dy = dyStringNew(0);
	struct _pf_type *elType = type->children;
	struct _pf_base *elBase = elType->base;
	boolean needComma = FALSE;
	int c;
	char toBuf[5];
	void *v;

	dir = pfDirNew(0, elType);
	idAdd(idHash, dir);
	for (;;)
	    {
	    c = fgetc(f);
	    if (c == ')')
		break;
	    if (needComma)
		{
		if (c != ',')
		    errAbort("Expecting comma");
		}
	    else
		{
		needComma = TRUE;
		ungetc(c, f);
		}
	    dyStringClear(dy);
	    scanDyString(f, dy);
	    if (fread(toBuf, 1, 1, f) != 1)
		errAbort("Unexpected end of file");
	    if (memcmp(toBuf, ":", 1) != 0)
		errAbort("Expecting 'to'");
	    switch (elBase->singleType)
		{
		case pf_stArray:
		case pf_stClass:
		case pf_stString:
		case pf_stDir:
		    {
		    scanField(f, &v, elType, idHash);
		    break;
		    }
		case pf_stBit:
		    {
		    _pf_Bit b;
		    scanField(f, &b, elType, idHash);
		    v = CloneVar(&b);
		    break;
		    }
		case pf_stByte:
		    {
		    _pf_Byte b;
		    scanField(f, &b, elType, idHash);
		    v = CloneVar(&b);
		    break;
		    }
		case pf_stShort:
		    {
		    _pf_Short b;
		    scanField(f, &b, elType, idHash);
		    v = CloneVar(&b);
		    break;
		    }
		case pf_stInt:
		    {
		    _pf_Int b;
		    scanField(f, &b, elType, idHash);
		    v = CloneVar(&b);
		    break;
		    }
		case pf_stLong:
		    {
		    _pf_Long b;
		    scanField(f, &b, elType, idHash);
		    v = CloneVar(&b);
		    break;
		    }
		case pf_stFloat:
		    {
		    _pf_Float b;
		    scanField(f, &b, elType, idHash);
		    v = CloneVar(&b);
		    break;
		    }
		case pf_stDouble:
		    {
		    _pf_Double b;
		    scanField(f, &b, elType, idHash);
		    v = CloneVar(&b);
		    break;
		    }
		case pf_stChar:
		    {
		    _pf_Char b;
		    scanField(f, &b, elType, idHash);
		    v = CloneVar(&b);
		    break;
		    }
		case pf_stVar:
		    errAbort("Can't scan data containing var types.");
		    break;
		default:
		    internalErr();
		    break;
		}
	    hashAdd(dir->hash, dy->string, v);
	    }
	dyStringFree(&dy);
	}
    }
return dir;
}

static void scanField(FILE *f, void *data, struct _pf_type *type, 
	struct hash *idHash)
/* Print out a data from a single field of given type. */
{
switch (type->base->singleType)
    {
    case pf_stBit:
	{
	_pf_Bit *p = data;
	*p = scanInt(f);
	break;
	}
    case pf_stByte:
	{
        _pf_Byte *p = data;
	*p = scanInt(f);
	break;
	}
    case pf_stShort:
	{
        _pf_Short *p = data;
	*p = scanInt(f);
	break;
	}
    case pf_stInt:
	{
        _pf_Int *p = data;
	*p = scanInt(f);
	break;
	}
    case pf_stLong:
	{
        _pf_Long *p = data;
	*p = scanLong(f);
	break;
	}
    case pf_stFloat:
	{
        _pf_Float *p = data;
	*p = scanDouble(f);
	break;
	}
    case pf_stDouble:
	{
        _pf_Double *p = data;
	*p = scanDouble(f);
	break;
	}
    case pf_stChar:
	{
        _pf_Char *p = data;
	*p = scanChar(f);
	break;
	}
    case pf_stString:
	{
        _pf_String *p = data;
	*p = scanString(f);
	break;
	}
    case pf_stClass:
	{
	struct _pf_object **p = data;
        *p = scanClass(f, type, idHash);
	break;
	}
    case pf_stArray:
        {
	struct _pf_array **p = data;
        *p = scanArray(f,  type, idHash);
	break;
	}
    case pf_stDir:
        {
	struct _pf_dir **p = data;
        *p = scanDir(f, type, idHash);
	break;
	}
    case pf_stToPt:
    case pf_stFlowPt:
	errAbort("Can't scan data containing var of (function pointer) types.");
	break;
    case pf_stVar:
	errAbort("Can't scan data containing var types.");
	break;
    default:
	internalErr();
	break;
    }
}


void _pf_getObj(FILE *f, _pf_Stack *stack)
/* Read in variable from file. */
{
int typeId = stack[1].Var.typeId;
struct _pf_type *type = _pf_type_table[typeId];
struct _pf_base *base = type->base;
union _pf_varless val = stack[1].Var.val;
struct hash *idHash = NULL;
int c;

switch (base->singleType)
    {
    case pf_stBit:
	c = getc(f);
	if (c == '0')
	    stack[0].Bit = 0;
	else if (c == '1')
	    stack[0].Bit = 1;
	else
	    errAbort("Not a bit");
	break;
    case pf_stByte:
	{
	stack[0].Byte = scanInt(f);
	break;
	}
    case pf_stShort:
	stack[0].Short = scanInt(f);
	break;
    case pf_stInt:
	stack[0].Int = scanInt(f);
	break;
    case pf_stLong:
	stack[0].Long = scanLong(f);
	break;
    case pf_stFloat:
	stack[0].Float = scanDouble(f);
	break;
    case pf_stDouble:
	stack[0].Double = scanDouble(f);
	break;
    case pf_stChar:
	stack[0].Char = scanChar(f);
	break;
    case pf_stString:
	stack[0].String = scanString(f);
	break;
    case pf_stClass:
	idHash = newHash(18);
	stack[0].Obj = scanClass(f, type, idHash);
	break;
    case pf_stArray:
	idHash = newHash(18);
	stack[0].Array = scanArray(f, type, idHash);
	break;
    case pf_stDir:
	idHash = newHash(18);
	stack[0].Dir = scanDir(f, type, idHash);
	break;
    case pf_stToPt:
    case pf_stFlowPt:
	errAbort("Can't scan data containing var of (function pointer) types.");
	break;
    case pf_stVar:
	errAbort("Can't scan data containing var types.");
	break;
    default:
	errAbort("unknown type");
	break;
    }

hashFree(&idHash);
/* Clean up input. */
if (base->needsCleanup)
    {
    if (val.Obj != NULL && --val.Obj->_pf_refCount <= 0)
	val.Obj->_pf_cleanup(val.Obj, stack->Var.typeId);
    }

/* Return type rest of output. */
stack[0].Var.typeId = typeId;
}

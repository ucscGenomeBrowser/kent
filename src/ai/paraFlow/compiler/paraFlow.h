

struct pfScope
/* This controls the scope of symbols. */
    {
    struct pfScore *next;	/* Next in list */
    struct pfSymTab *typeSym;	/* Symbol table for types. */
    struct pfSymTab *varSym;	/* Symbol table for functions & variables */
    struct pfStatement *statements;	/* Statements in this scope. */
    };

enum pfSingleType
    {
    pfstByte,
    pfstShort,
    pfstInt,
    pfstLong,
    pfstFloat,
    pfstDouble,
    pfstString,
    pfstArray,
    pfstList,'
    pfstDir,
    pfstTree,
    pfstPointer,
    pfstClass,
    };

struct pfFuncType
/* Information on the type of a function */
    {
    struct pfDef *inputList;	/* List of inputs */
    struct pfDef *outputList;	/* List of outputs. */
    };

struct pfClassType
/* Information on the type of a class. */
    {
    struct pfType *parent;	/* Parent in object hierarchy */
    struct pfDef *components;	/* Component elements of this class. */
    };

struct pfType
/* A type, may have parents in object heirarchy, and
 * be composed of multiple elements. */
    {
    struct pfType *next;
    char *name;			/* Type name */
    int size;			/* Storage size required */
    enum pfSingleType type;	/* Basic type of this node. */
    union 
    	{
    	struct pfFuncType funcVal; 	/* Information for function types. */
	struct ffClassType classVal;	/* Information for class types. */
	struct pfType *baseVal;		/* Base type for collections & pointers. */
	} parts;
    };

struct pfDef
/* A type, name, and optionally an initialization */
    {
    struct pfDef *next;
    struct pfType *type;	/* Class of this object. */
    char *name;			/* Name of this object instance */
    struct pfExp *init;		/* Optional initialization for data objects */
    struct pfScope *scope;	/* Handles object body */
    };

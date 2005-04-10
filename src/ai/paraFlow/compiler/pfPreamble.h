/* pfPreamble - this gets included at the start of
 * paraFlow-generated C code. */

struct string;
struct array;
struct list;
struct tree;
struct dir;
struct var;
struct pfType;

typedef struct string *String;
typedef struct array *Array;
typedef struct list *List;
typedef struct tree *Tree;
typedef struct dir *Dir;
typedef struct var Var;

typedef char Bit;
typedef unsigned char Byte;
typedef short Short;
typedef int Int;
typedef long long Long;
typedef float Float;
typedef double Double;

union pfVarless
    {
    Bit Bit;
    Byte Byte;
    Short Short;
    Int Int;
    Long Long;
    Float Float;
    Double Double;
    String String;
    Array Array;
    List List;
    Tree Tree;
    Dir Dir;
    void *v;
    };

struct var
    {
    union pfVarless val;
    struct pfType *type;
    };

union pfStack
    {
    Bit Bit;
    Byte Byte;
    Short Short;
    Int Int;
    Long Long;
    Float Float;
    Double Double;
    String *String;
    Array *Array;
    List *List;
    Tree *Tree;
    Dir *Dir;
    void *v;
    Var Var;
    };

typedef union pfStack PfStack;


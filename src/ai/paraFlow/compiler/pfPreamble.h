/* pfPreamble - this gets included at the start of
 * paraFlow-generated C code. */

struct string;
struct array;
struct list;
struct tree;
struct dir;
struct var;

typedef struct string String;
typedef struct array Array;
typedef struct list List;
typedef struct tree Tree;
typedef struct dir Dir;
typedef struct var Var;

typedef char Bit;
typedef unsigned char Byte;
typedef short Short;
typedef int Int;
typedef long long Long;
typedef float Float;
typedef double Double;

union pfStack
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
    Var Var;
    void *v;
    };

typedef pfStack PfStack;


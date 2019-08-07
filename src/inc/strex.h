/* strex.h - interface to string expression language,  currently used in tabToTabDir
 * to describe how the output fields are filled in from input fields. */


/* Parsing out something into strex */
struct strexParse;    /* A parser generated tree */

struct strexParse *strexParseString(char *s);
/* Parse out string expression in s and return root of tree. */

void strexParseDump(struct strexParse *p, int depth, FILE *f);
/* Dump out strexParse tree and children. */



/* Evaluating a parsed out strex expression */

typedef char* (*StrexEvalLookup)(void *symbols, char *key);
/* Callback to lookup value ov key in a symbol table */

char *strexEvalAsString(struct strexParse *p, void *symbols, StrexEvalLookup lookup);
/* Evaluating a strex expression on a symbol table with a lookup function for variables and
 * return result as a string value. */


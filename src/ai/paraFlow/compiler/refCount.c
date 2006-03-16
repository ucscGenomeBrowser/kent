/* refCount - stuff to manipulate object reference counts.
 * We have to use a little inline assembler to get these to
 * happen in a thread-safe manner. */

#include "common.h"
#include "pfCompile.h"
#include "cCoder.h"
#include "refCount.h"

#if defined(PARALLEL)

void codeVarIncRef(FILE *f, char *varName)
/* Bump ref count of a variable on pentium family. */
{
fprintf(f, 
"asm(\"lock ;\"\n"
"\t\"incl %%1;\"\n"
"\t:\"=m\" (%s->_pf_refCount)\n"
"\t:\"m\" (%s->_pf_refCount)\n"
"\t:\"memory\");\n", varName, varName);
}

void codeStackIncRef(FILE *f, int stack)
/* Bump ref count of something on expression stack on pentium family. */
{
fprintf(f, 
"asm(\"lock ;\"\n"
"\t\"incl %%1;\"\n"
"\t:\"=m\" (%s[%d].Obj->_pf_refCount)\n"
"\t:\"m\" (%s[%d].Obj->_pf_refCount)\n"
"\t:\"memory\");\n", stackName, stack, stackName, stack);
}

void codeVarDecRef(FILE *f, char *varName)
/* Bump ref count of a variable on pentium family. */
{
fprintf(f, 
"asm(\"lock ;\"\n"
"\t\"decl %%1;\"\n"
"\t:\"=m\" (%s->_pf_refCount)\n"
"\t:\"m\" (%s->_pf_refCount)\n"
"\t:\"memory\");\n", varName, varName);
}

void codeStackDecRef(FILE *f, int stack)
/* Bump ref count of something on expression stack on pentium family. */
{
fprintf(f, 
"asm(\"lock ;\"\n"
"\t\"decl %%1;\"\n"
"\t:\"=m\" (%s[%d].Obj->_pf_refCount)\n"
"\t:\"m\" (%s[%d].Obj->_pf_refCount)\n"
"\t:\"memory\");\n", stackName, stack, stackName, stack);
}

#else

void codeVarIncRef(FILE *f, char *varName)
/* Bump ref count of a variable generically but not thread-safely. */
{
fprintf(f, "%s->_pf_refCount += 1;\n", varName);
}

void codeStackIncRef(FILE *f, int stack)
/* Bump ref count of something on expression stack not thread-safely. */
{
fprintf(f, "%s[%d].Obj->_pf_refCount += 1;\n", stackName, stack);
}

void codeVarDecRef(FILE *f, char *varName)
/* Dec ref count of a variable generically but not thread-safely. */
{
fprintf(f, "%s->_pf_refCount -= 1;\n", varName);
}

void codeStackDecRef(FILE *f, int stack)
/* Dec ref count of something on expression stack not thread-safely. */
{
fprintf(f, "%s[%d].Obj->_pf_refCount -= 1;\n", stackName, stack);
}


#endif

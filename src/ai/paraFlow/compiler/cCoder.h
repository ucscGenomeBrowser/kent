/* cCoder - C code generator for paraFlow. */

#ifndef CCODER_H
#define CCODER_H

extern char *stackName;
extern char *stackType;

void codeStatement(struct pfCompile *pfc, FILE *f, struct pfParse *pp);
/* Emit C code for one statement. */

int codeExpression(struct pfCompile *pfc, FILE *f,
	struct pfParse *pp, int stack, boolean addRef);
/* Emit code for one expression.  Returns how many items added
 * to stack. */

void codeParamAccess(struct pfCompile *pfc, FILE *f, 
	struct pfBaseType *base, int offset);
/* Print out code to access paramater of given type at offset. */

void codeCleanupVarNamed(struct pfCompile *pfc, FILE *f, 
	struct pfType *type, char *name);
/* Emit cleanup code for variable of given type and name. */

void codeCleanupStackPos(struct pfCompile *pfc, FILE *f,
     struct pfType *type, int stack);
/* Generate cleanup code for stack position. */

void codeBaseType(struct pfCompile *pfc, FILE *f, struct pfBaseType *base);
/* Print out type info for C. */

void codeCase(struct pfCompile *pfc, FILE *f, struct pfParse *pp);
/* Emit C code for case statement. */

void pfCodeC(struct pfCompile *pfc, struct pfParse *program, char *baseDir, char *mainName);
/* Generate C code for program. */

#endif /* CCODER_H */



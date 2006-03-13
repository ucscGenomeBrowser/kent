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

struct dyString *codeVarName(struct pfCompile *pfc, struct pfVar *var);
/* Return  variable name from C point of view.  (Easy unless it's static). */

void codeCleanupVarNamed(struct pfCompile *pfc, FILE *f, 
	struct pfType *type, char *name);
/* Emit cleanup code for variable of given type and name. */

void codeCleanupVar(struct pfCompile *pfc, FILE *f, 
        struct pfVar *var);
/* Emit cleanup code for variable of given type and name. */

void codeScopeVars(struct pfCompile *pfc, FILE *f, struct pfScope *scope,
	boolean zeroUninitialized);
/* Print out variable declarations associated with scope. */

void codeCleanupStackPos(struct pfCompile *pfc, FILE *f,
     struct pfType *type, int stack);
/* Generate cleanup code for stack position. */

void codeBaseType(struct pfCompile *pfc, FILE *f, struct pfBaseType *base);
/* Print out type info for C. */

void codeStartElInCollectionIteration(struct pfCompile *pfc, FILE *f,
	int stack, struct pfScope *scope, struct pfParse *elIxPp, 
	struct pfParse *collectionPp, boolean reverse);
/* This highly technical routine generates some of the code for
 * foreach and para actions.  */

void codeEndElInCollectionIteration(struct pfCompile *pfc, FILE *f,
	struct pfScope *scope, struct pfParse *elIxPp, 
	struct pfParse *collectionPp, boolean reverse);
/* This highly technical routine generates some of the code for
 * foreach and para actions.  */

void codeCase(struct pfCompile *pfc, FILE *f, struct pfParse *pp);
/* Emit C code for case statement. */

void codeParaBlocks(struct pfCompile *pfc, FILE *f, struct pfParse *module);
/* Create functions for code inside of para blocks. */

void codeParaDo(struct pfCompile *pfc, FILE *f,
	struct pfParse *foreach, boolean isPara);
/* Emit C code for para ... do statement. */

void pfCodeC(struct pfCompile *pfc, struct pfParse *program, char *baseDir, char *mainName);
/* Generate C code for program. */

#endif /* CCODER_H */



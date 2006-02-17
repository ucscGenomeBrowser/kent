/* cCoder - C code generator for paraFlow. */

#ifndef CCODER_H
#define CCODER_H

void codeBaseType(struct pfCompile *pfc, FILE *f, struct pfBaseType *base);
/* Print out type info for C. */

void pfCodeC(struct pfCompile *pfc, struct pfParse *program, char *baseDir, char *mainName);
/* Generate C code for program. */

#endif /* CCODER_H */



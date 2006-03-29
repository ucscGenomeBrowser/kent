/* asmCoder - driver for assembly code generation from parse tree. */

#ifndef ASMCODER_H
#define ASMCODER_H

struct dyString *asmCoder(struct pfCompile *pfc, struct pfParse *program, 
	char *baseDir, char *baseName);
/* asmCoder - driver for assembly code generation from parse tree. 
 * Returns list of .s and .o files in string. */

#endif /* ASMCODER_H */

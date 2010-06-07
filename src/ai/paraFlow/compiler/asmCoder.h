/* asmCoder - driver for assembly code generation from parse tree. */

#ifndef ASMCODER_H
#define ASMCODER_H

#ifdef UNUSED
struct asmCoder
/* A polymorphic object that helps organize generating
 * assembly code for different processors */
    {
    void *cpuInfo;	/* Info specific for particular coder. */

    void (*codeList)(struct asmCoder *ac, struct isxList *isxList);
    /* Code a list of instructions. */

    void (*saveCode)(struct asmCoder *ac, FILE *f);
    /* Save code generated so far to file. */

    void (*freeCode)(struct asmCoder *ac, FILE *f);
    /* Free up code generated so far. */

    void (*initCodeSection)(struct asmCoder *ac);
    /* Initialize function or other code section. */

    void (*initFuncVars)(struct asmCoder *ac,
    	struct pfCompile *pfc, struct ctar *ctar, struct hash *varHash);
    /* Set up variables and offsets for parameters and local variables
     * in hash. */

    void (*saveFuncStart)(struct asmCoder *ac,
    	struct pfCompile *pfc, char *cName, boolean isGlobal, FILE *f);
    /* Start coding up a function. */

    void (*saveFuncEnd)(struct asmCoder *ac, struct pfCompile *pfc, FILE *f);
    /* Finish coding up a function in pentium assembly language. */

    void (*moduleStart)(struct asmCoder *ac, FILE *f, char *moduleName);
    /* Code up stuff at start of every module. */

    void (*moduleEnd)(struct asmCoder *ac, FILE *f, char *moduleName);
    /* Code up stuff at end of every module */

    void (*codeInittedModuleVars)(struct asmCoder *ac, 
    	struct isxList *isxList, FILE *f);
    /* Code up module vars that are initialized with constants. */

    void (*codeUninittedModuleVars)(struct asmCoder *ac, 
    	struct isxList *isxList, FILE *f);
    /* Code up module vars that are initialized with zero. */
    };
#endif /* UNUSED */

struct dyString *asmCoder(struct pfCompile *pfc, struct pfParse *program, 
	char *baseDir, char *baseName);
/* asmCoder - driver for assembly code generation from parse tree. 
 * Returns list of .s and .o files in string. */

#endif /* ASMCODER_H */

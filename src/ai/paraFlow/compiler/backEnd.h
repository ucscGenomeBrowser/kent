/* backEnd.h - interface between compiler front and back ends.
 * The front end is responsible for parsing, type checking,
 * and creating intermediate code.  The back end is responsible
 * for creating assembly language. */
#ifndef PFBACKEND_H
#define PFBACKEND_H

#ifndef PFPREAMBLE.H
#include "pfPreamble.h"
#endif

enum isxValType;

enum pfbSegment {pfbNone, pfbData, pfbCode, pfbBss, pfbString};
/* Types of segment */

struct pfBackEnd 
/* Interface with back end. */
    {
    char *name;			/* Back end name */
    enum pfbSegment segment;	/* Current segment type */
    char *cPrefix;		/* Prefix for C symbols. */
    void (*dataSegment)(struct pfBackEnd *backEnd, FILE *f);
    	/* Switch to data segment */
    void (*codeSegment)(struct pfBackEnd *backEnd, FILE *f);
    	/* Switch to code segment */
    void (*bssSegment)(struct pfBackEnd *backEnd, FILE *f);
        /* Switch to bss segment */
    void (*stringSegment)(struct pfBackEnd *backEnd, FILE *f);
        /* Switch to string segment */
    void (*emitLabel)(struct pfBackEnd *backEnd, char *label, int aliSize,
    	boolean isGlobal, FILE *f);	/* Emit label aligned to aliSize. */
    void (*emitAscii)(struct pfBackEnd *backEnd, char *string,int size,FILE *f);
                        /* Emit string of ascii chars of given size. */
    void (*emitByte)(struct pfBackEnd *backEnd, _pf_Byte x, FILE *f);
    			/* Emit Byte */
    void (*emitShort)(struct pfBackEnd *backEnd, _pf_Short x, FILE *f);
    			/* Emit Short */
    void (*emitInt)(struct pfBackEnd *backEnd, _pf_Int x, FILE *f);
    			/* Emit Int */
    void (*emitLong)(struct pfBackEnd *backEnd, _pf_Long x, FILE *f);
    			/* Emit Long */
    void (*emitFloat)(struct pfBackEnd *backEnd, _pf_Float x, FILE *f);
    			/* Emit Float */
    void (*emitDouble)(struct pfBackEnd *backEnd, _pf_Double x, FILE *f);
    			/* Emit Double */
    void (*emitPointer)(struct pfBackEnd *backEnd, char *label, FILE *f);
    			/* Emit Pointer */
    int (*alignData)(struct pfBackEnd *backEnd, int offset, 
    	enum isxValType type);	/* Return offset plus any alignment needed */
    int (*dataSize)(struct pfBackEnd *backEnd, enum isxValType type);	
    	/* Return size of data */
    };

struct pfBackEnd *backEndFind(char *name);
/* Find named back end. */

void backEndLocalPointer(struct pfBackEnd *back, int id, FILE *f);
/* Emit code for pointer labeled LXX where XX is id. */

int backEndTempLabeledString(struct pfCompile *pfc, char *s, FILE *f);
/* Put out string, label it LNN for some number N, and return N. */

struct backEndString
/* A string labeled for assembler. */
    {
    struct backEndString *next;
    int id;			/* String ID.  Label is L before ID */
    char *string;		/* String itself.  Not allocated here. */
    };

int backEndStringAdd(struct pfCompile *pfc,
	struct backEndString **pList, char *string);
/* Add string and label to list.  Return string ID. */

void backEndStringEmitAll(struct pfBackEnd *back,
	struct backEndString *list, FILE *f);
/* Emit all labeled strings to file. */

#endif /* PFBACKEND_H */

#ifndef PENTCODE_H
#define PENTCODE_H

#define pentCodeBufSize 256

struct pentCoder
/* A factory for pentCodes */
    {
    struct hash *storeHash;	/* Save strings here to reuse */
    struct pentCode *list;	/* List of instructions */
    char destBuf[pentCodeBufSize];	/* Space to print dest */
    char sourceBuf[pentCodeBufSize];	/* Space to print source */
    int tempIx;				/* Index (negative) for temps */
    };

struct pentCoder *pentCoderNew();
/* Make new pentCoder. */

void pentCoderFree(struct pentCoder **pCoder);
/* Free up pentCoder. */

void pentCoderAdd(struct pentCoder *coder, char *op, char *source, char *dest);
/* Add new instruction */

struct pentCode
/* Pentium code.  None of the strings are allocated here. */
    {
    struct pentCode *next;	/* Next in list */
    char *op;			/* Opcode. NULL here means just a label */
    char *source;		/* Source operand, may be NULL */
    char *dest;			/* Destination operand, may be NULL */
    };

struct pentCode *pentCodeNew(struct pentCoder *coder,
	char *op, char *source, char *dest);
/* Make a new bit of pentium code. */

#define pentCodeFree(ppt) freez(ppt)
/* Free up a pentium code. */

#define pentCodeFreeList(list) slFreeList(list)
/* Free up a list of pentium code. */

void pentCodeSave(struct pentCode *code, FILE *f);
/* Save single instruction to file */

void pentCodeSaveAll(struct pentCode *code, FILE *f);
/* Save list of instructions. */

#endif /* PENTCODE_H */


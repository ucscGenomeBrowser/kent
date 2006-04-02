/* pentCode - An in-memory structure for pentium assembly
 * code, and some routines to work with it. */

#include "common.h"
#include "hash.h"
#include "isx.h"
#include "pentCode.h"

struct pentCoder *pentCoderNew()
/* Make new pentCoder. */
{
struct pentCoder *coder;
AllocVar(coder);
coder->storeHash = hashNew(16);
return coder;
}

void pentCoderFree(struct pentCoder **pCoder)
/* Free up pentCoder. */
{
struct pentCoder *coder = *pCoder;
if (coder != NULL)
    {
    hashFree(&coder->storeHash);
    pentCodeFreeList(&coder->list);
    freez(pCoder);
    }
}

void pentCoderAdd(struct pentCoder *coder, char *op, char *source, char *dest)
/* Add new instruction */
{
struct pentCode *code = pentCodeNew(coder, op, source, dest);
slAddHead(&coder->list, code);
}

struct pentCode *pentCodeNew(struct pentCoder *coder,
	char *op, char *source, char *dest)
/* Make a new bit of pentium code. */
{
struct pentCode *code;
AllocVar(code);
code->op = hashStoreName(coder->storeHash, op);
code->source = hashStoreName(coder->storeHash, source);
code->dest = hashStoreName(coder->storeHash,dest);
return code;
}

void pentCodeSave(struct pentCode *code, FILE *f)
/* Save single instruction to file */
{
if (code->op)
    {
    if (code->source && code->dest)
	fprintf(f, "\t%s\t%s,%s\n", code->op, code->source, code->dest);
    else if (code->dest)
        fprintf(f, "\t%s\t%s\n", code->op, code->dest);
    else if (code->source)
        fprintf(f, "\t%s\t%s\n", code->op, code->source);
    else
        fprintf(f, "\t%s\n", code->op);
    }
else
    fprintf(f, "%s:\n", code->dest);
}

void pentCodeSaveAll(struct pentCode *code, FILE *f)
/* Save list of instructions. */
{
for (;code != NULL; code = code->next)
    pentCodeSave(code, f);
}



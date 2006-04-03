/* pentConst - stuff to code constants for the pentium. */

#ifndef PENTCONST_H
#define PENTCONST_H

char *pentFloatOrLongLabel(char *buf, int bufSize, enum isxValType valType, 
	struct isxAddress *iad);
/* Return label associated with floating point or long constant. */

void pentCodeLocalConsts(struct isxList *isxList, 
	struct hash *uniqHash, FILE *f);
/* Print code that helps local non-int constant initialization. 
 * for any sources in instruction. */

#endif /* PENTCONST_H */


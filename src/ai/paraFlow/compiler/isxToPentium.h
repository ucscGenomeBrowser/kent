/* isxToPentium - convert isx code to Pentium code.  This basically 
 * follows the register picking algorithm in Aho/Ulman. */

#ifndef ISXTOPENTIUM_H
#define ISXTOPENTIUM_H

void pentFromIsx(struct isxList *isxList, FILE *f);
/* Convert isx code to pentium instructions in file. */

int pentTypeSize(enum isxValType valType);
/* Return size of a val type */

#endif /* ISXTOPENTIUM_H */

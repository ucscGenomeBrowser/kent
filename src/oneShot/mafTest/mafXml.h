/* mafXml - stuff to read/write XML version of MAF */
#ifndef MAFXML_H
#define MAFXML_H

#ifndef MAF_H
#include "maf.h"
#endif

/* AutoXML Generated Routines - typically not called directly. */
void mafMafSave(struct mafFile *obj, int indent, FILE *f);
/* Save mafFile to file. */

struct mafFile *mafMafLoad(char *fileName);
/* Load mafFile from file. */

void mafAliSave(struct mafAli *obj, int indent, FILE *f);
/* Save mafAli to file. */

struct mafAli *mafAliLoad(char *fileName);
/* Load mafAli from file. */

void mafSSave(struct mafComp *obj, int indent, FILE *f);
/* Save mafComp to file. */

struct mafComp *mafSLoad(char *fileName);
/* Load mafComp from file. */


#endif /* MAFXML_H */


/* nonCodingUi.h - enums and char arrays for Non-Coding gene UI features */
#include "common.h"

#ifndef NONCODINGUI_H
#define NONCODINGUI_H

/********* Biotype related controls *********/
/* Types: miRNA, rRNA, snRNA, pseudogene, snoRNA, misc_RNA */

extern char *nonCodingTypeLabels[];
extern char *nonCodingTypeStrings[];
extern char *nonCodingTypeDataName[];
extern char *nonCodingTypeIncludeStrings[];
extern boolean nonCodingTypeIncludeDefault[];
extern boolean nonCodingTypeIncludeCart[];

extern int nonCodingTypeLabelsSize;
extern int nonCodingTypeStringsSize;
extern int nonCodingTypeDataNameSize;
extern int nonCodingTypeIncludeStringsSize;
extern int nonCodingTypeIncludeDefaultSize;
extern int nonCodingTypeIncludeCartSize;

#endif /* NONCODINGUI_H */


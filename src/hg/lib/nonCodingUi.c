/* nonCodingUi.c - char arrays for Non-Coding gene UI features */
#include "nonCodingUi.h"
#include "common.h"

static char const rcsid[] = "$Id: nonCodingUi.c,v 1.2 2008/09/17 18:10:14 kent Exp $";

/************ Biotype related controls *********/
/* Types: miRNA, rRNA, snRNA, pseudogene, snoRNA, misc_RNA */

char *nonCodingTypeLabels[] = {
    "miRNA",
    "rRNA",
    "snRNA",
    "Pseudogene",
    "snoRNA",
    "miscRNA",
};
char *nonCodingTypeStrings[] = {
    "nonCodingTypemiRNA",
    "nonCodingTyperRNA",
    "nonCodingTypesnRNA",
    "nonCodingTypePseudogene",
    "nonCodingTypesnoRNA",
    "nonCodingTypeMiscRNA",
};
char *nonCodingTypeDataName[] = {
    "miRNA",
    "rRNA",
    "snRNA",
    "pseudogene",
    "snoRNA",
    "misc_RNA",
};
char *nonCodingTypeIncludeStrings[] = {
    "nonCodingTypemiRNAInclude",
    "nonCodingTyperRNAInclude",
    "nonCodingTypesnRNAInclude",
    "nonCodingTypePseudogeneInclude",
    "nonCodingTypesnoRNAInclude",
    "nonCodingTypeMiscRNAInclude",
};
boolean nonCodingTypeIncludeDefault[] = {
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
};
boolean nonCodingTypeIncludeCart[] = {
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
};

/* Array sizes - all the same */
int nonCodingTypeLabelsSize = ArraySize(nonCodingTypeLabels);
int nonCodingTypeStringsSize = ArraySize(nonCodingTypeStrings);
int nonCodingTypeDataNameSize = ArraySize(nonCodingTypeDataName);
int nonCodingTypeIncludeStringsSize = ArraySize(nonCodingTypeIncludeStrings);
int nonCodingTypeIncludeDefaultSize = ArraySize(nonCodingTypeIncludeDefault);
int nonCodingTypeIncludeCartSize = ArraySize(nonCodingTypeIncludeCart);

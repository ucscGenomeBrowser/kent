/* humanPhenotypeUi.c - char arrays for humanPhenotype UI features */
#include "humanPhenotypeUi.h"
#include "common.h"

/* some stuff for mutation type choices */
/* labels for checkboxes */
char *variantTypeLabel[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
};

/* names for checkboxes */
char *variantTypeString[] = {
    "humPhen.filter.sub",
    "humPhen.filter.ins",
    "humPhen.filter.dup",
    "humPhen.filter.del",
    "humPhen.filter.complex",
};

/* values in the db */
char *variantTypeDbValue[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
};

int variantTypeSize = ArraySize(variantTypeString);

/* some stuff for the mutation location choices */
/* labels for checkboxes */
char *variantLocationLabel[] = {
    "exon",
    "intron",
    "5' utr",
    "3' utr",
    "not within known transcription unit",
};

/* names for checkboxes */
char *variantLocationString[] = {
    "humPhen.filter.xon",
    "humPhen.filter.intron",
    "humPhen.filter.utr5",
    "humPhen.filter.utr3",
    "humPhen.filter.intergenic",
};

char *variantLocationDbValue[] = {
    "exon",
    "intron",
    "5'utr",
    "3'utr",
    "not within known transcription unit",
};

int variantLocationSize = ArraySize(variantLocationLabel);

/* hgMutUi.c - char arrays for humanPhenotype UI features */
#include "hgMutUi.h"
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
    "hgMut.filter.sub",
    "hgMut.filter.ins",
    "hgMut.filter.dup",
    "hgMut.filter.del",
    "hgMut.filter.complex",
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
    "5' UTR",
    "3' UTR",
    "not within known transcription unit",
};

/* names for checkboxes */
char *variantLocationString[] = {
    "hgMut.filter.xon",
    "hgMut.filter.intron",
    "hgMut.filter.utr5",
    "hgMut.filter.utr3",
    "hgMut.filter.intergenic",
};

char *variantLocationDbValue[] = {
    "exon",
    "intron",
    "5' UTR",
    "3' UTR",
    "not within known transcription unit",
};

int variantLocationSize = ArraySize(variantLocationLabel);


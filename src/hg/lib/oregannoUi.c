/* oregannoUi.c - char arrays for oreganno UI features */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "oregannoUi.h"
#include "common.h"


/* some stuff for mutation type choices */
/* labels for checkboxes */
char *oregannoTypeLabel[] = {
    "regulatory polymorphism",
    "transcription factor binding site",
    "regulatory region",
    "miRNA binding site",
    "regulatory haplotype",
};

/* names for checkboxes */
char *oregannoTypeString[] = {
    "oreganno.filter.poly",
    "oreganno.filter.tfbs",
    "oreganno.filter.reg",
    "oreganno.filter.mirna",
    "oreganno.filter.haplo",
};

/* values in the db */
char *oregannoTypeDbValue[] = {
    "Regulatory Polymorphism",
    "Transcription Factor Binding Site",
    "Regulatory Region",
    "miRNA Binding Site",
    "Regulatory Haplotype",
};

int oregannoTypeSize = ArraySize(oregannoTypeString);

/* list of attribute fields for oreganno, in order to be displayed */
char *oregannoAttributes[] = {
    "SrcLink",
    "Extlink",
    "type",
    "Gene",
    "TFbs",
    "Dataset",
    "Evidence",
    "EvidenceSubtype",
};

char *oregannoAttrLabel[] = {
    "Links",
    "Links",
    "Region type",
    "Gene",
    "Transcription factor",
    "Dataset",
    "Evidence type",
    "Evidence subtype(s)",
};

int oregannoAttrSize = ArraySize(oregannoAttributes);

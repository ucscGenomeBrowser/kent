/* genomeVarUi.c - char arrays for Genome Variation UI features */
#include "genomeVarUi.h"
#include "common.h"

/* some stuff for mutation type choices */
/* labels for checkboxes */
char *mutationTypeLabel[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
};

/* names for checkboxes */
char *mutationTypeString[] = {
    "genomeVar.filter.sub",
    "genomeVar.filter.ins",
    "genomeVar.filter.dup",
    "genomeVar.filter.del",
    "genomeVar.filter.complex",
};

/* values in the db */
char *mutationTypeDbValue[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
};

int mutationTypeSize = ArraySize(mutationTypeString);

/* some stuff for the mutation location choices */
/* labels for checkboxes */
char *mutationLocationLabel[] = {
    "exon",
    "intron",
    "5' UTR",
    "3' UTR",
    "not within known transcription unit",
};

/* names for checkboxes */
char *mutationLocationString[] = {
    "genomeVar.filter.xon",
    "genomeVar.filter.intron",
    "genomeVar.filter.utr5",
    "genomeVar.filter.utr3",
    "genomeVar.filter.intergenic",
};

char *mutationLocationDbValue[] = {
    "exon",
    "intron",
    "5' UTR",
    "3' UTR",
    "not within known transcription unit",
};

int mutationLocationSize = ArraySize(mutationLocationLabel);

/* list in display order of attribute type, key that is used in table */
char *mutationAttrTypeKey[] = {
    "rnaNtchange",
    "protEffect",
    "phenoCommon",
    "phenoOfficial",
    "geneVarsDis",
    "hap",
    "ethnic",
    "links",
};

/* list in display order of attribute type display names */
char *mutationAttrTypeDisplay[] = {
    "RNA nucleotide change",
    "Effect on Protein",
    "Common name",
    "Official nomenclature",
    "Variation and Disease Info",
    "Haplotype",
    "Ethnicity/Nationality",
    "External links",
};

/* category for each type above, match up by array index */
char *mutationAttrCategory[] = {
    "RNA effect",
    "Protein effect",
    "Patient/Subject phenotype",
    "Patient/Subject phenotype",
    "Data related to gene locus",
    "Other Ancillary data",
    "Other Ancillary data",
    "Other Ancillary data",
};

int mutationAttrSize = ArraySize(mutationAttrTypeKey);

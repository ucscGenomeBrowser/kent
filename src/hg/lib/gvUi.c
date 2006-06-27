/* gvUi.c - char arrays for Genome Variation UI features */
#include "gvUi.h"
#include "common.h"

/***************Filters**************/

/* some stuff for mutation type choices */
/* labels for checkboxes */
char *gvTypeLabel[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
};

/* names for checkboxes */
char *gvTypeString[] = {
    "gvPos.filter.sub",
    "gvPos.filter.ins",
    "gvPos.filter.dup",
    "gvPos.filter.del",
    "gvPos.filter.complex",
};

/* values in the db */
char *gvTypeDbValue[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
};

int gvTypeSize = ArraySize(gvTypeString);

/* some stuff for the mutation location choices */
/* labels for checkboxes */
char *gvLocationLabel[] = {
    "exon",
    "intron",
    "5' UTR",
    "3' UTR",
    "not within known transcription unit",
};

/* names for checkboxes */
char *gvLocationString[] = {
    "gvPos.filter.xon",
    "gvPos.filter.intron",
    "gvPos.filter.utr5",
    "gvPos.filter.utr3",
    "gvPos.filter.intergenic",
};

char *gvLocationDbValue[] = {
    "exon",
    "intron",
    "5' UTR",
    "3' UTR",
    "not within known transcription unit",
};

int gvLocationSize = ArraySize(gvLocationLabel);

char *gvSrcString[] = {
    "gvPos.filter.src.SP",
    "gvPos.filter.src.LSDB",
};

char *gvSrcDbValue[] = {
    "UniProt (Swiss-Prot/TrEMBL)",
    "LSDB",
};

int gvSrcSize = ArraySize(gvSrcString);

char *gvAccuracyLabel[] = {
    "estimated coordinates",
};

char *gvAccuracyString[] = {
    "gvPos.filter.estimate",
};

unsigned char *gvAccuracyDbValue[] = {
    "0",
};

int gvAccuracySize = ArraySize(gvAccuracyLabel);

/***************Attribute display**************/

/* list in display order of attribute type, key that is used in table */
char *gvAttrTypeKey[] = {
    "commonName",
    "alias",
    "links",
    "rnaNtChange",
    "protEffect",
    "phenoCommon",
    "phenoOfficial",
    "geneVarsDis",
    "hap",
    "ethnic",
};

/* list in display order of attribute type display names */
char *gvAttrTypeDisplay[] = {
    "Common name",
    "Alias",
    "External links",
    "RNA nucleotide change",
    "Effect on Protein",
    "Phenotype common name",
    "Phenotype official nomenclature",
    "Variation and Disease information related to gene locus",
    "Haplotype",
    "Ethnicity/Nationality",
};

/* category for each type above, match up by array index */
char *gvAttrCategory[] = {
    "Alias",
    "Alias",
    "Links",
    "RNA effect",
    "Protein effect",
    "Patient/Subject phenotype",
    "Patient/Subject phenotype",
    "Data related to gene locus",
    "Other Ancillary data",
    "Other Ancillary data",
};

int gvAttrSize = ArraySize(gvAttrTypeKey);

/***************Color options**************/

char *gvColorLabels[] = {
    "purple",
    "green",
    "orange",
    "blue",
    "brown",
    "black",
};

int gvColorLabelSize = ArraySize(gvColorLabels);

char *gvColorTypeLabels[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
    "unknown",
};

char *gvColorTypeStrings[] = {
    "gvColorTypeSub",
    "gvColorTypeIns",
    "gvColorTypeDup",
    "gvColorTypeDel",
    "gvColorTypeComplex",
    "gvColorTypeUnk",
};

char *gvColorTypeDefault[] = {
    "purple",
    "green",
    "orange",
    "blue",
    "brown",
    "black",
};

char *gvColorTypeBaseChangeType[] = {
    "substitution",
    "insertion",
    "duplication",
    "deletion",
    "complex",
    "unknown",
};

/* all type arrays are same size */
int gvColorTypeSize = ArraySize(gvColorTypeStrings);

/* snp125Ui.c - char arrays for snp UI features */
#include "snp125Ui.h"
#include "common.h"

/****** Some stuff for snp colors *******/

char *snp125ColorLabel[] = {
    "red",
    "green",
    "blue",
    "black",
};

int snp125ColorLabelSize = ArraySize(snp125ColorLabel);


/****** color source controls *******/
/* This keeps track of which SNP feature is used for color definition. */
/* Available SNP features: Molecule Type, Class, Validation, Function */

char *snp125ColorSourceLabels[] = {
    "Molecule Type",
    "Class",
    "Validation",
    "Function",
};

char *snp125ColorSourceStrings[] = {
    "snp125ColorSourceMolType",
    "snp125ColorSourceClass",
    "snp125ColorSourceValid",
    "snp125ColorSourceFunc",
};

// why are these arrays?
char *snp125ColorSourceDataName[] = {
    "snp125ColorSource",
};
// could also make Class the default
char *snp125ColorSourceDefault[] = {
    "Function",
};
char *snp125ColorSourceCart[] = {
    "Function",
};

int snp125ColorSourceLabelsSize   = ArraySize(snp125ColorSourceLabels);
int snp125ColorSourceStringsSize  = ArraySize(snp125ColorSourceStrings);
int snp125ColorSourceDataNameSize = ArraySize(snp125ColorSourceDataName);
int snp125ColorSourceDefaultSize  = ArraySize(snp125ColorSourceDefault);
int snp125ColorSourceCartSize     = ArraySize(snp125ColorSourceCart);


/****** MolType related controls *******/
/* Types: unknown, genomic, cDNA */

char *snp125MolTypeLabels[] = {
    "Unknown",
    "Genomic",
    "cDNA",
};
char *snp125MolTypeStrings[] = {
    "snp125MolTypeUnknown",
    "snp125MolTypeGenomic",
    "snp125MolTypecDNA",
};
char *snp125MolTypeDataName[] = {
    "unknown",
    "genomic",
    "cDNA",
};
char *snp125MolTypeDefault[] = {
    "red",
    "black",
    "blue",
};
char *snp125MolTypeCart[] = {
    "red",
    "black",
    "blue",
};
char *snp125MolTypeIncludeStrings[] = {
    "snp125MolTypeUnknownInclude",
    "snp125MolTypeGenomicInclude",
    "snp125MolTypecDNAInclude",
};
boolean snp125MolTypeIncludeDefault[] = {
    TRUE,
    TRUE,
    TRUE,
};
boolean snp125MolTypeIncludeCart[] = {
    TRUE,
    TRUE,
    TRUE,
};

// all of these sizes are the same
int snp125MolTypeLabelsSize   = ArraySize(snp125MolTypeLabels);
int snp125MolTypeStringsSize  = ArraySize(snp125MolTypeStrings);
int snp125MolTypeDataNameSize = ArraySize(snp125MolTypeDataName);
int snp125MolTypeDefaultSize  = ArraySize(snp125MolTypeDefault);
int snp125MolTypeCartSize     = ArraySize(snp125MolTypeCart);
int snp125MolTypeIncludeStringsSize  = ArraySize(snp125MolTypeIncludeStrings);

/****** Class related controls *******/
/* Types: unknown, snp, insertion, deletion, range */

char *snp125ClassLabels[] = {
    "Unknown",
    "Single Nucleotide Polymorphism",
    "Insertion",
    "Deletion",
    "Range",
};
char *snp125ClassStrings[] = {
    "snp125ClassUnknown",
    "snp125ClassSnp",
    "snp125ClassIn",
    "snp125ClassDel",
    "snp125ClassRange",
};
char *snp125ClassDataName[] = {
    "unknown",
    "simple",
    "insertion",
    "deletion",
    "range",
};
char *snp125ClassDefault[] = {
    "red",
    "black",
    "blue",
    "red",
    "green",
};
char *snp125ClassCart[] = {
    "red",
    "black",
    "blue",
    "red",
    "green",
};
char *snp125ClassIncludeStrings[] = {
    "snp125ClassUnknownInclude",
    "snp125ClassSimpleInclude",
    "snp125ClassInsertionInclude",
    "snp125ClassDeletionInclude",
    "snp125ClassRangeInclude"
};
boolean snp125ClassIncludeDefault[] = {
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
};
boolean snp125ClassIncludeCart[] = {
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
};

int snp125ClassLabelsSize   = ArraySize(snp125ClassLabels);
int snp125ClassStringsSize  = ArraySize(snp125ClassStrings);
int snp125ClassDataNameSize = ArraySize(snp125ClassDataName);
int snp125ClassDefaultSize  = ArraySize(snp125ClassDefault);
int snp125ClassCartSize     = ArraySize(snp125ClassCart);
int snp125ClassIncludeStringsSize = ArraySize(snp125ClassIncludeStrings);

/****** Validation related controls *******/
/* Types: unknown, by-cluster, by-frequency, by-submitter, by-2hit-2allele, by-hapmap */

char *snp125ValidLabels[] = {
    "Unknown",
    "By Cluster",
    "By Frequency",
    "By Submitter",
    "By 2 Hit / 2 Allele",
    "By HapMap",
};
char *snp125ValidStrings[] = {
    "snp125ValidUnknown",
    "snp125ValidCluster",
    "snp125ValidFrequency",
    "snp125ValidSubmitter",
    "snp125Valid2H2A",
    "snp125ValidHapMap",
};
char *snp125ValidDataName[] = {
    "unknown",
    "by-cluster",
    "by-frequency",
    "by-submitter",
    "by-2hit-2allele",
    "by-hapmap",
};
char *snp125ValidDefault[] = {
    "red",
    "black",
    "black",
    "black",
    "black",
    "green",
};
char *snp125ValidCart[] = {
    "red",
    "black",
    "black",
    "black",
    "black",
    "green",
};
char *snp125ValidIncludeStrings[] = {
    "snp125ValidUnknownInclude",
    "snp125ValidClusterInclude",
    "snp125ValidFrequencyInclude",
    "snp125ValidSubmitterInclude",
    "snp125Valid2H2AInclude",
    "snp125ValidHapMapInclude",
};
boolean snp125ValidIncludeDefault[] = {
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
};
boolean snp125ValidIncludeCart[] = {
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
};

int snp125ValidLabelsSize   = ArraySize(snp125ValidLabels);
int snp125ValidStringsSize  = ArraySize(snp125ValidStrings);
int snp125ValidDataNameSize = ArraySize(snp125ValidDataName);
int snp125ValidDefaultSize  = ArraySize(snp125ValidDefault);
int snp125ValidCartSize     = ArraySize(snp125ValidCart);
int snp125ValidIncludeStringsSize     = ArraySize(snp125ValidIncludeStrings);

/****** function related controls *******/
/* unknown, coding, coding-synon, coding-nonsynon,
 * intron, splice-site, exception */

char *snp125FuncLabels[] = {
    "Unknown",
    "Coding",
    "Coding - Synonymous",
    "Coding - Non-Synonymous",
    "Untranslated",
    "Intron",
    "Splice site",
    "Exception",
};
char *snp125FuncStrings[] = {
    "snp125FuncUnknown",
    "snp125FuncCoding",
    "snp125FuncSynon",
    "snp125FuncNonSynon",
    "snp125FuncUntranslated",
    "snp125FuncIntron",
    "snp125FuncSplice",
    "snp125FuncException",
};
char *snp125FuncDataName[] = {
    "unknown",
    "coding",
    "coding-synon",
    "coding-nonsynon",
    "untranslated",
    "intron",
    "splice-site",
    "exception",
};
char *snp125FuncDefault[] = {
    // "black",
    "red",
    "blue",
    "green",
    "red",
    "blue",
    "blue",
    "red",
    "black",
};
char *snp125FuncCart[] = {
    // "black",
    "red",
    "blue",
    "green",
    "red",
    "blue",
    "blue",
    "red",
    "black",
};
char *snp125FuncIncludeStrings[] = {
    "snp125FuncUnknownInclude",
    "snp125FuncCodingInclude",
    "snp125FuncSynonInclude",
    "snp125FuncNonSynonInclude",
    "snp125FuncUntranslatedInclude",
    "snp125FuncIntronInclude",
    "snp125FuncSpliceInclude",
    "snp125FuncExceptionInclude",
};
boolean snp125FuncIncludeDefault[] = {
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
};
boolean snp125FuncIncludeCart[] = {
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
};

int snp125FuncLabelsSize   = ArraySize(snp125FuncLabels);
int snp125FuncStringsSize  = ArraySize(snp125FuncStrings);
int snp125FuncDataNameSize = ArraySize(snp125FuncDataName);
int snp125FuncDefaultSize  = ArraySize(snp125FuncDefault);
int snp125FuncCartSize     = ArraySize(snp125FuncCart);
int snp125FuncIncludeStringsSize     = ArraySize(snp125FuncIncludeStrings);

/* minimum Average Heterozygosity cutoff  */

float snp125AvHetCutoff = 0.0;


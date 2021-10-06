/* snpUi.c - char arrays for snp UI features */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "snpUi.h"
#include "common.h"

/****** Some stuff for snp colors *******/

char *snpColorLabel[] = {
    "red",
    "green",
    "blue",
    "black",
    "exclude",
};

int snpColorLabelSize = ArraySize(snpColorLabel);

/****** Some stuff for snpMapSource related controls *******/
/* dbSnp, Affy10K, Affy10Kv2, Affy50K_HindIII, Affy50K_XbaI */

char *snpMapSourceLabels[] = {
    "Bac Overlaps",
    "Random",
    "Mixed",
    "Other",
    "Affymetrix Genotyping Array 10K",
    "Affymetrix Genotyping Array 120K",
};
char *snpMapSourceStrings[] = {
    "snpMapSourceBacOverlap",
    "snpMapSourceRandom",
    "snpMapSourceMixed",
    "snpMapSourceOther",
    "snpMapSourceAffy10K",
    "snpMapSourceAffy120K",
};
char *snpMapSourceDataName[] = {
    "BAC_OVERLAP",
    "RANDOM",
    "MIXED",
    "OTHER",
    "Affy10K",
    "Affy120K",
};
char *snpMapSourceDefault[] = {
    "black",
    "blue",
    "red",
    "red",
    "green",
    "green",
};
char *snpMapSourceCart[] = {
    "black",
    "blue",
    "red",
    "red",
    "green",
    "green",
};

int snpMapSourceLabelsSize   = ArraySize(snpMapSourceLabels);
int snpMapSourceStringsSize  = ArraySize(snpMapSourceStrings);
int snpMapSourceDataNameSize = ArraySize(snpMapSourceDataName);
int snpMapSourceDefaultSize  = ArraySize(snpMapSourceDefault);
int snpMapSourceCartSize     = ArraySize(snpMapSourceCart);

/****** Some stuff for snpMapType related controls *******/
/* SingleNP, indel, segnemtal */

char *snpMapTypeLabel[] = {
    "include",
    "exclude",
};
char *snpMapTypeLabels[] = {
    "Single Nucleotide Polymorphisms",
    "Insertions and Deletions",
    "Segmental Duplications",
};
char *snpMapTypeStrings[] = {
    "snpMapTypeSingle",
    "snpMapTypeIndels",
    "snpMapTypeSegmental",
};
char *snpMapTypeDataName[] = {
    "SNP",
    "INDEL",
    "SEGMENTAL",
};
char *snpMapTypeDefault[] = {
    "include",
    "include",
    "include",
};
char *snpMapTypeCart[] = {
    "include",
    "include",
    "include",
};

int snpMapTypeLabelSize    = ArraySize(snpMapTypeLabel);
int snpMapTypeLabelsSize   = ArraySize(snpMapTypeLabels);
int snpMapTypeStringsSize  = ArraySize(snpMapTypeStrings);
int snpMapTypeDataNameSize = ArraySize(snpMapTypeDataName);
int snpMapTypeDefaultSize  = ArraySize(snpMapTypeDefault);
int snpMapTypeCartSize     = ArraySize(snpMapTypeCart);

/****** Some stuff for snpColorSource related controls *******/
/* Source, Molecule Type, Class, Validation, Function */

char *snpColorSourceLabels[] = {
    "Source",
    "Molecule Type",
    "Variant Class",
    "Validation Status",
    "Functional Class",
    "Location Type",
    "Black",
};

char *snpColorSourceStrings[] = {
    "snpColorSourceSource",
    "snpColorSourceMolType",
    "snpColorSourceClass",
    "snpColorSourceValid",
    "snpColorSourceFunc",
    "snpColorSourceLocType",
    "snpColorSourceBlack",
};
char *snpColorSourceDataName[] = {
    "snpColorSource",
};
char *snpColorSourceDefault[] = {
    "snpColorSourceFunc",
};
char *snpColorSourceCart[] = {
    "snpColorSourceFunc",
};

int snpColorSourceLabelsSize   = ArraySize(snpColorSourceLabels);
int snpColorSourceStringsSize  = ArraySize(snpColorSourceStrings);
int snpColorSourceDataNameSize = ArraySize(snpColorSourceDataName);
int snpColorSourceDefaultSize  = ArraySize(snpColorSourceDefault);
int snpColorSourceCartSize     = ArraySize(snpColorSourceCart);

/****** Some stuff for snpSource related controls *******/
/* dbSnp, Affy10K, Affy10Kv2, Affy50K_HindIII, Affy50K_XbaI */

char *snpSourceLabels[] = {
    "dbSnp",
    "Affymetrix Genotyping Array 10K",
    "Affymetrix Genotyping Array 10K v2",
    "Affymetrix Genotyping Array 50K HindIII",
    "Affymetrix Genotyping Array 50K XbaI",
};
char *snpSourceStrings[] = {
    "snpSourceDbSnp",
    "snpSourceAffy10K",
    "snpSourceAffy10Kv2",
    "snpSourceAffy50KHindIII",
    "snpSourceAffy50KXbaI",
};
char *snpSourceDataName[] = {
    "dbSnp",
    "Affy10K",
    "Affy10Kv2",
    "Affy50K_HindIII",
    "Affy50K_XbaI",
};
char *snpSourceDefault[] = {
    "black",
    "blue",
    "red",
    "green",
    "green",
};
char *snpSourceCart[] = {
    "black",
    "blue",
    "red",
    "green",
    "green",
};

int snpSourceLabelsSize   = ArraySize(snpSourceLabels);
int snpSourceStringsSize  = ArraySize(snpSourceStrings);
int snpSourceDataNameSize = ArraySize(snpSourceDataName);
int snpSourceDefaultSize  = ArraySize(snpSourceDefault);
int snpSourceCartSize     = ArraySize(snpSourceCart);

/****** Some stuff for snpMolType related controls *******/
/* unknown, genomic, cDNA, mito, chloro */

char *snpMolTypeLabels[] = {
    "Unknown",
    "Genomic",
    "cDNA",
    "Mitochondrial",
    "Chloroplast",
};
char *snpMolTypeStrings[] = {
    "snpMolTypeUnknown",
    "snpMolTypeGenomic",
    "snpMolTypecDNA",
    "snpMolTypeMito",
    "snpMolTypeChloro",
};
char *snpMolTypeDataName[] = {
    "unknown",
    "genomic",
    "cDNA",
    "mito",
    "chloro",
};
char *snpMolTypeDefault[] = {
    "red",
    "black",
    "blue",
    "green",
    "exclude",
};
char *snpMolTypeCart[] = {
    "red",
    "black",
    "blue",
    "green",
    "exclude",
};

int snpMolTypeLabelsSize   = ArraySize(snpMolTypeLabels);
int snpMolTypeStringsSize  = ArraySize(snpMolTypeStrings);
int snpMolTypeDataNameSize = ArraySize(snpMolTypeDataName);
int snpMolTypeDefaultSize  = ArraySize(snpMolTypeDefault);
int snpMolTypeCartSize     = ArraySize(snpMolTypeCart);

/****** Some stuff for snpClass related controls *******/
/* unknown, snp, in-del, het, microsat, named, no-variation, mixed,
 * mnp */

char *snpClassLabels[] = {
    "Unknown",
    "Single Nucleotide Polymorphism",
    "Insertion / Deletion",
    "Heterozygous",
    "Microsatellite",
    "Named",
    "No Variation",
    "Mixed",
    "Multiple Nucleotide Polymorphism",
};
char *snpClassStrings[] = {
    "snpClassUnknown",
    "snpClassSnp",
    "snpClassInDel",
    "snpClassHet",
    "snpClassMicrosat",
    "snpClassNamed",
    "snpClassNoVariation",
    "snpClassMixed",
    "snpClassMnp",
};
char *snpClassDataName[] = {
    "unknown",
    "snp",
    "in-del",
    "het",
    "microsat",
    "named",
    "no-variation",
    "mixed",
    "mnp",
};
char *snpClassDefault[] = {
    "red",
    "black",
    "blue",
    "red",
    "green",
    "green",
    "red",
    "black",
    "blue",
};
char *snpClassCart[] = {
    "red",
    "black",
    "blue",
    "red",
    "green",
    "green",
    "red",
    "black",
    "blue",
};

int snpClassLabelsSize   = ArraySize(snpClassLabels);
int snpClassStringsSize  = ArraySize(snpClassStrings);
int snpClassDataNameSize = ArraySize(snpClassDataName);
int snpClassDefaultSize  = ArraySize(snpClassDefault);
int snpClassCartSize     = ArraySize(snpClassCart);

/****** Some stuff for snpValid related controls *******/
/* unknown, other-pop, by-frequency, by-cluster, by-2hit-2allele,
 * by-hapmap, genotype */

char *snpValidLabels[] = {
    "Unknown",
    "Other Population",
    "By Frequency",
    "By Cluster",
    "By 2 Hit / 2 Allele",
    "By HapMap",
    "By Genotype",
};
char *snpValidStrings[] = {
    "snpValidUnknown",
    "snpValidOtherPop",
    "snpValidFrequency",
    "snpValidCluster",
    "snpValid2H2A",
    "snpValidHapMap",
    "snpValidGenotype",
};
char *snpValidDataName[] = {
    "unknown",
    "other-pop",
    "by-frequency",
    "by-cluster",
    "by-2hit-2allele",
    "by-hapmap",
    "genotype",
};
char *snpValidDefault[] = {
    "red",
    "black",
    "black",
    "black",
    "black",
    "blue",
    "green",
};
char *snpValidCart[] = {
    "red",
    "black",
    "black",
    "black",
    "black",
    "blue",
    "green",
};

int snpValidLabelsSize   = ArraySize(snpValidLabels);
int snpValidStringsSize  = ArraySize(snpValidStrings);
int snpValidDataNameSize = ArraySize(snpValidDataName);
int snpValidDefaultSize  = ArraySize(snpValidDefault);
int snpValidCartSize     = ArraySize(snpValidCart);

/****** Some stuff for snpFunc related controls *******/
/* unknown, locus-region, coding, coding-synon, coding-nonsynon,
 * mrna-utr, intron, splice-site, reference, exception */

char *snpFuncLabels[] = {
    "Unknown",
    "Locus Region",
    "Coding",
    "Coding - Synonymous",
    "Coding - Non-Synonymous",
    "mRNA/UTR",
    "Intron",
    "Splice site",
    "Reference",
    "Exception",
};
char *snpFuncStrings[] = {
    "snpFuncUnknown",
    "snpFuncLocus",
    "snpFuncCoding",
    "snpFuncSynon",
    "snpFuncNonSynon",
    "snpFuncmRnaUtr",
    "snpFuncIntron",
    "snpFuncSplice",
    "snpFuncReference",
    "snpFuncException",
};
char *snpFuncDataName[] = {
    "unknown",
    "locus-region",
    "coding",
    "coding-synon",
    "coding-nonsynon",
    "mrna-utr",
    "intron",
    "splice-site",
    "reference",
    "exception",
};
char *snpFuncDefault[] = {
    "black",
    "black",
    "blue",
    "green",
    "red",
    "blue",
    "blue",
    "red",
    "black",
    "black",
};
char *snpFuncCart[] = {
    "black",
    "black",
    "blue",
    "green",
    "red",
    "blue",
    "blue",
    "red",
    "black",
    "black",
};

int snpFuncLabelsSize   = ArraySize(snpFuncLabels);
int snpFuncStringsSize  = ArraySize(snpFuncStrings);
int snpFuncDataNameSize = ArraySize(snpFuncDataName);
int snpFuncDefaultSize  = ArraySize(snpFuncDefault);
int snpFuncCartSize     = ArraySize(snpFuncCart);

/****** Some stuff for snpLocType related controls *******/
/* unknown, range, exact, between */

char *snpLocTypeLabels[] = {
    "Unknown",
    "Range",
    "Exact",
    "Between",
};
char *snpLocTypeStrings[] = {
    "snpLocTypeUnknown",
    "snpLocTypeRange",
    "snpLocTypeExact",
    "snpLocTypeBetween",
};
char *snpLocTypeDataName[] = {
    "unknown",
    "range",
    "exact",
    "between",
};
char *snpLocTypeDefault[] = {
    "red",
    "green",
    "blue",
    "black",
};
char *snpLocTypeCart[] = {
    "red",
    "green",
    "blue",
    "black",
};

int snpLocTypeLabelsSize   = ArraySize(snpLocTypeLabels);
int snpLocTypeStringsSize  = ArraySize(snpLocTypeStrings);
int snpLocTypeDataNameSize = ArraySize(snpLocTypeDataName);
int snpLocTypeDefaultSize  = ArraySize(snpLocTypeDefault);
int snpLocTypeCartSize     = ArraySize(snpLocTypeCart);

/****** Some stuff for snpLocType related controls *******/
/* minimum Average Heterozygosity cutoff  */

float snpAvHetCutoff = 0.0;


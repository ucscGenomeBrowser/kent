/* hui - human genome browser user interface controls that are shared
 * between more than one CGI. */
#ifndef HUI_H
#define HUI_H

#include "cart.h"

char *hUserCookie();
/* Return our cookie name. */

char *wrapWhiteFont(char *s);
/* Write white font around s */

#define HELP_DIR "/usr/local/apache/htdocs/goldenPath/help" 

/* Definitions for ruler pseudo-track.  It's not yet a full-fledged
 * track, so it can't appear in trackDb. */
#define RULER_TRACK_NAME        "ruler"
#define RULER_TRACK_LABEL       "Base Position"
#define RULER_TRACK_LONGLABEL   "Genome Base Position"

/* Definition for oligo match track. */
#define oligoMatchVar "hgt.oligoMatch" 
#define oligoMatchDefault "aaaaa"
#define OLIGO_MATCH_TRACK_NAME "oligoMatch"
#define OLIGO_MATCH_TRACK_LABEL "Short Match"
#define OLIGO_MATCH_TRACK_LONGLABEL "Perfect Match to Short Sequence"

/* Display of bases on the ruler, and multiple alignments.
 * If present, indicates reverse strand */
#define COMPLEMENT_BASES_VAR    "complement"

/* Configuration variable to cause ruler zoom to zoom to base level */
#define RULER_BASE_ZOOM_VAR      "rulerBaseZoom"

/* Maf track display variables and their values */
#define MAF_GENEPRED_VAR  "mafGenePred"
#define MAF_FRAMING_VAR   "mafFrame"
#define MAF_DOT_VAR       "mafDot"
#define MAF_CHAIN_VAR     "mafChain"

/******  Some stuff for tables of controls ******/
#define CONTROL_TABLE_WIDTH 610

struct controlGrid
/* Keep track of a control grid (table) */
    {
    int columns;	/* How many columns in grid. */
    int columnIx;	/* Index (0 based) of current column. */
    char *align;	/* Which way to align. */
    boolean rowOpen;	/* True if have opened a row. */
    };

struct controlGrid *startControlGrid(int columns, char *align);
/* Start up a control grid. */

void controlGridStartCell(struct controlGrid *cg);
/* Start a new cell in control grid. */

void controlGridEndCell(struct controlGrid *cg);
/* End cell in control grid. */

void endControlGrid(struct controlGrid **pCg);
/* Finish up a control grid. */

void controlGridEndRow(struct controlGrid *cg);
/* Force end of row. */

/******  Some stuff for hide/dense/full controls ******/
enum trackVisibility 
/* How to look at a track. */
    {
    tvHide=0, 		/* Hide it. */
    tvDense=1,          /* Squish it together. */
    tvFull=2,           /* Expand it out. */
    tvPack=3,           /* Zig zag it up and down. */
    tvSquish=4,         /* Pack with thin boxes and no labels. */
    };  

enum trackVisibility hTvFromString(char *s);
/* Given a string representation of track visibility, return as
 * equivalent enum. */

enum trackVisibility hTvFromStringNoAbort(char *s);
/* Given a string representation of track visibility, return as
 * equivalent enum. */

char *hStringFromTv(enum trackVisibility vis);
/* Given enum representation convert to string. */

void hTvDropDownClass(char *varName, enum trackVisibility vis, boolean canPack, char *class);
/* Make track visibility drop down for varName with style class */

void hTvDropDown(char *varName, enum trackVisibility vis, boolean canPack);
/* Make track visibility drop down for varName 
 * uses style "normalText" */

/****** Some stuff for stsMap related controls *******/
enum stsMapOptEnum {
   smoeGenetic = 0,
   smoeGenethon = 1,
   smoeMarshfield = 2,
   smoeDecode = 3,
   smoeGm99 = 4,
   smoeWiYac = 5,
   smoeWiRh = 6,
   smoeTng = 7,
};

enum stsMapOptEnum smoeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *smoeEnumToString(enum stsMapOptEnum x);
/* Convert from enum to string representation. */

void smoeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for stsMapMouseNew related controls *******/
enum stsMapMouseOptEnum {
   smmoeGenetic = 0,
   smmoeWig = 1,
   smmoeMgi = 2,
   smmoeRh = 3,
};

enum stsMapMouseOptEnum smmoeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *smmoeEnumToString(enum stsMapMouseOptEnum x);
/* Convert from enum to string representation. */

void smmoeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for stsMapRat related controls *******/
enum stsMapRatOptEnum {
   smroeGenetic = 0,
   smroeFhh = 1,
   smroeShrsp = 2,
   smroeRh = 3,
};

enum stsMapRatOptEnum smroeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *smroeEnumToString(enum stsMapRatOptEnum x);
/* Convert from enum to string representation. */

void smroeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for snp colors *******/

enum snpColorEnum {
    snpColorRed,
    snpColorGreen,
    snpColorBlue,
    snpColorBlack,
    snpColorExclude
};

static char *snpColorLabel[] = {
    "red",
    "green",
    "blue",
    "black",
    "exclude",
};

enum snpColorEnum snpColorStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpColorEnumToString(enum snpColorEnum x);
/* Convert from enum to string representation. */

/****** Some stuff for snpMapSource related controls *******/

/* dbSnp, Affy10K, Affy10Kv2, Affy50K_HindIII, Affy50K_XbaI */

static char *snpMapSourceLabels[] = {
    "Bac Overlaps",
    "Random",
    "Mixed",
    "Other",
    "Affymetrix Genotyping Array 10K",
    "Affymetrix Genotyping Array 120K",
};

static char *snpMapSourceStrings[] = {
    "snpMapSourceBacOverlap",
    "snpMapSourceRandom",
    "snpMapSourceMixed",
    "snpMapSourceOther",
    "snpMapSourceAffy10K",
    "snpMapSourceAffy120K",
};

static char *snpMapSourceDataName[] = {
    "BAC_OVERLAP",
    "RANDOM",
    "MIXED",
    "OTHER",
    "Affy10K",
    "Affy120K",
};

static char *snpMapSourceDefault[] = {
    "black",
    "blue",
    "red",
    "red",
    "green",
    "green",
};

static char *snpMapSourceCart[] = {
    "black",
    "blue",
    "red",
    "red",
    "green",
    "green",
};

int snpMapSourceIndex(char *string);
/* Convert from string to index representation. */

char *snpMapSourceString(int x);
/* Convert from index to string representation. */

int snpMapSourceStringToEnum(char *string);
/* Convert from string to enum representation. */

int snpMapSourceLabelStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpMapSourceLabel(int x);
/* Convert from enum to string representation. */

int snpMapSourceColorStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpMapSourceColorEnumToString(int x);
/* Convert from enum to string representation. */

int snpMapSourceDefaultStringToEnum(char *string);
/* Convert from string to enum representation. */

int snpMapSourceDefaultEnum(char *string);
/* Convert from string to enum representation. */

char *snpMapSourceDefaultEnumToString(int x);
/* Convert from enum to string representation. */

char *snpMapSourceDefaultString(int x);
/* Convert from enum to string representation. */

int snpMapSourceDataStringToEnum(char *string);
/* Convert from string to enum representation. */

int snpMapSourceDataIndex(char *string);
/* Convert from string to index representation. */

char *snpMapSourceDataString(int x);
/* Convert from index to string representation. */

char *snpMapSourceDataEnumToString(int x);
/* Convert from enum to string representation. */

/****** Some stuff for snpMapType related controls *******/

/* SingleNP, indel, segnemtal */
enum snpMapTypeEnum {
    snpMapTypeInclude,
    snpMapTypeExclude
};

static char *snpMapTypeLabel[] = {
    "include",
    "exclude",
};

static char *snpMapTypeLabels[] = {
    "Single Nucleotide Polymorphisms",
    "Insertions and Deletions",
    "Segmental Duplications",
};

static char *snpMapTypeStrings[] = {
    "snpMapTypeSingle",
    "snpMapTypeIndels",
    "snpMapTypeSegmental",
};

static char *snpMapTypeDataName[] = {
    "SNP",
    "INDEL",
    "SEGMENTAL",
};

static char *snpMapTypeDefault[] = {
    "include",
    "include",
    "include",
};

static char *snpMapTypeCart[] = {
    "include",
    "include",
    "include",
};

int snpMapTypeIndex(char *string);
/* Convert from string to index representation. */

char *snpMapTypeString(int x);
/* Convert from index to string representation. */

enum snpMapTypeEnum snpMapTypeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpMapTypeEnumToString(enum snpMapTypeEnum x);
/* Convert from enum to string representation. */

enum snpMapTypeEnum snpMapTypeLabelStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpMapTypeLabelStr(int x);
/* Convert from enum to string representation. */

char *snpMapTypeDefaultString(int x);
/* Convert from index to string representation. */

enum snpMapTypeEnum snpMapTypeDefaultStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpMapTypeDefaultEnumToString(enum snpMapTypeEnum x);
/* Convert from enum to string representation. */

enum snpMapTypeEnum snpMapTypeDataStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpMapTypeDataString(int x);
/* Convert from index to string representation. */

char *snpMapTypeDataEnumToString(enum snpMapTypeEnum x);
/* Convert from enum to string representation. */

/****** Some stuff for snpColorSource related controls *******/

/* Source, Molecule Type, Class, Validation, Function */

enum snpColorSourceEnum {
    snpColorSourceSource,
    snpColorSourceMolType,
    snpColorSourceClass,
    snpColorSourceValid,
    snpColorSourceFunc,
    snpColorSourceBlack
};

static char *snpColorSourceLabel[] = {
    "Source",
    "MolType",
    "Class",
    "Valid",
    "Func",
    "Black",
};

static char *snpColorSourceDataName[] = {
    "snpColor",
};

static char *snpColorSourceDefault[] = {
    "source",
};

static char *snpColorSourceCart[] = {
    "source",
};

int snpColorSourceIndex(char *string);
/* Convert from string to index representation. */

char *snpColorSourceString(int x);
/* Convert from index to string representation. */

enum snpColorSourceEnum snpColorSourceDefaultStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpColorSourceDefaultEnumToString(enum snpColorSourceEnum x);
/* Convert from enum to string representation. */

/****** Some stuff for snpSource related controls *******/

/* dbSnp, Affy10K, Affy10Kv2, Affy50K_HindIII, Affy50K_XbaI */
static char *snpSourceLabels[] = {
    "dbSnp",
    "Affymetrix Genotyping Array 10K",
    "Affymetrix Genotyping Array 10K v2",
    "Affymetrix Genotyping Array 50K HindIII",
    "Affymetrix Genotyping Array 50K XbaI",
};

static char *snpSourceStrings[] = {
    "snpSourceDbSnp",
    "snpSourceAffy10K",
    "snpSourceAffy10Kv2",
    "snpSourceAffy50KHindIII",
    "snpSourceAffy50KXbaI",
};

static char *snpSourceDataName[] = {
    "dbSnp",
    "Affy10K",
    "Affy10Kv2",
    "Affy50K_HindIII",
    "Affy50K_XbaI",
};

static char *snpSourceDefault[] = {
    "black",
    "blue",
    "red",
    "green",
    "green",
};

static char *snpSourceCart[] = {
    "black",
    "blue",
    "red",
    "green",
    "green",
};

int snpSourceIndex(char *string);
/* Convert from string to index representation. */

char *snpSourceString(int x);
/* Convert from index to string representation. */

int snpSourceLabelStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpSourceLabel(int x);
/* Convert from enum to string representation. */

int snpSourceColorStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpSourceColorEnumToString(int x);
/* Convert from enum to string representation. */

int snpSourceDefaultStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpSourceDefaultString(int x);
/* Convert from index to string representation. */

char *snpSourceDefaultEnumToString(int x);
/* Convert from enum to string representation. */

int snpSourceDataIndex(char *string);
/* Convert from string to enum representation. */

char *snpSourceDataEnumToString(int x);
/* Convert from enum to string representation. */

char *snpSourceDataString(int x);
/* Convert from index to string representation. */

/****** Some stuff for snpMolType related controls *******/

/* unknown, genomic, cDNA, mito, chloro */
static char *snpMolTypeLabels[] = {
    "Unknown",
    "Genomic",
    "cDNA",
    "Mitochondrial",
    "Chloroplast",
};

static char *snpMolTypeStrings[] = {
    "snpMolTypeUnknown",
    "snpMolTypeGenomic",
    "snpMolTypecDNA",
    "snpMolTypeMito",
    "snpMolTypeChloro",
};

static char *snpMolTypeDataName[] = {
    "unknown",
    "genomic",
    "cDNA",
    "mito",
    "chloro",
};

static char *snpMolTypeDefault[] = {
    "red",
    "black",
    "blue",
    "green",
    "green",
};

static char *snpMolTypeCart[] = {
    "red",
    "black",
    "blue",
    "green",
    "green",
};

int snpMolTypeIndex(char *string);
/* Convert from string to index representation. */

char *snpMolTypeString(int x);
/* Convert from index to string representation. */

int snpMolTypeStringToEnum(char *string);
/* Convert from string to enum representation. */

int snpMolTypeLabelStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpMolTypeLabel(int x);
/* Convert from enum to string representation. */

int snpMolTypeStateStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpMolTypeStateEnumToString(int x);
/* Convert from enum to string representation. */

int snpMolTypeDefaultStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpMolTypeDefaultString(int x);
/* Convert from index to string representation. */

char *snpMolTypeDefaultEnumToString(int x);
/* Convert from enum to string representation. */

int snpMolTypeDataStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpMolTypeDataEnumToString(int x);
/* Convert from enum to string representation. */

char *snpMolTypeDataString(int x);
/* Convert from index to string representation. */

int snpMolTypeDataIndex(char *string);
/* Convert from string to index representation. */

/****** Some stuff for snpClass related controls *******/

/* unknown, snp, in-del, het, microsat, named, no-variation, mixed, mnp */
static char *snpClassLabels[] = {
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

static char *snpClassStrings[] = {
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

static char *snpClassDataName[] = {
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

static char *snpClassDefault[] = {
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

static char *snpClassCart[] = {
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

int snpClassIndex(char *string);
/* Convert from string to index representation. */

char *snpClassString(int x);
/* Convert from index to string representation. */

int snpClassStringToEnum(char *string);
/* Convert from string to enum representation. */

int snpClassLabelStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpClassLabel(int x);
/* Convert from enum to string representation. */

int snpClassColorStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpClassColorEnumToString(int x);
/* Convert from enum to string representation. */

int snpClassStateStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpClassStateEnumToString(int x);
/* Convert from enum to string representation. */

int snpClassDefaultStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpClassDefaultString(int x);
/* Convert from index to string representation. */

char *snpClassDefaultEnumToString(int x);
/* Convert from enum to string representation. */

int snpClassDataStringToEnum(char *string);
/* Convert from string to enum representation. */

int snpClassDataIndex(char *string);
/* Convert from string to index representation. */

char *snpClassDataEnumToString(int x);
/* Convert from enum to string representation. */

char *snpClassDataString(int x);
/* Convert from index to string representation. */

/****** Some stuff for snpValid related controls *******/

/* unknown, other-pop, by-frequency, by-cluster, by-2hit-2allele, by-hapmap, genotype */

static char *snpValidLabels[] = {
    "Unknown",
    "Other Population",
    "By Frequency",
    "By Cluster",
    "By 2 Hit / 2 Allele",
    "By HapMap",
    "By Genotype",
};

static char *snpValidStrings[] = {
    "snpValidUnknown",
    "snpValidOtherPop",
    "snpValidFrequency",
    "snpValidCluster",
    "snpValid2H2A",
    "snpValidHapMap",
    "snpValidGenotype",
};

static char *snpValidDataName[] = {
    "unknown",
    "other-pop",
    "by-frequency",
    "by-cluster",
    "by-2hit-2allele",
    "by-hapmap",
    "genotype",
};

static char *snpValidDefault[] = {
    "red",
    "black",
    "black",
    "black",
    "black",
    "blue",
    "green",
};

static char *snpValidCart[] = {
    "red",
    "black",
    "black",
    "black",
    "black",
    "blue",
    "green",
};

int snpValidIndex(char *string);
/* Convert from string to index representation. */

char *snpValidString(int x);
/* Convert from index to string representation. */

int snpValidStringToEnum(char *string);
/* Convert from string to enum representation. */

int snpValidLabelStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpValidLabel(int x);
/* Convert from enum to string representation. */

int snpValidColorStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpValidColorEnumToString(int x);
/* Convert from enum to string representation. */

int snpValidStateStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpValidStateEnumToString(int x);
/* Convert from enum to string representation. */

int snpValidDefaultStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpValidDefaultString(int x);
/* Convert from index to string representation. */

char *snpValidDefaultEnumToString(int x);
/* Convert from enum to string representation. */

int snpValidDataStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpValidDataEnumToString(int x);
/* Convert from enum to string representation. */

char *snpValidDataString(int x);
/* Convert from index to string representation. */

/****** Some stuff for snpFunc related controls *******/

/* unknown, locus-region, coding, coding-synon, coding-nonsynon, mrna-utr, intron, splice-site, reference, exception */
static char *snpFuncLabels[] = {
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

static char *snpFuncStrings[] = {
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

static char *snpFuncDataName[] = {
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

static char *snpFuncDefault[] = {
    "black",
    "black",
    "black",
    "green",
    "red",
    "blue",
    "blue",
    "blue",
    "black",
    "black",
};

static char *snpFuncCart[] = {
    "black",
    "black",
    "black",
    "green",
    "red",
    "blue",
    "blue",
    "blue",
    "black",
    "black",
};

int snpFuncIndex(char *string);
/* Convert from string to index representation. */

char *snpFuncString(int x);
/* Convert from index to string representation. */

int snpFuncStringToEnum(char *string);
/* Convert from string to enum representation. */

int snpFuncLabelStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpFuncLabel(int x);
/* Convert from enum to string representation. */

int snpFuncColorStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpFuncColorEnumToString(int x);
/* Convert from enum to string representation. */

int snpFuncStateStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpFuncStateEnumToString(int x);
/* Convert from enum to string representation. */

int snpFuncDefaultStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpFuncDefaultString(int x);
/* Convert from index to string representation. */

char *snpFuncDefaultEnumToString(int x);
/* Convert from enum to string representation. */

int snpFuncDataStringToEnum(char *string);
/* Convert from string to enum representation. */

char *snpFuncDataEnumToString(int x);
/* Convert from enum to string representation. */

char *snpFuncDataString(int x);
/* Convert from index to string representation. */

/****** Some stuff for fishClones related controls *******/
enum fishClonesOptEnum {
   fcoeFHCRC = 0,
   fcoeNCI = 1,
   fcoeSC = 2,
   fcoeRPCI = 3,
   fcoeCSMC = 4,
   fcoeLANL = 5,
   fcoeUCSF = 6,
};

enum fishClonesOptEnum fcoeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *fcoeEnumToString(enum fishClonesOptEnum x);
/* Convert from enum to string representation. */

void fcoeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for recombRate related controls *******/
enum recombRateOptEnum {
   rroeDecodeAvg = 0,
   rroeDecodeFemale = 1,
   rroeDecodeMale = 2,
   rroeMarshfieldAvg = 3,
   rroeMarshfieldFemale = 4,
   rroeMarshfieldMale = 5,
   rroeGenethonAvg = 6,
   rroeGenethonFemale = 7,
   rroeGenethonMale = 8,
};

enum recombRateOptEnum rroeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *rroeEnumToString(enum recombRateOptEnum x);
/* Convert from enum to string representation. */

void rroeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for recombRateRat related controls *******/
enum recombRateRatOptEnum {
   rrroeShrspAvg = 0,
   rrroeFhhAvg = 1,
};

enum recombRateRatOptEnum rrroeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *rrroeEnumToString(enum recombRateRatOptEnum x);
/* Convert from enum to string representation. */

void rrroeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for recombRateMouse related controls *******/
enum recombRateMouseOptEnum {
   rrmoeWiAvg = 0,
   rrmoeMgdAvg = 1,
};

enum recombRateMouseOptEnum rrmoeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *rrmoeEnumToString(enum recombRateMouseOptEnum x);
/* Convert from enum to string representation. */

void rrmoeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for cghNci60 related controls *******/
enum cghNci60OptEnum {
   cghoeTissue = 0,
   cghoeBreast = 1,
   cghoeCns = 2,
   cghoeColon = 3,
   cghoeLeukemia = 4,
   cghoeLung = 5,
   cghoeMelanoma = 6,
   cghoeOvary = 7,
   cghoeProstate = 8,
   cghoeRenal = 9,
   cghoeAll = 10,
};

enum cghNci60OptEnum cghoeStringToEnum(char *string);
/* Convert from string to enum representation. */

char *cghoeEnumToString(enum cghNci60OptEnum x);
/* Convert from enum to string representation. */

void cghoeDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for Expression Data tracks in general *******/

struct expdFilter
/* Info on one type of expression data filter. */
{
    struct expdFilter *next;  /* Next in list. */
    char *filterInclude;         /* Identifier associated with items to include, NULL indicates include all. */
    char *filterExclude;         /* Identifier associated with items to exclude, NULL indicates exclude none. */
    boolean redGreen;         /* True if red/green color scheme, Otherwise blue/red color scheme. */
};

/*** Some Stuff for the NCI60 track ***/

enum nci60OptEnum {
   nci60Tissue = 0,
   nci60All = 1,
   nci60Breast = 2,
   nci60Cns = 3,
   nci60Colon = 4,
   nci60Leukemia = 5,
   nci60Melanoma = 6,
   nci60Ovary = 7,
   nci60Prostate = 8,
   nci60Renal = 9,
   nci60Nsclc = 10,
   nci60Duplicates = 11,
   nci60Unknown = 12
};

enum nci60OptEnum nci60StringToEnum(char *string);
/* Convert from string to enum representation. */

char *nci60EnumToString(enum nci60OptEnum x);
/* Convert from enum to string representation. */

void nci60DropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	chain track color options	*/
enum chainColorEnum {
   chainColorChromColors = 0,
   chainColorScoreColors = 1,
   chainColorNoColors = 2,
};

enum chainColorEnum chainColorStringToEnum(char *string);
/* Convert from string to enum representation. */

char *chainColorEnumToString(enum chainColorEnum x);
/* Convert from enum to string representation. */

void chainColorDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track Windowing combining function option	*/
enum wiggleWindowingEnum {
   wiggleWindowingMax = 0,
   wiggleWindowingMean = 1,
   wiggleWindowingMin = 2,
};

enum wiggleWindowingEnum wiggleWindowingStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleWindowingEnumToString(enum wiggleWindowingEnum x);
/* Convert from enum to string representation. */

void wiggleWindowingDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track use Smoothing option, 0 and 1 is the same as Off	*/
enum wiggleSmoothingEnum {
   wiggleSmoothingOff = 0,
   wiggleSmoothing2 = 1,
   wiggleSmoothing3 = 2,
   wiggleSmoothing4 = 3,
   wiggleSmoothing5 = 4,
   wiggleSmoothing6 = 5,
   wiggleSmoothing7 = 6,
   wiggleSmoothing8 = 7,
   wiggleSmoothing9 = 8,
   wiggleSmoothing10 = 9,
   wiggleSmoothing11 = 10,
   wiggleSmoothing12 = 11,
   wiggleSmoothing13 = 12,
   wiggleSmoothing14 = 13,
   wiggleSmoothing15 = 14,
   wiggleSmoothing16 = 15,
};

enum wiggleSmoothingEnum wiggleSmoothingStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleSmoothingEnumToString(enum wiggleSmoothingEnum x);
/* Convert from enum to string representation. */

void wiggleSmoothingDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track y Line Mark on/off option	*/
enum wiggleYLineMarkEnum {
   wiggleYLineMarkOff = 0,
   wiggleYLineMarkOn = 1,
};

enum wiggleYLineMarkEnum wiggleYLineMarkStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleYLineMarkEnumToString(enum wiggleYLineMarkEnum x);
/* Convert from enum to string representation. */

void wiggleYLineMarkDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track use AutoScale option	*/
enum wiggleScaleOptEnum {
   wiggleScaleManual = 0,
   wiggleScaleAuto = 1,
};

enum wiggleScaleOptEnum wiggleScaleStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleScaleEnumToString(enum wiggleScaleOptEnum x);
/* Convert from enum to string representation. */

void wiggleScaleDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track type of graph option	*/
enum wiggleGraphOptEnum {
   wiggleGraphPoints = 0,
   wiggleGraphBar = 1,
};

enum wiggleGraphOptEnum wiggleGraphStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleGraphEnumToString(enum wiggleGraphOptEnum x);
/* Convert from enum to string representation. */

void wiggleGraphDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*	Wiggle track Grid lines on/off option	*/
enum wiggleGridOptEnum {
   wiggleHorizontalGridOn = 0,
   wiggleHorizontalGridOff = 1,
};

enum wiggleGridOptEnum wiggleGridStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleGridEnumToString(enum wiggleGridOptEnum x);
/* Convert from enum to string representation. */

void wiggleGridDropDown(char *var, char *curVal);
/* Make drop down of options. */

/*** Some Stuff for the cdsColor track ***/

enum cdsColorOptEnum {
   cdsColorNoInterpolation = 0,
   cdsColorLinearInterpolation = 1,
};

enum cdsColorOptEnum cdsColorStringToEnum(char *string);
/* Convert from string to enum representation. */

char *cdsColorEnumToString(enum cdsColorOptEnum x);
/* Convert from enum to string representation. */

void cdsColorDropDown(char *var, char *curVal, int size);
/* Make drop down of options.*/

/*** Some Stuff for the base position (ruler) controls ***/

#define ZOOM_1PT5X      "1.5x"
#define ZOOM_3X         "3x"
#define ZOOM_10X        "10x"
#define ZOOM_BASE       "base"

void zoomRadioButtons(char *var, char *curVal);
/* Make a list of radio buttons for all zoom options */

/*** Some Stuff for the wiggle track ***/

enum wiggleOptEnum {
   wiggleNoInterpolation = 0,
   wiggleLinearInterpolation = 1,
};

enum wiggleOptEnum wiggleStringToEnum(char *string);
/* Convert from string to enum representation. */

char *wiggleEnumToString(enum wiggleOptEnum x);
/* Convert from enum to string representation. */

void wiggleDropDown(char *var, char *curVal);
/* Make drop down of options. */



/*** Some Stuff for the GCwiggle track ***/

enum GCwiggleOptEnum {
   GCwiggleNoInterpolation = 0,
   GCwiggleLinearInterpolation = 1,
};

enum GCwiggleOptEnum GCwiggleStringToEnum(char *string);
/* Convert from string to enum representation. */

char *GCwiggleEnumToString(enum GCwiggleOptEnum x);
/* Convert from enum to string representation. */

void GCwiggleDropDown(char *var, char *curVal);
/* Make drop down of options. */



/*** Some Stuff for the chimp track ***/

enum chimpOptEnum {
   chimpNoInterpolation = 0,
   chimpLinearInterpolation = 1,
};

enum chimpOptEnum chimpStringToEnum(char *string);
/* Convert from string to enum representation. */

char *chimpEnumToString(enum chimpOptEnum x);
/* Convert from enum to string representation. */

void wiggleDropDown(char *var, char *curVal);
/* Make drop down of options. */





/*** Some Stuff for the AFFY track ***/

enum affyOptEnum {
    affyChipType = 0,
    affyId = 1,
    affyTissue = 2,
    affyAllData = 3,
};

enum affyOptEnum affyStringToEnum(char *string);
/* Convert from string to enum representation. */

char *affyEnumToString(enum affyOptEnum x);
/* Convert from enum to string representation. */

void affyDropDown(char *var, char *curVal);
/* Make drop down of options. */

/****** Some stuff for Rosetta related controls *******/

enum rosettaOptEnum {
    rosettaAll =0,
    rosettaPoolOther=1,
    rosettaPool=2,
    rosettaOther=3
};

enum rosettaExonOptEnum {
    rosettaConfEx,
    rosettaPredEx,
    rosettaAllEx
};

enum rosettaOptEnum rosettaStringToEnum(char *string);
/* Convert from string to enum representation. */

char *rosettaEnumToString(enum rosettaOptEnum x);
/* Convert from enum to string representation. */

void rosettaDropDown(char *var, char *curVal);
/* Make drop down of options. */

enum rosettaExonOptEnum rosettaStringToExonEnum(char *string);
/* Convert from string to enum representation of exon types. */

char *rosettaExonEnumToString(enum rosettaExonOptEnum x);
/* Convert from enum to string representation of exon types. */

void rosettaExonDropDown(char *var, char *curVal);
/* Make drop down of exon type options. */


/****** Some stuff for mRNA and EST related controls *******/

struct mrnaFilter
/* Info on one type of mrna filter. */
   {
   struct  mrnaFilter *next;	/* Next in list. */
   char *label;	  /* Filter label. */
   char *key;     /* Suffix of cgi variable holding search pattern. */
   char *table;	  /* Associated table to search. */
   char *pattern; /* Pattern to find. */
   int mrnaTableIx;	/* Index of field in mrna table. */
   struct hash *hash;  /* Hash of id's in table that match pattern */
   };

struct mrnaUiData
/* Data for mrna-specific user interface. */
   {
   char *filterTypeVar;	/* cgi variable that holds type of filter. */
   char *logicTypeVar;	/* cgi variable that indicates logic. */
   struct mrnaFilter *filterList;	/* List of filters that can be applied. */
   };

struct mrnaUiData *newBedUiData(char *track);
/* Make a new  in extra-ui data structure for a bed. */

struct mrnaUiData *newMrnaUiData(char *track, boolean isXeno);
/* Make a new  in extra-ui data structure for mRNA. */

struct trackNameAndLabel
/* Store track name and label. */
   {
   struct trackNameAndLabel *next;
   char *name;	/* Name (not allocated here) */
   char *label; /* Label (not allocated here) */
   };

int trackNameAndLabelCmp(const void *va, const void *vb);
/* Compare to sort on label. */

struct hash *makeTrackHash(char *database, char *chrom);
/* Make hash of trackDb items for this chromosome. */

char *genePredDropDown(struct cart *cart, struct hash *trackHash,  
                                        char *formName, char *varName);
/* Make gene-prediction drop-down().  Return track name of
 * currently selected one.  Return NULL if no gene tracks. */

#endif /* HUI_H */


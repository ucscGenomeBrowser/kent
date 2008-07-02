/* snp125Ui.c - enums & char arrays for snp UI features and shared util code */
#include "snp125Ui.h"
#include "common.h"

static char const rcsid[] = "$Id: snp125Ui.c,v 1.31 2008/07/02 20:59:40 angie Exp $";

char *snp125OrthoTable(struct trackDb *tdb, int *retSpeciesCount)
/* Look for a setting that specifies a table with orthologous alleles.
 * If retSpeciesCount is not null, set it to the number of other species
 * whose alleles are in the table. Do not free the returned string. */
{
char *table = trackDbSetting(tdb, "chimpMacaqueOrthoTable");
int speciesCount = 2;
if (table == NULL)
    {
    table = trackDbSetting(tdb, "chimpOrangMacOrthoTable");
    speciesCount = 3;
    }
if (retSpeciesCount != NULL)
    *retSpeciesCount = speciesCount;
return table;
}


boolean snp125ExtendedNames = TRUE;

float snp125AvHetCutoff = 0.0;
int snp125WeightCutoff = 1;

/****** Some stuff for snp colors *******/

char *snp125ColorLabel[] = {
    "red",
    "green",
    "blue",
    "gray",
    "black",
};

int snp125ColorLabelSize = ArraySize(snp125ColorLabel);


/****** color source controls *******/
/* This keeps track of which SNP feature is used for color definition. */
/* Available SNP features: Molecule Type, Class, Validation, Function */

char *snp125ColorSourceLabels[] = {
    "Location Type",
    "Class",
    "Validation",
    "Function",
    "Molecule Type",
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
int snp125ColorSourceDataNameSize = ArraySize(snp125ColorSourceDataName);
int snp125ColorSourceDefaultSize  = ArraySize(snp125ColorSourceDefault);
int snp125ColorSourceCartSize     = ArraySize(snp125ColorSourceCart);

/* As of dbSNP 128, locType is ignored: */
char *snp128ColorSourceLabels[] = {
    "Class",
    "Validation",
    "Function",
    "Molecule Type",
};

int snp128ColorSourceLabelsSize   = ArraySize(snp128ColorSourceLabels);

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
/* Types: unknown, snp, in-del (locType exact), heterozygous, 
          microsatellite, named, no variation, mixed, mnp, 
	  insertion (constructed from class = in-del, locType = between)
	  deletion (constructed from class = in-del, locType = range) */

char *snp125ClassLabels[] = {
    "Unknown",
    "Single Nucleotide Polymorphism",
    "In/Del",
    "Heterozygous",
    "Microsatellite",
    "Named",
    "No Variation",
    "Mixed",
    "Mnp",
    "Insertion",
    "Deletion",
};
char *snp125ClassStrings[] = {
    "snp125ClassUnknown",
    "snp125ClassSingle",
    "snp125ClassIn-del",
    "snp125ClassHet",
    "snp125ClassMicrosatellite",
    "snp125ClassNamed",
    "snp125ClassNoVar",
    "snp125ClassMixed",
    "snp125ClassMnp",
    "snp125ClassInsertion",
    "snp125ClassDeletion",
};
char *snp125ClassDataName[] = {
    "unknown",
    "single",
    "in-del",
    "heterozygous",
    "microsatellite",
    "named",
    "no variation",
    "mixed",
    "mnp",
    "insertion",
    "deletion",
};
char *snp125ClassDefault[] = {
    "red",    // unknown
    "black",  // single
    "black",  // in-del
    "black",  // het
    "blue",   // microsatellite
    "blue",   // named
    "black",  // no variation
    "green",  // mixed
    "green",  // mnp
    "black",  // insertion
    "red",    // deletion
};
char *snp125ClassCart[] = {
    "red",    // unknown
    "black",  // single
    "black",  // in-del
    "black",  // het
    "blue",   // microsatellite
    "blue",   // named
    "black",  // no variation
    "green",  // mixed
    "green",  // mnp
    "black",  // insertion
    "red",    // deletion
};
char *snp125ClassIncludeStrings[] = {
    "snp125ClassUnknownInclude",
    "snp125ClassSingleInclude",
    "snp125ClassIn-delInclude",
    "snp125ClassHetInclude",
    "snp125ClassMicrosatelliteInclude",
    "snp125ClassNamedInclude",
    "snp125ClassNoVarInclude",
    "snp125ClassMixedInclude",
    "snp125ClassMnpInclude",
    "snp125ClassInsertionInclude",
    "snp125ClassDeletionInclude",
};
boolean snp125ClassIncludeDefault[] = {
    TRUE,  // unknown
    TRUE,  // single
    TRUE,  // in-del
    TRUE,  // het
    TRUE,  // microsatellite
    TRUE,  // named
    TRUE,  // no variation
    TRUE,  // mixed
    TRUE,  // mnp
    TRUE,  // insertion
    TRUE,  // deletion
};
boolean snp125ClassIncludeCart[] = {
    TRUE,  // unknown
    TRUE,  // single
    TRUE,  // in-del
    TRUE,  // het
    TRUE,  // microsatellite
    TRUE,  // named
    TRUE,  // no variation
    TRUE,  // mixed
    TRUE,  // mnp
    TRUE,  // insertion
    TRUE,  // deletion
};

// all of these sizes are the same
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

// all of these sizes are the same
int snp125ValidLabelsSize   = ArraySize(snp125ValidLabels);
int snp125ValidStringsSize  = ArraySize(snp125ValidStrings);
int snp125ValidDataNameSize = ArraySize(snp125ValidDataName);
int snp125ValidDefaultSize  = ArraySize(snp125ValidDefault);
int snp125ValidCartSize     = ArraySize(snp125ValidCart);
int snp125ValidIncludeStringsSize     = ArraySize(snp125ValidIncludeStrings);

/****** function related controls *******/
/* Values are a subset of snpNNN.func values:
 * unknown, locus, coding, coding-synon, coding-nonsynon,
 * untranslated, intron, splice-site, cds-reference */

char *snp125FuncLabels[] = {
    "Unknown",
    "Locus",
    "Coding - Synonymous",
    "Coding - Non-Synonymous",
    "Untranslated",
    "Intron",
    "Splice Site",
    "Reference (coding)",
};
char *snp125FuncStrings[] = {
    "snp125FuncUnknown",
    "snp125FuncLocus",
    "snp125FuncSynon",
    "snp125FuncNonSynon",
    "snp125FuncUntranslated",
    "snp125FuncIntron",
    "snp125FuncSplice",
    "snp125FuncReference",
};
char *snp125FuncDataName[] = {
    "unknown",
    "locus",
    "coding-synon",
    "coding-nonsynon",
    "untranslated",
    "intron",
    "splice-site",
    "cds-reference",
};
char *snp125FuncDefault[] = {
    "black",   // unknown
    "black",   // locus
    "green",  // coding-synon
    "red",    // coding-nonsynon
    "blue",   // untranslated
    "black",  // intron
    "red",    // splice-site
    "black",  // cds-reference
};
char *snp125FuncCart[] = {
    "gray",  // unknown
    "blue",  // locus
    "green", // coding-synon
    "red",   // coding-nonsynon
    "blue",  // untranslated
    "black", // intron
    "red",   // splice-site
    "blue", // cds-reference
};

/* NCBI has added some new, more specific function types that map onto 
 * pre-existing simpler function classes.  This mapping is an array of 
 * arrays, each of which has the simpler type (from snp125FuncDataName
 * above) followed by more specific subtypes, if any.  All arrays are
 * NULL-terminated. */
static char *locusSyn[] =
    {"locus",		"gene-segment", "near-gene-3", "near-gene-5", NULL};
static char *nonsynonSyn[] =
    {"coding-nonsynon",	"nonsense", "missense", "frameshift", NULL};
static char *untranslatedSyn[] =
    {"untranslated",	"untranslated-3", "untranslated-5", NULL};
static char *spliceSyn[] =
    {"splice-site",	"splice-3", "splice-5", NULL};
static char *cdsRefSyn[] =
    {"cds-reference",	"coding", "coding-synonymy-unknown",
     NULL};
char **snp125FuncDataSynonyms[] = {
    locusSyn,
    nonsynonSyn,
    untranslatedSyn,
    spliceSyn,
    cdsRefSyn,
    NULL
};

char *snp125FuncIncludeStrings[] = {
    "snp125FuncUnknownInclude",
    "snp125FuncLocusInclude",
    "snp125FuncSynonInclude",
    "snp125FuncNonSynonInclude",
    "snp125FuncUntranslatedInclude",
    "snp125FuncIntronInclude",
    "snp125FuncSpliceInclude",
    "snp125FuncReferenceInclude",
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

// all of these sizes are the same
int snp125FuncLabelsSize   = ArraySize(snp125FuncLabels);
int snp125FuncStringsSize  = ArraySize(snp125FuncStrings);
int snp125FuncDataNameSize = ArraySize(snp125FuncDataName);
int snp125FuncDefaultSize  = ArraySize(snp125FuncDefault);
int snp125FuncCartSize     = ArraySize(snp125FuncCart);
int snp125FuncIncludeStringsSize     = ArraySize(snp125FuncIncludeStrings);


/****** LocType related controls *******/
/* Types: unknown, range, exact, between,
          rangeInsertion, rangeSubstitution, rangeDeletion */

char *snp125LocTypeLabels[] = {
    "Unknown",
    "Range",
    "Exact",
    "Between",
    "RangeInsertion",
    "RangeSubstitution",
    "RangeDeletion",
};
char *snp125LocTypeStrings[] = {
    "snp125LocTypeUnknown",
    "snp125LocTypeRange",
    "snp125LocTypeExact",
    "snp125LocTypeBetween",
    "snp125LocTypeRangeInsertion",
    "snp125LocTypeRangeSubstitution",
    "snp125LocTypeDeletion",
};
char *snp125LocTypeDataName[] = {
    "unknown",
    "range",
    "exact",
    "between",
    "rangeInsertion",
    "rangeSubstitution",
    "rangeDeletion",
};
char *snp125LocTypeDefault[] = {
    "black",
    "red",
    "black",
    "blue",
    "green",
    "green",
    "green",
};
char *snp125LocTypeCart[] = {
    "black",
    "red",
    "black",
    "blue",
    "green",
    "green",
    "green",
};
char *snp125LocTypeIncludeStrings[] = {
    "snp125LocTypeUnknownInclude",
    "snp125LocTypeRangeInclude",
    "snp125LocTypeExactInclude",
    "snp125LocTypeBetweenInclude",
    "snp125LocTypeRangeInsertionInclude",
    "snp125LocTypeRangeSubstitutionInclude",
    "snp125LocTypeRangeDeletionInclude",
};
boolean snp125LocTypeIncludeDefault[] = {
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
};
boolean snp125LocTypeIncludeCart[] = {
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
    TRUE,
};

// all of these sizes are the same
int snp125LocTypeLabelsSize   = ArraySize(snp125LocTypeLabels);
int snp125LocTypeStringsSize  = ArraySize(snp125LocTypeStrings);
int snp125LocTypeDataNameSize = ArraySize(snp125LocTypeDataName);
int snp125LocTypeDefaultSize  = ArraySize(snp125LocTypeDefault);
int snp125LocTypeCartSize     = ArraySize(snp125LocTypeCart);
int snp125LocTypeIncludeStringsSize  = ArraySize(snp125LocTypeIncludeStrings);


/* snp125Ui.c - enums & char arrays for snp UI features and shared util code */
#include "snp125Ui.h"
#include "snp125.h"
#include "common.h"


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

/****** Some stuff for snp colors *******/

char *snp125ColorLabel[] = {
    "red",
    "green",
    "blue",
    "gray",
    "black",
};

int snp125ColorArraySize = ArraySize(snp125ColorLabel);


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

char *snp125ColorSourceOldVar = "snp125ColorSource";

int snp125ColorSourceArraySize   = ArraySize(snp125ColorSourceLabels);

/* As of dbSNP 128, locType is ignored: */
char *snp128ColorSourceLabels[] = {
    "Class",
    "Validation",
    "Function",
    "Molecule Type",
};

int snp128ColorSourceArraySize   = ArraySize(snp128ColorSourceLabels);

/* As of dbSNP 132, we have some new choices: */
char *snp132ColorSourceLabels[] = {
    "Class",
    "Validation",
    "Function",
    "Molecule Type",
    "Unusual Conditions (UCSC)",
    "Miscellaneous Attributes (dbSNP)",
    "Allele Frequencies",
};

int snp132ColorSourceArraySize   = ArraySize(snp132ColorSourceLabels);

/****** MolType related controls *******/
/* Types: unknown, genomic, cDNA, [rarely] mito */

char *snp125MolTypeLabels[] = {
    "Unknown",
    "Genomic",
    "cDNA",
    "Mito"
};
char *snp125MolTypeOldColorVars[] = {
    "snp125MolTypeUnknown",
    "snp125MolTypeGenomic",
    "snp125MolTypecDNA",
    "snp125MolTypeMito",
};
char *snp125MolTypeDataName[] = {
    "unknown",
    "genomic",
    "cDNA",
    "mito"
};
char *snp125MolTypeDefault[] = {
    "red",
    "black",
    "blue",
    "green"
};
static char *snp125MolTypeOldIncludeVars[] = {
    "snp125MolTypeUnknownInclude",
    "snp125MolTypeGenomicInclude",
    "snp125MolTypecDNAInclude",
};

int snp125MolTypeArraySize   = ArraySize(snp125MolTypeLabels);

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
char *snp125ClassOldColorVars[] = {
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
static char *snp125ClassOldIncludeVars[] = {
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

int snp125ClassArraySize   = ArraySize(snp125ClassLabels);

/****** Validation related controls *******/
/* Types: unknown, by-cluster, by-frequency, by-submitter, by-2hit-2allele, by-hapmap */

char *snp125ValidLabels[] = {
    "Unknown",
    "By Cluster",
    "By Frequency",
    "By Submitter",
    "By 2 Hit / 2 Allele",
    "By HapMap",
    "By 1000 Genomes Project",
};
char *snp125ValidOldColorVars[] = {
    "snp125ValidUnknown",
    "snp125ValidCluster",
    "snp125ValidFrequency",
    "snp125ValidSubmitter",
    "snp125Valid2H2A",
    "snp125ValidHapMap",
    "snp125Valid1000Genomes",
};
char *snp125ValidDataName[] = {
    "unknown",
    "by-cluster",
    "by-frequency",
    "by-submitter",
    "by-2hit-2allele",
    "by-hapmap",
    "by-1000genomes",
};
char *snp125ValidDefault[] = {
    "red",
    "black",
    "black",
    "black",
    "black",
    "green",
    "blue",
};
static char *snp125ValidOldIncludeVars[] = {
    "snp125ValidUnknownInclude",
    "snp125ValidClusterInclude",
    "snp125ValidFrequencyInclude",
    "snp125ValidSubmitterInclude",
    "snp125Valid2H2AInclude",
    "snp125ValidHapMapInclude",
    "snp125Valid1000GenomesInclude",
};

int snp125ValidArraySize   = ArraySize(snp125ValidLabels);

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
char *snp125FuncOldColorVars[] = {
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

/* NCBI has added some new, more specific function types that map onto 
 * pre-existing simpler function classes.  This mapping is an array of 
 * arrays, each of which has the simpler type (from snp125FuncDataName
 * above) followed by more specific subtypes, if any.  All arrays are
 * NULL-terminated. */
static char *locusSyn[] =
    {"locus",		"gene-segment", "near-gene-3", "near-gene-5", NULL};
static char *nonsynonSyn[] =
    {"coding-nonsynon",	"nonsense", "missense", "frameshift", "stop-loss", "cds-indel",
     "coding-synonymy-unknown", "cds-synonymy-unknown", NULL};
static char *untranslatedSyn[] =
    {"untranslated",	"untranslated-3", "untranslated-5", NULL};
static char *spliceSyn[] =
    {"splice-site",	"splice-3", "splice-5", NULL};
static char *cdsRefSyn[] =
    {"cds-reference",	"coding",
     NULL};
char **snp125FuncDataSynonyms[] = {
    locusSyn,
    nonsynonSyn,
    untranslatedSyn,
    spliceSyn,
    cdsRefSyn,
    NULL
};

static char *snp125FuncOldIncludeVars[] = {
    "snp125FuncUnknownInclude",
    "snp125FuncLocusInclude",
    "snp125FuncSynonInclude",
    "snp125FuncNonSynonInclude",
    "snp125FuncUntranslatedInclude",
    "snp125FuncIntronInclude",
    "snp125FuncSpliceInclude",
    "snp125FuncReferenceInclude",
};

int snp125FuncArraySize   = ArraySize(snp125FuncLabels);

// Map func terms (from all snpNNN to date) to Sequence Ontology terms and IDs:
struct snpFuncSO
    {
    char *funcTerm;	// term found in snpNNN.func
    char *soTerm;	// corresponding Sequence Ontology term
    char *soId;		// corresponding Sequence Ontology accession
    };

static struct snpFuncSO snpFuncToSO[] = {
    { "locus", "feature_variant", "SO:0001878" },
    { "locus-region", "feature_variant", "SO:0001878" },
    { "coding", "coding_sequence_variant", "SO:0001580" },
    { "coding-synon", "synonymous_variant", "SO:0001819" },
    { "coding-nonsynon", "protein_altering_variant", "SO:0001818" },
    { "untranslated", "UTR_variant", "SO:0001622" },
    { "mrna-utr", "UTR_variant", "SO:0001622" },
    { "intron", "intron_variant", "SO:0001627" },
    { "splice-site", "splice_site_variant", "SO:0001629" },
    { "cds-reference", "coding_sequence_variant", "SO:0001580" },
    { "cds-synonymy-unknown", "coding_sequence_variant", "SO:0001580" },
    { "near-gene-3", "downstream_gene_variant", "SO:0001632" },
    { "near-gene-5", "upstream_gene_variant", "SO:0001631" },
    { "ncRNA", "nc_transcript_variant", "SO:0001619" },
    { "nonsense", "stop_gained", "SO:0001587" },
    { "missense", "missense_variant", "SO:0001583" },
    { "stop-loss", "stop_lost", "SO:0001578" },
    { "frameshift", "frameshift_variant", "SO:0001589" },
    { "cds-indel", "inframe_indel", "SO:0001820" },
    { "untranslated-3", "3_prime_UTR_variant", "SO:0001624" },
    { "untranslated-5", "5_prime_UTR_variant", "SO:0001623" },
    { "splice-3", "splice_acceptor_variant", "SO:0001574" },
    { "splice-5", "splice_donor_variant", "SO:0001575" },
    // And some that dbSNP doesn't use at this point, but we do, to match Ensembl:
    { "inframe_insertion", "inframe_insertion", "SO:0001821" },
    { "inframe_deletion", "inframe_deletion", "SO:0001822" },
    { "stop_retained_variant", "stop_retained_variant", "SO:0001567" },
    { "splice_region_variant", "splice_region_variant", "SO:0001630" },
    { NULL, NULL, NULL }
};

static boolean snpSOFromFunc(char *funcTerm, char **retSoTerm, char **retSoId)
/* Look up snpNNN.func term (or SO term) in static array snpFuncToSO and set
 * corresponding Sequence Ontology term and accession; return TRUE if found. */
{
if (isEmpty(funcTerm))
    return FALSE;
int i;
for (i = 0;  snpFuncToSO[i].funcTerm != NULL;  i++)
    {
    struct snpFuncSO *info = &(snpFuncToSO[i]);
    if (sameString(funcTerm, info->funcTerm) || sameString(funcTerm, info->soTerm))
	{
	if (retSoTerm != NULL)
	    *retSoTerm = info->soTerm;
	if (retSoId != NULL)
	    *retSoId = info->soId;
	return TRUE;
	}
    }
return FALSE;
}

#define MISO_BASE_URL "http://sequenceontology.org/browser/current_release/term/"

char *snpMisoLinkFromFunc(char *funcTerm)
/* If we can map funcTerm to a Sequence Ontology term, return a link to the MISO SO browser;
 * otherwise just return the same term. funcTerm may be a comma-separated list of terms. */
{
char *soId = NULL, *soTerm = NULL;
struct dyString *dy = dyStringNew(256);
char *terms[128];
int termCount = chopCommas(cloneString(funcTerm), terms);
int i;
for (i = 0;  i < termCount;  i++)
    {
    if (i > 0)
	dyStringAppend(dy, ", ");
    boolean gotSO = snpSOFromFunc(terms[i], &soTerm, &soId);
    if (gotSO)
	dyStringPrintf(dy, "<A HREF=\""MISO_BASE_URL"%s\" TARGET=_BLANK>%s</A>", soId, soTerm);
    else
	dyStringAppend(dy, terms[i]);
    }
return dyStringCannibalize(&dy);
}

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
char *snp125LocTypeOldColorVars[] = {
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
static char *snp125LocTypeOldIncludeVars[] = {
    "snp125LocTypeUnknownInclude",
    "snp125LocTypeRangeInclude",
    "snp125LocTypeExactInclude",
    "snp125LocTypeBetweenInclude",
    "snp125LocTypeRangeInsertionInclude",
    "snp125LocTypeRangeSubstitutionInclude",
    "snp125LocTypeRangeDeletionInclude",
};

int snp125LocTypeArraySize   = ArraySize(snp125LocTypeLabels);

/****** Exception related controls *******/
char *snp132ExceptionLabels[] = {
    "None",
    "RefAlleleMismatch",
    "RefAlleleRevComp",
    "DuplicateObserved",
    "MixedObserved",
    "FlankMismatchGenomeLonger",
    "FlankMismatchGenomeEqual",
    "FlankMismatchGenomeShorter",
    "NamedDeletionZeroSpan",
    "NamedInsertionNonzeroSpan",
    "SingleClassLongerSpan",
    "SingleClassZeroSpan",
    "SingleClassTriAllelic",
    "SingleClassQuadAllelic",
    "ObservedWrongFormat",
    "ObservedTooLong",
    "ObservedContainsIupac",
    "ObservedMismatch",
    "MultipleAlignments",
    "NonIntegerChromCount",
    "AlleleFreqSumNot1",
    "SingleAlleleFreq",
    "InconsistentAlleles",
};

char *snp132ExceptionVarName[] = {
    "NoExceptions",
    "RefAlleleMismatch",
    "RefAlleleRevComp",
    "DuplicateObserved",
    "MixedObserved",
    "FlankMismatchGenomeLonger",
    "FlankMismatchGenomeEqual",
    "FlankMismatchGenomeShorter",
    "NamedDeletionZeroSpan",
    "NamedInsertionNonzeroSpan",
    "SingleClassLongerSpan",
    "SingleClassZeroSpan",
    "SingleClassTriAllelic",
    "SingleClassQuadAllelic",
    "ObservedWrongFormat",
    "ObservedTooLong",
    "ObservedContainsIupac",
    "ObservedMismatch",
    "MultipleAlignments",
    "NonIntegerChromCount",
    "AlleleFreqSumNot1",
    "SingleAlleleFreq",
    "InconsistentAlleles",
};

char *snp132ExceptionDefault[] = {
    "black",	// NoExceptions
    "red",	// RefAlleleMismatch
    "red",	// RefAlleleRevComp
    "red",	// DuplicateObserved
    "red",	// MixedObserved
    "red",	// FlankMismatchGenomeLonger
    "red",	// FlankMismatchGenomeEqual
    "red",	// FlankMismatchGenomeShorter
    "red",	// NamedDeletionZeroSpan
    "red",	// NamedInsertionNonzeroSpan
    "red",	// SingleClassLongerSpan
    "red",	// SingleClassZeroSpan
    "gray",	// SingleClassTriAllelic
    "gray",	// SingleClassQuadAllelic
    "red",	// ObservedWrongFormat
    "gray",	// ObservedTooLong
    "gray",	// ObservedContainsIupac
    "red",	// ObservedMismatch
    "red",	// MultipleAlignments
    "gray",	// NonIntegerChromCount
    "gray",	// AlleleFreqSumNot1
    "gray",	// SingleAlleleFreq
    "gray",	// InconsistentAlleles
};

int snp132ExceptionArraySize = ArraySize(snp132ExceptionLabels);

/****** Miscellaneous attributes (dbSNP's bitfields) related controls *******/

char *snp132BitfieldLabels[] = {
    "None",
    "Clinically Associated",
    "MAF >= 5% in Some Population",
    "MAF >= 5% in All Populations",
    "Appears in OMIM/OMIA",
    "Has Microattribution/Third-Party Annotation",
    "Submitted by Locus-Specific Database",
    "Genotype Conflict",
    "Ref SNP Cluster has Nonoverlapping Alleles",
    "Some Assembly's Allele Does Not Match Observed",
};

char *snp132BitfieldVarName[] = {
    "NoBitfields",
    "ClinicallyAssoc",
    "Maf5SomePop",
    "Maf5AllPops",
    "HasOmimOmia",
    "MicroattrTpa",
    "SubmittedByLsdb",
    "GenotypeConflict",
    "RsClusterNonoverlappingAlleles",
    "DbSnpObservedMismatch",
};

char *snp132BitfieldDataName[] = {
    "",
    "clinically-assoc",
    "maf-5-some-pop",
    "maf-5-all-pops",
    "has-omim-omia",
    "microattr-tpa",
    "submitted-by-lsdb",
    "genotype-conflict",
    "rs-cluster-nonoverlapping-alleles",
    "observed-mismatch",
};

char *snp132BitfieldDefault[] = {
    "black",	// NoBitfields
    "red",	// ClinicallyAssoc
    "blue",	// Maf5SomePop
    "green",	// Maf5AllPops
    "red",	// HasOmimOmia
    "red",	// MicroattrTpa
    "red",	// SubmittedByLsdb
    "gray",	// GenotypeConflict
    "gray",	// RsClusterNonoverlappingAlleles
    "gray",	// DbSnpObservedMismatch
};

int snp132BitfieldArraySize = ArraySize(snp132BitfieldLabels);


struct slName *snp125FilterFromCart(struct cart *cart, char *track, char *attribute,
				    boolean *retFoundInCart)
/* Look up snp125 filter settings in the cart, keeping backwards compatibility with old
 * cart variable names. */
{
struct slName *values = NULL;
boolean foundInCart = FALSE;
char cartVar[256];
safef(cartVar, sizeof(cartVar), "%s.include_%s", track, attribute);
if (cartListVarExists(cart, cartVar))
    {
    foundInCart = TRUE;
    values = cartOptionalSlNameList(cart, cartVar);
    }
else
    {
    char **oldVarNames = NULL, **oldDataName = NULL;
    int oldArraySize = 0;
    if (sameString(attribute, "molType"))
	{
	oldVarNames = snp125MolTypeOldIncludeVars;
	oldDataName = snp125MolTypeDataName;
	oldArraySize = snp125MolTypeArraySize;
	}
    else if (sameString(attribute, "class"))
	{
	oldVarNames = snp125ClassOldIncludeVars;
	oldDataName = snp125ClassDataName;
	oldArraySize = snp125ClassArraySize;
	}
    else if (sameString(attribute, "valid"))
	{
	oldVarNames = snp125ValidOldIncludeVars;
	oldDataName = snp125ValidDataName;
	oldArraySize = snp125ValidArraySize;
	}
    else if (sameString(attribute, "func"))
	{
	oldVarNames = snp125FuncOldIncludeVars;
	oldDataName = snp125FuncDataName;
	oldArraySize = snp125FuncArraySize;
	}
    else if (sameString(attribute, "locType"))
	{
	oldVarNames = snp125LocTypeOldIncludeVars;
	oldDataName = snp125LocTypeDataName;
	oldArraySize = snp125LocTypeArraySize;
	}
    if (oldVarNames != NULL)
	{
	int i;
	for (i=0; i < oldArraySize; i++)
	    if (cartVarExists(cart, oldVarNames[i]))
		{
		foundInCart = TRUE;
		break;
		}
	if (foundInCart)
	    {
	    for (i=0; i < oldArraySize; i++)
		if (cartUsualBoolean(cart, oldVarNames[i], TRUE))
		    slNameAddHead(&values, oldDataName[i]);
	    }
	}
    }
if (retFoundInCart != NULL)
    *retFoundInCart = foundInCart;
return values;
}

char *snp125OldColorVarToNew(char *oldVar, char *attribute)
/* Abbreviate an old cart var name -- new name is based on track plus this. Don't free result. */
{
char *ptr = oldVar;
if (startsWith("snp125", oldVar))
    {
    ptr += strlen("snp125");
    char upCaseAttribute[256];
    safecpy(upCaseAttribute, sizeof(upCaseAttribute), attribute);
    upCaseAttribute[0] = toupper(upCaseAttribute[0]);
    if (startsWith(upCaseAttribute, ptr))
	ptr += strlen(upCaseAttribute);
    }
return ptr;
}

enum snp125ColorSource snp125ColorSourceFromCart(struct cart *cart, struct trackDb *tdb)
/* Look up color source in cart, keeping backwards compatibility with old cart var names. */
{
char cartVar[512];
safef(cartVar, sizeof(cartVar), "%s.colorSource", tdb->track);
char *snp125ColorSourceDefault = snp125ColorSourceLabels[SNP125_DEFAULT_COLOR_SOURCE];
char *colorSourceCart = cartUsualString(cart, cartVar,
					cartUsualString(cart, snp125ColorSourceOldVar,
							snp125ColorSourceDefault));
int cs = stringArrayIx(colorSourceCart, snp125ColorSourceLabels, snp125ColorSourceArraySize);
int version = snpVersion(tdb->table);
if (version >= 132)
    // The enum begins with locType, which is not in the array, so add 1 to enum:
    cs = 1 + stringArrayIx(colorSourceCart, snp132ColorSourceLabels, snp132ColorSourceArraySize);
if (cs < 0)
    cs = SNP125_DEFAULT_COLOR_SOURCE;
return (enum snp125ColorSource)cs;
}

char *snp125ColorSourceToLabel(struct trackDb *tdb, enum snp125ColorSource cs)
/* Due to availability of different color sources in several different versions,
 * this is not just an array lookup, hence the encapsulation. Don't modify return value. */
{
int version = snpVersion(tdb->table);
if (version >= 132)
    {
    if (cs < 1 || cs >= snp132ColorSourceArraySize+1)
	errAbort("Bad color source for build 132 or later (%d)", cs);
    return snp132ColorSourceLabels[cs-1];
    }
else
    {
    if (cs < 0 || cs >= snp125ColorSourceArraySize)
	errAbort("Bad color source for build 131 or earlier (%d)", cs);
    return snp125ColorSourceLabels[cs];
    }
}

/* dbSnpJsonToTab - Extract fields from dbSNP JSON files, write out as tab-separated BED+. */

/* Copyright (C) 2019 The Regents of the University of California */

#include "common.h"
#include "basicBed.h"
#include "bigDbSnp.h"
#include "binRange.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "errCatch.h"
#include "hash.h"
#include "indelShift.h"
#include "iupac.h"
#include "jsonQuery.h"
#include "linefile.h"
#include "obscure.h"
#include "options.h"
#include "regexHelper.h"
#include "soTerm.h"
#include "twoBit.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dbSnpJsonToTab - Extract fields from dbSNP JSON files, write out as tab-separated track files\n"
  "usage:\n"
  "   dbSnpJsonToTab assemblyList refSeqToUcsc refsnp-chrN.json[.gz] outRoot\n"
  "assemblyList is a comma-separated set of assembly_name values in JSON, e.g. 'GRCh37.p13,GRCh38.p12'\n"
  "refSeqToUcsc is tab-sep, 4 columns: RefSeq acc (NC_...), assembly_name, twoBitFile, ucscName (chr...)\n"
  "Track files starting with outRoot will be created:\n"
  "  outRoot.<assembly>.bigDbSnp: BED4+ for main dbSNP track in each assembly in assemblyList\n"
  "                               (final columns need to be added later by bedJoinTabOffset)\n"
  "  outRootDetails.tab: tab-separated allele frequency counts and other details (for bedJoinTabOffset)\n"
  "  outRoot.<assembly>.badCoords.bed: plain BED4 with range of inconsistent SPDI coords\n"
  "Additional files are written out for error reporting:\n"
  "  outRootMerged.tab: two columns, obsolete dbSNP rs# ID and current rs# ID\n"
  "  outRootErrors.tab: lines of error info for rejected JSON objects\n"
  "  outRootWarnings.tab: warnings to report to dbSNP for rejected mappings or frequencies\n"
  "\n"
  "\n"
  "options:\n"
  "   -freqSourceOrder=LIST      LIST is an ordered, comma-sep list of project names (no spaces)\n"
  "                              for frequency data submitted to dbSNP, e.g. '1000genomes,GnomAD'\n"
  "                              If given, then the bigDbSnp minorAlleleFreq array uses the order.\n"
  "                              Otherwise minorAlleleFreq contains only one frequency, from the\n"
  "                              project with the highest sample count reported for that variant.\n"
  "   -equivRegions=FILE         Each line of FILE contains space-separated acc:start:end regions\n"
  "                              that are equivalent, for example PAR1 or PAR2 on X and Y,\n"
  "                              or an alt sequence and the corresponding portion of its chrom.\n"
  "                              These use RefSeq accesions not UCSC chr names, so that multiple\n"
  "                              assemblies can be described in the same file.\n"
  );
}

// Globals
int freqSourceCount = 0;
char **freqSourceOrder = NULL;
struct hash *seqIsRc = NULL;
struct hash *ncGrcToChrom = NULL;
struct hash *ncToTwoBitChrom = NULL;
struct hash *ncToSeqWin = NULL;

/* Command line validation table. */
static struct optionSpec options[] = {
    {"freqSourceOrder", OPTION_STRING},
    {"equivRegions", OPTION_STRING},
    {NULL, 0},
};

struct outStreams
/* Container for various output files that are not assembly-specific */
    {
    FILE *details;    // outRootDetails.tab
    FILE *merged;     // outRootMerged.tab
    FILE *err;        // outRootErrors.tab
    FILE *warn;       // outRootWarnings.tab
    };

static FILE *openOutFile(char *format, char *outRoot)
/* Open a file for writing, named by format (which must have exactly one %s) and outRoot. */
{
struct dyString *name = dyStringCreate(format, outRoot);
FILE *outF = mustOpen(name->string, "w");
dyStringFree(&name);
return outF;
}

static struct outStreams *outStreamsOpen(char *outRoot)
/* Open output file handles */
{
struct outStreams *outStreams;
AllocVar(outStreams);
outStreams->details = openOutFile("%sDetails.tab", outRoot);
outStreams->merged = openOutFile("%sMerged.tab", outRoot);
outStreams->err = openOutFile("%sErrors.tab", outRoot);
outStreams->warn = openOutFile("%sWarnings.tab", outRoot);
return outStreams;
}

static void outStreamsClose(struct outStreams **pOutStreams)
/* Close file handles and free struct. */
{
if (*pOutStreams != NULL)
    {
    struct outStreams *outStreams = *pOutStreams;
    carefulClose(&outStreams->details);
    carefulClose(&outStreams->merged);
    carefulClose(&outStreams->err);
    carefulClose(&outStreams->warn);
    freez(pOutStreams);
    }
}

struct spdiBed
/* dbSNP 2.0 variant representation: {Sequence, Position, Deletion, Insertion}, expanded to
 * cover ambiguous placement region for indels when applicable. Add chromEnd to get bed3+. */
{
    struct spdiBed *next;
    char *chrom;           // Sequence ID
    uint chromStart;       // Position: 0-based start
    uint chromEnd;         // Position + number of deleted bases
    char *del;             // Deleted sequence (starting at pos) -- or number of deleted bases
    char *ins;             // Inserted sequence
};

struct alObs
// Unit of variant allele frequency observation
    {
    struct alObs *next;
    char *allele;   // Variant allele sequence
    int obsCount;   // Number of chromosomes on which allele was observed by a project
    int totalCount; // Number of chromosomes on which project observed some allele of this variant
    int studyVersion; // Version of this study, there can be multiple for each allele.
    };

struct sharedProps
/* Properties of a refsnp that are shared by all mappings to all assemblies. */
    {
    // Extracted from JSON:
    char *name;                       // rs# ID
    int freqSourceCount;              // Number of projects reporting allele frequency observations
    struct spdiBed **freqSourceSpdis; // Array of per-project frequency allele SPDIs
    struct alObs **freqSourceObs;     // Array of per-project observation lists
    char **freqSourceMajorAl;         // Array of per-project major allele
    char **freqSourceMinorAl;         // Array of per-project minor allele
    double *freqSourceMaf;            // Array of per-project minor allele frequencies
    int biggestSourceIx;              // index of freqSourceObs with highest totalCount
    boolean *freqIsRc;                // Per-project flag for sequence isRc relative to ptlp
    struct slInt *soTerms;            // Sequence Ontology functional effect terms
    struct slName *submitters;        // dbSNP submitter handles
    struct slInt *pubMedIds;          // PubMed article citations
    struct slName *clinVarAccs;       // Accessions of ClinVar annotations, i.e. ClinVar RCV*
    struct slName *clinVarSigs;       // Reported ClinVar significance
    enum bigDbSnpClass class;         // {snv, mnv, ins, del, delins, identity}
    enum soTerm maxFuncImpact;        // {codingNonSyn, splice, codingSyn, intron, upstream, downstream, intergenic}
    int commonCount;                  // Number of projects reporting minor allele freq >= 1%
    int rareCount;                    // Number of projects reporting minor allele freq < 1%
    boolean freqIncomplete;           // At least one project provided no alt allele (only del==ins)
    boolean freqNotMapped;            // At least one project's report was not mapped to own asmbly
    };

struct spdiBed *spdiBedNewLm(char *seq, int pos, char *del, char *ins, struct lm *lm)
/* Assume seq, del, and ins belong to lm and don't need to be cloned; alloc spdi in lm.
 * Checking validity of alleles is up to caller, but this does check whether del is a number
 * instead of sequence. */
{
struct spdiBed *spdi;
lmAllocVar(lm, spdi);
spdi->chrom = seq;
spdi->chromStart = pos;
spdi->del = del;
spdi->ins = ins;
if (isAllDigits(del))
    spdi->chromEnd = pos + atoi(del);
else
    spdi->chromEnd = spdi->chromStart + strlen(del);
return spdi;
}

static boolean seqIsNucleotide(const char *seq)
/* Return TRUE unless seq contains a character that is not in the IUPAC nucleotide set. */
{
if (seq == NULL)
    return FALSE;
const char *p;
for (p = seq;  *p != '\0';  p++)
    {
    if (!isIupac(*p))
        return FALSE;
    }
return TRUE;
}

static struct spdiBed *parseSpdis(struct slRef *spdiList, char *name, struct lm *lm)
/* Transform a list of SPDI JSON objects into a list of spdiBed. errAbort on invalid values. */
{
struct spdiBed *spdiBList = NULL;
struct slRef *spdiRef;
for (spdiRef = spdiList;  spdiRef != NULL;  spdiRef = spdiRef->next)
    {
    struct jsonElement *spdiEl = spdiRef->val;
    char *seq = jsonQueryString(spdiEl, "spdi", "seq_id", lm);
    int pos = jsonQueryInt(spdiEl, "spdi", "position", -1, lm);
    char *del = jsonQueryString(spdiEl, "spdi", "deleted_sequence", lm);
    char *ins = jsonQueryString(spdiEl, "spdi", "inserted_sequence", lm);
    if (seq == NULL)
        errAbort("Missing or null seq_id in spdi for %s", name);
    if (pos < 0)
        errAbort("Invalid or missing position in spdi for %s", name);
    if (del == NULL)
        errAbort("Missing or null deleted_sequence in spdi for %s", name);
    if (ins == NULL)
        errAbort("Missing or null inserted_sequence in spdi for %s", name);
    if (! seqIsNucleotide(del))
        errAbort("Invalid deleted_sequence '%s' in spdi for %s", del, name);
    if (! seqIsNucleotide(ins))
        errAbort("Invalid inserted_sequence '%s' in spdi for %s", ins, name);
    slAddHead(&spdiBList, spdiBedNewLm(seq, pos, del, ins, lm));
    }
slReverse(&spdiBList);
return spdiBList;
}

static boolean spdiBedListSameRef(struct spdiBed *spdiBList, char *rsId)
/* Return TRUE if all spdiBed objects have the same chrom, chromStart and del.
 * errAbort if diff chrom, return FALSE for diff start/del. */
{
if (spdiBList && spdiBList->next)
    {
    struct spdiBed *spdiB;
    for (spdiB = spdiBList->next;  spdiB != NULL;  spdiB = spdiB->next)
        {
        if (differentString(spdiBList->chrom, spdiB->chrom))
            errAbort("Mismatching SPDI seq ('%s' vs '%s') for %s",
                     spdiBList->chrom, spdiB->chrom, rsId);
        if (spdiBList->chromStart != spdiB->chromStart)
            {
            warn("Mismatching SPDI pos (%s: %d vs %d) for %s",
                 spdiB->chrom, spdiBList->chromStart, spdiB->chromStart, rsId);
            return FALSE;
            }
        if (differentString(spdiBList->del, spdiB->del))
            {
            warn("Mismatching SPDI del (%s:%d: '%s' vs '%s') for %s",
                 spdiB->chrom, spdiB->chromStart, spdiBList->del, spdiB->del, rsId);
            return FALSE;
            }
        }
    }
return TRUE;
}

static void spdiBedListAssertSameRef(struct spdiBed *spdiBList)
/* Sanity check -- raise assertion if spdiBList elements do not all have the same seq, pos, del. */
{
if (spdiBList && spdiBList->next)
    {
    assert(spdiBList->chromEnd == spdiBList->chromStart + strlen(spdiBList->del));
    struct spdiBed *spdiB;
    for (spdiB = spdiBList->next;  spdiB != NULL;  spdiB = spdiB->next)
        {
        assert(spdiB->chrom == spdiBList->chrom || sameString(spdiB->chrom, spdiBList->chrom));
        assert(spdiB->chromStart == spdiBList->chromStart);
        assert(spdiB->chromEnd == spdiBList->chromEnd);
        assert(spdiB->del == spdiBList->del || sameString(spdiB->del, spdiBList->del));
        }
    }
}

static enum bigDbSnpClass varTypeToClass(char *varType)
/* Convert JSON variant_type to bigDbSnp enum form. */
{
if (sameString(varType, "snv"))
    return bigDbSnpSnv;
else if (sameString(varType, "mnv"))
    return bigDbSnpMnv;
else if (sameString(varType, "ins"))
    return bigDbSnpIns;
else if (sameString(varType, "del"))
    return bigDbSnpDel;
else if (sameString(varType, "delins"))
    return bigDbSnpDelins;
else if (sameString(varType, "identity"))
    return bigDbSnpIdentity;
else
    errAbort("varTypeToClass: unrecognized variant_type '%s'.", varType);
return bigDbSnpSnv;  // we never get here; appease gcc
}

static int freqSourceToIx(char *source, struct slName **pSourceList, struct lm *lm)
/* Convert source name to index using freqSourceOrder if defined, otherwise sourceList
 * (expand sourceList if source is not already in the list). */
{
int ix = -1;
if (freqSourceOrder)
    {
    ix = stringArrayIx(source, freqSourceOrder, freqSourceCount);
    if (ix < 0)
        errAbort("freqSourceOrder does not contains '%s'", source);
    }
else
    {
    ix = slNameFindIx(*pSourceList, source);
    if (ix < 0)
        {
        ix = slCount(*pSourceList);
        slAddTail(pSourceList, lmSlName(lm, source));
        }
    }
return ix;
}

static struct alObs *parseAlObs(struct jsonElement *obsEl, char **retSource,
                                struct spdiBed **retSpdiB, struct lm *lm)
/* Parse one allele observation. */
{
struct alObs *obs = NULL;
lmAllocVar(lm, obs);
char *source = jsonQueryString(obsEl, "alObs", "study_name", lm);
if (source == NULL)
    errAbort("parseAlObs: Missing or null study_name");
struct slRef *spdiRef = jsonQueryElement(obsEl, "alObs", "observation", lm);
struct spdiBed *spdiB = parseSpdis(spdiRef, source, lm);
obs->allele = spdiB->ins;
obs->obsCount = jsonQueryInt(obsEl, "alObs", "allele_count", -1, lm);
obs->totalCount = jsonQueryInt(obsEl, "alObs", "total_count", -1, lm);
obs->studyVersion = jsonQueryInt(obsEl, "alObs", "study_version", -1, lm);
if (obs->obsCount < 0)
    errAbort("parseAlObs: allele_count not reported for %s (ins_seq %s)",
             source, obs->allele);
if (obs->totalCount < 0)
    errAbort("parseAlObs: total_count not reported for %s (ins_seq %s)",
             source, obs->allele);
if (obs->totalCount == 0)
    errAbort("parseAlObs: got a total_count of 0 for %s (ins_seq %s)",
             source, obs->allele);
*retSource = source;
*retSpdiB = spdiB;
return obs;
}

static void addMissingRefAllele(struct sharedProps *props, char *rsId, struct lm *lm)
/* If the reference allele was omitted from frequency reports (dbSNP JIRA VR-178), infer the count
 * and add the reference allele to lists of spdis and obs. */
{
int ix;
for (ix = 0;  ix < props->freqSourceCount;  ix++)
    {
    if (props->freqSourceSpdis[ix] == NULL)
        continue;
    boolean gotRef = FALSE;
    int sumCounts = 0;
    struct spdiBed *spdiB;
    struct alObs *obs;
    for (spdiB = props->freqSourceSpdis[ix], obs = props->freqSourceObs[ix];
         spdiB != NULL && obs != NULL;
         spdiB = spdiB->next, obs = obs->next)
        {
        if (sameString(spdiB->del, spdiB->ins))
            {
            gotRef = TRUE;
            break;
            }
        sumCounts += obs->obsCount;
        }
    if (!gotRef)
        {
        struct spdiBed *firstSpdiB = props->freqSourceSpdis[ix];
        struct alObs *firstObs = props->freqSourceObs[ix];
        if (sumCounts > firstObs->totalCount)
            errAbort("addMissingRefAllele: %s, source %d, total_count is %d < sum of non-ref "
                     "counts %d", rsId, ix, firstObs->totalCount, sumCounts);
        struct spdiBed *spdiB = spdiBedNewLm(firstSpdiB->chrom, firstSpdiB->chromStart,
                                             firstSpdiB->del, firstSpdiB->del, lm);
        struct alObs *obs;
        lmAllocVar(lm, obs);
        obs->allele = spdiB->del;
        obs->obsCount = firstObs->totalCount - sumCounts;
        obs->totalCount = firstObs->totalCount;
        slAddHead(&props->freqSourceSpdis[ix], spdiB);
        slAddHead(&props->freqSourceObs[ix], obs);
        }
    }
}

static void stripOldStudyVersions(struct sharedProps *props, char *rsId)
/* After frequency allele observations have been sorted into per-project lists,
 * make sure the alleles look like [ACGT]+ and the reported total_counts are consistent.
 * The sum of obsCounts may be less than total_count (no-calls), but not greater. */
{
int ix;
for (ix = 0;  ix < props->freqSourceCount;  ix++)
    {
    int maxStudyVersion = 0;
    boolean multipleVersions = FALSE;
    struct alObs *obsList = props->freqSourceObs[ix], *obs;
    for (obs = obsList;  obs != NULL;  obs = obs->next)
        {
	if (obs == obsList)
	    {
	    maxStudyVersion = obs->studyVersion;
	    }
        else
	    {
	    if (obs->studyVersion > maxStudyVersion)
		{
		maxStudyVersion = obs->studyVersion;
		multipleVersions = TRUE;
		}
	    else if (obs->studyVersion != maxStudyVersion)
		{
		multipleVersions = TRUE;
		}
	    }
        }
    if (multipleVersions)
	{
	struct alObs *newObsList = NULL;
	struct alObs *nextObs = NULL;
	struct spdiBed *spdiBList = props->freqSourceSpdis[ix], *spdiB, *nextSpdiB, *newSpdiBList = NULL;
        for (obs = obsList, spdiB = spdiBList;  obs != NULL; obs = nextObs, spdiB = nextSpdiB)
	    {
	    nextObs = obs->next;
	    nextSpdiB = spdiB->next;
	    if (obs->studyVersion == maxStudyVersion)
		{
		slAddHead(&newObsList, obs);
		slAddHead(&newSpdiBList, spdiB);
		}
	    }
	props->freqSourceObs[ix]   = newObsList;
	props->freqSourceSpdis[ix] = newSpdiBList;
	}
    }
}

static void checkFreqSourceObs(struct sharedProps *props, char *rsId)
/* After frequency allele observations have been sorted into per-project lists,
 * make sure the alleles look like [ACGT]+ and the reported total_counts are consistent.
 * The sum of obsCounts may be less than total_count (no-calls), but not greater. */
{
int ix;
for (ix = 0;  ix < props->freqSourceCount;  ix++)
    {
    int totalCount = 0;
    struct alObs *obsList = props->freqSourceObs[ix], *obs;
    for (obs = obsList;  obs != NULL;  obs = obs->next)
        {
        if (! seqIsNucleotide(obs->allele))
            errAbort("checkFreqSourceObs: invalid observed allele '%s' for %s", obs->allele, rsId);
        if (obs->obsCount < 0)
            errAbort("checkFreqSourceObs: invalid obsCount %d for %s", obs->obsCount, rsId);
        if (obs->obsCount > obs->totalCount)
            errAbort("checkFreqSourceObs: obsCount %d > totalCount %d for %s",
                     obs->obsCount, obs->totalCount, rsId);
        totalCount += obs->obsCount;
        }
    for (obs = obsList;  obs != NULL;  obs = obs->next)
        {
        if (obs->totalCount < totalCount)
            errAbort("checkFreqSourceObs: obsCounts sum to %d, but total_count is %d for %s",
                     totalCount, obs->totalCount, rsId);
        else if (obs->totalCount != obsList->totalCount)
            errAbort("checkFreqSourceObs: inconsistent totalCount (%d for '%s' != %d for '%s' "
                     "for %s", obs->totalCount, obs->allele, obsList->totalCount, obsList->allele,
                     rsId);
        }
    }
}

static void parseFrequencies(struct slRef *annotationsRef, struct sharedProps *props,
                             char *rsId, struct lm *lm)
/* Extract per-project allele frequency count data from JSON.  Note: the alleles are from the
 * reference assembly that the project used; current reference might be different. */
{
struct slRef *obsList = jsonQueryElementList(annotationsRef, "annotations", "frequency[*]", lm);
if (obsList != NULL)
    {
    if (freqSourceOrder)
        {
        props->freqSourceCount = freqSourceCount;
        lmAllocArray(lm, props->freqSourceSpdis, freqSourceCount);
        lmAllocArray(lm, props->freqSourceObs, freqSourceCount);
        lmAllocArray(lm, props->freqIsRc, freqSourceCount);
        }
    else
        {
        props->freqSourceCount = 0;
        int allocSize = slCount(obsList);
        lmAllocArray(lm, props->freqSourceSpdis, allocSize);
        lmAllocArray(lm, props->freqSourceObs, allocSize);
        lmAllocArray(lm, props->freqIsRc, allocSize);
        }
    struct slName *unorderedSourceList = NULL;
    int maxSampleCount = 0;
    int biggestSourceIx = 0;
    int validCount = 0;
    struct slRef *obsRef;
    for (obsRef = obsList;  obsRef != NULL;  obsRef = obsRef->next)
        {
        struct jsonElement *obsEl = obsRef->val;
        char *source = NULL;
        struct spdiBed *spdiB = NULL;
        struct alObs *obs = parseAlObs(obsEl, &source, &spdiB, lm);
        int ix = freqSourceToIx(source, &unorderedSourceList, lm);
        props->freqIsRc[ix] = hashIntValDefault(seqIsRc, spdiB->chrom, -1);
        if (props->freqIsRc[ix] >= 0)
            {
            slAddHead(&props->freqSourceSpdis[ix], spdiB);
            slAddHead(&props->freqSourceObs[ix], obs);
            if (obs->totalCount > maxSampleCount)
                {
                maxSampleCount = obs->totalCount;
                biggestSourceIx = ix;
                }
            validCount++;
            }
        else
            props->freqNotMapped = TRUE;
        }
    if (props->freqNotMapped)
        warn("Frequency report not mapped to own assembly for %s", props->name);
    if (validCount > 0)
        {
        if (!freqSourceOrder)
            props->freqSourceCount = slCount(unorderedSourceList);
        int ix;
        for (ix = 0;  ix < props->freqSourceCount; ix++)
            {
            slReverse(&props->freqSourceSpdis[ix]);
            slReverse(&props->freqSourceObs[ix]);
            }
        addMissingRefAllele(props, rsId, lm);
        stripOldStudyVersions(props, rsId);
        checkFreqSourceObs(props, rsId);
        props->biggestSourceIx = biggestSourceIx;
        }
    else
        {
        props->freqSourceCount = 0;
        props->freqSourceSpdis = NULL;
        props->freqSourceObs = NULL;
        props->freqIsRc = NULL;
        }
    }
}

static struct slInt *soTermStringIdToIdList(struct slName *soTermNames, struct lm *lm)
/* Given a list of strings like "SO:0001627", convert them to enum soTerm, sort by functional
 * impact, discard duplicates and return as slInt list with highest impact first. */
{
struct slInt *intList = NULL;
if (soTermNames)
    {
    int termCount = slCount(soTermNames);
    enum soTerm termArray[termCount];
    struct slName *term;
    int i;
    for (i = 0, term = soTermNames;  term != NULL;  i++, term = term->next)
        termArray[i] = soTermStringIdToId(term->name);
    assert(i == termCount);
    qsort(termArray, termCount, sizeof termArray[0], soTermCmp);
    for (i = 0;  i < termCount; i++)
        {
        if (i == 0 || termArray[i] != termArray[i-1])
            {
            struct slInt *intEl;
            lmAllocVar(lm, intEl);
            intEl->val = termArray[i];
            slAddHead(&intList, intEl);
            }
        }
    slReverse(&intList);
    }
return intList;
}

struct seqWindow *getChromSeq(struct hash *ncToSeqWin, char *nc, struct hash *ncToTwoBitChrom)
/* Look up nc in ncToSeqWin.  If not found, look it up in ncToTwoBitChrom, make a seqWin,
 * store it in ncToSeqWin and return it.  Return NULL if neither hash has nc. */
{
struct seqWindow *seqWin = hashFindVal(ncToSeqWin, nc);
if (seqWin == NULL)
    {
    char *twoBitChrom = hashFindVal(ncToTwoBitChrom, nc);
    if (twoBitChrom != NULL)
        {
        struct dnaSeq *dnaSeq = twoBitLoadAll(twoBitChrom);
        if (dnaSeq != NULL)
            {
            // Use memSeqWindow instead of twoBitSeqWindow to avoid keeping a bunch of open
            // twoBitFile handles.
            // Hmm, a dnaSeqWindow would be more efficient, no cloning and strlen...
            // or at the least, memSeqWindowNew could take a size arg.
            seqWin = memSeqWindowNew(twoBitChrom, dnaSeq->dna);
            dnaSeqFree(&dnaSeq);
            hashAdd(ncToSeqWin, nc, seqWin);
            }
        }
    }
if (seqWin == NULL)
    errAbort("No twoBit file and chrom found in refSeqToUcsc file for seq '%s'", nc);
return seqWin;
}

static boolean maybeExpandRange(struct spdiBed *spdiBTrim, struct seqWindow *seqWin,
                                struct spdiBed *spdiB, struct lm *lm)
/* If spdiBTrim is a pure insertion or deletion within a repetitive region,
 * then expand spdiB to the full repetitive range and return TRUE.
 * spdiBTrim is assumed to already have minimal representation (see trimSpdi). */
{
boolean changed = FALSE;
uint refTrimStart = spdiBTrim->chromStart;
uint refTrimEnd = spdiBTrim->chromEnd;
char *altTrim = spdiBTrim->ins;
int refTrimLen = refTrimEnd - refTrimStart;
int altTrimLen = strlen(altTrim);
if (indelShiftIsApplicable(refTrimLen, altTrimLen))
    {
    // Now shift the minimal representation left and right and compare with spdiB coords.
    uint leftStart = refTrimStart, leftEnd = refTrimEnd;
    char leftAlt[altTrimLen+1];
    safecpy(leftAlt, sizeof leftAlt, altTrim);
    int leftShifted = indelShift(seqWin, &leftStart, &leftEnd, leftAlt,
                                 INDEL_SHIFT_NO_MAX, isdLeft);
    uint rightStart = refTrimStart, rightEnd = refTrimEnd;
    char rightAlt[altTrimLen+1];
    safecpy(rightAlt, sizeof rightAlt, altTrim);
    int rightShifted = indelShift(seqWin, &rightStart, &rightEnd, rightAlt,
                                  INDEL_SHIFT_NO_MAX, isdRight);
    if (leftShifted || rightShifted)
        {
        changed = TRUE;
        // Expand spdiB->del to cover the whole range.,
        int newRefLen = rightEnd - leftStart;
        spdiB->del = lmAlloc(lm, newRefLen+1);
        seqWindowCopy(seqWin, leftStart, newRefLen, spdiB->del, newRefLen+1);
        // Concatenate ref bases to the left, altTrim, ref bases to the right -> new alt
        char refLeft[leftShifted+1];
        seqWindowCopy(seqWin, leftStart, leftShifted, refLeft, sizeof refLeft);
        char refRight[rightShifted+1];
        seqWindowCopy(seqWin, refTrimEnd, rightShifted, refRight, sizeof refRight);
        int newAltLen = leftShifted + altTrimLen + rightShifted;
        spdiB->ins = lmAlloc(lm, newAltLen+1);
        safef(spdiB->ins, newAltLen+1, "%s%s%s", refLeft, altTrim, refRight);
        spdiB->chromStart = leftStart;
        spdiB->chromEnd = rightEnd;
        }
    }
return changed;
}

static void checkObsVsSpdi(struct alObs *obsList, struct spdiBed *spdiBList)
/* assert that obsList and spdiBList have same length and each obs->allele is the same as the
 * corresponding spdiB->ins. */
{
assert(slCount(spdiBList) == slCount(obsList));
struct spdiBed *spdiB;
struct alObs *obs;
for (spdiB = spdiBList, obs = obsList;
     spdiB != NULL;
     spdiB = spdiB->next, obs = obs->next)
    {
    assert(sameString(obs->allele, spdiB->ins));
    }
}

static struct spdiBed *trimSpdi(struct spdiBed *spdiB, struct lm *lm)
/* Return a new spdiBed that is the minimal representation of spdiB. */
{
struct spdiBed *spdiBTrim = spdiBedNewLm(spdiB->chrom, spdiB->chromStart,
                                         lmCloneString(lm, spdiB->del),
                                         lmCloneString(lm, spdiB->ins), lm);
int refTrimLen = 0, altTrimLen = 0;
trimRefAlt(spdiBTrim->del, spdiBTrim->ins, &spdiBTrim->chromStart, &spdiBTrim->chromEnd,
           &refTrimLen, &altTrimLen);
return spdiBTrim;
}

static void spdiBedListNormalizeRange(struct spdiBed *spdiBList, struct seqWindow *seqWin,
                                      struct lm *lm)
/* If members of spdiBList have inconsistent chromStart/chromEnd/del then expand chromStart,
 * chromEnd, del and ins to cover the full range of all elements in list.  Ref allele is
 * not included when finding range -- it is reset using an alt's final del. */
{
if (spdiBList && spdiBList->next)
    {
    struct spdiBed *spdiBRef = NULL, *spdiBAlt = NULL;
    char *firstChrom = NULL;
    int minChromStart = BIGNUM;
    int maxChromEnd = 0;
    boolean inconsistent = FALSE;
    struct spdiBed *spdiB;
    for (spdiB = spdiBList;  spdiB != NULL;  spdiB = spdiB->next)
        {
        if (firstChrom == NULL)
            firstChrom = spdiB->chrom;
        else if (differentString(spdiB->chrom, firstChrom))
            errAbort("spdiBedListNormalizeRange: inconsistent chrom '%s' vs. '%s'",
                     firstChrom, spdiB->chrom);
        if (spdiB->del == spdiB->ins || sameString(spdiB->del, spdiB->ins))
            {
            if (spdiBRef != NULL)
                errAbort("spdiBedListNormalizeRange: multiple ref allele SPDIs not supported.");
            spdiBRef = spdiB;
            }
        else
            {
            if (spdiBAlt == NULL)
                spdiBAlt = spdiB;
            else if (spdiB->chromStart != spdiBAlt->chromStart ||
                     spdiB->chromEnd != spdiBAlt->chromEnd)
                inconsistent = TRUE;
            else if (! (spdiB->del == spdiBAlt->del || sameString(spdiB->del, spdiBAlt->del)))
                errAbort("spdiBedListNormalizeRange: same coords %s|%d|%d, different del "
                         "'%s' / '%s'",
                         spdiB->chrom, spdiB->chromStart, spdiB->chromEnd,
                         spdiB->del, spdiBAlt->del);
            if (spdiB->chromStart < minChromStart)
                minChromStart = spdiB->chromStart;
            if (spdiB->chromEnd > maxChromEnd)
                maxChromEnd = spdiB->chromEnd;
            }
        }
    if (inconsistent)
        {
        int newDelLen = maxChromEnd - minChromStart;
        char *newDel = lmAlloc(lm, newDelLen+1);
        seqWindowCopy(seqWin, minChromStart, newDelLen, newDel, newDelLen+1);
        for (spdiB = spdiBList;  spdiB != NULL;  spdiB = spdiB->next)
            {
            // Skip ref allele for now, it is updated at end.
            if (sameString(spdiB->del, spdiB->ins))
                continue;
            // If needed, expand chromStart/chromEnd and add bases from the reference to the
            // left and/or right of del and ins.
            int padLeft = spdiB->chromStart - minChromStart;
            int padRight = maxChromEnd - spdiB->chromEnd;
            if (padLeft > 0 || padRight > 0)
                {
                spdiB->chromStart = minChromStart;
                spdiB->chromEnd = maxChromEnd;
                spdiB->del = newDel;
                // Concatenate left padding, old ins value, and right padding to make new ins.
                int oldInsLen = strlen(spdiB->ins);
                int newInsLen = padLeft + oldInsLen + padRight;
                char *newIns = lmAlloc(lm, newInsLen+1);
                safencpy(newIns, newInsLen+1, newDel, padLeft);
                safencpy(newIns+padLeft, newInsLen+1-padLeft, spdiB->ins, oldInsLen);
                safencpy(newIns+padLeft+oldInsLen, newInsLen+1-padLeft-oldInsLen,
                         newDel+newDelLen-padRight, padRight);
                spdiB->ins = newIns;
                }
            }
        }
    // Now that alts are settled, fix ref allele.
    if (spdiBRef && spdiBAlt &&
        (spdiBRef->chromStart != spdiBAlt->chromStart || spdiBRef->chromEnd != spdiBAlt->chromEnd))
        {
        spdiBRef->chromStart = spdiBAlt->chromStart;
        spdiBRef->chromEnd = spdiBAlt->chromEnd;
        spdiBRef->del = spdiBRef->ins = spdiBAlt->del;
        }
    }
}

static void setObsToSpdi(struct alObs *obsList, struct spdiBed *spdiBList)
/* Set alleles in obsList to match ins in spdiBList. */
{
assert(slCount(spdiBList) == slCount(obsList));
struct spdiBed *spdiB;
struct alObs *obs;
for (spdiB = spdiBList, obs = obsList;
     spdiB != NULL;
     spdiB = spdiB->next, obs = obs->next)
    {
    obs->allele = spdiB->ins;
    }
}

static void spdiNormalizeFreq(struct sharedProps *props, struct hash *ncToTwoBitChrom,
                              struct hash *ncToSeqWin, struct lm *lm)
/* Allele frequency counts come directly from VCF, so although they are encoded as SPDI, they do
 * not cover the entire repetitive range.  Expand variant alleles on the sequence on which they
 * were originally reported; if there is an indel difference between the reporting assembly and
 * the mapping assembly then this will resolve the actual alleles correctly (sometimes ref and alt
 * may be different for different assemblies). */
{
int sIx;
for (sIx = 0;  sIx < props->freqSourceCount;  sIx++)
    {
    struct spdiBed *spdiBList = props->freqSourceSpdis[sIx];
    if (spdiBList != NULL)
        {
        spdiBedListAssertSameRef(spdiBList);
        struct seqWindow *seqWin = getChromSeq(ncToSeqWin, spdiBList->chrom, ncToTwoBitChrom);
        struct alObs *obsList = props->freqSourceObs[sIx];
        // Sanity check that spdiBList and obsList are consistent before we start tweaking:
        checkObsVsSpdi(obsList, spdiBList);
        boolean changed = FALSE;
        struct spdiBed *spdiB;
        for (spdiB = spdiBList;  spdiB != NULL;  spdiB = spdiB->next)
            {
            // The reference allele (del == ins) can't be shifted in a meaningful way --
            // skip it for now and see how the alt alleles (the actual changes) might be shifted.
            if (sameString(spdiB->del, spdiB->ins))
                continue;
            struct spdiBed *spdiBTrim = trimSpdi(spdiB, lm);
            if (maybeExpandRange(spdiBTrim, seqWin, spdiB, lm))
                changed = TRUE;
            else if (spdiBTrim->chromStart != spdiB->chromStart ||
                     spdiBTrim->chromEnd != spdiB->chromEnd)
                {
                // Use the trimmed version
                changed = TRUE;
                spdiB->chromStart = spdiBTrim->chromStart;
                spdiB->chromEnd = spdiBTrim->chromEnd;
                spdiB->del = spdiBTrim->del;
                spdiB->ins = spdiBTrim->ins;
                }
            }
        if (changed)
            {
            spdiBedListNormalizeRange(spdiBList, seqWin, lm);
            setObsToSpdi(obsList, spdiBList);
            }
        spdiBedListAssertSameRef(spdiBList);
        }
    }
}

static double mafFromSource(struct sharedProps *props, int sourceIx,
                            char **retMajorAllele, char **retMinorAllele, struct lm *lm)
/* Compare allele counts from source and return minor allele frequency (2nd-highest).
 * Set *retMajorAllele to allele with highest count (for comparison with mapped ref alleles).
 * If there are no observations from source, return NAN (divide by zero). */
{
char *majorAllele = NULL, *minorAllele = NULL;
int majorAlleleCount = -1, minorAlleleCount = -1;
double totalCount = 0;
struct alObs *obs;
for (obs = props->freqSourceObs[sourceIx];  obs != NULL;  obs = obs->next)
    {
    if (obs->obsCount > majorAlleleCount)
        {
        minorAllele = majorAllele;
        minorAlleleCount = majorAlleleCount;
        majorAlleleCount = obs->obsCount;
        majorAllele = obs->allele;
        }
    else if (obs->obsCount > minorAlleleCount)
        {
        minorAllele = obs->allele;
        minorAlleleCount = obs->obsCount;
        }
    totalCount = obs->totalCount;
    }
if (totalCount > 0)
    {
    if (majorAlleleCount < 0)
        errAbort("no allele observed despite nonzero totalCount...?");
    if (minorAlleleCount < 0)
        {
        // Allele and count are provided for only one allele -- inferring the other allele
        // would be dicey, so just add a note and erase the freq data from this source and
        // return NaN.
        props->freqIncomplete = TRUE;
        props->freqSourceSpdis[sourceIx] = NULL;
        props->freqSourceObs[sourceIx] = NULL;
        *retMajorAllele = NULL;
        *retMinorAllele = NULL;
        warn("Incomplete freq data from source %d for %s", sourceIx, props->name);
        return NAN;
        }
    }
*retMajorAllele = lmCloneString(lm, majorAllele);
*retMinorAllele = lmCloneString(lm, minorAllele);
// If no data from source, divide by zero will return -nan or -inf. (Use isfinite() to test)
return (double)minorAlleleCount / totalCount;
}

static void sortFrequencies(struct sharedProps *props, struct lm *lm)
/* Determine major/minor allele and per-project MAF (if given).
 * If -freqOrder option was given, then fill in props->freqSource* arrays in that order.
 * Otherwise, choose the MAF from the project with the largest sample count as a fallback. */
{
if (props->freqSourceCount > 0)
    {
    if (freqSourceOrder != NULL)
        {
        lmAllocArray(lm, props->freqSourceMajorAl, freqSourceCount);
        lmAllocArray(lm, props->freqSourceMinorAl, freqSourceCount);
        lmAllocArray(lm, props->freqSourceMaf, freqSourceCount);
        int sIx;
        for (sIx = 0;  sIx < freqSourceCount;  sIx++)
            {
            props->freqSourceMaf[sIx] = mafFromSource(props, sIx,
                                                      &props->freqSourceMajorAl[sIx],
                                                      &props->freqSourceMinorAl[sIx], lm);
            if (isfinite(props->freqSourceMaf[sIx]))
                {
                if (props->freqSourceMaf[sIx] >= 0.01)
                    props->commonCount++;
                else
                    props->rareCount++;
                }
            }
        }
    else
        {
        // No freqSourceOrder -- just use source with biggest totalCount for this variant.
        props->freqSourceCount = 1;
        lmAllocArray(lm, props->freqSourceMajorAl, props->freqSourceCount);
        lmAllocArray(lm, props->freqSourceMinorAl, props->freqSourceCount);
        lmAllocArray(lm, props->freqSourceMaf, props->freqSourceCount);
        props->freqSourceMaf[0] = mafFromSource(props, props->biggestSourceIx,
                                                &props->freqSourceMajorAl[0],
                                                &props->freqSourceMinorAl[0], lm);
        if (isfinite(props->freqSourceMaf[0]))
            {
            if (props->freqSourceMaf[0] >= 0.01)
                props->commonCount++;
            else
                props->rareCount++;
            }
        }
    if (props->freqIncomplete)
        {
        // At least one freq project was erased due to incomplete data.  If it was the only
        // project with any data, set props->freqSourceCount to 0 (no freq data).
        int sIx;
        boolean gotFreq = FALSE;
        for (sIx = 0;  sIx < props->freqSourceCount;  sIx++)
            {
            if (isfinite(props->freqSourceMaf[sIx]))
                gotFreq = TRUE;
            }
        if (!gotFreq)
            {
            props->freqSourceCount = 0;
            props->freqSourceMajorAl = NULL;
            props->freqSourceMinorAl = NULL;
            }
        }
    }
}

static void dyStringPrintSlNameList(struct dyString *dy, struct slName *list, char *sep); // forward declaration

static struct sharedProps *extractProps(struct jsonElement *top, struct lm *lm)
/* Extract the properties shared by all mappings of a refsnp from JSON, alloc & return. */
{
struct sharedProps *props = NULL;
lmAllocVar(lm, props);
char *rsNumber = jsonQueryString(top, "top", "refsnp_id", lm);
char rsName[strlen(rsNumber)+3];
safef(rsName, sizeof rsName, "rs%s", rsNumber);
props->name = lmCloneString(lm, rsName);
char *varType = jsonQueryString(top, "top", "primary_snapshot_data.variant_type", lm);
props->class = varTypeToClass(varType);
struct slRef *annotationsRef = jsonQueryElement(top, "top",
                                                 "primary_snapshot_data.allele_annotations[*]", lm);
parseFrequencies(annotationsRef, props, rsName, lm);
spdiNormalizeFreq(props, ncToTwoBitChrom, ncToSeqWin, lm);
sortFrequencies(props, lm);
struct slName *soTermNames = jsonQueryStringList(annotationsRef, "annotations",
                                                 "assembly_annotation[*].genes[*].rnas[*]"
                                                 ".sequence_ontology[*].accession", lm);
soTermNames = slCat(soTermNames,
                    jsonQueryStringList(annotationsRef, "annotations",
                                        "assembly_annotation[*].genes[*].rnas[*]"
                                        ".protein.sequence_ontology[*].accession", lm));
props->soTerms = soTermStringIdToIdList(soTermNames, lm);
props->maxFuncImpact = props->soTerms ? props->soTerms->val : soUnknown;
props->submitters = jsonQueryStrings(top, "top",
                                    "primary_snapshot_data.support[id.type=subsnp].submitter_handle",
                                        lm);
slUniqify(&props->submitters, slNameCmp, NULL);
props->pubMedIds = jsonQueryInts(top, "top", "citations[*]", lm);
props->clinVarAccs = jsonQueryStringList(annotationsRef, "annotations",
				    "clinical[*].accession_version", lm);
props->clinVarSigs = jsonQueryStringList(annotationsRef, "annotations",
                                          "clinical[*].clinical_significances[*]", lm);
if (slCount(props->clinVarAccs) != slCount(props->clinVarSigs))  // extra steps to deal with rare cases of elements with multiple significances listed.
    {
    struct dyString *dy = dyStringNew(256);
    dyStringClear(dy);
    dyStringPrintSlNameList(dy, props->clinVarSigs, ",");
    char *origClinVarSigs = cloneString(dy->string);

    props->clinVarSigs = NULL;

    for (struct slRef *anno=annotationsRef; anno; anno=anno->next)
	{
        // to achieve the needed effect, step through the top-level annotationsRef one element at a time, instead of as a list.
	struct slRef *annoAnnoNext = anno->next;
	anno->next = NULL;  // suppress the siblings temporarily so we process each element one at a time.
	for (int c = 0; ; ++c)
	    {
	    char queryString[256];
	    safef(queryString, sizeof queryString, "clinical[%d].clinical_significances[*]", c);
	    struct slName *theseSigNames = jsonQueryStringList(anno, "annotations", queryString, lm);
	    if (!theseSigNames)
		{
		break;
		}
            // re-encode the separation of multiple values with a html encoding instead of a comma
	    // so they will not mess up the comma-separated list that acts like an array.
            if (slCount(theseSigNames) >=2)
		{
                dyStringClear(dy);
		for(struct slName *slTemp = theseSigNames; slTemp; slTemp=slTemp->next)
		    {	
		    dyStringPrintf(dy, "%s", slTemp->name);
		    if (slTemp->next)
			{
			dyStringPrintf(dy,  "&#44;");
			}
		    }
		slAddHead(&props->clinVarSigs, slNameNew(dy->string));
                dyStringClear(dy);
		dyStringPrintSlNameList(dy, theseSigNames, ",");
		warn("comma separator html-encoded in %s which has multiple clinical_significances [%s]", rsName, dy->string);
		}
	    else  // just one on the list
		{
		slAddHead(&props->clinVarSigs, theseSigNames);
		}
	    }
	anno->next = annoAnnoNext;  // restore the pointer
	}
    slReverse(&props->clinVarSigs);
    // recreate the error to re-create the original to confirm correct result, with all fields in the same order.
    dyStringClear(dy);
    dyStringPrintSlNameList(dy, props->clinVarSigs, ",");
    char *badReconstruction = replaceChars(dy->string, "&#44;", ",");
    assert(sameString(origClinVarSigs, badReconstruction));
    freeMem(badReconstruction);
    freeMem(origClinVarSigs);
    dyStringFree(&dy);
    }

return props;
}

static void setBdsFreqData(struct sharedProps *props, boolean isRc, struct bigDbSnp *bds,
                           struct dyString *dyUcscNotes, struct lm *lm)
/* Use isRc, props to fill in bds->majorAllele, ->minorAllele, determine whether ref is maj/min.
 * Add notes to dyUcscNotes if applicable. */
{
if (props->freqSourceCount > 0)
    {
    bds->freqSourceCount = props->freqSourceCount;
    lmAllocArray(lm, bds->majorAllele, props->freqSourceCount);
    lmAllocArray(lm, bds->minorAllele, props->freqSourceCount);
    lmAllocArray(lm, bds->minorAlleleFreq, props->freqSourceCount);
    char *firstMajorAllele = NULL;
    boolean refIsMinor = FALSE;
    boolean diffMajor = FALSE;
    int sIx;
    for (sIx = 0;  sIx < props->freqSourceCount;  sIx++)
        {
        boolean freqIsRc = props->freqIsRc[sIx];
        bds->minorAlleleFreq[sIx] = props->freqSourceMaf[sIx];
        bds->majorAllele[sIx] = lmCloneString(lm, emptyForNull(props->freqSourceMajorAl[sIx]));
        bds->minorAllele[sIx] = lmCloneString(lm, emptyForNull(props->freqSourceMinorAl[sIx]));
        if (isfinite(bds->minorAlleleFreq[sIx]))
            {
            if (isRc != freqIsRc)
                {
                reverseComplement(bds->majorAllele[sIx], strlen(bds->majorAllele[sIx]));
                reverseComplement(bds->minorAllele[sIx], strlen(bds->minorAllele[sIx]));
                }
            char *majorAllele = bds->majorAllele[sIx];
            if (differentString(bds->ref, majorAllele))
                refIsMinor = TRUE;
            if (firstMajorAllele == NULL)
                firstMajorAllele = majorAllele;
            else if (majorAllele != NULL &&
                     differentString(firstMajorAllele, majorAllele))
                diffMajor = TRUE;
            }
        }
    if (refIsMinor)
        dyStringAppend(dyUcscNotes, bdsRefIsMinor ",");
    if (diffMajor)
        dyStringAppend(dyUcscNotes, bdsDiffMajor ",");
    }
}

static void checkRareRef(struct sharedProps *props, char *ref, boolean isRc,
                         struct dyString *dyUcscNotes)
/* If frequency counts are reported, test whether the reference has ever been observed,
 * and if so whether it is rare, i.e. < 1%.  If so, add to dyUcscNotes. */
{
double rareThreshold = 0.01;
boolean haveFrequencies = FALSE, refWasObserved = FALSE;
double maxRefAF = 0.0;
int ix;
for (ix = 0;  ix < props->freqSourceCount;  ix++)
    {
    if (props->freqSourceObs[ix] != NULL)
        haveFrequencies = TRUE;
    boolean freqIsRc = props->freqIsRc[ix];
    int refLen = strlen(ref);
    char refCpy[refLen+1];
    safecpy(refCpy, sizeof refCpy, ref);
    if (isRc != freqIsRc)
        reverseComplement(refCpy, refLen);
    struct spdiBed *spdiB;
    struct alObs *obs;
    for (spdiB = props->freqSourceSpdis[ix], obs = props->freqSourceObs[ix];
         spdiB != NULL && obs != NULL;
         spdiB = spdiB->next, obs = obs->next)
        {
        if (sameString(refCpy, spdiB->ins))
            {
            if (obs->obsCount > 0)
                {
                refWasObserved = TRUE;
                double refAF = (double)obs->obsCount / (double)obs->totalCount;
                if (refAF > maxRefAF)
                    maxRefAF = refAF;
                }
            break;
            }
        }
    if (maxRefAF >= rareThreshold)
        break;
    }
if (haveFrequencies && maxRefAF < rareThreshold)
    {
    dyStringAppend(dyUcscNotes, bdsRefIsRare ",");
    if (!refWasObserved)
        dyStringAppend(dyUcscNotes, bdsRefIsSingleton ",");
    }
}

static enum bigDbSnpClass bigDbSnpClassFromAlleles(struct bigDbSnp *bds)
/* Return what we expect bigDbSnpClass to be based on allele lengths. */
{
enum bigDbSnpClass alleleClass = 0;
if (bds->altCount == 0)
    alleleClass = bigDbSnpIdentity;
else
    {
    int refLen = strlen(bds->ref);
    boolean altLenZero = TRUE;
    boolean altLenLtRef = FALSE;
    boolean altLenEqRef = FALSE;
    boolean altLenGtRef = FALSE;
    int i;
    for (i = 0;  i < bds->altCount;  i++)
        {
        int altLen = strlen(bds->alts[i]);
        if (altLen < refLen)
            altLenLtRef = TRUE;
        else if (altLen == refLen)
            altLenEqRef = TRUE;
        else if (altLen > refLen)
            altLenGtRef = TRUE;
        if (altLen > 0)
            altLenZero = FALSE;
        }
    if (!altLenLtRef &&  altLenEqRef && !altLenGtRef)
        {
        if (refLen == 1)
            alleleClass = bigDbSnpSnv;
        else
            alleleClass = bigDbSnpMnv;
        }
    else if (bds->altCount == 0)
        alleleClass = bigDbSnpIdentity;
    else if (refLen == 0)
        alleleClass = bigDbSnpIns;
    else if (altLenZero)
        alleleClass = bigDbSnpDel;
    else
        alleleClass = bigDbSnpDelins;
    }
return alleleClass;
}

static void checkIndelPlacement(struct bigDbSnp *bds, struct seqWindow *seqWin,
                                struct lm *lm)
/* If bds is/has a pure insertion or deletion in a repetitive genomic region, make sure that
 * bds's SPDI-based coords cover the whole repetitive region as expected. */
{
int i;
for (i = 0;  i < bds->altCount;  i++)
    {
    char *alt = bds->alts[i];
    if (isEmpty(bds->ref) || isEmpty(alt) ||
        stringIn(bds->ref, alt) || stringIn(alt, bds->ref))
        {
        // First trim ref and alt to their minimal version for indelShift to work with.
        // There may be nothing to trim because they're usually minimal already and
        // stringIn catches false positive like A/TAAC, and that's fine.
        struct spdiBed *spdiB = spdiBedNewLm(bds->chrom, bds->chromStart, bds->ref, alt, lm);
        struct spdiBed *spdiBTrim = trimSpdi(spdiB, lm);

        maybeExpandRange(spdiBTrim, seqWin, spdiB, lm);
        if (spdiB->chromStart != bds->chromStart || spdiB->chromEnd != bds->chromEnd)
            errAbort("Range of %s (%s|%d|%d ref='%s', alt='%s') "
                     "could be expanded to %s|%d|%d\n",
                     bds->name, bds->chrom, bds->chromStart, bds->chromEnd, bds->ref, alt,
                     bds->chrom, spdiB->chromStart, spdiB->chromEnd);
        }
    }
}

static void addClinVarSigs(struct dyString *dyUcscNotes, struct sharedProps *props)
/* If clinVarSigs indicate benign, pathogenic, or both (conflicting), add ucscNote. */
{
boolean isBenign = FALSE, isPathogenic = FALSE;
struct slName *sig;
for (sig = props->clinVarSigs;  sig != NULL;  sig = sig->next)
    {
    if (sameString(sig->name, "likely-benign") ||
             sameString(sig->name, "benign") ||
             sameString(sig->name, "benign-likely-benign"))
        {
        isBenign = TRUE;
        }
    else if (sameString(sig->name, "pathogenic") ||
             sameString(sig->name, "likely-pathogenic") ||
             sameString(sig->name, "pathogenic-likely-pathogenic"))
        {
        isPathogenic = TRUE;
        }
    else if (sameString(sig->name, "conflicting-interpretations-of-pathogenicity"))
        {
        isBenign = TRUE;
        isPathogenic = TRUE;
        break;
        }
    }
if (isBenign && isPathogenic)
    dyStringAppend(dyUcscNotes, bdsClinvarConflicting ",");
else if (isBenign)
    dyStringAppend(dyUcscNotes, bdsClinvarBenign ",");
else if (isPathogenic)
    dyStringAppend(dyUcscNotes, bdsClinvarPathogenic ",");
}

static boolean delMismatchesGenome(struct bigDbSnp *bds, struct seqWindow *seqWin)
/* Return TRUE if bds->ref (spdi del) does not match assembly sequence at bds coords.
 * Sometimes the genome has an N and dbSNP has a more specific IUPAC character.
 * errAbort if there's some other kind of inconsistency. */
{
int refLen = strlen(bds->ref);
uint refStart = bds->chromStart;
uint refEnd = bds->chromEnd;
if (refLen != refEnd - refStart)
    errAbort("Inconsistent ref and coords for %s: ref is '%s' (%d bases) "
             "but position is %s|%d|%d (%d bases)",
             bds->name, bds->ref, refLen, bds->chrom, refStart, refEnd, refEnd - refStart);
char genomeRef[refLen+1];
seqWindowCopy(seqWin, refStart, refLen, genomeRef, sizeof genomeRef);
if (differentString(genomeRef, bds->ref))
    {
    if (strchr(genomeRef, 'N'))
        return TRUE;
    else
        errAbort("bds->ref '%s' mismatches genome sequence '%s'", bds->ref, genomeRef);
    }
return FALSE;
}

static boolean hasAmbiguousFreqAllele(struct sharedProps *props)
/* Return TRUE if any allele from any frequency source contains an IUPAC ambiguous character. */
{
int sIx;
for (sIx = 0;  sIx < props->freqSourceCount;  sIx++)
    {
    struct alObs *obs;
    for (obs = props->freqSourceObs[sIx];  obs != NULL;  obs = obs->next)
        if (anyIupac(obs->allele))
            return TRUE;
    }
return FALSE;
}

static boolean freqHasNonRefAltAllele(struct sharedProps *props, boolean isRc, struct bigDbSnp *bds)
/* Return TRUE if any allele from any frequency source does not match any ref/alt (SPDI) allele. */
{
int sIx;
for (sIx = 0;  sIx < props->freqSourceCount;  sIx++)
    {
    boolean freqIsRc = props->freqIsRc[sIx];
    struct alObs *obs;
    for (obs = props->freqSourceObs[sIx];  obs != NULL;  obs = obs->next)
        {
        int alLen = strlen(obs->allele);
        char alCpy[alLen+1];
        safecpy(alCpy, sizeof alCpy, obs->allele);
        if (freqIsRc != isRc)
            reverseComplement(alCpy, alLen);
        boolean foundIt = FALSE;
        if (sameString(alCpy, bds->ref))
            foundIt = TRUE;
        else
            {
            int i;
            for (i = 0;  i < bds->altCount;  i++)
                if (sameString(alCpy, bds->alts[i]))
                    {
                    foundIt = TRUE;
                    break;
                    }
            }
        if (!foundIt)
            return TRUE;
        }
    }
return FALSE;
}

static void addUcscNotes(struct sharedProps *props, boolean isRc, boolean isMultiMapper,
                         struct bigDbSnp *bds, struct seqWindow *seqWin,
                         struct dyString *dyUcscNotes, struct lm *lm)
/* Record interesting conditions if applicable. */
{
if (props->class != bigDbSnpClassFromAlleles(bds))
    dyStringAppend(dyUcscNotes, bdsClassMismatch ",");
if (props->clinVarAccs != NULL)
    dyStringAppend(dyUcscNotes, bdsClinvar ",");
addClinVarSigs(dyUcscNotes, props);
if (props->commonCount > 0)
    {
    dyStringAppend(dyUcscNotes, bdsCommonSome ",");
    if (props->rareCount == 0)
        dyStringAppend(dyUcscNotes, bdsCommonAll ",");
    else
        dyStringAppend(dyUcscNotes, bdsRareSome ",");
    }
else if (props->rareCount > 0 || props->freqSourceCount == 0)
    {
    dyStringAppend(dyUcscNotes, bdsRareSome ",");
    dyStringAppend(dyUcscNotes, bdsRareAll ",");
    }
if (props->freqIncomplete)
    dyStringAppend(dyUcscNotes, bdsFreqIncomplete ",");
if (props->freqNotMapped)
    dyStringAppend(dyUcscNotes, bdsFreqNotMapped ",");
if (isRc)
    dyStringAppend(dyUcscNotes, bdsRevStrand ",");
if (isMultiMapper)
    dyStringAppend(dyUcscNotes, bdsMultiMap ",");
checkRareRef(props, bds->ref, isRc, dyUcscNotes);
if (delMismatchesGenome(bds, seqWin))
    dyStringAppend(dyUcscNotes, bdsRefMismatch ",");
if (anyIupac(bds->ref))
    dyStringAppend(dyUcscNotes, bdsRefIsAmbiguous ",");
int i;
for (i = 0;  i < bds->altCount;  i++)
    if (anyIupac(bds->alts[i]))
        dyStringAppend(dyUcscNotes, bdsAltIsAmbiguous ",");
if (hasAmbiguousFreqAllele(props))
    dyStringAppend(dyUcscNotes, bdsFreqIsAmbiguous ",");
if (freqHasNonRefAltAllele(props, isRc, bds))
    dyStringAppend(dyUcscNotes, bdsFreqNotRefAlt ",");
bds->ucscNotes = lmCloneString(lm, dyUcscNotes->string);
}

static void calcShiftBases(struct bigDbSnp *bds)
/* Return the max length of uncertain placement from all alt alleles. */
{
int maxBasesTrimmed = 0;
int refLen = bds->chromEnd - bds->chromStart;
char refTrim[refLen+1];
int i;
for (i = 0;  i < bds->altCount;  i++)
    {
    // Make copies of ref and alt, trim the copies, measure the change.
    int refTrimLen = refLen;
    safecpy(refTrim, sizeof refTrim, bds->ref);
    int altTrimLen = strlen(bds->alts[i]);
    char altTrim[altTrimLen+1];
    safecpy(altTrim, sizeof altTrim, bds->alts[i]);
    uint start = bds->chromStart, end = bds->chromEnd;
    trimRefAlt(refTrim, altTrim, &start, &end, &refTrimLen, &altTrimLen);
    int basesTrimmed = refLen - refTrimLen;
    if (basesTrimmed > maxBasesTrimmed)
        maxBasesTrimmed = basesTrimmed;
    if (maxBasesTrimmed >= refLen)
        break;
    }
bds->shiftBases = min(refLen, maxBasesTrimmed);
}

static struct bigDbSnp *parsePlacement(struct jsonElement *pl, struct sharedProps *props,
                                       boolean isMultiMapper, struct dyString *dy, struct lm *lm)
/* Return a bigDbSnp with information about one placement (mapping). */
{
struct bigDbSnp *bds;
lmAllocVar(lm, bds);
struct slRef *allSpdiRef = jsonQueryElement(pl, "placement", "alleles[*].allele.spdi", lm);
bds->chrom = jsonQueryString(pl, "placement", "seq_id", lm);
if (bds->chrom == NULL)
    errAbort("Missing or NULL seq_id for %s", props->name);
struct spdiBed *spdiBList = parseSpdis(allSpdiRef, props->name, lm);
if (!sameString(bds->chrom, spdiBList->chrom))
    errAbort("seq_id mismatch: placement has '%s' but spdi has '%s for %s'",
             bds->chrom, spdiBList->chrom, props->name);
if (! spdiBedListSameRef(spdiBList, props->name))
    {
    // Bad coords/alleles for this placement (usually due to mapping bug) -- don't proceed.
    return NULL;
    }
bds->chromStart = spdiBList->chromStart;
bds->chromEnd = spdiBList->chromEnd;
bds->name = props->name;
bds->ref = spdiBList->del;
// spdiBList may or may not include no-change allele (del == ins)
lmAllocArray(lm, bds->alts, slCount(spdiBList));
struct spdiBed *spdiB;
for (spdiB = spdiBList;  spdiB != NULL;  spdiB = spdiB->next)
    {
    if (!sameString(bds->ref, spdiB->ins))
        bds->alts[bds->altCount++] = spdiB->ins;
    }
calcShiftBases(bds);
boolean isRc = jsonQueryBoolean(pl, "placement",
                                "placement_annot.is_aln_opposite_orientation", FALSE, lm);
dyStringClear(dy);
setBdsFreqData(props, isRc, bds, dy, lm);
bds->maxFuncImpact = props->maxFuncImpact;
bds->class = props->class;
struct seqWindow *seqWin = getChromSeq(ncToSeqWin, bds->chrom, ncToTwoBitChrom);
checkIndelPlacement(bds, seqWin, lm);
addUcscNotes(props, isRc, isMultiMapper, bds, seqWin, dy, lm);
return bds;
}

static void equivRegionAdd(struct hash *equivRegions, struct bed3 *region, struct bed3 *regionList)
/* Associate regionList with region by looking up the binKeeper for region->chrom in equivRegions
 * and adding regionList to the binKeeper for region start & end. */
{
struct binKeeper *bk = hashFindVal(equivRegions, region->chrom);
if (bk == NULL)
    {
    bk = binKeeperNew(0, BINRANGE_MAXEND_512M);
    hashAdd(equivRegions, region->chrom, bk);
    }
binKeeperAdd(bk, region->chromStart, region->chromEnd, regionList);
}

static struct hash *getEquivRegions()
/* If not done already, read -equivRegions file if specified and parse it into data structure
 * of equivalent regions for detecting legitimate cases of multiple mappings. */
{
static struct hash *equivRegions = NULL;
static boolean alreadyInited = FALSE;
if (!alreadyInited)
    {
    char *equivRegionsFile = optionVal("equivRegions", NULL);
    if (isNotEmpty(equivRegionsFile))
        {
        equivRegions = hashNew(0);
        struct lineFile *lf = lineFileOpen(equivRegionsFile, TRUE);
        char *line;
        while (lineFileNextReal(lf, &line))
            {
            // Build a list of equivalent regions; hash each region->chrom to list.
            char *words[128];
            int wordCount = chopLine(line, words);
            if (wordCount >= ArraySize(words))
                lineFileAbort(lf, "getEquivRegions: too many words per line (%d)", wordCount);
            struct bed3 *regionList = NULL;
            int i;
            for (i = 0;  i < wordCount;  i++)
                {
                char *chrom = cloneString(words[i]);
                char *colon = strchr(chrom, ':');
                if (colon)
                    {
                    char *startStr = colon+1;
                    char *nextColon = strchr(startStr, ':');
                    if (! nextColon)
                        lineFileAbort(lf, "getEquivRegions: expected each word to have "
                                      "0 or 2 colons, but got '%s'", words[i]);
                    *colon = '\0';
                    *nextColon = '\0';
                    char *endStr = nextColon+1;
                    if (!isAllDigits(startStr) || !isAllDigits(endStr))
                        lineFileAbort(lf, "getEquivRegions: expected a number following each colon, "
                                      "but got '%s'", words[i]);
                    int start = atoi(startStr);
                    int end = atoi(endStr);
                    slAddHead(&regionList, bed3New(chrom, start, end));
                    }
                else
                    slAddHead(&regionList, bed3New(chrom, 0, 0));
                }
            // Sort so that a main chromosome goes before alts/fixes.
            slSort(&regionList, bedCmp);
            // Associate all regions with the same list.  we can use pointer equality later.
            struct bed3 *region;
            for (region = regionList;  region != NULL;  region = region->next)
                equivRegionAdd(equivRegions, region, regionList);
            }
        lineFileClose(&lf);
        }
    else
        warn("-equivRegions=FILE not specified; will not be able to detect variants "
             "that are mapped to multiple non-equivalent regions.");
    alreadyInited = TRUE;
    }
return equivRegions;
}

static struct slRef *equivRegionFind(struct hash *equivRegions, struct bed3 *region,
                                     struct lm *lm)
/* Return ref list of equivalent region lists that overlap region. */
{
struct binElement *beList = NULL;
struct binKeeper *bk = hashFindVal(equivRegions, region->chrom);
if (bk)
    {
    int start = region->chromStart;
    int end = region->chromEnd;
    // binKeeperFind can't find overlap with 0-length insertions (start == end) so expand those:
    if (start == end)
        {
        start--;
        end++;
        }
    beList = binKeeperFind(bk, start, end);
    }
struct slRef *refList = NULL;
struct binElement *binEl;
for (binEl = beList;  binEl != NULL;  binEl = binEl->next)
    lmRefAdd(lm, &refList, binEl->val);
slReverse(&refList);
slFreeList(&beList);
return refList;
}

static struct bed3 *bed3FromPlRef(struct slRef *plRef, struct lm *lm)
/* Extract a bed3 from the first SPDI found in the first placement in plRef. */
{
struct jsonElement *pl = plRef->val;
struct slRef *spdiRef = jsonQueryElement(pl, "placement", "alleles[0].allele.spdi", lm);
struct jsonElement *spdiEl = spdiRef->val;
struct bed3 *region;
lmAllocVar(lm, region);
region->chrom = jsonQueryString(spdiEl, "first spdi", "seq_id", lm);
region->chromStart = jsonQueryInt(spdiEl, "first spdi", "position", -1, lm);
char *ref = jsonQueryString(spdiEl, "first spdi", "deleted_sequence", lm);
region->chromEnd = region->chromStart + strlen(ref);
return region;
}

static struct bed3 *bed3ListFromPlRef(struct slRef *plRefList, struct lm *lm)
/* Return a list of bed3 derived from placement SPDIs in plRef. */
{
struct bed3 *list = NULL;
struct slRef *plRef;
for (plRef = plRefList;  plRef != NULL;  plRef = plRef->next)
    slAddHead(&list, bed3FromPlRef(plRef, lm));
slReverse(&list);
return list;
}

static boolean uniqChroms(struct bed3 *bedList)
/* Return TRUE if each item of *sorted* bedList has a different chrom. */
{
boolean uniq = TRUE;
if (bedList != NULL)
    {
    struct bed3 *bed;
    for (bed = bedList;  bed->next != NULL;  bed = bed->next)
        if (sameString(bed->chrom, bed->next->chrom))
            {
            uniq = FALSE;
            break;
            }
    }
return uniq;
}

static boolean allMappingsHaveMatch(struct slRef *equivRegionsA[], struct slRef *equivRegionsB[],
                                    int plCount)
// Return true if each placement's list of equivRegion sets in A has at least one set that
// matches at least one set from some other placement's list in B.  A and B may/may not be the same.
{
int i;
for (i = 0;  i < plCount;  i++)
    {
    struct slRef *plEquivList = equivRegionsA[i];
    boolean foundMatch = FALSE;
    int j;
    for (j = 0;  j < plCount;  j++)
        {
        if (j == i)
            continue;
        struct slRef *otherEquivList = equivRegionsB[j];
        struct slRef *plEquiv;
        for (plEquiv = plEquivList;  plEquiv != NULL;  plEquiv = plEquiv->next)
            {
            struct slRef *otherEquiv;
            for (otherEquiv = otherEquivList;  otherEquiv;  otherEquiv = otherEquiv->next)
                {
                // We can use pointer equality because getEquivRegions assigns same list
                // pointer to each region in list.
                if (plEquiv->val == otherEquiv->val)
                    {
                    foundMatch = TRUE;
                    break;
                    }
                }
            }
        }
    if (! foundMatch)
        return FALSE;
    }
return TRUE;
}

static boolean detectMultiMappers(struct slRef *placements, struct lm *lm)
/* Return TRUE if placements map to more than one genomic locus, aside from legitimate cases
 * like the PAR regions on X and Y, or alt/fix sequences and their corresponding chromosomal
 * locations. */
{
struct hash *equivRegions = getEquivRegions();
int plCount = slCount(placements);
if (equivRegions && plCount > 1)
    {
    // Get a bed3 list of placements and make sure there aren't multiple mappings on the same chrom.
    struct bed3 *bedList = bed3ListFromPlRef(placements, lm);
    slSort(&bedList, bedCmp);
    if (!uniqChroms(bedList))
        return TRUE;
    // Gather up equivRegions lists for all placements.  If any placement has no equivRegions,
    // then we're done.
    struct slRef *plEquivRegions[plCount];
    struct bed3 *bed;
    int i;
    for (i = 0, bed = bedList;  bed != NULL;  bed = bed->next, i++)
        {
        struct slRef *listOfLists = equivRegionFind(equivRegions, bed, lm);
        if (listOfLists == NULL)
            return TRUE;
        plEquivRegions[i] = listOfLists;
        }
    if (!allMappingsHaveMatch(plEquivRegions, plEquivRegions, plCount))
        {
        // Sometimes the test for equivalent regions fails because all regions point to a
        // common location -- but that location does not have a mapping, so its suite of
        // regions are not in the mix.  For example, a variant is mapped to 4 alts of the
        // same chrom -- but not to the chrom itself, so all regions point to the main chrom,
        // but none of the 4 mappings' regions are in common with each other.
        // So try harder before giving up: add equivalent regions that overlap original equivs.
        struct slRef *plExpandedEquivs[plCount];
        for (i = 0, bed = bedList;  bed != NULL;  bed = bed->next, i++)
            {
            struct slRef *expandedEquivs = NULL;
            struct slRef *plEquiv;
            for (plEquiv = plEquivRegions[i];  plEquiv != NULL;  plEquiv = plEquiv->next)
                {
                struct slRef *setRef;
                for (setRef = plEquiv;  setRef != NULL;  setRef = setRef->next)
                    {
                    struct bed3 *set = setRef->val;
                    struct bed3 *region;
                    for (region = set;  region != NULL;  region = region->next)
                        {
                        if (differentString(bed->chrom, region->chrom))
                            expandedEquivs = slCat(equivRegionFind(equivRegions, region, lm),
                                                   expandedEquivs);
                        }
                    }
                }
            plExpandedEquivs[i] = expandedEquivs;
            }
        if (!allMappingsHaveMatch(plEquivRegions, plExpandedEquivs, plCount))
            return TRUE;
        }
    }
return FALSE;
}

static void dyStringPrintSlNameList(struct dyString *dy, struct slName *list, char *sep)
/* Append list of names with separator to dy; separator appears after every item. Skip empty names. */
{
struct slName *item;
for (item = list;  item != NULL;  item = item->next)
    {
    if (isNotEmpty(item->name))
        {
        dyStringAppend(dy, item->name);
        dyStringAppend(dy, sep);
        }
    }
}

static void dyStringPrintSlIntList(struct dyString *dy, struct slInt *list, char *sep)
/* Append list of names with separator to dy; separator appears after every item. */
{
struct slInt *item;
for (item = list;  item != NULL;  item = item->next)
    {
    dyStringPrintf(dy, "%d", item->val);
    dyStringAppend(dy, sep);
    }
}

static void appendFrequencies(struct sharedProps *props, struct dyString *dy)
/* Condense the frequency data in props into an autoSql array of compact strings and append to dy.
 * Also add autoSql int array of total_count because those may vary between variants. */
{
dyStringPrintf(dy, "%d\t", props->freqSourceCount);
int ix;
for (ix = 0;  ix < props->freqSourceCount;  ix++)
    {
    boolean freqIsRc = props->freqIsRc[ix];
    struct alObs *obs;
    for (obs = props->freqSourceObs[ix];  obs != NULL;  obs = obs->next)
        {
        if (obs != props->freqSourceObs[ix])
            dyStringAppendC(dy, '|');
        if (freqIsRc)
            // Store all frequencies on + strand of "preferred top level placement"
            reverseComplement(obs->allele, strlen(obs->allele));
        dyStringPrintf(dy, "%s:%d", obs->allele, obs->obsCount);
        if (freqIsRc)
            // rc again to restore original values
            reverseComplement(obs->allele, strlen(obs->allele));
        }
    dyStringAppendC(dy, ',');
    }
dyStringAppendC(dy, '\t');
for (ix = 0;  ix < props->freqSourceCount;  ix++)
    {
    if (props->freqSourceObs[ix] != NULL)
        dyStringPrintf(dy, "%d,", props->freqSourceObs[ix]->totalCount);
    else
        dyStringAppend(dy, "0,");
    }
}

static void writeDetails(struct sharedProps *props, struct dyString *dy, FILE *outPropsF)
/* Write props out as tab-sep line to outPropsF. */
{
dyStringClear(dy);
dyStringPrintf(dy, "%s\t", props->name);
appendFrequencies(props, dy);
dyStringPrintf(dy, "\t%d\t", slCount(props->soTerms));
dyStringPrintSlIntList(dy, props->soTerms, ",");
dyStringPrintf(dy, "\t%d\t", slCount(props->clinVarAccs));
dyStringPrintSlNameList(dy, props->clinVarAccs, ",");
dyStringAppendC(dy, '\t');
dyStringPrintSlNameList(dy, props->clinVarSigs, ",");

assert(slCount(props->clinVarAccs) == slCount(props->clinVarSigs));

dyStringPrintf(dy, "\t%d\t", slCount(props->submitters));
dyStringPrintSlNameList(dy, props->submitters, ",");
dyStringPrintf(dy, "\t%d\t", slCount(props->pubMedIds));
dyStringPrintSlIntList(dy, props->pubMedIds, ",");
dyStringAppendC(dy, '\n');
fputs(dy->string, outPropsF);
}

static void updateSeqIsRc(struct slRef *placements, struct lm *lm)
/* Note is_aln_opposite_orientation flag for each mapped sequence, regardless of whether it's
 * in the assembly that we're working on; later we can look up sequences from freq reports. */
{
hashIntReset(seqIsRc);
struct slRef *plRef;
for (plRef = placements;  plRef != NULL;  plRef = plRef->next)
    {
    char *seqId = jsonQueryString(plRef->val, "placement", "seq_id", lm);
    boolean isRc = jsonQueryBoolean(plRef->val, "placement",
                                    "placement_annot.is_aln_opposite_orientation", FALSE, lm);
    struct hashEl *hel = hashLookup(seqIsRc, seqId);
    if (hel)
        {
        if (isRc != ptToInt(hel->val))
            errAbort("updateSeqIsRc: sequence '%s' has both true and false for "
                     "placement_annot.is_aln_opposite_orientation", seqId);
        }
    else
        hashAddInt(seqIsRc, seqId, isRc);
    }
}

struct perAssembly
// For each assembly whose mappings we're extracting from JSON, we need the name,
// a jsonQuery path to extract just the mappings for that assembly, and an output file.
    {
    struct perAssembly *next;
    char *name;                  // assembly_name e.g. GRCh38.p12
    char *placementPath;         // jsonQuery path to placements on assembly
    char *plIsTopLevelPath;      // jsonQuery path to find whether placement is top level in assembly
    FILE *outF;                  // bigDbSnp output file
    FILE *outBad;                // buggy coords BED4 output file
    };

static void perAssemblyFree(struct perAssembly **pPa)
/* Close *pPa's outF and free mem. */
{
if (pPa)
    {
    struct perAssembly *pa = *pPa;
    freeMem(pa->name);
    carefulClose(&pa->outF);
    freeMem(pa->placementPath);
    freeMem(pa->plIsTopLevelPath);
    freez(pPa);
    }
}

static void perAssemblyFreeList(struct perAssembly **pPaList)
/* Close & free a list of perAssembly. */
{
if (pPaList)
    {
    struct perAssembly *el, *next;
    for (el = *pPaList;  el != NULL;  el = next)
        {
        next = el->next;
        perAssemblyFree(&el);
        }
    }
}

static void writeMerged(struct jsonElement *top, FILE *outMergedF, struct lm *lm)
/* If any merged IDs are given, write the mappings out to outMergedF. */
{
struct slName *mergedIds = jsonQueryStrings(top, "top", "dbsnp1_merges[*].merged_rsid", lm);
if (mergedIds)
    {
    char *rsId = jsonQueryString(top, "top", "refsnp_id", lm);
    struct slName *mergedId;
    for (mergedId = mergedIds;  mergedId != NULL;  mergedId = mergedId->next)
        fprintf(outMergedF, "rs%s\trs%s\n", mergedId->name, rsId);
    }
}

static char *getChrom(char *refSeqAcc, char *assembly)
/* Look up "refSeqAcc assembly" in ncGrcToChrom; if found, return chrom, otherwise return
 * refSeqAcc for translation later (e.g. NC_012920.1 liftOver to hg19 chrM).  Don't free ret. */
{
char ncGrc[strlen(refSeqAcc) + 1 + strlen(assembly) + 1];
safef(ncGrc, sizeof ncGrc, "%s %s", refSeqAcc, assembly);
char *chrom = hashFindVal(ncGrcToChrom, ncGrc);
return chrom ? chrom : refSeqAcc;
}

static void writeBadCoords(struct jsonElement *pl, char *rsId, char *assembly, FILE *outBad,
                           struct lm *lm)
/* Write BED4 with range encompassing inconsistent SPDI coords. */
{
struct slRef *allSpdiRef = jsonQueryElement(pl, "placement", "alleles[*].allele.spdi", lm);
struct spdiBed *spdiBList = parseSpdis(allSpdiRef, rsId, lm);
char *chrom = getChrom(spdiBList->chrom, assembly);
uint minChromStart = BIGNUM;
uint maxChromEnd = 0;
struct spdiBed *spdiB;
for (spdiB = spdiBList;  spdiB != NULL;  spdiB = spdiB->next)
    {
    if (spdiB->chromStart < minChromStart)
        minChromStart = spdiB->chromStart;
    if (spdiB->chromEnd > maxChromEnd)
        maxChromEnd = spdiB->chromEnd;
    }
fprintf(outBad, "%s\t%u\t%u\t%s\n", chrom, minChromStart, maxChromEnd, rsId);
}

static void parseDbSnpJson(char *line, struct perAssembly *assemblyProps,
                           struct outStreams *outStreams)
/* Each line of dbSNP's file contains one JSON blob describing an rs# variant.  Parse JSON,
 * extract the bits that we want to keep, print out BED+ and accompanying files. */
{
struct lm *lm = lmInit(1<<16);
struct jsonElement *top = jsonParseLm(line, lm);
struct sharedProps *props = NULL;
struct bigDbSnp *bds = NULL;
struct slRef *allPlacements = jsonQueryElement(top, "top",
                                               "primary_snapshot_data.placements_with_allele[*]",
                                               lm);
updateSeqIsRc(allPlacements, lm);
struct dyString *dyScratch = dyStringNew(0);
struct perAssembly *ap;
for (ap = assemblyProps;  ap != NULL;  ap = ap->next)
    {
    struct slRef *placements = jsonQueryElement(top, "top", ap->placementPath, lm);
    // Filter out scaffolds that are independent in some other assembly, but part of a regular
    // chrom in the current assembly
    struct slRef *plRef, *plRefNext, *filtered = NULL;
    for (plRef = placements;  plRef != NULL;  plRef = plRefNext)
        {
        plRefNext = plRef->next;
        struct jsonElement *pl = plRef->val;
        boolean isTopLevel = jsonQueryBoolean(pl, "placement", ap->plIsTopLevelPath, FALSE, lm);
        if (isTopLevel)
            slAddHead(&filtered, plRef);
        }
    slReverse(&filtered);
    placements = filtered;
    boolean multiMapper = detectMultiMappers(placements, lm);
    struct errCatch *errCatch = errCatchNew();
//#*** NOTE: currently in some cases the alleles are incorrect on alt sequences where the
//#*** alt has a different allele of the same variant (or just different sequence).
//#*** We detect at least some of those by inconsistent pos/del in alt placement spdis,
//#*** but who knows, there could be more.  Ultimately we will need to compare assembly
//#*** sequences... unless/until dbSNP fixes their projection onto alts (JIRA VR-176).
    if (errCatchStart(errCatch))
        {
        for (plRef = placements;  plRef != NULL;  plRef = plRef->next)
            {
            struct jsonElement *pl = plRef->val;
            if (props == NULL)
                {
                // Write props only if we have at least one placement.
                props = extractProps(top, lm);
                writeDetails(props, dyScratch, outStreams->details);
                writeMerged(top, outStreams->merged, lm);
                }
            bds = parsePlacement(pl, props, multiMapper, dyScratch, lm);
            if (bds)
                {
                bds->chrom = getChrom(bds->chrom, ap->name);
                bigDbSnpTabOut(bds, ap->outF);
                // Prevent stale bds in errCatch below:
                bds = NULL;
                }
            else
                {
                writeBadCoords(pl, props->name, ap->name, ap->outBad, lm);
                }
            }
        }
    errCatchEnd(errCatch);
    char *rsId = jsonQueryString(top, "top", "refsnp_id", lm);
    char *seqId = bds ? bds->chrom : "NOSEQ";
    if (errCatch->gotError)
        {
        fprintf(outStreams->err, "%s\t%s\t%s", rsId, seqId, errCatch->message->string);
        }
    else if (isNotEmpty(errCatch->message->string))
	{
        fprintf(outStreams->warn, "%s\t%s\t%s", rsId, seqId, errCatch->message->string);
        }
    errCatchFree(&errCatch);
    }
dyStringFree(&dyScratch);
lmCleanup(&lm);
}

static struct slName *initFreqSourceOrder()
/* If -freqSourceOrder option was given, extract source names from the comma-sep list
 * into globals freqSourceCount and freqSourceOrder[]. */
{
char *freqSourceOrderStr = optionVal("freqSourceOrder", NULL);
struct slName *sources = NULL;
if (freqSourceOrderStr)
    {
    sources = slNameListFromComma(freqSourceOrderStr);
    freqSourceCount = slCount(sources);
    AllocArray(freqSourceOrder, freqSourceCount);
    struct slName *source;
    int i;
    for (i = 0, source = sources;  i < freqSourceCount;  i++, source = source->next)
        freqSourceOrder[i] = source->name;
    }
return sources;
}

static struct perAssembly *getAssemblyProps(char *assemblyList, char *outRoot)
/* Split comma-sep assemblyList and make placement path and bigDbSnp output file handle for each. */
{
struct perAssembly *assemblyProps = NULL;
struct slName *assembly, *assemblies = slNameListFromComma(assemblyList);
for (assembly = assemblies;  assembly != NULL;  assembly = assembly->next)
    {
    struct perAssembly *ap;
    AllocVar(ap);
    ap->name = cloneString(assembly->name);
    // Fancy path to get all placements that are on the desired assembly.
    struct dyString *dy = dyStringCreate("primary_snapshot_data.placements_with_allele"
                                  "[placement_annot.seq_id_traits_by_assembly[*].assembly_name=%s]",
                                         assembly->name);
    ap->placementPath = dyStringCannibalize(&dy);
    dy = dyStringCreate("placement_annot.seq_id_traits_by_assembly[assembly_name=%s].is_top_level",
                        assembly->name);
    ap->plIsTopLevelPath = dyStringCannibalize(&dy);
    char prefix[2048];
    safef(prefix, sizeof prefix, "%s.%s", outRoot, assembly->name);
    ap->outF = openOutFile("%s.bigDbSnp", prefix);
    ap->outBad = openOutFile("%s.badCoords.bed", prefix);
    slAddHead(&assemblyProps, ap);
    }
return assemblyProps;
}

static void parseRefSeqToUcsc(char *refSeqToUcsc, struct hash *ncGrcToChrom,
                              struct hash *ncToTwoBitChrom)
/* Extract mappings of {NC_*, GRC*} to chrom and NC_* to {twoBitFile, chrom}. */
{
struct lineFile *lf = lineFileOpen(refSeqToUcsc, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *row[5];
    int wordCount = chopTabs(line, row);
    lineFileExpectWords(lf, 4, wordCount);
    char *acc = row[0];
    char *assembly = row[1];
    char *twoBitFile = row[2];
    char *chrom = row[3];
    if (!regexMatch(acc, "^N[A-Z]_[0-9]+\\.[0-9]+$"))
        lineFileAbort(lf, "'%s' does not look like a RefSeq sequence accession "
                      "(expecting N*_number.number in first column)", acc);
    if (!startsWith("GRCh", assembly))
        lineFileAbort(lf, "'%s' does not look like a GRCh* assembly name "
                      "(expecting GRCh* in second column", assembly);
    if (!endsWith(twoBitFile, ".2bit"))
        lineFileAbort(lf, "'%s' does not look like a twoBit file "
                      "(expecting *.2bit in third column)", twoBitFile);
    if (!regexMatch(chrom, "^chr[0-9A-Za-z_]+$"))
        lineFileAbort(lf, "'%s' does not look like an hg* chrom name "
                      "(expecting 'chr' + alphanumeric/_ in fourth column)", chrom);
    char accAssembly[strlen(acc) + 1 + strlen(assembly) + 1];
    safef(accAssembly, sizeof accAssembly, "%s %s", acc, assembly);
    hashAdd(ncGrcToChrom, accAssembly, cloneString(chrom));
    char twoBitChrom[strlen(twoBitFile) + 1 + strlen(chrom) + 1];
    safef(twoBitChrom, sizeof twoBitChrom, "%s:%s", twoBitFile, chrom);
    hashAdd(ncToTwoBitChrom, acc, cloneString(twoBitChrom));
    }
lineFileClose(&lf);
}

void dbSnpJsonToTab(char *assemblyList, char *refSeqToUcsc, char *jsonFile, char *outRoot)
/* dbSnpJsonToTab - Extract fields from dbSNP JSON files, write out as tab-separated BED+. */
{
struct slName *sources = initFreqSourceOrder();
struct lineFile *lf = lineFileOpen(jsonFile, TRUE);
struct outStreams *outStreams = outStreamsOpen(outRoot);
struct perAssembly *assemblyProps = getAssemblyProps(assemblyList, outRoot);
seqIsRc = hashNew(0);
ncGrcToChrom = hashNew(0);
ncToTwoBitChrom = hashNew(0);
ncToSeqWin = hashNew(0);
parseRefSeqToUcsc(refSeqToUcsc, ncGrcToChrom, ncToTwoBitChrom);
char *line = NULL;
while (lineFileNext(lf, &line, NULL))
    parseDbSnpJson(line, assemblyProps, outStreams);
lineFileClose(&lf);
perAssemblyFreeList(&assemblyProps);
outStreamsClose(&outStreams);
// Free these up for squeaky-clean valgrind --leak-check
// [well, except for parseOptions and errCatch getStack/getThreadVars, but whatever]
hashFree(&seqIsRc);
hashFree(&ncGrcToChrom);
hashFree(&ncToTwoBitChrom);
hashFree(&ncToSeqWin);
slNameFreeList(&sources);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
dbSnpJsonToTab(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

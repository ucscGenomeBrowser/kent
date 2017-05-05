// geneAlleles - Find gene alleles (haplotypes) from the 1000 Genomes data

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "vcfBits.h"
#include "pgSnp.h"
#include "jsHelper.h"
#include "hPrint.h"
#include "haplotypes.h"

// Some helpful defines
#define DIVIDE_SAFE(num, denom, ifZero) ((denom) > 0 ? ((double)(num) / (denom)) : (ifZero) )
#define CALC_MEAN(sum,  N)      DIVIDE_SAFE(sum, N, 0)
#define CALC_VAR( sumSq,N)      DIVIDE_SAFE(sumSq, (N - 1), 0)
#define CALC_SD(  sumSq,N)      sqrt( CALC_VAR(sumSq,N) )


struct haploExtras *haplotypeExtrasDefault(char *db,int lmSize)
// Return a haploExtras support structure with all defaults set.  It will include
// local memory (lm) out of which almost all haplotype memory will be allocated.
// When done with haplotypes, free everything with haplotypeExtraFree().
{
struct haploExtras *he = NULL;

if (lmSize <= 0)
    lmSize = (1024 * 1024);
struct lm *lm = lmInit(lmSize);

lmAllocVar(lm,he);
he->lm = lm;

he->db = db;
if (differentString(he->db,"hg19"))
    errAbort("Requested '%s', but currently only 'hg19' is supported.", he->db);

he->chrom = NULL;
he->chromStart = 0;
he->chromEnd = 0;

// Alternate table to UCSC knownGenes?
he->geneTable = lmCloneString(lm,HAPLO_GENES_TABLE);
he->canonical = FALSE;

// input must be a vcf file that contains data for chrom
he->vcfTrack = lmCloneString(lm,HAPLO_1000_GENOMES_TRACK);
he->readBed = FALSE;

he->justStats  = FALSE;

he->outTableOrFile = NULL;
he->outToFile = FALSE;

he->readable   = FALSE;
he->justModel  = NULL;
he->modelLimit = 0;
he->homozygousOnly = FALSE;      // Report non-homozygous alleles too
he->synonymous     = FALSE;      // ignore synonymous by default
he->variantMinPct  = HAPLO_VARIANT_MIN_PCT_DEFAULT;

// Subjects, Populations or Commonalities
he->populationsToo   = FALSE;  // Show populations instead of subjects
he->populationsMinor = FALSE;  // Show commonality populations instead of subjects
he->populationMinPct = 0;     // Only list populations with this minimum percentage of penetration

// Other options into support struct
he->append = FALSE;        // Create or append to existing table/outFile

he->growTree  = TRUE;      // Haplotypes are related via elmTree and named accordingly
he->test      = FALSE;

he->tmpDir    = NULL;

return he;
}

void haplotypeExtrasFree(struct haploExtras **pHe)
// Releases a haploExtras structure which will release the lm that contains almost all
// haplotype stuctures and values obtained with this token.
// WARNING: many pointers are rendered obsolete after this call.  Only do it at end of processing.
{
struct haploExtras *he = *pHe;
if (he->conn != NULL)
    hFreeConn(&(he->conn));

struct lm *lm = he->lm; // Must divorce pointer here because he is allocated from lm!
lmCleanup(&lm);
*pHe = NULL;
}

char *haplotypesDiscoverVcfFile(struct haploExtras *he, char *chrom)
// Discovers, if necessary and returns the VCF File to use
// This function changes the he->inFile attribute and returns it, too
{
if (he->conn == NULL)
    he->conn = hAllocConn(he->db);

he->inFile = NULL;
if (he->vcfTrack != NULL)
    {
    struct trackDb *tdb = hTrackDbForTrackAndAncestors(he->db, he->vcfTrack);
    if (tdb != NULL)
        {
        char *vcfFileName = bbiNameFromSettingOrTableChrom(tdb, he->conn, tdb->table, chrom);
        he->inFile = hReplaceGbdb(vcfFileName); // XX very small memory leak?
        }
    }

return he->inFile;
}


// -----------------------------------
// haplotype variants support routines
// -----------------------------------
int variantCmp(const void *va, const void *vb)
// Compare to sort variants back to sequence order
{
const struct hapVar *a = *((struct hapVar **)va);
const struct hapVar *b = *((struct hapVar **)vb);
int ret = (a->chromStart - b->chromStart);
if (ret == 0) // sort deterministically
    ret = (a->chromEnd - b->chromEnd);

return ret;
}

static struct hapVar *variantFill(struct haploExtras *he, struct variantBits *vBits,
                                  unsigned boundStart, unsigned boundEnd, char *strand,
                                  unsigned char alleleIx)
// Fill one variant struct from a vcf record
{
struct vcfRecord *record = vBits->record;
assert(alleleIx < record->alleleCount);  // Note 0 is reference so should never be seen

// Has one already been created and cached?
if (vBits->variants != NULL)
    {
    if (vBits->variants[alleleIx] != NULL)
        return vBits->variants[alleleIx];
    }
else
    vBits->variants = lmAlloc(he->lm,sizeof(void *) * record->alleleCount);

// Have to create one then
struct hapVar *variant;
lmAllocVar(he->lm,variant);
static char *emptyString = "";

int occurs = vcfVariantBitsAlleleOccurs(vBits,alleleIx,FALSE);
variant->frequency = ((double)occurs / vBitsSubjectChromCount(vBits));
variant->vType      = vBits->vType;
variant->chromStart = max(record->chromStart,boundStart);
variant->chromEnd   = min(boundEnd,record->chromEnd);
variant->origStart  = record->chromStart; // UI needs original boundaries
variant->origEnd    = record->chromEnd;
variant->reversed   = FALSE;
if (variant->chromStart >= variant->chromEnd)
    errAbort("%s:(%d)-(%d) bound by %d %d has negative length.",
             record->name,record->chromStart,record->chromEnd,boundStart,boundEnd);
if (variant->vType != vtSNP)  // Clips first base (1000Genomes includes prev base on insert/delete)
    variant->chromStart = max(record->chromStart + 1,boundStart);

if (record->name)
    variant->id     = lmCloneString(he->lm,record->name);
if (record->alleles && record->alleles[alleleIx] != NULL)
    {
    // Make some adjustments for bounds
    char *val = record->alleles[alleleIx];
    if (variant->vType == vtDeletion)
        {
        if (sameWord(val,"<DEL>"))
            val = emptyString; // ref allele will be len=2 so don't always set to empty string
        else
            {
            if(alleleIx == 0 && strlen(val) <= 1) // The reference
                { // Terrible secret of vcf 1000Genomes deletions. ref does not have sequence.
                  // Here we fill in with a taste of the deleted seq (ref seq);
                int end = min(variant->chromEnd,variant->chromStart + 10);
                if (he->justModel != NULL)   // UI version shows just one model at a time
                    end = variant->chromEnd; // If single model go ahead and spend the memory
                struct dnaSeq *dnaSeq = hDnaFromSeq(he->db, he->chrom,
                                                    variant->chromStart + 1, end,dnaUpper);
                if (dnaSeq != NULL)
                    {
                    val = lmCloneString(he->lm,dnaSeq->dna);
                    freeMem(dnaSeq);
                    }
                //variant->chromStart++; // Clips first base  (Not needed)
                }
            else
                val++; // Clips first base (1000Genomes includes prev base on insert/delete)
            }
        }
    else if (variant->vType == vtInsertion)
        val++; // Clips first base (1000Genomes includes prev base on insert/delete)

    int len = strlen(val);
    int clipFront = (boundStart - variant->chromStart);
    if (clipFront > 0
    && (variant->vType != vtInsertion || strand[0] == '+')) // insertion sequence depend on strand
        {
        val += clipFront;
        len -= clipFront;
        }
    if (variant->chromEnd > boundEnd
    &&  (variant->vType != vtInsertion || strand[0] == '-')) // insertion sequence depend on strand
        len -= (variant->chromEnd - boundEnd);
    if (len > 0)
        variant->val = lmCloneStringZ(he->lm,val,len);
    else
        variant->val = emptyString;
    }

vBits->variants[alleleIx] = variant;
return variant;
}

static void variantListsReorganize(struct variantBits *vBitsList)
// During construction variant lists are always stored as arrays, because they are shared
// memory between haplotypes.  When construction is finished, the ref haplotype will
// will have an array of ref variants but each variant will be the head of an slList
// of all variant alleles at that location
{
struct variantBits *vBits = vBitsList;
for ( ; vBits != NULL; vBits = vBits->next)
    {
    if (vBits->variants == NULL)
        continue;

    struct hapVar *variantAlleles = NULL;
    int ix = vBits->record->alleleCount - 1;
    for ( ; ix >= 0; ix--)
        {
        assert(vBits->variants[ix] != NULL);
        slAddHead(&variantAlleles,vBits->variants[ix]);
        }
    // The head is now the reference allele ix == 0 !
    }
}


#define HAPLOTYPE_SUFFIX_SZ 8

static char *suffixLetter(int order)
// Returns a letter in the order of a-zA-Z, but if the value exceeds 52
// returns number surround by parens: "(53)" 
{
if (order == 0)
    return "";

static char buf[HAPLOTYPE_SUFFIX_SZ];
if (order > 52)
    safef(buf,HAPLOTYPE_SUFFIX_SZ,"(%d)",order);
else
    {
    char *p = buf;
    if (order <= 26)
        *p++ = 'a' + (order - 1);
    else //if (order <= 52)
        *p++ = 'A' + (order - 27);

    *p++ = '\0';
    }
return buf;
}

/* No longer terribly interested in tree dimensions

static int suffixWidth(char *suffix)
// Returns the width of the first generation found in the suffix string
{
if (suffix == NULL)
    return 0;
if (*suffix != '(')
    {
    int letterIx = *suffix - 'A';
    if (letterIx < 26)
        return (27 + letterIx);
    else
        {
        letterIx = *suffix - 'a';
        return (1 + letterIx);
        }
    }
char buf[HAPLOTYPE_SUFFIX_SZ];
char *p = buf;
char *q = suffix + 1;
while (*q != ')')
    *p++ = *q++;
*p = '\0';
return atoi(buf);
}
*/

static int suffixDepth(char *suffix)
// Returns the depth (generations) found in the suffix string
{
if (suffix == NULL)
    return 0;
if (strchr(suffix,'(') == NULL)
    return strlen(suffix);
else //what?
    {
    int depth = 0;
    char *p = suffix;
    for ( ; *p != '\0'; p++)
        {
        depth++;
        if (*p == '(')
            {
            for (++p; *p != '\0' && *p != ')'; p++)
                ;
            }
        }
    return depth;
    }
}


// --------------------------------
// Lower level haplotypes functions
// --------------------------------
static int homozygousSubjects(char *subjectIds)
// Returns the count of subjects that are either haploid or have both haploid ids represented
// in subject string.  Note, subjectIds are of format "id1-a,id1-b,id2,id3-b"
// and in this example id1 and id2 are both counted as "homozygous" returning 2.
{
// NOTE: does work for chrX even on PAR regions as male IDs will not be appended with'-a'
//       in non-PAR regions but will have '-a' and '-b' in PAR regions.
int homozygous = 0;
struct slName *id, *ids = slNameListFromComma(subjectIds); // Does not cannibalize memory
while ((id = slPopHead(&ids)) != NULL)
    {
    if (id->name[0] != '\0' ) // Not already discovered as part of a pair
        {
        char *p = strchr(id->name,'-');
        if (p == NULL)
            homozygous++;
        else
            {
            p++;
                 if (*p == 'a')
                     *p =  'b';
            else if (*p == 'b')
                     *p =  'a';
            struct slName *paired = slNameFind(ids, id->name);
            if (paired != NULL)
                {
                homozygous++;
                paired->name[0] = '\n';
                }
            }
        }
    slNameFree(id);
    }
return homozygous;
}

#define POP_MAJOR   "majorGroup"
#define POP_MINOR   "minorGroup"
#define POP_TABLE   "thousandGenomeSubjects"
#define POP_GROUPS_TABLE "thousandGenomeGroups"
#define POP_NAME  0
#define POP_COUNT 1
#define POP_FEMS 2

struct popGroup
// population group struct
    {
    struct popGroup *next;
    char *name;
    char *desc;
    int subjectN;
    int females;  // males are subjects - females obviously.
    int chromN;   // Depends upon chrom and location
    };

static struct popGroup *popGroupsGet(struct haploExtras *he,struct haplotypeSet *hapSet,
                                     boolean minorPopulations)
// Returns popGroup names from thousand Genomes
{
struct popGroup *popGroups = NULL;

// Descending order so list will not need to be reversed.
#define POP_NAME_QUERY "select name, subjects, females, description from " \
                       POP_GROUPS_TABLE " where type = '%s' order by name desc"
#define POP_DESC 3
char *type = (minorPopulations ? "minor" : "major");
char buf[256];
sqlSafef(buf,sizeof(buf),POP_NAME_QUERY,type);

if (he->conn == NULL)
    he->conn = hAllocConn(he->db);

struct sqlResult *sr = sqlGetResult(he->conn, buf);
char **row = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct popGroup *popGroup;
    lmAllocVar(he->lm,popGroup);
    popGroup->name = lmCloneString(he->lm,row[POP_NAME]);
    popGroup->desc = lmCloneString(he->lm,row[POP_DESC]);
    popGroup->subjectN = sqlSigned(row[POP_COUNT]);
    popGroup->females  = sqlSigned(row[POP_FEMS]);
    if (hapSet->chromN == (hapSet->subjectN * 2))      // autosomes and PAR chrX
        popGroup->chromN = (popGroup->subjectN * 2);  // Diploid
    else if (lastChar(hapSet->chrom) == 'X')                        // non-PAR chrX
        popGroup->chromN = popGroup->subjectN + popGroup->females;  // females are dipliod
    else                                                         // chrY
        {
        popGroup->subjectN -= popGroup->females;
        popGroup->chromN    = popGroup->subjectN;                // haploid
        }

    slAddHead(&popGroups, popGroup);
    }
sqlFreeResult(&sr);

return popGroups;
}

static char *condenseSubjectsToPopulations(struct haploExtras *he,
                                           struct haplotypeSet *hapSet, struct haplotype *haplo,
                                           boolean minorPopulations, double limitPct, double *skew)
// From a string of 1000 genome subjects (or subject chroms) returns the list of populations
// and percentages of representation in the set.  If populationsMinor then the 27 100Genomes groups
// are returned, otherwise only the 5 major groupings are used.  If limitPct is non-zero then
// only those populations that are represented at or above the limit are returned.
// If skew is provided, it will be filled with a measure of skewing across populations.
{
// Basic query:
// select distinct commonality, count(*) count from thousandGenomePopulations
//  where subject in ( "HG00096","HG00607","HG00608","HG00610","NA18517","NA18519")
//  group by commonality order by count desc;
#define POP_QUERY_BEG  "select distinct t1.%s grp, count(t1.subject) count, " \
                              "t2.females, t2.subjects" \
                       " from " POP_TABLE " t1, " POP_GROUPS_TABLE " t2" \
                       " where t2.name = t1.%s and t1.subject in ('"
#define POP_QUERY_END  "') group by grp order by count desc"
#define POP_TOTAL 3

if (haplo->subjects == 0 || haplo->subjectIds == NULL
||  haplo->subjectIds[0] == '\0' || limitPct >= 100)
    return NULL;

char *pop = (minorPopulations ? POP_MINOR : POP_MAJOR);

// For calculating skew, it helps to know the number of groups
static int popGroups = 0;
if (popGroups == 0)
    { // Do not expect major vs. minor to change within a single run
    #define POP_QUERY_GROUP_SIZE  "select count(distinct %s) from " POP_TABLE
    char buf[128];
    sqlSafef(buf,sizeof(buf),POP_QUERY_GROUP_SIZE,pop);
    popGroups = sqlQuickNum(he->conn, buf);
    assert(popGroups > 1);
    }

// NOTE: overkill in memory accounting instead of dyString to:
//       1) ensure result is in he->lm, 2) use nifty strSwapStrs()
// Build the query string, converting NA18519-a,NA18519-b... to 'NA18519-a','NA18519-b'...
int sizeQ = strlen(NOSQLINJ "") + strlen(POP_QUERY_BEG) + strlen(POP_QUERY_END)
          + strlen(haplo->subjectIds) + (haplo->subjects * 3) + (strlen(pop) * 2);

char *popQuery = lmAlloc(he->lm,sizeQ);

char *p = popQuery;
sqlSafef(p,sizeQ,POP_QUERY_BEG,pop,pop);
p += strlen(p);
safecpy(p,sizeQ - (p - popQuery),haplo->subjectIds);
size_t count = strSwapStrs(p,sizeQ - (p - popQuery),",","','");
assert(count == haplo->subjects - 1);
p += strlen(p);
sqlSafefFrag(p,sizeQ - (p - popQuery),POP_QUERY_END);//,pop);

// Do our best to calculate memory size for results string
int sizeR = (popGroups + 1) * 12;
char *popResult = lmAlloc(he->lm,sizeR);
p =   popResult;

if (he->conn == NULL)
    he->conn = hAllocConn(he->db);

// Prepare for scoring
#define SCORE_BY_FST_VAR
#ifdef SCORE_BY_FST_VAR
// Scoring by fixationIndex = varS / (hapFreq*(1-hapFreq)
// This scoring relies on variance between fequencies for each subgroup
double hapFreq = (double)haplo->subjects / hapSet->chromN;
double expectFreq = hapFreq;
#else//ifndef SCORE_BY_FST_VAR
// Scoring by simple standard deviation of distribution to each group
// Note the difference in expected frequency for each group
double expectFreq = ((double)1 / popGroups);//(double)haplo->subjects / hapSet->chromN;
#endif//def SCORE_BY_FST_VAR
double sumOfSquares = 0;

// Now we are ready to retrieve the results
char **row = NULL;
int rowCount = 0;
struct sqlResult *sr = sqlGetResult(he->conn, popQuery);
if (he->justModel != NULL)
    assert(haplo->ho->popGroups == NULL);
while ((row = sqlNextRow(sr)) != NULL)
    {
    count = sqlSigned(row[POP_COUNT]);

    // For skew calculation
#ifdef SCORE_BY_FST_VAR
    size_t groupChromN = sqlSigned(row[POP_TOTAL]);
    // selected amount is people so convert to chromN
    if (hapSet->chromN == (hapSet->subjectN * 2))   // autosomes and PAR chrX
        groupChromN *= 2;                          // Diploid
    else if (lastChar(hapSet->chrom) == 'X')                  // non-PAR chrX
        groupChromN += sqlSigned(row[POP_FEMS]);             // females are dipliod
    else                                                         // chrY
        groupChromN -= sqlSigned(row[POP_FEMS]);                // only males have it
    // prob of identity of 2 individuals in population
    double hapFreqGrp = ((double)count / groupChromN); // haplotype freq
    double diff = hapFreqGrp - expectFreq; // variance between group freq
#else//ifndef SCORE_BY_FST_VAR
    double diff = ((double)count / haplo->subjects) - expectFreq; // variance between group freq
#endif//def SCORE_BY_FST_VAR
    sumOfSquares += (diff * diff); // always positive
    rowCount++;

    // Add to string only if above limit
    float pct = ((float)count / haplo->subjects) * 100;
    if (limitPct == 0 || pct >= limitPct)
        {
        safef(p, sizeR - (p - popResult), "%s:%-.1f%%,",row[POP_NAME],pct);
        p += strlen(p);
        }
    // popGroups if single model (hgGene CGI):
    if (he->justModel != NULL)
        slPairAdd(&(haplo->ho->popGroups), row[POP_NAME], (void *)count);

    }
sqlFreeResult(&sr);

if (popResult != p)
    *(--p) = '\0'; // removes the last commma
else if (limitPct > 0)
    {
    safef(popResult,sizeR,"all:<%.0f%%",limitPct);
    p += strlen(popResult);
    }

// Finish calculating skew
if (rowCount < popGroups)                // fill in for the buckets with zero subjects
    sumOfSquares += (popGroups - rowCount) * (expectFreq * expectFreq);
#ifdef SCORE_BY_FST_VAR
    // Using macros to calculate ensures no zero denominators!
    double score = DIVIDE_SAFE( CALC_VAR(sumOfSquares,popGroups), (hapFreq * (1 - hapFreq)), 0);
    //score *= (double)haplo->subjects / hapSet->chromN;
#else//ifndef SCORE_BY_FST_VAR
    double score = CALC_SD(sumOfSquares,popGroups);
#endif//def SCORE_BY_FST_VAR
if (skew != NULL)
    *skew = score;

verbose(2,"condenseSubjectsToPopulations(): subjects:%d buckets:%d score:%lf %s\n",
        haplo->subjects,popGroups,score,popResult);

return popResult;
}



char *haploVarId(struct hapVar *var)
// Returns a string to use a the class or ID for a given variant
{
static char class[256];

if (var->id != NULL && var->id[0] != '\0')
    {
    char *id = cgiEncodeFull(var->id);
    stripChar(id, '.');
    if (id[0] != '\0')
        return id; // leaked
    }

safef(class,sizeof(class),"var%d",var->chromStart);
return class;
}


int haplotypePopularityCmp(const void *va, const void *vb)
// Compare to sort haplotypes by frequency of occurrence
{
const struct haplotype *a = *((struct haplotype **)va);
const struct haplotype *b = *((struct haplotype **)vb);
int ret = a->subjects - b->subjects;
if (ret == 0)
    ret = (a->homCount - b->homCount);
if (ret == 0)
    ret = (a->variantCount - b->variantCount);
if (ret == 0)
    ret = strcmp(a->suffixId,b->suffixId);

return ret;
}

int hapSetSecondIdCmp(const void *va, const void *vb)
// Compare to sort haplotypes in secondId, then haplotype Id order
{
const struct haplotypeSet *a = *((struct haplotypeSet **)va);
const struct haplotypeSet *b = *((struct haplotypeSet **)vb);
int ret = strcmp(a->secondId,b->secondId);
if (ret == 0) // sort deterministically
    ret = strcmp(a->commonId,b->commonId);
if (ret == 0)
    ret = (a->chromStart - b->chromStart);

return ret;
}


static struct haplotype *haplotypeDefault(struct haploExtras *he, int score)
// Makes an empty haplotype struct with defaults
{
struct haplotype *haplo;
lmAllocVar(he->lm,haplo);

haplo->suffixId      = NULL;
haplo->variantCount  = 0;
haplo->variants      = NULL;
haplo->subjects      = score;
haplo->subjectIds    = NULL;
haplo->bitCount      = 0;    // No variants so no bits
haplo->bits          = NULL;
haplo->ho            = NULL;
haplo->generations   = 0;
haplo->descendants   = 0;
return haplo;
}

INLINE void haploOptionalsEnable(struct haploExtras *he, struct haplotype *haplo)
// Makes sure haplotype optionals can be used
{
if (haplo->ho == NULL)
    haplo->ho = lmAlloc(he->lm,sizeof(struct haploOptionals));
}


static struct haplotype *haplotypeForReferenceGenome(struct haploExtras *he,
                                                     struct haplotypeSet *hapSet,
                                                     struct variantBits *vBitsList,
                                                     int homzygousCount, int chromN)
// Makes a reference haplotype.  If a pHaploList list is provided, it assumes that the entire
// list is for the same model and calculates the appropriate subjects and bitmap size
{
// First discover if there is a reference haplotype already included in the set.
if (hapSet->refHap != NULL)
    return hapSet->refHap;

struct haplotype *nonRefHap = hapSet->haplos;
for ( ; nonRefHap != NULL; nonRefHap = nonRefHap->next)
    {
    if (haploIsReference(nonRefHap))
        {
        hapSet->refHap = nonRefHap;
        break;
        }
    }
struct haplotype *refHap = hapSet->refHap;

// If not found, have to create it
if (refHap == NULL)
    {
    refHap = haplotypeDefault(he, 0);
    refHap->suffixId      = "" ;   // Cannot be null for sort
    refHap->variantCount  = 0;     // by definition
    refHap->variants      = NULL;  // by definition
    refHap->subjectIds    = NULL;  // by definition
    refHap->subjects      = chromN; // chrX is the problem, since PAR has more chroms than non-PAR
    refHap->homCount      = homzygousCount;

    if (hapSet->haplos != NULL)
        {
        nonRefHap = hapSet->haplos;

        // Create empty bitmap for reference variants
        refHap->variantsCovered = nonRefHap->variantsCovered;
        refHap->bitCount =            nonRefHap->bitCount;
        refHap->bits = lmBitAlloc(he->lm,refHap->bitCount);

        // Determine refHap subjects from all nonRefHap subjects
        for ( ; nonRefHap != NULL; nonRefHap = nonRefHap->next)
            refHap->subjects -= nonRefHap->subjects;

        // refHap is only in haplos list if it is found in the VCF data set.
        if (refHap->subjects > 0)
            slAddHead(&(hapSet->haplos),refHap);
        }

    hapSet->refHap = refHap;
    }

// Fill in ALL ref variants
// NOTE this is done even if hapSet->refHap was discovered as already in the list
if (vBitsList != NULL)
    {
    assert(refHap->variants == NULL);
    struct hapVar *variants = NULL; // temporary slList for sorting
    struct hapVar *variant = NULL;
    int ix = 0;
    struct variantBits *vBits = vBitsList;
    for ( ; vBits != NULL; ix++, vBits = vBits->next)
        {
        variant = variantFill(he,vBits,hapSet->chromStart,hapSet->chromEnd,hapSet->strand,0);
        verbose(3,"variantIx %d  id:%s  val:%s\n",ix,variant->id,variant->val);
        slAddHead(&variants,variant);
        }
    assert(slCount(variants) == refHap->variantsCovered);
    if (variants != NULL)
        {
        slSort(&variants,variantCmp);  // Must stay in bitmap order!
        refHap->variants = lmAlloc(he->lm,sizeof(struct hapVar *) * slCount(variants));
        ix = 0;
        while ((variant = slPopHead(&variants)) != NULL)
            refHap->variants[ix++] = variant;
        }
    }

// Make sure set has these values
hapSet->variantsCovered = refHap->variantsCovered;
//assert(hapSet->variantsCovered > 0);
if (refHap->bitCount > 0 && hapSet->variantsCovered > 0)
    hapSet->bitsPerVariant = refHap->bitCount / hapSet->variantsCovered;

return refHap;
}

static struct haplotype *haplotypeFill(struct haploExtras *he, struct variantBits *vBitsList,
                                      struct haploBits *hBits, struct haplotype *parent,
                                      struct haplotypeSet *hapSet, int siblingOrder)
// Makes a haplotype struct from hBits and vBits and possibly a parent.
// This routine can be used when haplotypes are being picked off an elmTree,
// or straight from an hBits list, without a parent tree node.
{
struct haplotype *haplo;

// Create an appropriate suffix showing hierarchy
if (parent != NULL)
    {
    // If parent, then the parent's memory is in same pool as child.
    haplo = haplotypeDefault(he, 0);
    int suffixSize = strlen(parent->suffixId) + HAPLOTYPE_SUFFIX_SZ;
    haplo->suffixId = lmAlloc(he->lm,suffixSize);
    safef(haplo->suffixId,suffixSize,"%s%s",parent->suffixId,suffixLetter(siblingOrder));
    haploOptionalsEnable(he,haplo);
    haplo->generations = parent->generations + 1;
    }
else
    {
    // If no parent, then need to allocate from this pool
    haplo = haplotypeDefault(he, 0);
    haplo->suffixId = lmAlloc(he->lm,HAPLOTYPE_SUFFIX_SZ);
    if (siblingOrder < 0)
        { // haplos above 64 bits will NOT be uniquely named
        unsigned bitId = *(unsigned *)&(hBits->bits);
        int bitCount = hBitsSlotCount(hBits);
        if (bitCount/8 > sizeof(unsigned))
            bitId = bitId >> (sizeof(unsigned) * 8 - bitCount);
        safef(haplo->suffixId,HAPLOTYPE_SUFFIX_SZ,"%u",bitId);
        }
    else
        safef(haplo->suffixId,HAPLOTYPE_SUFFIX_SZ,"%s",suffixLetter(siblingOrder));
    haploOptionalsEnable(he,haplo);
    haplo->generations = 1;
    }

// On to haploid genome IDs and all variants
haplo->subjectIds = lmCloneString(he->lm,hBits->ids);
haplo->subjects   = countChars(haplo->subjectIds,',') + 1;
haplo->homCount = homozygousSubjects(haplo->subjectIds);

haplo->variantCount = hBits->bitsOn;
haplo->variantsCovered = hBits->variantSlots;
//haplo->hBits = hBits;               // JUST FOR DEBUG

// Always keep track of the bits
haplo->bitCount = hBitsSlotCount(hBits);
haplo->bits = lmBitClone(he->lm,hBits->bits,haplo->bitCount);

if (vBitsList != NULL)  // Going all out on variants
    {
    int ix = 0;
    int bitIx = 0;
    int bitCount = hBitsSlotCount(hBits);
    struct hapVar *variants = NULL; // temporary slList for sorting
    struct hapVar *variant = NULL;
    for (; ix < hBits->bitsOn && bitIx < bitCount; ix++)
        {
        bitIx = bitFindSet(hBits->bits,bitIx,bitCount);
        if (bitIx >= bitCount)
            break;
        struct variantBits *vBits = vcfHaploBitIxToVariantBits(hBits, bitIx, vBitsList);
        unsigned char variantIx = vcfHaploBitsToVariantAlleleIx(hBits,bitIx);
        assert(variantIx >= 1 && variantIx <= 3); // 0 is reference so we shouldn't see it!
        variant = variantFill(he,vBits,hapSet->chromStart,hapSet->chromEnd,hapSet->strand,
                              variantIx);
        verbose(3,"variantIx %d  id:%s  val:%s\n",variantIx,variant->id,variant->val);
        slAddHead(&variants,variant);

        bitIx = variantNextFromBitIx(hBits,bitIx); // Look at this bit only once.
        }
    if (variants != NULL)
        {
        slSort(&variants,variantCmp);
        assert(slCount(variants) == haplo->variantCount);
        haplo->variants = lmAlloc(he->lm,sizeof(struct hapVar *) * haplo->variantCount);
        ix = 0;
        while ((variant = slPopHead(&variants)) != NULL)
            haplo->variants[ix++] = variant;
        }
    }

return haplo;
}


// --------------------------------------------
// Support for organizing haplotypes by elmTree
// --------------------------------------------
struct haploPickExtras {
// Picking gene haplotypes off the elmTree requires these extras
    struct haploExtras *he;        // carry globals for local memory, etc
    struct haplotypeSet *hapSet;   // Carries the haplotype set being built
    struct variantBits *vBitsList; // creating haplotype requires the variants
    int majorBranches;             // Used for created suffixes
};

boolean haplotypesPick(struct slList *element, struct slList *parent,void *extra,
                           struct slList **result)
// Routine for picking alleles (haplotypes) from an elmTree.  Passed into rElmTreeClimb()
{
struct haploPickExtras *e = extra;
struct haploBits *hBits = (struct haploBits *)element;

// pick nodes with real haplotypes (as opposed to branches with common bits leading to real leaves)
if (hBitsIsReal(hBits))
    {
    int siblingCount = 0; // sibling count is needed to properly id haplotypes
    struct haplotype *parentHap = (struct haplotype *)parent;
    if (parentHap != NULL)
        {
        haploOptionalsEnable(e->he,parentHap);
        parentHap->descendants++;
        siblingCount = parentHap->descendants;
        }
    else
        {
        e->majorBranches++;
        siblingCount = e->majorBranches;
        }
    struct haplotype *haplo = haplotypeFill(e->he,e->vBitsList,hBits,
                                            parentHap,e->hapSet,siblingCount);
    *result = (struct slList *)haplo;
    }
return TRUE;
}


static struct haplotype *haplotypesHarvest(struct haploExtras *he,struct vcfFile *vcff,
                                           struct haplotypeSet *hapSet,
                                           struct variantBits *vBitsList,
                                           struct haploBits *hBitsList, int hapCount)
// If requested, grows elmTree with haplotype bits and picks haplotypes off the tree.
// Otherwise, simply creates haplotypes from the hBits list.
// alleles (haplotypes) named by relation to each other (if by tree) or by bitmap
{
struct haplotype *haplos = NULL;

// Only one?
if (hapCount == 1)
    haplos = haplotypeFill(he,vBitsList,hBitsList,NULL,hapSet,1);
else if (he->growTree)
    {
    // make tree of relations (Note the use of vcf memory, so that the whole tree can be dropped)
    struct elmNode *elmTree = elmTreeGrow(vcfFileLm(vcff), (struct slList *)hBitsList,
                                          vcfFileLm(vcff), vcfHaploBitsCmp, vcfHaploBitsMatching);
    if (verboseLevel() >= 2) // verbose timing at every step
        verboseTime(2, "%s: grew tree with %d nodes.\n",hapSet->commonId,slCount(elmTree));

    // Now convert tree to haplotypes...
    struct haploPickExtras extra;
    extra.he = he;
    extra.hapSet = hapSet;
    extra.vBitsList = vBitsList;
    extra.majorBranches = 0;
    rElmTreeClimb(elmTree, NULL, &extra, haplotypesPick, (struct slList **)&haplos);
    if (verboseLevel() >= 2) // verbose timing at every step
        verboseTime(2, "%s: picked %d haplotypes from tree.\n",hapSet->commonId,hapCount);
    }
else
    {
    struct haploBits *hBits = hBitsList;
    int ix = 0;
    for( ; hBits != NULL; hBits = hBits->next, ix++)
        {
        struct haplotype *haplo = haplotypeFill(he, vBitsList, hBits, NULL, hapSet, ix);
        if (haplo != NULL)
            slAddHead(&haplos,haplo);
        }
    if (verboseLevel() >= 2) // verbose timing at every step
        verboseTime(2, "%s: generated %d haplotypes VCF data.\n",hapSet->commonId,hapCount);
    }
//slReverse(&haplos); // Leave list in descending order: entire list will be reversed as one.
assert(haplos != NULL);
assert(hapCount == slCount(haplos));
hapSet->haplos = haplos;

return haplos;
}

// ---------------------------
// Tune-ups for haplotype Sets
// ---------------------------

static char *lookupGeneSymbol(struct haploExtras *he, char *commonId,char *otherId)
{
char *secondId = NULL;
if (sameString(he->geneTable,HAPLO_GENES_TABLE))
    {
    char query[256];
    sqlSafef(query, sizeof(query), "select geneSymbol from " HAPLO_GENES_2ND_ID_TABLE
                                " where kgID = '%s'",commonId);
    if (he->conn == NULL)
        he->conn = hAllocConn(he->db);

    secondId = sqlQuickString(he->conn, query);
    }
if (secondId == NULL)
    secondId = lmCloneString(he->lm,otherId);

return secondId;
}

static struct haplotypeSet *haploSetBegin(struct haploExtras *he,void *model,boolean isGenePred,
                                          char *chrom,unsigned chromStart,unsigned chromEnd,
                                          char *strand,char *id)
{
struct haplotypeSet *hapSet;
lmAllocVar(he->lm,hapSet);

// Now fill in the blanks
hapSet->isGenePred      = isGenePred;
hapSet->model           = model;
hapSet->chrom           = lmCloneString(he->lm,chrom);
hapSet->chromStart      = chromStart;
hapSet->chromEnd        = chromEnd;
hapSet->strand[0]       = strand[0];
hapSet->strand[1]       = '\0';
hapSet->commonId        = lmCloneString(he->lm,id);
if (isGenePred)
    {
    struct genePred *gp = model;
    hapSet->wideStart       = gp->txStart;
    hapSet->wideEnd         = gp->txEnd;
    hapSet->secondId        = lookupGeneSymbol(he, gp->name,gp->name2);
    }
return hapSet;
}
#define haploSetBeginForGene(he,gp) \
        haploSetBegin(he,gp,TRUE,(gp)->chrom,(gp)->cdsStart,(gp)->cdsEnd,(gp)->strand,(gp)->name)


boolean haploSetIncludesNonRefHaps(struct haplotypeSet *hapSet)
// returns TRUE if the haplotypeSet has atleast 1 non-ref haplotype
{
if (hapSet == NULL)
    return FALSE;

struct haplotype *haplo = hapSet->haplos;
for ( ; haplo != NULL; haplo = haplo->next)
    {
    if (!haploIsReference(haplo))
        return TRUE;
    }
return FALSE;
}


int haploSetsNonRefCount(struct haplotypeSet *haploSets)
// returns the number of haplotypeSets that include at least 1 non-ref haplotype
{
if (haploSets == NULL)
    return 0;
int nonRefCount= 1;

struct haplotypeSet *hapSet = haploSets;
for ( ; hapSet != NULL; hapSet = hapSet->next)
    {
    if (haploSetIncludesNonRefHaps(hapSet))
        nonRefCount++;
    }
return nonRefCount;
}


int haploSetsHaplotypeCount(struct haplotypeSet *haploSets, boolean nonRefOnly)
// returns the number of haplotypes in the haplotypes set list
{
if (haploSets == NULL)
    return 0;
int hapCount= 1;

struct haplotypeSet *hapSet = haploSets;
for ( ; hapSet != NULL; hapSet = hapSet->next)
    {
    hapCount += slCount(hapSet->haplos);

    assert(hapSet->refHap != NULL);
    // If not nonReference, remove the refHap from the count IF it was found in the VCF data
    if (nonRefOnly && hapSet->refHap->subjects > 0)
        hapCount--;
    // If counting all haplotypes, then include ref even if it was not seen in the data
    else if (!nonRefOnly && hapSet->refHap->subjects == 0)
        hapCount++;
    }
return hapCount;
}


static void haploStrandOrient(struct haplotype *haplo)
{
// reverse bitmap.  NOTE: 2bit variants will have bit reversed too.
if (haplo->bitCount > 0 && haplo->bits != NULL)
    bitReverseRange(haplo->bits, 0, haplo->bitCount);

// reverse variants
if (haplo->variants != NULL)
    {
    int ixA = 0;
    int ixB = (haplo->variantCount > 0 ? haplo->variantCount : haplo->variantsCovered) - 1;
    for ( ; ixA < ixB; ixA++, ixB--)
        {
        assert(haplo->variants[ixA] != NULL && haplo->variants[ixB] != NULL);
        struct hapVar *varA = haplo->variants[ixA];
        haplo->variants[ixA] = haplo->variants[ixB];
        haplo->variants[ixB] = varA;
        }

    // Now reverse the DNA seq.
    ixA = (haplo->variantCount > 0 ? haplo->variantCount : haplo->variantsCovered) - 1;
    for ( ; ixA >= 0; ixA--)
        {
        struct hapVar *var = haplo->variants[ixA];
        assert(var != NULL);
        if (!var->reversed)
            {
            var->reversed = TRUE; // This is necessary because var mem is shared between haps!
            int len = strlen(var->val);
            if (len > 0)
                {
                reverseComplement(var->val, len);
                }
            }
        }
    // Note: aa sequence should be created AFTER this function, not before
    }
}


int haploSetStrandOrient(struct haploExtras *he,struct haplotypeSet *hapSet)
// Orients all haplotypes in a set to the negative strand, if appropriate.
// The bitmap and variants are reversed.
// Returns a count of haplotype structures that have been altered
{
if (hapSet->strand[0] != '-')
    return 0;

int reversed = 0;

// Orient refHap first.
haploStrandOrient(hapSet->refHap);
reversed++;

struct haplotype *haplo = hapSet->haplos;
for ( ; haplo != NULL; haplo = haplo->next)
    {
    if (haplo != hapSet->refHap) // refHap may or may not be a part of the set
        {
        haploStrandOrient(haplo);
        reversed++;
        }
    }
return reversed;
}

#define RANGE_LIMIT(val,min,max) {       if ((val) > (max)) (val) = (max); \
                                   else  if ((val) < (min)) (val) = (min); }

static struct hapVar *variantToUse(struct hapVar *refVar,struct haplotype * haplo)
// Determines whether this haplotye has the reference or non-reference var
{
struct hapVar *varToUse = refVar; // Default to refVar
if (haplo != NULL && haplo->variantCount > 0) // Non-ref haplotype
    {
    // Determine if non-ref variant should be used
    int nrIx = 0;
    for( ; nrIx < haplo->variantCount; nrIx++)
        {
        if (refVar->id != NULL && haplo->variants[nrIx]->id != NULL)
            {
            if (sameString(refVar->id,haplo->variants[nrIx]->id))
                {
                varToUse = haplo->variants[nrIx];
                break;
                }
            }
        else if (refVar->chromStart == haplo->variants[nrIx]->chromStart
             &&  refVar->chromEnd   == haplo->variants[nrIx]->chromEnd  )
            {
            varToUse = haplo->variants[nrIx];
            break;
            }
        }
    }
return varToUse;
}


void haploSetCalcScores(struct haploExtras *he,struct haplotypeSet *hapSet,
                        int subjectN, int chromN)
// Score all haplotypes in a single haplotype set.  Pass in subject and chrom counts of dataset
// used to generate the haplotype.
{
// This is based upon a pValue as
// haploScore = -log10(likelihood of N haplotypes with exact variants)
// homScore   = -log10(likelihood of M homozygous given N haplotypes)
// But the scores are normalized by varaintsCovered so different genes can be compared
hapSet->subjectN = subjectN;
hapSet->chromN   = chromN;

struct haplotype *refHap = hapSet->refHap;
assert(refHap != NULL);
if (refHap->variants == NULL)
    return;

// Better scoring:
// don't reward many variants over few: normalize hapProb by number of variants
// NOT DONE: do not assume independence: Markov chain by 1?

// Now walk through all haplotypes
struct haplotype *haplo = hapSet->haplos;
for ( ; haplo != NULL; haplo = haplo->next)
    {
    // calculate expected haplotype count
    long double hapProb = 1;
    //boolean prevVarReference = TRUE;
    int ix = 0;
    for( ; ix < refHap->variantsCovered; ix++)
        {
        assert(refHap->variants[ix]);
        struct hapVar *varToUse = variantToUse(refHap->variants[ix],haplo);
        //if (ix > 0 && varToUse == prevVarReference) // markov chain
        //    hapProb *= (varToUse->frequency + (1 - prevVarReference->frequency)/2);
        //else
        hapProb *= varToUse->frequency;
        //prevVarReference = thisVarReference;
        }

    // Now hapProb is probability of finding one haplotype with this exact variant combination
    // by chance.  However...
    // Normalize probability by number of variants so that long genes don't gain score advantage
    long double hapProbNormalized = (hapProb * refHap->variantsCovered);

    // Now calculate expected occurances by a Bonferroni type adjustment
    // for the number of chroms examined:
    int hapsExpected = roundl(hapProb * chromN);

    // Now if this is found more than expected, calc the -log10 pValue of what is unexpected
    if (haplo->subjects > hapsExpected)
        haplo->haploScore = -1 * log10l(powl(hapProbNormalized, (haplo->subjects - hapsExpected)));
    else if (haplo->subjects < hapsExpected)
        haplo->haploScore =      log10l(powl(hapProbNormalized, (hapsExpected - haplo->subjects)));
    else
        haplo->haploScore = 0;

    // homozygous score may be much more convincing still
    if (subjectN < chromN)
        {
        // Based upon haplotype occurrences:
        hapProb = (double)haplo->subjects / chromN;
        long double homProb = (hapProb * hapProb);
        // Again get the -log10 pValue of what is unexpected.
        int homsExpected = roundl(homProb * subjectN);
        if (haplo->homCount > homsExpected)
            haplo->homScore = -1 * log10l(powl(homProb,(haplo->homCount - homsExpected)));
        else if (haplo->homCount < homsExpected)
            haplo->homScore =      log10l(powl(homProb,(homsExpected - haplo->homCount)));
        else
            haplo->homScore = 0;
        }
    else
        haplo->homScore = haplo->haploScore; // haploid score!

    // cap these:
    RANGE_LIMIT(haplo->haploScore,-99999,99999);
    RANGE_LIMIT(haplo->homScore,  -99999,99999);

    // Now that we handled the scoring, lets also convert the subjectIds to populations
    if (haplo->subjectIds != NULL && he->populationsToo)
        {
        haploOptionalsEnable(he,haplo);
        haplo->ho->popDist = condenseSubjectsToPopulations(he,hapSet,haplo,
                                                          he->populationsMinor,he->populationMinPct,
                                                         &(haplo->ho->popScore));
        }
    }
}

// ----------------------------------------------------
// Code for converting haplotypes to DNA or AA sequence
// ----------------------------------------------------

static struct hapBlocks *makeInvariantBlock(struct haploExtras *he, struct genePred *gp,
                                            int chromBound1,int chromBound2,boolean *coding)
// Fill in an invariant seqBlock based upon supplied gene model and boundaries
{
struct hapBlocks *block;
lmAllocVar(he->lm,block);
block->ix = -1; // invariant
// NOTE that using the genePred function accounts for introns
// TRUE,TRUE means stranded and intronic pos okay
boolean codingLocal = FALSE;
int bound1 = genePredBaseToCodingPos(gp,chromBound1,TRUE,&codingLocal);
int bound2 = genePredBaseToCodingPos(gp,chromBound2,TRUE,&codingLocal);

//assert(gp->strand[0] == '-' || bound1 <= bound2);
//if (gp->strand[0] != '-' && bound1 > bound2)
//    printf("Expected %d less then %d.  %d-%d<br>",bound1,bound2,chromBound1,chromBound2);

if (bound1 > bound2)
    {
    block->beg =  max(bound2,0);
    block->len = (bound1 - block->beg);
    }
else
    {
    block->beg =  max(bound1,0);
    block->len = (bound2 - block->beg);
    }

if (coding)
    *coding = codingLocal;
return block;
}

static struct hapBlocks *makeVariantBlock(struct haploExtras *he, struct genePred *gp,
                                          int ix,struct hapVar *refVar)
// Fill in a variant seqBlock sized for the largest variant of the set
{
// Determine max length of this variant
int maxDnaLen = 0;
struct hapVar *var = refVar;
for ( ; var != NULL; var = var->next)
    {
    if (var->val != NULL && var->val[0] != '\0')
        {
        int len = strlen(var->val);
        if (maxDnaLen < len)
            maxDnaLen = len;
        }
    }

// Deletions break some rules.  Not only may they overlap other variants,
// but their length is dependent upon the invariant coordinates of the reference variant.
if (refVar->vType == vtDeletion)
    {
    struct hapBlocks *refBlock = makeInvariantBlock(he,gp,refVar->chromStart,refVar->chromEnd,NULL);
    maxDnaLen = refBlock->len;
    //freeMem(refBlock); // lm
    }

// Now make chunk for this variant
struct hapBlocks *block;
lmAllocVar(he->lm,block);
block->ix = ix; // variant!
block->beg = 0; // Variant always begins at the beginning of variant val
block->len = maxDnaLen;
return block;
}

//#define HAPLO_WIP

static void geneHapSetAddDnaBlocks(struct haploExtras *he, struct haplotypeSet *hapSet)
// Returns structs defining the beg and end positions of DNA sequence including
// invariant reference sequence and variant sequences normalized for alignment of
// haplotypes.  NOTE: beg is 0-based from start of first coding exon, and introns are removed.
{
// NOTE: if variants are non-overlapping then creating a list of blocks works fine:
// [...invariant...][var][...invariant...][var][...invariant...]
// These blocks are then used to create a haplotype full seq by inserting the [var] where needed.
struct haplotype *refHap = hapSet->refHap;
assert(refHap->variants != NULL);
boolean reverse = (hapSet->strand[0] == '-');
struct hapBlocks *blockList = NULL;
struct hapBlocks *block = NULL;

// Chop up the sequence by walking through defined variants in + strand chrom position order
// Note that negative strand haplotypes have their variants in reverse order.
int last = hapSet->chromStart;  // confusing but this is correct for + AND -
int ix = (reverse ? refHap->variantsCovered - 1 : 0);
for ( ; 0 <= ix && ix < refHap->variantsCovered; ix += (reverse ? -1 : 1))
    {
    struct hapVar *refVar = refHap->variants[ix];
    assert(refVar != NULL);

    //assert(last <= refVar->chromStart); // Should be going forward without overlap

    // First, make chunk for invariant stretch BEFORE current variant
    boolean coding = FALSE;
    // Deletions need to be accounted for as 0 length blocks!
    int varStart = refVar->chromStart;
    if (refVar->vType == vtDeletion && reverse)
        varStart = refVar->chromEnd;
    block = makeInvariantBlock(he,hapSet->model,last,varStart,&coding);
#ifdef HAPLO_WIP
    printf("block:%d-%d  abs:%d-%d  %s<BR>",
           block->beg,(block->beg + block->len),last,refVar->chromStart,
           (!coding && ix > 0 && refVar->vType == vtSNP ? "INTRONIC!":""));
#endif// HAPLO_WIP
    slAddHead(&blockList,block);

    // Now make the variant block
    block = makeVariantBlock(he,hapSet->model,ix,refVar);
#ifdef HAPLO_WIP
    printf("ix:%2d  len:%d  var:%d-%d  val:%s<BR>",block->ix,block->len,
           refVar->chromStart, refVar->chromEnd,
           (refVar->val && refVar->val[0] ? refVar->val : "-"));
#endif// HAPLO_WIP
    slAddHead(&blockList,block);

    // next invariant block will start after this variant ends

    // Problem if the block is a deletion.  Deletions could overlap other variants,
    // screwing up accounting, so deletions are always treated as 0 length blocks
    if (refVar->vType == vtDeletion && !reverse)
        last = refVar->chromStart;
    else
        last = refVar->chromEnd;
    }
// Now we need to add one more chunk for the invariant sequence AFTER the last variant
block = makeInvariantBlock(he,hapSet->model,last,hapSet->chromEnd,NULL);
#ifdef HAPLO_WIP
boolean coding;
int fullLen = genePredBaseToCodingPos(hapSet->model,hapSet->chromEnd,FALSE,&coding);
printf("block:%d-%d  abs:%d-%d  set:%d-%d len:%d<BR>",
       block->beg,(block->beg + block->len),last,hapSet->chromEnd,
       hapSet->chromStart,hapSet->chromEnd,fullLen);
#endif// HAPLO_WIP
slAddHead(&blockList,block);
if (!reverse) // If on - strand then the blocks are already in - strand order!
    slReverse(&blockList);

hapSet->hapBlocks = blockList;
}

DNA *haploDnaSequence(struct haploExtras *he, struct haplotypeSet *hapSet,
                        struct haplotype *haplo, boolean alignmentGaps, boolean hilite)
// Generate and return the full DNA CODING sequence of a haplotype.
// When haplo is NULL, returns reference sequence.
// If no alignmentGaps or hilites then raw sequence is saved in haplo->ho->dnaSeq.
{
if (!hapSet->isGenePred)
    return NULL;  // TODO extend to non-genePred haplotypes (when some exist!)

if (hapSet->hapBlocks == NULL && hapSet->isGenePred)
    geneHapSetAddDnaBlocks(he, hapSet);

struct haplotype *refHap = hapSet->refHap;

if (haplo == NULL || haplo == refHap)
    {
    haploOptionalsEnable(he,refHap);
    if (refHap->ho->dnaSeq == NULL)
        {
        struct dnaSeq *dnaSeq = genePredGetDna(he->db, hapSet->model, TRUE, dnaUpper);
        assert(dnaSeq != NULL);
        refHap->ho->dnaSeq = dnaSeq->dna;  // Note this will be leaked
        refHap->ho->dnaLen = dnaSeq->size;
        freeMem(dnaSeq);
        }
    if (!alignmentGaps && !hilite)
        return refHap->ho->dnaSeq;  // Nothing more should be needed!
    }
else
    {
    haploOptionalsEnable(he,haplo);
    if (!alignmentGaps && !hilite && haplo->ho->dnaSeq != NULL)
        return haplo->ho->dnaSeq;
    }

// We will need to construct the dna string from the hapBlocks
struct dyString *dy = dyStringNew(2048);
struct hapBlocks *block = hapSet->hapBlocks;
int skipLen = 0;
int skipIx = -1;
for ( ; block != NULL; block = block->next)
    {
    if (block->ix == -1) // invariant blocks are easy
        {
        if (block->len > 0)
            {
            assert(refHap->ho != NULL && refHap->ho->dnaLen >= block->beg);
            if (skipLen < block->len) // skipLen is due to prior deletion variant
                {
                dyStringAppendN(dy,refHap->ho->dnaSeq + block->beg + skipLen,
                                (block->len - skipLen));
                skipLen = 0;
                }
            else
                skipLen -= block->len;
            }
        }
    else // variant
        {
        assert(block->len > 0);
        struct hapVar *refVar = refHap->variants[block->ix];
        boolean deletion = (refVar->vType == vtDeletion);
        struct hapVar *varToUse = variantToUse(refVar,haplo);

        // Deletion blocks are tricky as they cover no distance in block list
        // in order to allow them to overlap other variants.
        if (skipLen > 0 && deletion) // Previous deletion overlapping new deletion
            {
            // Ref vars are just skipped, but non-ref vars may require adjusting skipLen
            if (varToUse != refVar)
                {
                assert(skipIx != -1); // Must be set if skipLen is set
                int startDiff = varToUse->dnaOffset - refHap->variants[skipIx]->dnaOffset;
                if (skipLen < (startDiff + block->len))
                    {
                    skipLen = startDiff + block->len;
                    skipIx = block->ix;
                    }
                }
            continue;
            }
        if (skipLen >= block->len) // Non-deletions are easier
            {
            skipLen -= block->len;
            continue;
            }


        if (hilite)
            dyStringPrintf(dy,"<B class='%s%s'>",haploVarId(varToUse),
                             (varToUse != refVar ? " textAlert" : ""));

        // Now we can handle the variants identically
        int len = 0;
        if (varToUse->val != NULL && varToUse->val[0] != '\0')
            len = strlen(varToUse->val);

        if (len > 0)
            {
            assert(deletion || len <= block->len);
            // Because deletions can overlap other variants, they are treated here
            // as if 0 length.  That way ref shows actual sequence a single char
            if (deletion)
                {
                //assert(varToUse == refVar);  // Not a valid assumption
                len = block->len;
                }
            else
                {
                if (skipLen < len)
                    dyStringAppend(dy,varToUse->val+skipLen);
                }
            skipLen = 0;
            }
        else if (deletion)
            {
            assert(varToUse != refVar);
#ifdef HAPLO_WIP
            printf("DELETE ix:%d length:%d blockLen:%d<BR>",
                   block->ix,len,block->len);
#endif//def HAPLO_WIP
            skipLen = block->len; // special delete accounting requires skipping part of next block
            skipIx = block->ix;
            }

        // may have to fill in!
        if (alignmentGaps && block->len > len)
            dyStringAppendMultiC(dy,'.', block->len - len);

        if (hilite)
            dyStringAppend(dy,"</B>");
        }
    }
if (haplo != NULL && !alignmentGaps && !hilite)
    {
    haplo->ho->dnaSeq = dyStringContents(dy);
    haplo->ho->dnaLen = dyStringLen(dy);
    }
return (DNA *)dyStringCannibalize(&dy);
}

#define AA_STOP_TAG "[stop]"
#define AA_STOP_CODE_A "TAA"
#define AA_STOP_CODE_B "TAG"
#define AA_STOP_CODE_C "TGA"

// NOTE: Could be useful to expose this DNA to AA translation code
static AA *aaSequence(char *dnaSequence, int *pLen)
// Translate and return full poly-AA sequence from non-gapped DNA sequence.
// Stop codons will terminate sequence with "[stop]".
// Length minus [stop] will be filled into pLen if provided
{
int dnaLen = (strlen(dnaSequence) / 3) + 32;  // plus "[stop]"
AA *aaSeq = needMem(dnaLen + 10); // plus slop

dnaTranslateSome(dnaSequence, aaSeq, dnaLen);

// Check to see if last codon is a stop codon.  It should be!
int aaLen = strlen(aaSeq);
assert((dnaLen - aaLen) > 6); // of course there is room for a stop tag
DNA *dnaPos = dnaSequence + (aaLen * 3);
if (strncasecmp(AA_STOP_CODE_A,dnaPos,3) == 0
||  strncasecmp(AA_STOP_CODE_B,dnaPos,3) == 0
||  strncasecmp(AA_STOP_CODE_C,dnaPos,3) == 0)
    strcpy(aaSeq + aaLen,AA_STOP_TAG);
else
    {
    dnaPos += 3;
    if ((dnaPos - dnaSequence) < dnaLen
    &&  (strncasecmp(AA_STOP_CODE_A,dnaPos,3) == 0
    ||   strncasecmp(AA_STOP_CODE_B,dnaPos,3) == 0
    ||   strncasecmp(AA_STOP_CODE_C,dnaPos,3) == 0))
        strcpy(aaSeq + aaLen,AA_STOP_TAG);
    }
if ( pLen != NULL)
    *pLen = aaLen;
// Consider warning on truncation (no stop and less than 3 bases left)
return aaSeq;
}

static AA *aaTriplets(char *aaSeq)
// Quick means to output aaSeq spaced by 3 chars to align with dnaSeq
{
struct dyString *dy = dyStringNew(strlen(aaSeq) * 3);
char *aa = aaSeq;
while (*aa != '\0' && *aa != '[')
    dyStringPrintf(dy,"%c..",*aa++);
if (*aa == '[')
    dyStringAppend(dy,aa);
return dyStringCannibalize(&dy);
}


void geneHapSetAaVariants(struct haploExtras *he,
                          struct haplotypeSet *hapSet, struct haplotype *haplo)
// Determine haplotype AA variants using actual AA sequence.
// If non-reference haplotype is NULL, then refHap variants AA are set.
// This routine will generate the AA seq if not found, but dnaSeq must exist
{
// Determining AA values for variants is tricky, since each location is potentially
// affected by upstream variants.  Therefore, it is necessary to find the variant
// location in the aaSeq and then carve out the value in place.  This must be done
// haplotype by haplotype, unlike DNA variant values which should be common to all
// haplotypes that share that variant.

struct haplotype *refHap = hapSet->refHap;
int dnaLen = 0;

// Since the AA sequence may be terminated with a stop tag, we should stop before that
assert(refHap->ho != NULL && refHap->ho->dnaSeq != NULL);
if (refHap->ho->aaSeq == NULL)
    refHap->ho->aaSeq  = aaSequence(refHap->ho->dnaSeq,&(refHap->ho->aaLen));
if (haplo != NULL)
    {
    assert(haplo->ho != NULL && haplo->ho->dnaSeq != NULL);
    if (haplo->ho->aaSeq == NULL)
        haplo->ho->aaSeq  = aaSequence(haplo->ho->dnaSeq,&(haplo->ho->aaLen));
    }

// Walk through blocks adding up lengths until
struct hapBlocks *block = hapSet->hapBlocks;
for ( ; block != NULL; block = block->next)
    {
    if (block->ix < 0) // invariant block
        dnaLen += block->len;
    else
        {
        struct hapVar *refVar = refHap->variants[block->ix];
        int refDnaLen = (refVar->chromEnd - refVar->chromStart);

        if (haplo != NULL && haplo != hapSet->refHap)
            {
            struct hapVar *hapVar = variantToUse(refVar,haplo);
            // Is there a matching hapVar?
            if (hapVar != refVar)
                {
                hapVar->aaOffset = dnaLen / 3;
                int varDnaLen = (hapVar->vType == vtDeletion
                                || hapVar->val == NULL ? 0 : strlen(hapVar->val));
                if (hapVar->vType != vtDeletion && hapVar->vType != vtInsertion)
                    {
                    hapVar->aaLen = (varDnaLen == 0 ? 0 : 1 + (varDnaLen/3));
                    if (hapVar->aaLen > 0 && hapVar->aaOffset < haplo->ho->aaLen)
                        {
                        hapVar->aaVal = lmCloneStringZ(he->lm,haplo->ho->aaSeq + hapVar->aaOffset,
                                                       hapVar->aaLen);
#ifdef HAPLO_WIP
                        bitsOut(stdout,haplo->bits,0,haplo->bitCount,FALSE);
                        printf(" ix:%d AA:%d SNP %s:%d  %c/%s<BR>",block->ix,
                               refVar->aaOffset + 1,hapSet->chrom,refVar->chromStart,
                               refHap->ho->aaSeq[refVar->aaOffset],hapVar->aaVal);
#endif//def HAPLO_WIP
                        }
                    //else
                    //    hapVar->aaVal = NULL;  // haps share var memory, so don't overwrite
                    }
                else
                    {
                    hapVar->aaLen = -1; // frame shift!
                    if (varDnaLen > refDnaLen)
                        hapVar->aaVal = lmCloneString(he->lm,"[>>]");
                    else
                        hapVar->aaVal = lmCloneString(he->lm,"[<<]");
#ifdef HAPLO_WIP
                    bitsOut(stdout,haplo->bits,0,haplo->bitCount,FALSE);
                    printf(" ix:%d AA:%d %s %s:%d  %c/%s<BR>",block->ix,
                           refVar->aaOffset + 1,(hapVar->vType == vtInsertion ? "INS":"DEL"),
                           hapSet->chrom,refVar->chromStart,
                           refHap->ho->aaSeq[refVar->aaOffset],hapVar->aaVal);
#endif//def HAPLO_WIP
                    }
                if (hapVar->vType != vtDeletion)
                    dnaLen += varDnaLen;
                }
            else  // haplo doesn't contain this variant
                {
                if (refVar->vType != vtDeletion && refVar->vType != vtInsertion)
                    dnaLen += block->len; // Both Deletions and insertions are accounted for as 0
                }
            }
        else // Else just filling refVar
            {
            refVar->aaOffset = dnaLen / 3;
            refVar->aaLen = (refDnaLen == 0 ? 0 : 1 + (refDnaLen/3));
            if (refVar->aaLen > 0 && refVar->aaOffset < refHap->ho->aaLen)
                {
                //assert(refVar->aaOffset + refVar->aaLen <= refHap->aaLen);
                refVar->aaLen = min(refVar->aaLen,(refHap->ho->aaLen - refVar->aaOffset));
                refVar->aaVal = lmCloneStringZ(he->lm,refHap->ho->aaSeq + refVar->aaOffset,
                                               refVar->aaLen);
                }
            else
                refVar->aaVal = NULL;

#ifdef HAPLO_WIP
            if (refVar->vType != vtInsertion && refHap->ho->dnaSeq[dnaLen] != refVar->val[0])
                printf("AA:%d %s %s:%d  Expected ref:%c found:%c val:%s<BR>",refVar->aaOffset + 1,
                     (refVar->vType == vtSNP ? "SNP" : refVar->vType == vtDeletion ? "DEL" : "???"),
                     hapSet->chrom,refVar->chromStart,refHap->ho->dnaSeq[dnaLen],refVar->val[0],
                     refVar->aaVal == NULL ? "" : refVar->aaVal);
#endif//def HAPLO_WIP

            if (refVar->vType != vtDeletion) // deletions treated as 0 len blocks to avoid overlap
                dnaLen += refDnaLen;
            }
        }
    }
}


AA *haploAaSequence(struct haploExtras *he, struct haplotypeSet *hapSet,
                    struct haplotype *haplo, boolean triplet, boolean markVars)
// returns AA sequence for a haplotype.  if haplo is NULL returns AA seq for reference.
// If triplet view, then AAs are padded to align with DNA sequence as "L..P..".
// If markVars, then a single AA at the start of variant position is wrapped with a <span>
// with class matching the variantId. Only use markVars with refHap!
// If not triplet or marVars, then the raw sequence is stored in haplo->ho->aaSeq.
{
struct haplotype *refHap = hapSet->refHap;
struct haplotype *theHap = refHap;

// Requires DNA sequence and AA sequence
assert(refHap->ho != NULL && refHap->ho->dnaSeq != NULL);
if (refHap->ho->aaSeq == NULL)
    refHap->ho->aaSeq  = aaSequence(refHap->ho->dnaSeq,&(refHap->ho->aaLen));
if (haplo != NULL && haplo != refHap)
    {
    assert(haplo->ho != NULL && haplo->ho->dnaSeq != NULL);
    if (haplo->ho->aaSeq == NULL)
        haplo->ho->aaSeq  = aaSequence(haplo->ho->dnaSeq,&(haplo->ho->aaLen));
    theHap = haplo;
    }

// straight sequence?
if (!triplet && !markVars)
    return theHap->ho->aaSeq;

AA *aaSeq = theHap->ho->aaSeq;
if (triplet)
    aaSeq = aaTriplets(theHap->ho->aaSeq);

if (!markVars)
    return aaSeq;

assert(haplo == NULL);  // Only reference Haplotypes get to be marked!

// Bold and with or without triplet view
int aaLen = strlen(aaSeq);
struct dyString *dy = dyStringNew(aaLen  + (hapSet->variantsCovered * 20));

// rely upon variants with aaOffsets!
// This requires that geneHapSetAaVariants() was already called.
//assert(refHap->variants != NULL
//    && (refHap->variants[0]->aaVal != NULL || refHap->variants[0]->vType != vtInsertion));
char *lastP = aaSeq;
struct hapVar *lastVar = NULL;
int ix = 0;
for ( ; ix < refHap->variantsCovered; ix++)
    {
    struct hapVar *var = refHap->variants[ix];
    int beg = var->aaOffset;
    if (triplet)
        beg *= 3;
    if (aaSeq + beg > lastP) // two SNPs could fall into same AA position
        {
        int len = (aaSeq + beg) - lastP;
        dyStringAppendN(dy,lastP, len);
        lastP += len;
        if (aaSeq + aaLen <= lastP)
            break; // ran out of sequence!
        }

    // In rare cases, two variants fall on the same AA!
    if (lastVar != NULL)
        {
        // Not ideal but just skip it!
        if (var->aaOffset == lastVar->aaOffset)
            continue;
        }

    dyStringPrintf(dy,"<span class='%s'>",haploVarId(var));

    // All variants will be marked with a SINGLE AA position, regardless of their length!
    int len = 1;
    if (triplet)
        len *= 3;

    dyStringAppendN(dy,lastP,len);
    lastP += len;

    dyStringAppend(dy,"</span>");
    if (aaSeq + aaLen <= lastP)
        break; // ran out of sequence!

    lastVar = var;
    }
// Add the rest
if (aaSeq + aaLen > lastP)
    {
    int len = (aaSeq + aaLen) - lastP;
    dyStringAppendN(dy,lastP, len);
    }

return dyStringCannibalize(&dy);
}


// --------------------------------
// gene haplotype alleles HTML code
// --------------------------------

// ----------- Some repetitive tasks ...
static char *refVaraintTitle(struct haplotypeSet *hapSet,struct hapVar *refVar,boolean dnaView)
// returns the standard variant tooltip title string
{
static char title[1024]; // use it or loose it.
char *cur = title;
char *end = cur + sizeof(title);

// chrX:1234 SNP A|G|C (0.50|0.25|0.25) rs1234
// chrX:1234 Insertion (relative to reference): -|AC... (0.015|0.985) length:3954 rs1234
// chrX:1234 Deletion (relative to reference): AC...|- (0.985|0.015) length:3954 rs1234
// AA view prepends:
// AA:107 L|P chr...

// AA position and possible values (if AA view)
if (!dnaView && refVar->aaOffset >= 0)
    {
    safef(cur,(end - cur),"AA:%d ",refVar->aaOffset + 1);
    cur += strlen(cur);

    // AA change
    //if (refVar->aaLen <= 1)
        {
        // Walk through each allele
        struct hapVar *var = refVar;
        int maxLen = 0;
        for ( ; var != NULL; var = var->next)
            {
            if (var->aaLen == -1) // frameShift
                {
                if (refVar->vType == vtInsertion)
                    safef(cur,(end - cur),"+%d|",(refVar->dnaLen/3 + 1));
                else if (refVar->vType == vtDeletion)
                    safef(cur,(end - cur),"-%d|",((refVar->chromEnd - refVar->chromStart)/3 + 1));
                }
            else if (var->aaVal == NULL || var->aaVal[0] == '\0')
                {
                if (refVar->vType == vtDeletion)
                    safecpy(cur,(end - cur),"+|");
                else
                    safecpy(cur,(end - cur),"-|");
                }
            else
                {
                int len = strlen(var->aaVal);
                if (maxLen < len) // Will be used below!
                    maxLen = len;
                if (len > 5)
                    safef(cur,(end - cur),"%c%c...|",var->aaVal[0],var->aaVal[1]);
                else
                    safef(cur,(end - cur),"%s|",var->aaVal);
                }
            cur += strlen(cur);
            }
        cur -= 1;  // overwrite the last '\'
        safecpy(cur,(end - cur)," ");
        cur++;
        }
    }

// location:
safef(cur,(end - cur),"%s:%d ",hapSet->chrom,refVar->chromStart + 1);
cur += strlen(cur);

if (refVar->vType == vtSNP)
    {
    safecpy(cur,(end - cur),"SNP: ");
    cur += 5;
    }
else if (refVar->vType == vtInsertion)
    {
    safecpy(cur,(end - cur),"Insertion (relative to reference): ");
    cur += strlen(cur);
    }
else if (refVar->vType == vtDeletion)
    {
    safecpy(cur,(end - cur),"Deletion (relative to reference): ");
    cur += strlen(cur);
    }
else
    {
    safecpy(cur,(end - cur),"???: ");
    cur += 5;
    }

// Walk through each allele
struct hapVar *var = refVar;
int maxLen = 0;
for ( ; var != NULL; var = var->next)
    {
    if (var->val == NULL || var->val[0] == '\0')
        safecpy(cur,(end - cur),"-|");
    else
        {
        int len = strlen(var->val);
        if (maxLen < len) // Will be used below!
            maxLen = len;
        if (len > 5)
            safef(cur,(end - cur),"%c%c...|",var->val[0],var->val[1]);
        else
            safef(cur,(end - cur),"%s|",var->val);
        }
    cur += strlen(cur);
    }
cur -= 1;  // overwrite the last '\'
safecpy(cur,(end - cur)," (");
cur += 2;

// Now variant frequencies
for (var = refVar; var != NULL; var = var->next)
    {
    safef(cur,(end - cur),"%.3f|",var->frequency);
    cur += strlen(cur);
    }
cur -= 1;  // overwrite the last '\'
safecpy(cur,(end - cur),") ");
cur += 2;

// Length
if (refVar->vType == vtDeletion)
    {
    safef(cur,(end - cur),"length:%d ",(int)strlen(refVar->val));
    cur += strlen(cur);
    }
else if (refVar->vType == vtInsertion)
    {
    safef(cur,(end - cur),"length:%d ",maxLen);
    cur += strlen(cur);
    }
else if (refVar->vType != vtSNP) // unknown
    {
    safef(cur,(end - cur),"length:%d ",refVar->dnaLen);
    cur += strlen(cur);
    }

if (refVar->id && refVar->id[0] != '\0')
    safecpy(cur,(end - cur),refVar->id);

return title;
}

static char *refVariantLink(struct cart *cart, struct haplotypeSet *hapSet,int ix)
// returns a string URL to get to the hgc for the 1000 genomes varaint
{
// hgc?c=chr9&o=136132907&t=136132908&g=tgpPhase1&i=rs8176719
struct haplotype *refHap = hapSet->refHap;
static char link[512];
struct hapVar *refVar = refHap->variants[ix];
  safef(link,PATH_LEN,"%s?c=%s&o=%u&t=%u&g=%s&i=%s",
        hgcName(),hapSet->chrom,
      refVar->origStart,refVar->origEnd,
      HAPLO_1000_GENOMES_TRACK,refVar->id);
return link;
}


static char *clipLongVals(char *val, int largest,char emptyChar)
// Truncates large values for readable output
{
static char buf[16];
assert(largest < sizeof(buf));
if (val == NULL || *val == '\0')
    {
    buf[0] = emptyChar;
    buf[1] = '\0';
    return buf;
    }

int len = strlen(val);
if (len > largest)
    {
    strncpy(buf, val, (largest-3));
    strcpy(buf +      (largest-3), "+++");
    }
else
    strcpy(buf, val); // by always returning buf, strLower, etc can be used with impunity

return buf;
}

// NOTE: could be made public
static char *sequenceDiffsHilite(char *refSeq, char *difSeq,
                                 char *hiliteTokenStart, char *hiliteTokenEnd)
// Given a reference sequence and a variant sequence, returns the varaint sequence
// with differences hilighted
{
char *ref = refSeq;
char *dif = difSeq, *last = difSeq;
struct dyString *dy = NULL;
while (*ref != '\0' && *dif != '\0')
    {
    if (tolower(*ref) != tolower(*dif))
        {
        if (dy == NULL)
            {
            dy = dyStringNew(strlen(difSeq)
                             + (10 * strlen(hiliteTokenStart))
                             + (10 * strlen(hiliteTokenEnd)));
            }
        if (last != dif)
            dyStringAppendN(dy,last,(dif - last)); // catch up

        dyStringAppend(dy,hiliteTokenStart);
        for ( ; *dif != '\0' && tolower(*ref) != tolower(*dif); dif++)
            {
            dyStringAppendC(dy,*dif);
            if (*ref != '\0')
                ref++;
            }
        dyStringAppend(dy,hiliteTokenEnd);
        last = dif;
        }
    else
        {
        ref++;
        dif++;
        }
    }
if (last != dif && dy != NULL)
    dyStringAppendN(dy,last,(dif - last)); // finish up

if (dy != NULL)
    return dyStringCannibalize(&dy);

return refSeq;
}

// These defines are used in CSS and js so change with caution
#define TABLE_ID           "alleles"
#define ALLELE             "allele"
#define VARIANT_CLASS      "var"
#define SEQUENCE_CLASS     "seq"
#define POPULATION_CLASS   "pop"
#define REFERENCE_CLASS    "ref"
#define HAPLOTYPE_CLASS    "hap"
#define HOMOZYGOUS_CLASS   "hom"
#define SCORE_CLASS        "score"
#define TOGGLE_BUTTON      "toggleButton"
#define TOO_MANY_HAPS      100

static void printWithSignificantDigits(double number, int min, int max,int digits)
// Prints out a frequency as a percent
{
if (number > max)
    hPrintf(">%d",max);
else if (number == max)
    hPrintf("%d",max);
else if (number < min)
    hPrintf("<%d",min);
else if (number == min)
    hPrintf("%d",min);
else if (number > pow(10,digits - 1) - 1) // special case when rounding close to max
    hPrintf("%.1f", number);
else
    {
    int places = digits;
    for ( ; places > 0; places--)
        {
        if (number >= pow(10,places - 1) - 1)
            {
            hPrintf("%.*f", (digits - places), number);
            return;
            }
        }
    hPrintf("%.*f", digits, number);
    }
}
#define printFreqAsPercent(freq) printWithSignificantDigits((freq) * 100,0,100,3)

static void geneAllelesTable(struct cart *cart,
                             struct haploExtras *he, struct haplotypeSet *hapSet,
                             boolean dnaView, boolean fullSeq, boolean tripletView)
// Print gene alleles HTML table.
{
int haploCount = slCount(hapSet->haplos);
assert(haploCount >= 1);
if (haploCount == 1 && haploIsReference(hapSet->haplos))
    {
    hPrintf("No non-reference alleles found for this gene model.\n");
    return;
    }
boolean diploid = differentWord(hapSet->chrom,"chrY"); // What, no aliens?
boolean showScores   = cartUsualBoolean(cart, HAPLO_SHOW_SCORES, FALSE);
boolean showRareHaps = cartUsualBoolean(cart, HAPLO_RARE_HAPS, FALSE);

struct haplotype *refHap = hapSet->refHap;
struct haplotype *haplo  = NULL;
int ix = 0;

// Need to set up DNA and AA sequences
//struct hapBlocks *hapBlocks = NULL;
if (!dnaView || fullSeq)
    {
    haploDnaSequence(he, hapSet, NULL, FALSE, FALSE);// No gaps or hilite so set in struct
    tolowers(refHap->ho->dnaSeq); // for ever after let this be lower case!
    if (!dnaView)
        geneHapSetAaVariants(he, hapSet, NULL);

    // Go ahead and set AA vals of all haplos up front.  This ensures titles have AA vals
    for (haplo = hapSet->haplos, ix=0; haplo != NULL && ix < TOO_MANY_HAPS;
         haplo = haplo->next, ix++)
        {
        haploDnaSequence(he, hapSet, haplo, FALSE, FALSE);// no gaps or hilite so set in struct
        if (!dnaView)
            geneHapSetAaVariants(he, hapSet, haplo);
        }
    }

// Sortable table
hPrintf("<TABLE id='" TABLE_ID "' class='sortable'><THEAD>");

// First Header Row
hPrintf("<TR valign='bottom' align='left'>");
hPrintf("<TD class='topHat andScore'%s><B>Haplotype</B></TD>", showScores ? " colspan=2" : "");
if (diploid)
    hPrintf("<TD class='topHat andScore'%s><B>Homozygous</B></TD>",showScores ? " colspan=2" : "");
int popGroupCount = 0;
struct popGroup *popGroups = NULL;
if (he->populationsToo)
    {
    popGroups = popGroupsGet(he,hapSet,he->populationsMinor);
    popGroupCount = slCount(popGroups);
    if (he->populationsMinor)
        hPrintf("<TD nowrap class='topHat andScore' colspan=%d "
                "title='Distribution of haplotypes across groups'>"
                "<B>Distribution across 1000 Genome groups</B></TD>",
                showScores ? popGroupCount + 1 : popGroupCount);
    else
        hPrintf("<TD nowrap class='topHat andScore' colspan=%d "
                "title='Distribution of haplotypes across groups'>"
                "<B>Distribution across major groups</B></TD>",
                showScores ? popGroupCount + 1 : popGroupCount);
    }

hPrintf("<TD class='topHat'>&nbsp;</TD>");
hPrintf("<TD class='topHat' colspan=%d><B>Variant Sites</B></TD>",refHap->variantsCovered);
if (fullSeq)
    {
    if (!dnaView && tripletView)
        hPrintf("<TD class='topHat dnaToo'><B><code>%s</code></B></TD>",refHap->ho->dnaSeq);
    else
        hPrintf("<TD nowrap class='topHat'><B>Predicted full sequence</B></TD>");
    }
hPrintf("</TR>\n");

// Second Header Row
// Note all headers must have id's for persistent sort order
hPrintf("<TR class='" ALLELE " sortable' valign='bottom'>");
hPrintf("<TH nowrap id='" HAPLOTYPE_CLASS "F' abbr='use'>Freq %%</TH>");// abbr='use': sort
hPrintf("<TH id='" HAPLOTYPE_CLASS "S' class='" SCORE_CLASS "%s' abbr='use' title="
        "'Length normalized probability of occurring by chance "
        "\n(log 10, above zero means more frequent than expected).' nowrap>Score</TH>",
        (showScores ? "" : " hidden"));
if (diploid)
    {
    hPrintf("<TH nowrap id='" HOMOZYGOUS_CLASS "F' abbr='use' title='Homozygous or Haploid'>"
            "Freq %%</TH>");
    hPrintf("<TH id='" HOMOZYGOUS_CLASS "S' class='" SCORE_CLASS "%s' abbr='use' title="
            "'Given haplotype frequency, probability of occurring homozygous by chance \n"
            "(log 10, below zero means less frequent than expected).' nowrap>"
            "Score</TH>",(showScores ? "" : " hidden"));
    }

// Show populations
if (he->populationsToo)
    {
    struct popGroup *popGroup = popGroups;
    for( ; popGroup != NULL; popGroup = popGroup->next)
        {
        hPrintf("<TH nowrap id='%s' abbr='use'"
                " title='%s [N=%d]'>%s %%</TH>",
                popGroup->name, popGroup->desc, popGroup->chromN, popGroup->name);
        }
    hPrintf("<TH id='" POPULATION_CLASS "S' class='" SCORE_CLASS "%s' abbr='use' title="
            "'Population score is calculated as the fixation index (Fst) using the frequencies "
            "of occurrence within groups as opposed to distribution across groups. "
            "Higher scores are more skewed across groups and affecting more individuals.'"
            ">Score</TH>",(showScores ? "" : " hidden"));
    }
hPrintf("<TH abbr='use' id='" REFERENCE_CLASS "'>Ref</TH>");

int varIx=0;
for ( ;varIx < refHap->variantsCovered; varIx++)
    {
    char *val = dnaView ? refHap->variants[varIx]->val
                        : refHap->variants[varIx]->aaVal;

    // Text hint to display the location
    char *varId = haploVarId(refHap->variants[varIx]);
    hPrintf("<TH id='%s' class='" VARIANT_CLASS " %s' title='%s'>",
            varId,varId,
            refVaraintTitle(hapSet,refHap->variants[varIx],dnaView));

    hPrintf("%s</TH>",clipLongVals(val,5,'-'));

    // Force these reference variants to lower case for future use!
    if (dnaView && val != NULL && val[0] != '\0')
        tolowers(val);
    }

if (fullSeq)
    {
    char *fullSeq = NULL;
    if (dnaView)
        fullSeq = haploDnaSequence(he, hapSet, NULL, TRUE, TRUE);// gaps and hilite
    else
        {
        assert(refHap->ho != NULL && refHap->ho->aaSeq != NULL);
        fullSeq = haploAaSequence(he, hapSet, NULL,tripletView,TRUE);
        }
    if (dnaView || !tripletView)
        hPrintf("<TH id='" SEQUENCE_CLASS "' class='" SEQUENCE_CLASS "' nowrap abbr='use'"
                " title='Full reference sequence (sorts on length)'>"
                "<code>%s</code></TH>", fullSeq );
    else // AA aligned to DNA triplets
        hPrintf("<TH id='" SEQUENCE_CLASS "' class='" SEQUENCE_CLASS "' nowrap abbr='use'"
                " title='Full reference sequence (sorts on length)'>"
                "<code>%s</code></TH>",fullSeq);
    }
hPrintf("</TR></THEAD><TBODY>\n");

float minFreq = 0.01;
boolean countHidden = 0, countRows = 0, countLowInterest = 0;
for (haplo = hapSet->haplos, ix=0; haplo != NULL && ix < TOO_MANY_HAPS; haplo = haplo->next, ix++)
    {
    if (haplo->subjects == 0) // reference haplotype may not be found in 1000 geneomes data!
        continue;

    float hapFreq = (float)haplo->subjects/hapSet->chromN;

    // Hide low interest alleles
    countRows++;
    if (hapFreq < minFreq)
        {
        hPrintf("<TR class='" ALLELE " rare%s'>",(showRareHaps ? "" : " hidden"));
        countLowInterest++;         // Count these regardless of whether they are shown
        if (!showRareHaps)
            countHidden++;
        }
    else
        hPrintf("<TR class='" ALLELE "'>");

    // First some numbers
    hPrintf("<TD abbr='%05d' title='N=%d of %d'>",
            (10000 - haplo->subjects), // (1 - hapFre) sorts descending
            haplo->subjects,hapSet->chromN );
    printFreqAsPercent(hapFreq);
    hPrintf("</TD>");

    // Would be nice to improve score to better highlight surprise
    hPrintf("<TD class='" SCORE_CLASS "%s' abbr='%08.1f'>",(showScores ? "" : " hidden"),
            99999 - haplo->haploScore);
    printWithSignificantDigits(haplo->haploScore, -1000, 1000,2);
    hPrintf("</TD>");

    if (diploid)
        {
        float homFreq = (float)haplo->homCount/hapSet->subjectN;
        if (haplo->homCount == 0)
            hPrintf("<TD abbr='20000' title='Allele not found homozygous'>");
        else
            hPrintf("<TD abbr='%05d' title='N=%d of %d'>",
                    (10000 - haplo->homCount), haplo->homCount, hapSet->subjectN);
        printFreqAsPercent(homFreq);
        hPrintf("</TD>");

        hPrintf("<TD class='" SCORE_CLASS "%s' abbr='%08.1f'>",(showScores ? "" : " hidden"),
                99999 - haplo->homScore);
        if (haplo->homCount == 0 && haplo->homScore == 0)
            hPrintf("&nbsp;");
        else
            printWithSignificantDigits(haplo->homScore, -1000, 1000,2);
        hPrintf("</TD>");
        }

    // Show populations
    if (he->populationsToo)
        {
        assert(haplo->ho != NULL);
        struct popGroup *popGroup = popGroups;
        for( ; popGroup != NULL; popGroup = popGroup->next)
            {
            size_t count = (size_t)slPairFindVal(haplo->ho->popGroups, popGroup->name);
            double popDist = (double)count / haplo->subjects;
            double popFreq = (double)count / popGroup->chromN;
            if (count > 0)
                {
                hPrintf("<TD class='%s' abbr='%06.4f' title='N=%d of %d (found in ",popGroup->name,
                        ((double)1 - popDist),(int)count,haplo->subjects);
                printFreqAsPercent(popFreq);
                hPrintf("%% of all %s)'>",popGroup->name);
                }
            else
                hPrintf("<TD class='%s' abbr='10000' title='Haplotype not found in %s group'>",
                        popGroup->name,popGroup->name);
            printFreqAsPercent(popDist);
            hPrintf("</TD>");
            }
        hPrintf("<TD class='" SCORE_CLASS "%s' abbr='%0.4f'>",
                (showScores ? "" : " hidden"),1 - haplo->ho->popScore);
        printWithSignificantDigits(haplo->ho->popScore, 0, 1,4);
        hPrintf("</TD>");
        }

    // Reference haplotype or not?
    if (haplo->variantCount > 0)
        hPrintf("<TD abbr='%s' class='" REFERENCE_CLASS "'>&nbsp;</TD>",haplo->suffixId);
    else
        hPrintf("<TD abbr='' class='" REFERENCE_CLASS "' "
                "title='This haplotype matches the reference assembly.'>ref</TD>");

    // Need to set AA variants haplotype by haplotype, because of shared memory
    if (!dnaView)
        geneHapSetAaVariants(he, hapSet, haplo);

    int bitIx = 0,varSlot = 0;
    for (bitIx = 0, varIx = 0; bitIx < haplo->bitCount; varSlot++, bitIx += hapSet->bitsPerVariant)
        {
        struct hapVar *varToUse = refHap->variants[varSlot];
        // Only care that a bit is set, not which or how many
        if (bitCountRange(haplo->bits, bitIx, hapSet->bitsPerVariant) > 0)
            {
            // Text hint to display the location
            hPrintf("<TD class='" VARIANT_CLASS " %s'>",
                    haploVarId(varToUse)); // reference var

            varToUse = haplo->variants[varIx++]; // Now set this to var allele
            }
        else
            hPrintf("<TD>");

        // Due to frame shifts and premature stops, a haplotype with a ref variant
        // may have no AA in that spot!
        char *val = varToUse->val;
        if (!dnaView)
            {
            val = varToUse->aaVal;
            assert(haplo->ho != NULL);
            if (varToUse->aaOffset == haplo->ho->aaLen)
                val = "]";
            else if (varToUse->aaOffset > haplo->ho->aaLen)
                val = " ";
            }

        if (varToUse != refHap->variants[varSlot])
            hPrintf(" <A HREF='%s' target='ucscDetail'>%s</A></TD>",
                    refVariantLink(cart,hapSet,varSlot),
                    clipLongVals(val,5,'-')); // Note: space before should bring var to top of sort
        else
            hPrintf("%s</TD>",clipLongVals(val,5,'-'));
        }

    // Handle full sequence
    if (fullSeq)
        {
        char *fullSeq = NULL;
        int seqLen = 0;
        if (dnaView)
            {
            fullSeq = haploDnaSequence(he, hapSet, haplo, TRUE, TRUE);// gaps and hilite
            if (haplo != refHap)
                {
                haploDnaSequence(he, hapSet, haplo, FALSE, FALSE);// no gaps
                //fullSeq = haplo->dnaSeq;
                }
            assert(haplo->ho != NULL);
            seqLen = haplo->ho->dnaLen;
            }
        else
            {
            assert(refHap->ho != NULL && refHap->ho->aaSeq != NULL
                 && haplo->ho != NULL &&  haplo->ho->aaSeq != NULL);
            char *refSeq = haploAaSequence(he, hapSet, NULL, tripletView,FALSE);
            char *nonRef = haploAaSequence(he, hapSet, haplo,tripletView,FALSE);
            fullSeq = sequenceDiffsHilite(refSeq,nonRef,"<B class='textAlert'>","</B>");
            seqLen = haplo->ho->aaLen;
            }
        if (dnaView)
	    {
            hPrintf("<TD class='" SEQUENCE_CLASS "' abbr='%08d' nowrap>"
                    "<code>%s</code></TD>", seqLen, fullSeq );
	    }
        else if (!tripletView)
	    {
            hPrintf("<TD class='" SEQUENCE_CLASS "' abbr='%08d' nowrap>"
                    "<code id='" SEQUENCE_CLASS "_2'>%s</code></TD>",
                    seqLen, fullSeq );
	    jsOnEventById("mousemove", SEQUENCE_CLASS "_2", "alleles.positionTitle(event,this);");
	    }
        else // AA aligned to DNA triplets
	    {
            hPrintf("<TD id='" SEQUENCE_CLASS "_2' class='" SEQUENCE_CLASS "' abbr='%08d' nowrap><code>%s</code>"
                    "<BR><code id='" SEQUENCE_CLASS "_2'>%s</code></TD>",
                    seqLen, sequenceDiffsHilite(refHap->ho->dnaSeq,haplo->ho->dnaSeq,
                                                "<B class='textAlert'>","</B>"), fullSeq);
	    jsOnEventById("mousemove", SEQUENCE_CLASS "_2", "alleles.positionTitle(event,this);");
	    }
        }
    hPrintf("</TR>\n");
    }
hPrintf("</TBODY></TABLE>\n");

// Show/Hide rare haplotypes button
if (countLowInterest > 0)
    {
    if (countHidden == 0)
        hPrintf("<span class='textAlert' id='alleleCounts'>"
                "All gene haplotypes shown: %d of %d.</span>\n",
                (countRows - countHidden), countRows);
    else
        hPrintf("<span class='textOpt' id='alleleCounts'>"
                "Common gene haplotypes shown: %d of %d.</span>\n",
                (countRows - countHidden), countRows);

    // If there were too many, say so
    if (haploCount >= TOO_MANY_HAPS)
        hPrintf("&nbsp;&nbsp;<span class='textAlert'>A total of %d haplotypes were discovered, "
                "which is too many to be displayed.  Only the most frequently occurring "
                "%d have been included.</span>\n", haploCount, TOO_MANY_HAPS);

    hPrintf("<BR><input type='button' id='" HAPLO_RARE_HAPS "' value='%s rare haplotypes' "
	    "class='" TOGGLE_BUTTON "' "
            "title='Show/Hide rare haplotypes that occur with a frequency of less than %g%%'>",
            (showRareHaps ? "Hide":"Show"),(minFreq * 100));
    jsOnEventById("click", HAPLO_RARE_HAPS, "alleles.rareAlleleToggle(this);");
    }
else
    {
    hPrintf("<BR>Common gene alleles shown: %d<BR>\n",countRows);
    }
// Population distribution buttons
boolean distMajor =  cartUsualBoolean(cart, HAPLO_MAJOR_DIST, FALSE);
boolean distMinor =  cartUsualBoolean(cart, HAPLO_MINOR_DIST, FALSE);
hPrintf("<input type='button' id='" HAPLO_MAJOR_DIST "' class='" TOGGLE_BUTTON "' "
        "value='%s populations' "
        "title='Show/Hide haplotype distribution across population groups'>",
        (distMajor ? "Hide": "Show"));
jsOnEventByIdF("click", HAPLO_MAJOR_DIST, "alleles.setAndRefresh(this.id,'%s');", (distMajor ? "[]":"1"));
if (distMajor)
    {
    hPrintf("<input type='button' id='" HAPLO_MINOR_DIST "' class='" TOGGLE_BUTTON "' "
            "value='%s' "
            "title='Show haplotype distribution across %s population groups'>",
            (distMinor ? "Major groups" : "1000 Genome groups"),
            (distMinor ? "major" : "1000 Genome"));
    jsOnEventByIdF("click", HAPLO_MINOR_DIST, "alleles.setAndRefresh(this.id,'%s');", (distMinor ? "[]":"1"));
    }

hPrintf("<input type='button' id='" HAPLO_SHOW_SCORES "' value='%s scoring' "
	"class='" TOGGLE_BUTTON "' "
        "title='Show/Hide all haplotype scores'>\n",(showScores ? "Hide":"Show"));
jsOnEventById("click", HAPLO_SHOW_SCORES, "alleles.scoresToggle(this);");

hPrintf("&nbsp;&nbsp;<input type='button' id='reset' value='Reset to defaults' "
        "class='" TOGGLE_BUTTON "' "
        "title='Reset all Gene haplotype alleles options back to default'>\n");
jsOnEventById("click", "reset", "return alleles.setAndRefresh('"HAPLO_RESET_ALL"',1);");

}

void geneAllelesTableAndControls(struct cart *cart, char *geneId,
                                 struct haploExtras *he, struct haplotypeSet *hapSet)
// Outputs the Gene Haplotype Alleles HTML table with button controls and AJAX replaceable div.
{
int haploCount = (hapSet == NULL ? 0 : slCount(hapSet->haplos));
boolean tooManyHaps = (haploCount > TOO_MANY_HAPS * 10); // For the present, the first N are shown
boolean noHaps = (tooManyHaps  // too many means show none!
                 || hapSet == NULL|| (haploCount == 1 && haploIsReference(hapSet->haplos)));

// alleles table will require ajax
jsIncludeFile("ajax.js",NULL);
jsIncludeFile("alleles.js",NULL);

// All content might be updated via ajax.  This defines the boundary
hPrintf("<div id='" HAPLO_TABLE "'><!-- " HAPLO_TABLE " begin -->\n");

// Brief intro
hPrintf("Generated from <A HREF='http://www.1000genomes.org/' TARGET=_BLANK>1000 Genomes</A> "
        "Phase1 variants (<A HREF='../goldenPath/help/haplotypes.html' "
        "title='Help on Gene haplotype alleles section' TARGET=_BLANK>help</A>). "
	"Note the association of SNP alleles within a haplotype is statistically imputed rather than directly "
	"observed in most cases.");

boolean rareVars     =  cartUsualBoolean(cart, HAPLO_RARE_VAR, FALSE);
boolean dnaView      =  cartUsualBoolean(cart, HAPLO_DNA_VIEW, FALSE);
boolean fullGeneView =  cartUsualBoolean(cart, HAPLO_FULL_SEQ, FALSE);
boolean tripletView  =  FALSE;
// Support toggling between common and rare variants
int variantCount = (haploCount == 0 ? 0 : hapSet->variantsCovered);
boolean tooManyVars = (variantCount > TOO_MANY_HAPS * 2);
if (!tooManyHaps && tooManyVars)
    {
    tooManyHaps = noHaps = TRUE;
    }

if (rareVars)
    {
    hPrintf(" <div id='alleleRareVars' class='textAlert'>All %d variant site%s "
            "(includes synonymous and rare variants with a frequency of less than %d%%).</div>\n",
            variantCount,(variantCount != 1 ? "s":""),HAPLO_COMMON_VARIANT_MIN_PCT);
    }
else
    {
    hPrintf(" <div id='alleleRareVars' class='textOpt'>"
            "Restricted to %d non-synonymous, common variant site%s with a frequency of "
            "occurrence of at least %g%%.</div>\n",variantCount,
            (variantCount != 1 ? "s":""),he->variantMinPct);
    }

// Table of buttons
hPrintf("<table BORDER=0 CELLSPACING=1 CELLPADDING=0><tr>\n");

// Restrict to common variants
if (!noHaps
||  (tooManyHaps == rareVars)) // too many and and rare or not too many and not rare
    {
    hPrintf("<td><input type='button' id='" HAPLO_RARE_VAR "' class='" TOGGLE_BUTTON "' "
            "value='%s' title='Include or Restrict to non-synonymous, common "
            "variants with a frequency of at least %d%%'></td>\n",
            (rareVars ? "Common variants only":"Include all variants"),
            HAPLO_COMMON_VARIANT_MIN_PCT);
    jsOnEventByIdF("click", HAPLO_RARE_VAR, "alleles.setAndRefresh(this.id,'%s');", (rareVars ? "[]":"1"));
    }

// DNA vs. AA, Full sequence
if (!noHaps)
    {
    hPrintf("<td><input type='button' id='" HAPLO_DNA_VIEW "' class='" TOGGLE_BUTTON "' "
            "value='Display as %s' "
            "title='Display variants and sequence as amino acids or DNA bases'></td>\n",
            (dnaView ? "amino acids" : "DNA bases"));
    jsOnEventByIdF("click", HAPLO_DNA_VIEW, "alleles.setAndRefresh(this.id,'%s');", (dnaView ? "[]":"1"));

    hPrintf("<td><input type='button' id='" HAPLO_FULL_SEQ "' class='" TOGGLE_BUTTON "' "
            "value='%s full sequence' "
            "title='Show/Hide predicted full sequence of each gene haplotype'></td>\n",
            (fullGeneView ? "Hide" : "Show"));
    jsOnEventByIdF("click", HAPLO_FULL_SEQ, "alleles.setAndRefresh(this.id,'%s');", (fullGeneView ? "[]": "1" ));

    tripletView  = cartUsualBoolean(cart, HAPLO_TRIPLETS,FALSE);
    if (!dnaView && fullGeneView)
	{
        hPrintf("<td><input type='button' id='" HAPLO_TRIPLETS "' class='" TOGGLE_BUTTON "' "
                "value='%s DNA triplet code' "
                "title='Show/Hide DNA sequence above amino acid sequence'>"
                "</td>\n", (tripletView ? "Hide" : "Show"));
	jsOnEventByIdF("click", HAPLO_TRIPLETS, "alleles.setAndRefresh(this.id,'%s');", (tripletView ? "[]": "1" ));
	}
    }
hPrintf("</tr></table>\n");

if (!noHaps)
    {
    // Say something if this is the negative strand
    if (hapSet->strand[0] == '-')
        hPrintf("<div class='textInfo'>All variations and sequence reflect the "
                "negative ('-' or \"reverse\") strand.</div>\n");

    // Sort in most popular order and make sure the haplotypes are oriented to their strand
    slSort(&(hapSet->haplos),haplotypePopularityCmp);
    slReverse(&(hapSet->haplos));
    haploSetStrandOrient(he,hapSet); // haplos should be oriented as gene strand
    }

// Any haplotypes to show?
if (tooManyHaps)
    {
    if (tooManyVars)
        hPrintf("<div class='textAlert'>%d haplotypes were found using %d varaints.  "
                "This is too many to be displayed."
                "</div>\n",haploCount,variantCount);
    else
        hPrintf("<div class='textAlert'>%d haplotypes were found which is too many to be displayed."
                "</div>\n",haploCount);
    }
else if (noHaps)
    hPrintf("<BR>0 non-reference haplotypes were found using %s variants.<BR>\n",
            rareVars ?
            "<span class='textOpt'>common</span> or <span class='textAlert'>rare</span>"
          : "<span class='textOpt'>common</span>");
else // Finally the table of haplotype alleles
    geneAllelesTable(cart, he, hapSet, dnaView, fullGeneView, tripletView );

// Close the container for ajax update.
hPrintf("<!-- " HAPLO_TABLE " end --></div>\n");
}

// --------------------------------------
// haplotype reading and writing routines
// --------------------------------------

// bed 12 +
#define HAPLO_BED "CREATE TABLE %s ("                                  \
                  "    bin smallint unsigned not null,\n"               \
                  "    chrom varchar(255) not null,\n"                   \
                  "    chromStart int unsigned not null,\n"               \
                  "    chromEnd int unsigned not null,\n"                  \
                  "    name varchar(255) not null,     # commonId:suffix\n" \
                  "    score int unsigned not null,\n"                       \
                  "    strand char(1) not null,\n"                            \
                  "    thickStart int unsigned not null, # Meaningless here\n" \
                  "    thickEnd int unsigned not null,  # Meaningless here\n"   \
                  "    itemRgb int unsigned not null,  # Item R,G,B color\n"     \
                  "    variantCount int unsigned not null, # Number of blocks\n"  \
                  "    variantSizes longblob null,   # Comma delimited list\n"     \
                  "    variantStarts longblob null, # relative to chromStart\n"     \
                  "    variantVals longblob null,  # non-ref values\n"               \
                  "    variantFreqs longblob null,# variant frequencies in dataset\n" \
                  "    variantIds longblob null,                # variant IDs\n"       \
                  "    variantsCovered int unsigned not null, # model variant count\n"  \
                  "    bits varchar(1024) null,             # bits as string\n"          \
                  "    commonId varchar(255) not null,    # accession less suffix\n"      \
                  "    secondId varchar(255) not null,  # typically gene/protein Id \n"    \
                  "    haploScore float not null,     # haplotype scoring statistic\n"      \
                  "    homScore float not null,        # homozygous scoring statistic\n"     \
                  "    popScore float not null,          # population scoring statistic\n"   \
                  "    homCount int unsigned not null,     # homozygous or haploid\n"       \
                  "    subjectCount int unsigned not null,   # subject chromosomes\n"     \
                  "    subjectIds longblob null, # Subject IDs: d1-a,id1-b,id2-b...\n" \
                  "    INDEX(chrom(8),bin),\n"          \
                  "    INDEX(chrom(8),chromStart),\n"    \
                  "    INDEX(name(20)),\n"                \
                  "    INDEX(secondId(20),name(20)) );\n"

#define HAPLO_STD_BED_FIELDS     12
enum geneAlleleTableFields // enum of geneAlleles table columns (0-based index)
    {
    gafVarVals = HAPLO_STD_BED_FIELDS,  // first non-std-bed column index
    gafVarFrequencies,
    gafVarIds,
    gafVarsCovered,
    gafBits,
    gafCommonId,
    gafSecondId,
    gafHaploScore,
    gafHomScore,
    gafPopScore,
    gafHomCount,
    gafSubjects,
    gafSubjectIds,
    gafFieldCount  // Last field of enum keeps track of size
    };
//#define HAPLO_BED_PLUS_FIELDS    (gafFieldCount - HAPLO_STD_BED_FIELDS)

static boolean haplotypeLoadFromRow(char *tableOrFile,struct haploExtras *he,
                                    struct haplotypeSet **pHaploSets, char **row)
// Loads a single gene haplotype from row of file or table
// Returns TRUE if a new haplotypeSet was added to the list
// NOTE: haplotypes loaded from file/table will be incomplete!
{
struct bed *bed = bedLoadN(row, HAPLO_STD_BED_FIELDS);  // Index to count
verbose(3,"Loaded one row from %s at %s:%d-%d %s\n",
        tableOrFile,bed->chrom,bed->chromStart,bed->chromEnd,bed->name);

// First fond or make haplotypeSet
// Note that is should be efficient if the input file is sorted on commonId;
boolean isNewHapSet = FALSE;
struct haplotypeSet *hapSet = *pHaploSets;
for ( ; hapSet != NULL && differentString(hapSet->commonId,row[gafCommonId]);
        hapSet = hapSet->next)
    ;

if (hapSet == NULL)
    {
    lmAllocVar(he->lm,hapSet);
    hapSet->chrom           = lmCloneString(he->lm,bed->chrom);
    hapSet->chromStart      = bed->thickStart;
    hapSet->chromEnd        = bed->thickEnd;
    hapSet->wideStart       = bed->chromStart;
    hapSet->wideEnd         = bed->chromEnd;
    hapSet->strand[0]       = bed->strand[0];
    hapSet->strand[1]       = '\0';
    hapSet->commonId        = lmCloneString(he->lm,row[gafCommonId]);
    if (strlen(row[gafSecondId]) > 0)
        hapSet->secondId    = lmCloneString(he->lm,row[gafSecondId]);
    hapSet->model           = NULL;  // Don't have it (could look it up?)
    hapSet->isGenePred      = FALSE; // How can we know?
    slAddHead(pHaploSets,hapSet);
    isNewHapSet = TRUE;
    }

// bed 12 + 11
// bed12: chrom chromStart chromEnd commonId:suffix score(hapCount) strand cdsStart cdsEnd
// bed12:  variantCount len,len,len start,start,start
// bed12+: val,val,val prob,prob,prob id,id,id bitmap commonId secondId
// bed12+: haploScore homScore homoCount subjectCount subjectId,subjectId,subjectId
struct haplotype *haplo = haplotypeDefault(he, bed->score);

char *suffix = strchr(bed->name,':');
if (suffix == NULL) // With suffix means non-reference haplotype
    haplo->suffixId       = "";
else //if (suffix != NULL) // With suffix means non-reference haplotype
    {
    *suffix++ = '\0';
    haplo->suffixId       = lmCloneString(he->lm,suffix);
    haploOptionalsEnable(he,haplo);
    haplo->generations = suffixDepth(haplo->suffixId);

    // There should be variants if this is a non-reference haplotype
    haplo->variantCount  = bed->blockCount;
    if (haplo->variantCount > 0)
        {
        haplo->variants      = lmAlloc(he->lm,sizeof(struct varaiant *) * haplo->variantCount);

        // VALUES: Could be zero length if only 1 variant that is deletion
        assert(row[gafVarVals] != NULL
            && countChars(row[gafVarVals],',') == haplo->variantCount - 1);
        char *vals = lmAlloc(he->lm,strlen(row[gafVarVals]) + 5);
        strcpy(vals,row[gafVarVals]);
        strSwapChar(vals,',','\0');

        // PROBABILITIES: floating point
        assert(row[gafVarFrequencies] != NULL
            && countChars(row[gafVarFrequencies],',') == haplo->variantCount - 1);
        char *freqs = lmAlloc(he->lm,strlen(row[gafVarFrequencies]) + 5);
        strcpy(freqs,row[gafVarFrequencies]);
        strSwapChar(freqs,',','\0');

        // Single variant without ID would have length == 0
        assert(row[gafVarIds] != NULL
            && countChars(row[gafVarIds],',') == haplo->variantCount - 1);
        char *ids = lmAlloc(he->lm,strlen(row[gafVarIds]) + 5);
        strcpy(ids,row[gafVarIds]);
        strSwapChar(ids,',','\0');

        int ix = 0;
        for ( ;ix < haplo->variantCount; ix++)
            {
            struct hapVar *variant;
            lmAllocVar(he->lm,variant);
            haplo->variants[ix] = variant;
            variant->chromStart = hapSet->chromStart + bed->chromStarts[ix];
            variant->chromEnd   = variant->chromStart + bed->blockSizes[ix];
            variant->id         = ids;
            variant->val        = vals;
            variant->frequency = sqlDouble(freqs);
            ids  += strlen(ids) + 1;
            vals += strlen(vals) + 1;
            freqs += strlen(freqs) + 1;
            // TODO: fill in vType somehow?
            }
        // NOTE: no slList of refHap->variants!
        }
    }

// Bitmap?
if (row[gafBits] != NULL)
    {
    haplo->bitCount   = strlen(row[gafBits]);
    haplo->bits = bitsIn(he->lm,row[gafBits], haplo->bitCount);
    }

// more counts, scores and subjectIds
haplo->variantsCovered = sqlSigned(row[gafVarsCovered]);
haplo->haploScore     = sqlDouble(row[gafHaploScore]);
haplo->homScore      = sqlDouble(row[gafHomScore]);
haplo->homCount     = sqlSigned(row[gafHomCount]);
haplo->subjects    = sqlSigned(row[gafSubjects]);
haplo->subjectIds = lmCloneString(he->lm,row[gafSubjectIds]);

if (he->populationsToo)
    {
    haploOptionalsEnable(he,haplo);
    if (differentString(row[gafPopScore],"nan")
    &&  differentString(row[gafPopScore],"0"  ))
        {
        haploOptionalsEnable(he,haplo);
        haplo->ho->popScore   = sqlDouble(row[gafPopScore]);
        }
    }

bedFree(&bed);

// Now finish out hapSet if needed
if (hapSet->refHap == NULL && haploIsReference(haplo))
    {
    hapSet->variantsCovered = haplo->variantsCovered;
    if (haplo->variantsCovered != 0)
        hapSet->bitsPerVariant  = haplo->bitCount / haplo->variantsCovered;
    hapSet->refHap = haplo;
    }

if (!haploIsReference(haplo) || haplo->subjects > 0)
    slAddHead(&(hapSet->haplos),haplo);

return isNewHapSet;
}

static int haploSetsPolishAfterLoad(struct haploExtras *he, struct haplotypeSet *haploSets)
// Returns number of haplotype sets that needed a refHap added
{
int hapSetCount = 0;
int refHapsAdded = 0;
struct haplotypeSet *hapSet = haploSets;
for ( ; hapSet != NULL; hapSet = hapSet->next)
    {
    hapSetCount++;

    // sort the most popular haplotypes first
    assert(hapSet->haplos != NULL);
    slSort(&(hapSet->haplos),haplotypePopularityCmp);

    // Count up the number of chroms covered
    hapSet->chromN = 0;
    struct haplotype *haplo = hapSet->haplos;
    for ( ; haplo != NULL; haplo = haplo->next)
        {
        hapSet->chromN += haplo->subjects;
        if (haploIsReference(haplo))
            hapSet->refHap = haplo;
        }

    // Total subjects derived from total chroms covered
    if (sameString(hapSet->chrom,"chrY"))
        hapSet->subjectN = hapSet->chromN;
    else if (differentString(hapSet->chrom,"chrX"))
        hapSet->subjectN = hapSet->chromN / 2;
    else // for chrX: chromN depends upon PAR or non-PAR
        hapSet->subjectN = THOUSAND_GENOME_SAMPLES; // Cheating
    if (hapSet->refHap == NULL)
        {
        hapSet->refHap = haplotypeForReferenceGenome(he,hapSet,NULL,0,hapSet->chromN);
        refHapsAdded++;
        }
    if (verboseLevel() >= 2)
        haploSetsPrintStats(he, stderr, hapSet->commonId, hapSet, 1);

    }
verbose(1, "Completed %d haploSets with %d refHaps added\n", hapSetCount, refHapsAdded);
return refHapsAdded;
}

int haploSetsLoadFromFile(char *inFile ,struct haploExtras *he,
                          struct haplotypeSet **pHaploSets)
// Loads a haplotypes bed file (bed12_5)
// Returns count of rows read.  The pGeneHapList will be returned in file order
// NOTE: haploSets loaded from file/table will be incomplete!
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
//int readCount = haplotypesLoadOneFile(lf,he,pGeneHapList);
int readCount = 0;

verbose(1, "Reading %s as haplotypes bed file.\n", lf->fileName);
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *words[gafFieldCount+1];
    int wordCount = chopTabs(line, words); // Tab delimited only!  No need to duplicate
    // ignore empty lines
    if (0 == wordCount)
        continue;
    lineFileExpectWords(lf, gafFieldCount, wordCount);

    haplotypeLoadFromRow(lf->fileName,he,pHaploSets,words);
    readCount++;
    }
// Need to complete the resulting hapSets
slReverse(pHaploSets);
haploSetsPolishAfterLoad(he, *pHaploSets);

verbose(1, "Loaded %d rows from %s\n", readCount, lf->fileName);

lineFileClose(&lf);

return readCount;
}

static void printHistogram(FILE *out, int *hist, int buckets, int firstN, char *title,char *label)
// Prints out histogram of "***"s for a given number of buckets, will size the lines
// based upon the firstN bucket values
{
// histograms
char *dots = "*****************************************************************";
int scale = 0;
int ix = 0;
for ( ;ix < firstN; ix++)
    scale = max(scale, hist[ix]);
int div = ceil(scale/(strlen(dots) * 0.8)); // Aim scale bucket to 80% of the distance
if (div < 1)
    div = 1;

fprintf(out, "%s:\nn   %s",title,label);
if (div > 1)
    fprintf(out, " (each * == %d %s)",div, label);
for (ix=0;ix < buckets - 1;ix++)
    fprintf(out, "\n%-2d   %5d %-.*s",
            ix,hist[ix],(int)ceil(hist[ix]/div),dots);
fprintf(out,      "\n>=%-2d %5d %-.*s\n\n",
            ix,hist[ix],(int)ceil(hist[ix]/div),dots);
}


void haploSetsPrintStats(struct haploExtras *he, FILE *out, char *commonToken,
                         struct haplotypeSet *haploSets, int limitModels)
// verbose simple stats
{
// Helpful tallying macros
#define COUNT_IF(count,condition)   { if (condition) (count)++; }
#define ACCUM(tally,val)   {                    (tally) += (val); }
#define SUM_SQ(sumsq,val)  {                    (sumsq) += ((bits64)(val) * (val)); }
#define TALLY_MAX(max,val) { if ((max) < (val)) (max)    = (val); }
#define TALLY_TWO(sumsq,max,val) { SUM_SQ(sumsq,val); TALLY_MAX(max,val); }
#define TALLY_ALL(tally,sumsq,max,val) { ACCUM(tally,val); TALLY_TWO(sumsq,max,val); }
#define TALLY_HIST(hist, count, last) { if ((count) < (last)) (hist)[count]++; \
                                                         else (hist)[last ]++; }

// basic counters: sets, haplotypes, non-reference, homozygous/haploid
int setsFound = 0, setsNr = 0,                                  setsHom    = 0;
int hapsFound = 0, hapsNr = 0, hapsNrForSet = 0, hapsNrMax = 0, homsForSet = 0;
//int wowsForSet = 0, setsWow = 0; // "wow" is hom/haploid or haploScore is high
//int wowTrigger = (he->variantMinPct == 1 ? 1000 :
//                  he->variantMinPct <= 5 ? 1000 - (100 * he->variantMinPct) : 500 );

// Towards stddev: subjects, variants, scores
int    subjects       = 0, subjectMax    = 0, variants       = 0, variantMax    = 0;
bits64                     subjectSumSq  = 0,                     variantSumSq  = 0;
double hapScoreSetMax = 0, hapScoreMax   = 0, homScoreSetMax = 0, homScoreMax   = 0;
bits64 hapsNrSumSq    = 0, hapScoreSumSq = 0, homScoreSumSq  = 0, popScoreSumSq = 0;
double popScoreSetMax = 0, popScoreMax   = 0;

// histogram of haplotypes per hapSet
#define HAPLO_HIST_BUCKETS 21
#define HAPLO_HIST_MAX_IX  (HAPLO_HIST_BUCKETS - 1)
int hapHist[HAPLO_HIST_BUCKETS];
int ix = 0;
for ( ;ix < HAPLO_HIST_BUCKETS;ix++)
    hapHist[ix] = 0;

struct haplotypeSet *hapSet = haploSets;
for ( ;hapSet != NULL; hapSet = hapSet->next)
    {
    struct haplotype *haplo = hapSet->haplos;
    for ( ;haplo != NULL; haplo = haplo->next)
        {
        if (he->homozygousOnly && haplo->homCount <= 0)
            continue;

        COUNT_IF(hapsFound,(haplo->subjects > 0));
        //COUNT_IF(wowsForSet,( haplo->haploScore >= wowTrigger &&
        //                     (haploIsReference(haplo) || haplo->homCount > 0)));

        TALLY_ALL(subjects,subjectSumSq,subjectMax,haplo->subjects); // 1kGenome chroms (>2000)
        TALLY_ALL(variants,variantSumSq,variantMax,haplo->variantCount);

        // accumulating haplotype counts where there are non-reference alleles
        if (!haploIsReference(haplo))
            {
            hapsNrForSet++;
            COUNT_IF(homsForSet,(haplo->homCount > 0)); // evidence of homozygous or haploid
            }

        // p-Value scores
        TALLY_MAX(hapScoreSetMax,haplo->haploScore);
        TALLY_MAX(homScoreSetMax,haplo->homScore);
        if (he->populationsToo && haplo->ho != NULL)
            TALLY_MAX(popScoreSetMax,haplo->ho->popScore);
        }

    // set level totals
    setsFound++;
    COUNT_IF(setsNr, (hapsNrForSet > 0));
    COUNT_IF(setsHom,(homsForSet   > 0));
    //COUNT_IF(setsWow,(wowsForSet   > 0));

    TALLY_ALL(hapsNr,hapsNrSumSq,  hapsNrMax,  hapsNrForSet  );
    TALLY_TWO(       hapScoreSumSq,hapScoreMax,hapScoreSetMax);
    TALLY_TWO(       homScoreSumSq,homScoreMax,homScoreSetMax);
    if (he->populationsToo)
        TALLY_TWO(   popScoreSumSq,popScoreMax,popScoreSetMax);
    TALLY_HIST(hapHist,hapsNrForSet,HAPLO_HIST_MAX_IX);   // Histogram for hapSets

    if (limitModels > 0 && limitModels <= setsFound)
        break;

    // Get ready for next model
    hapsNrForSet = 0;
    hapScoreSetMax = 0;
    homScoreSetMax = 0;
    popScoreSetMax = 0;
    homsForSet = 0;
    //wowsForSet = 0;
    }

// Every haplo has subjects and variants, so calculate mean and sd for one model or all models
double subjectMean = CALC_MEAN(subjects    , hapsFound);
double subjectSD   = CALC_SD(  subjectSumSq, hapsFound);
double variantMean = CALC_MEAN(variants    , hapsFound);
double variantSD   = CALC_SD(  variantSumSq, hapsFound);

if (setsFound == 1)
    {
    fprintf(out, "%s: haplotypes:%-3d hom/hap:%-3d ", commonToken, hapsNr, homsForSet );
    //fprintf(out, "wow:%-2d ",     wowsForSet);
    fprintf(out, "variants:%-3d ",hapSet->variantsCovered);
    fprintf(out, "p-Score max ");
    if (hapScoreSetMax >= 1000)
        fprintf(out,"haplo:>1000 ");
    else
        fprintf(out,"haplo:%-5.1f ",hapScoreSetMax);
    if (homScoreSetMax >= 1000)
        fprintf(out,"hom:>1000 ");
    else
        fprintf(out,"hom:%-5.1f ",homScoreSetMax);
    if (he->populationsToo)
        fprintf(out, "pop:%-5.1f ",popScoreSetMax);

    //fprintf(out, "major-branches:%3d  generations:%3d  ",    widthMax, depthMax);
    fprintf(out, "variants:max:%-4d mean:%-7.2lf sd:%-7.2f ",variantMax, variantMean, variantSD);
    fprintf(out, "subjects:max:%-4d mean:%-7.2lf sd:%-7.2f ",subjectMax, subjectMean, subjectSD);
    fprintf(out, ":%s\n",(hapSet != NULL ? hapSet->secondId : commonToken));
    }
else if (setsFound > 1)
    {
    // cap these:
    RANGE_LIMIT(hapScoreMax,-99999,99999);
    RANGE_LIMIT(homScoreMax,-99999,99999);

    // Each model may have multiple haplos so calculate means and SDs for all models here
    double hapsNrMean   = CALC_MEAN(hapsNr       , setsFound);
    double hapsNrSD     = CALC_SD(  hapsNrSumSq  , setsFound);
    double hapScoreMean = CALC_MEAN(hapScoreMax  , hapsFound);
    double hapScoreSD   = CALC_SD(  hapScoreSumSq, hapsFound);
    double homScoreMean = CALC_MEAN(homScoreMax  , hapsFound);
    double homScoreSD   = CALC_SD(  homScoreSumSq, hapsFound);
    double popScoreMean = CALC_MEAN(popScoreMax  , hapsFound);
    double popScoreSD   = CALC_SD(  popScoreSumSq, hapsFound);

    fprintf(out, "\nGene Model Haplotypes (alleles):%d\n",hapsFound);
    fprintf(out, "Models covered:%d  With non-reference haplotypes:%d  "
                 "Found homozygous/haploid:%d  ",setsFound, setsNr, setsHom);
    //fprintf(out, "interesting:%d",setsWow);
    fprintf(out, "\n");
    fprintf(out, "       Haplotypes: max:%-4d   mean:%-5.2lf  sd:%-5.2lf\n",
                               hapsNrMax, hapsNrMean,   hapsNrSD);
    fprintf(out, "      Haplo Score: max:%-6.1f mean:%-5.2lf  sd:%-5.2lf\n",
                             hapScoreMax, hapScoreMean,hapScoreSD);
    fprintf(out, "   Homo/Hap Score: max:%-6.1f mean:%-5.2lf  sd:%-5.2lf\n",
                             homScoreMax,homScoreMean,homScoreSD);
    if (he->populationsToo)
        fprintf(out, " Population Score: max:%-6.1f mean:%-5.2lf  sd:%-5.2lf\n",
                                 popScoreMax,popScoreMean,popScoreSD);
    fprintf(out, "         Variants: max:%-4d   mean:%-5.2lf  sd:%-5.2lf\n",
                              variantMax,variantMean, variantSD);
    fprintf(out, "         Subjects: max:%-4d   mean:%-5.2lf  sd:%-5.2lf\n\n",
                              subjectMax,subjectMean, subjectSD);

    printHistogram(out, hapHist, HAPLO_HIST_BUCKETS,5, "Models with n non-ref haps","models");
    }
}


#define VAL_OR_EMPTY(val)   (val != NULL ? val : "")
#define VAR_SEPARATOR(ix)  (ix == 0 ? '\t':',')
#define VAR_SEP_NOTAB(ix)  (ix == 0 ? "":",")

static void haplotypePrintReadable(FILE *out, struct haploExtras *he, struct haplotypeSet *hapSet,
                                   struct haplotype *haplo)
// Prints a single haplotype to a to file.
{
if (haplo->suffixId[0] != '\0')
    fprintf(out,"%s:%-10s ",hapSet->commonId,haplo->suffixId);
else
    fprintf(out,"%s            ",hapSet->commonId);
fprintf(out,"N:%-4d",haplo->subjects);
if (haplo->haploScore < -999)
    fprintf(out,"(<-999)");
else if (haplo->haploScore >= 1000)
    fprintf(out,"(>1000)");
else
    fprintf(out,"(%5.1f)",haplo->haploScore);
fprintf(out," homo/hap:%-3d",haplo->homCount);
if (haplo->homScore < -999)
    fprintf(out,"(<-999) ");
else if (haplo->homScore >= 1000)
    fprintf(out,"(>1000) ");
else
    fprintf(out,"(%5.1f) ",haplo->homScore);

if (he->populationsToo && haplo->ho != NULL)
    fprintf(out,"pop:%6.3f ",haplo->ho->popScore);
else
    fprintf(out,"pop:0.0 ");

//fprintf(out," 0x%lX  ",(size_t)haplo->hBits); // debugging
if (haplo->bits != NULL)
    bitsOut(out, haplo->bits, 0, haplo->bitCount, FALSE); // Best with zeros
fprintf(out,"  variants:");

// Print out variants (blockCount)
fprintf(out,"%d",haplo->variantCount);

int ix = 0;

// val,val,val
fprintf(out,"  [");
for (ix = 0; ix < haplo->variantCount; ix++)
    fprintf(out,"%s%s",VAR_SEP_NOTAB(ix),VAL_OR_EMPTY(haplo->variants[ix]->val));
fputc(']', out);
for (; ix < 5; ix++)
    fprintf(out,"  ");

// freq,freq,freq
fprintf(out,"\t(");
for (ix = 0; ix < haplo->variantCount; ix++)
    fprintf(out,"%s%.3f",VAR_SEP_NOTAB(ix), haplo->variants[ix]->frequency);
fputc(')', out);
for (; ix < 5; ix++)
    fprintf(out,"     ");
fputc('\t', out);

// len,len,len (blockSizes)
for (ix = 0; ix < haplo->variantCount; ix++)
    fprintf(out,"%s%d",VAR_SEP_NOTAB(ix),
            (haplo->variants[ix]->chromEnd - haplo->variants[ix]->chromStart));
for (; ix < 5; ix++)
    fprintf(out,"  ");
fputc('\t', out);

// start,start,start (blockStarts)
for (ix = 0; ix < haplo->variantCount; ix++)
    fprintf(out,"%s%d",VAR_SEP_NOTAB(ix),haplo->variants[ix]->chromStart + 1);

fputc('\n', out);
}

static void haplotypePrintTab(FILE *out, struct haploExtras *he, struct haplotypeSet *hapSet,
                              struct haplotype *haplo, boolean forLoadBed)
// Prints a single haplotype to a to file.
{
if (forLoadBed) // File for loadBed
    fprintf(out,"%u\t", hFindBin(hapSet->wideStart, hapSet->wideEnd));
// (chrom start end)
fprintf(out,"%s\t%d\t%d\t",hapSet->chrom,hapSet->wideStart,hapSet->wideEnd);
// (name)
if (haplo->suffixId != NULL && haplo->suffixId[0] != '\0')
    fprintf(out,"%s:%s\t",hapSet->commonId,haplo->suffixId);
else
    fprintf(out,"%s\t",hapSet->commonId);
// (score strand thickStart thickEnd itemRgb)    subjects also appears below
fprintf(out,"%d\t%s\t%d\t%d\t10040064\t",// bedParseRgb("153,51,0") dark orange
        (haplo->homCount ? 1000 : min(900,haplo->subjects * 10)),
        hapSet->strand,hapSet->chromStart,hapSet->chromEnd);

// Print out variants (blockCount)
fprintf(out,"%d",haplo->variantCount);

if (haploIsReference(haplo) || haplo->variants == NULL)
    fprintf(out,"\t\t\t\t\t");

int ix = 0;

// len,len,len (blockSizes)
for (ix = 0; ix < haplo->variantCount; ix++)
    fprintf(out,"%c%d",VAR_SEPARATOR(ix),
            (haplo->variants[ix]->chromEnd - haplo->variants[ix]->chromStart));

// start,start,start (blockStarts)
for (ix = 0; ix < haplo->variantCount; ix++)
    fprintf(out,"%c%d",VAR_SEPARATOR(ix),
            (haplo->variants[ix]->chromStart - hapSet->chromStart));

// val,val,val (bed 12 +: 13)
for (ix = 0; ix < haplo->variantCount; ix++)
    fprintf(out,"%c%s",VAR_SEPARATOR(ix),VAL_OR_EMPTY(haplo->variants[ix]->val));

// freq,freq,freq (bed 12 + 14)
for (ix = 0; ix < haplo->variantCount; ix++)
    fprintf(out,"%c%5.3f",VAR_SEPARATOR(ix), haplo->variants[ix]->frequency);

// id,id,id (bed 12 +: 15)
for (ix = 0; ix < haplo->variantCount; ix++)
    fprintf(out,"%c%s",VAR_SEPARATOR(ix),VAL_OR_EMPTY(haplo->variants[ix]->id));

fprintf(out,"\t%d\t",haplo->variantsCovered);

// haploGenome ids  (bed 12 +)
if (haplo->bitCount > 0 && haplo->bits != NULL)
    bitsOut(out, haplo->bits, 0, haplo->bitCount, FALSE);
fprintf(out,"\t%s\t%s\t",hapSet->commonId,hapSet->secondId);

// Scores can overrun range
if (haplo->haploScore < -99999)
    fprintf(out,"-99999\t");
else if (haplo->haploScore >= 99999)
    fprintf(out,"99999\t");
else
    fprintf(out,"%lf\t",haplo->haploScore);

if (haplo->homScore < -99999)
    fprintf(out,"-99999\t");
else if (haplo->homScore >= 99999)
    fprintf(out,"99999\t");
else
    fprintf(out,"%lf\t",haplo->homScore);

if (he->populationsToo && haplo->ho != NULL)
    fprintf(out,"%.4lf\t",haplo->ho->popScore);
else
    fprintf(out,"0\t");

fprintf(out,"%d\t%d\t",haplo->homCount,haplo->subjects);

if (haplo->subjects > 0 && haplo->subjectIds != NULL)
    fprintf(out,"%s",haplo->subjectIds);
fputc('\n', out);
}


int haploSetsPrint(struct haploExtras *he, char *outFile, boolean append,
                   struct haplotypeSet *haploSets, boolean forLoadBed)
// Print haplotypes sets to a to file.  Returns count of haplotypes printed
{
// Note, don't just pass in haploExtras, since same routine can be called
// directly and for output is to a table, but they have different needs
FILE *out = NULL;
if (sameWord("-",outFile))
    out = stdout;
else if (append)
    out = mustOpen(outFile, "a");
else
    out = mustOpen(outFile, "w");

int haploCount = 0;
int modelCount = 0;
struct haplotypeSet *hapSet = haploSets;
for ( ; hapSet != NULL; hapSet = hapSet->next)
    {
    if (he->justModel != NULL && differentString(he->justModel,hapSet->commonId))
        continue;

    struct haplotype *haplo = hapSet->haplos;
    for ( ; haplo != NULL; haplo = haplo->next)
        {
        if (he->homozygousOnly && haplo->homCount <= 0)
            continue;

        if (he->readable)
            haplotypePrintReadable(out, he, hapSet,haplo);
        else
            haplotypePrintTab(out, he, hapSet,haplo,forLoadBed);
        haploCount++;
        }

    // The reference haplotype will not be in the list of haplos, if it isn't represented in the
    // VCF data. However, we always want to print the reference haplotype
    if (!he->homozygousOnly && hapSet->refHap->subjects == 0)
        {
        if (he->readable)
            haplotypePrintReadable(out, he, hapSet,hapSet->refHap);
        else
            haplotypePrintTab(out, he, hapSet,hapSet->refHap,forLoadBed);
        haploCount++;
        }

    modelCount++;

    verbose(3,"haplotypesPrint() %s  models:%d  haps:%d\n",
            hapSet->commonId,modelCount,haploCount);

    if (haploCount > 0 && he->readable) // write out some totals
        haploSetsPrintStats(he, out, hapSet->commonId, hapSet, 1);
    }

if (he->readable && modelCount > 1)  // More than one, then totals
    {
    verbose(2,"haplotypesPrint() models:%d  haps:%d\n",
            modelCount,haploCount);
    haploSetsPrintStats(he, out, "all models", haploSets, 0);
    }

if (!he->readable && differentWord("-",outFile))
    fclose(out);

return haploCount;
}


int haploSetsWriteToTable(struct haploExtras *he, struct haplotypeSet *haploSets)
// Load database table from bedList.
{
struct dyString *dy = newDyString(1024);
char *tab = NULL;
int loadOptions = 0;

if (he->conn == NULL)
        he->conn = hAllocConn(he->db);

if (he->tmpDir != NULL)
    tab = cloneString(rTempName(he->tmpDir,"loadBed",".tab"));
else
    tab = cloneString("bed.tab");

// Drop/recreate table if not appending
if (!he->append)
    {
    sqlDyStringPrintf(dy, HAPLO_BED, he->outTableOrFile);
    verbose(2,"%s", dy->string);
    sqlRemakeTable(he->conn, he->outTableOrFile, dy->string);
    }

verbose(2, "Saving %s\n", tab);
int count = haploSetsPrint(he, tab, FALSE, haploSets, TRUE); // no append, forBedLoad

verbose(2, "Loading %s.%s\n", he->db, he->outTableOrFile);
sqlLoadTabFile(he->conn, tab, he->outTableOrFile, loadOptions);

// if temp dir specified, unlink file to make it disappear
if (he->tmpDir != NULL)
    unlink(tab);

return count;
}



// -------------------------
// gene model level routines
// -------------------------
static int geneModelVariantsLoadAll(struct haploExtras *he,struct vcfFile *vcff,
                                    struct genePred *gp, int *bases)
{
int variantCount = 0;

// for gene model grab all vcf records (verify file and chrom)
// May look beyond strict exon borders.
#define HAPLO_PROMOTER        0
//#define HAPLO_INTRON_5_PRIME 10
//#define HAPLO_INTRON_3_PRIME  5
#define HAPLO_INTRON_5_PRIME 0
#define HAPLO_INTRON_3_PRIME 0
int afterExon      = HAPLO_INTRON_5_PRIME;
int beforeExon     = HAPLO_INTRON_3_PRIME;
int firstExonLead  = HAPLO_PROMOTER;
int lastExonTail   = 0;
if (*(gp->strand) == '-') // strand matters on genes
    {
    afterExon      = HAPLO_INTRON_3_PRIME;
    beforeExon     = HAPLO_INTRON_5_PRIME;
    firstExonLead  = 0;
    lastExonTail   = HAPLO_PROMOTER;
    }

// Walk through exons only
verbose(3, "  %s: loading variants for %d exons.\n", gp->name, gp->exonCount);
int ix=0,lastEnd = 0;
int basesCovered = 0;
for (;ix < gp->exonCount;ix++)
    {
    // NOTE: exons can stretch beyond coding region!
    int beg = max(gp->exonStarts[ix], gp->cdsStart);
    int end = min(gp->exonEnds[ix],   gp->cdsEnd);
    // Adjustments to cover intron edges and possibly promoter
    if (ix == 0)
        beg -= firstExonLead;
    else
        beg -= beforeExon;
    if ((ix+1) == gp->exonCount)
        end += lastExonTail;
    else
        end += afterExon;
    
    if (beg >= end) // Could happen if exon not in coding region.
        continue;

    // Oh and make sure that exons+plus don't result in the same variants being found twice!
    if (beg < lastEnd)
        beg = lastEnd + 1;
    lastEnd = end;

    int variants = vcfTabixBatchRead(vcff, gp->chrom, beg, end, VCF_IGNORE_ERRS, -1);
    // TODO: make vcfTabixBatchRead() throw out dups that might result from variants > 1bp!
    
    verbose(3, "  %s exon %d: found %d variants in %s:%d-%d.\n", 
               gp->name, ix, variants,gp->chrom, beg, end);

    basesCovered += end - beg;
    variantCount += variants;
    }
if (bases)
    *bases = basesCovered;

return variantCount;
}

static int geneModelVariantBitsDropSynonymous(struct haploExtras *he,struct vcfFile *vcff,
                                              struct genePred *gp, struct variantBits **pvBitsList)
// removes synonymous SNPs from variants list.  Returns count of dropped variants
{
// Have exons already.  Must make bed list for pgSnp code
struct bed *bed = bedFromGenePred(gp);
assert(bed != NULL);

struct bed *gene = bedThickOnly(bed);
bedFree(&bed);
assert(gene != NULL);

struct variantBits *vBitsList = *pvBitsList;
struct variantBits *vBitsPassed = NULL;
struct variantBits *vBits = NULL;
int dropped = 0;
while ((vBits = slPopHead(&vBitsList)) != NULL)
    {
    // Something to evaluate?
    if (vBits->vType != vtSNP             // Must be a SNP
    ||  vBits->record->alleles  == NULL   // Must have a value
    ||  vBits->record->alleles[1] == NULL)
        {
        slAddHead(&vBitsPassed, vBits); // Not a SNP so not examined for sysnonymous affect
        continue;
        }

    // Exclude variants outside of coding region
    // NOTE: could examine blocks
    if (vBits->record->chromStart < gene->chromStart
    ||  vBits->record->chromEnd   > gene->chromEnd  )
        {
        slAddHead(&vBitsPassed, vBits); // not in coding region, which is okay
        continue;
        }

    struct pgCodon *codon = fetchCodons(he->db, gene, vBits->record->chromStart,
                                                      vBits->record->chromEnd);
    if (codon == NULL)
        {
        slAddHead(&vBitsPassed, vBits); // not in exon which is okay
        continue;
        }
    assert(strlen(codon->seq) == 3);

    char codonString[4];
    safecpy(codonString,sizeof(codonString),codon->seq);
    int len = strlen(codonString);
    boolean reverse = sameString(gene->strand, "-");

    if (reverse)
        reverseComplement(codonString, len);

    AA was = lookupCodon(codonString);

    int offset = (codon->regStart - codon->cdStart);
    if (reverse) // reverse does a number on the offset !
       {
        if (offset == 0)
            offset = 2;
        else if (offset == 2)
            offset = 0;
        }

    char *p = codon->seq + offset;
    if (*p != vBits->record->alleles[0][0]) // sanity check the data
        errAbort("SNP reference variant does not match: %c != %c %s %s regStart:%d cdStart;%d "
                 "chromStart%d\n",*p,vBits->record->alleles[0][0],gene->strand,codon->seq,
                 codon->regStart,codon->cdStart,vBits->record->chromStart);
    int ix = 1; // 0 is reference allele!
    for ( ; ix < vBits->record->alleleCount; ix++)
        {
        // swap in place!
        *p = vBits->record->alleles[ix][0]; // SNP, so just the first letter.
        safecpy(codonString,sizeof(codonString),codon->seq);

        if (reverse)
            reverseComplement(codonString, len);

        AA now = lookupCodon(codonString);
        if (was != now)
            break;
        }
    freeMem(codon);

    // If no mismatch was found
    if (ix == vBits->record->alleleCount)
        {
        dropped++;  // just drip this memory away.  It is on lm
        continue;
        }

    slAddHead(&vBitsPassed, vBits);
    }
if (vBitsPassed != NULL)
    slReverse(&vBitsPassed);
*pvBitsList = vBitsPassed;

// cleanup
bedFree(&gene);

if (dropped > 0)
verbose(2, "dropped %d variants as synonomous mutations.\n",dropped);

return dropped;
}

static struct variantBits *geneModelVariantBitsCreate(struct haploExtras *he,struct vcfFile *vcff,
                                                      struct genePred *gp, int *pBasesCovered)
// Returns a list of relevant variants as bitmap structures
{
// Load up all variants relevant to this gene model
int basesCovered = 0;
int variantCount = geneModelVariantsLoadAll(he,vcff,gp,&basesCovered);
if ( pBasesCovered )
    *pBasesCovered = basesCovered;

if (verboseLevel() >= 2) // NOTE: This test should be pushed into verboseTime!
    verboseTime(2, "%s: found %d variants in %d exons covering %d bases.\n",
                gp->name, variantCount,gp->exonCount,basesCovered);
if (variantCount == 0)
    return NULL;

// NOTE: no longer support homozygousOnly variants: it interferes with statistics
//       instead, all variants used to create haplotypes, then homozygous only are printed
struct variantBits *vBitsList = vcfRecordsToVariantBits(vcff, NULL, TRUE, FALSE, TRUE);
assert(slCount(vBitsList) == variantCount);
if (verboseLevel() >= 2) // verbose timing at every step
    verboseTime(2, "%s: created %d vBit maps.\n", gp->name, variantCount);

// Drop sparse variants.  This includes variants with 0 bits on!
// Note that chrX represents a problem, since PAR regions have more effective chroms than non-par
// So the variantMin will vary across chrX.  We will do the easiest thing by using a single loc:
int varaintMin = (vBitsSubjectChromCount(vBitsList) * he->variantMinPct / 100) + 1;
int dropped = vcfVariantBitsDropSparse(&vBitsList, varaintMin, FALSE);// ref-wrong OK
if (dropped > 0)
    verbose(2, "%s: dropped %d variants which were found fewer than %g%% haploid genomes\n",
            gp->name, dropped, he->variantMinPct);
variantCount = slCount(vBitsList);
if (variantCount == 0)
    {
    verbose(2, "%s: Has no non-reference, non-sparse variants.\n", gp->name);
    return NULL;
    }

// WARN about unphased
if (vBitsList->haplotypeSlots > 1) // not chrY or homozygous only
    {
    struct variantBits *vBits = vBitsList;
    for (; vBits != NULL; vBits = vBits->next)
        {
        if (bitFindSet(vBits->unphased,0,vcff->genotypeCount) < vcff->genotypeCount)
            errAbort("Atleast one unphased genotype has been found in file %s.\n", he->inFile);
        }
    }

// Cull synonymous SNPs as not desirable.
if (!he->synonymous)
    {
    (void)geneModelVariantBitsDropSynonymous(he, vcff, gp, &vBitsList);
    if (vBitsList == NULL)
        return NULL;
    }

return vBitsList;
}

static struct haploBits *haplotypeBitsCreate(struct haploExtras *he,struct vcfFile *vcff,
                                             char *regionId,  struct variantBits **pvBitsList)
// Creates haplotype bitmaps from variant bitmaps
{
// NOTE: sort variants in order of most popular.  This will make tree building more logical
//slSort(pvBitsList,vcfVariantMostPopularCmp);

// get haplotype Bits list (including ref haplotype)
struct variantBits *vBitsList = *pvBitsList;
struct haploBits *hBitsList = vcfVariantBitsToHaploBits(vcff, vBitsList, FALSE);
if (hBitsList == NULL)
    {
    verbose(2, "%s: with %d (%d bits) variants has produced 0 gene haplotypes.\n",
            regionId, slCount(vBitsList), vBitsList->bitsOn);
    vcfFileFlushRecords(vcff);
    return NULL;
    }
int hapCount = slCount(hBitsList);
if (verboseLevel() >= 2) // verbose timing at every step
    verboseTime(2, "%s: converted %d variants to %d hBit maps.\n",
                regionId, slCount(vBitsList), hapCount);

// collapse hBitsList on identical (store list of ids in hBits)
if (hapCount > 1) // || he->haploMin > 1) No longer supported: interferes with statistics
    {
    int collapsed = vcfHaploBitsListCollapseIdentical(vcff,&hBitsList,1);// he->haploMin); No longer
    if (collapsed > 0)
        {
        if (verboseLevel() >= 2) // verbose timing at every step
            verboseTime(2, "%s: collapsed %d haplotype maps into %d unique haplotypes.\n",
                        regionId,hapCount,slCount(hBitsList));
        hapCount = slCount(hBitsList);
        }
    else
        {
        if (verboseLevel() >= 2) // verbose timing at every step
            verboseTime(2, "%s: has %d unique haplotypes.\n",regionId,hapCount);
        }
    }
if (hapCount == 0)
    {
    vcfFileFlushRecords(vcff);
    return NULL;
    }

return hBitsList;
}

struct haplotypeSet *geneHapSetForOneModel(struct haploExtras *he,struct vcfFile *vcff,
                                           struct genePred *gp, boolean excludeRefOnly)
// Generate one or more gene alleles (haplotypes) for one gene model
{
struct haplotypeSet *hapSet = NULL;
int variantCount = 0;
int hapCount = 0;
int basesCovered = 0;
int subjectN = THOUSAND_GENOME_SUBJECTS(gp->chrom);
int chromN   = THOUSAND_GENOME_CHROMS(gp->chrom);

// Loads relevant variants into bitmaps
struct variantBits *vBitsList = geneModelVariantBitsCreate(he, vcff, gp, &basesCovered);
if (vBitsList != NULL)
    {
    variantCount = slCount(vBitsList);

    // Note that the number of subjects and chromosomes should be taken from VCF dataset
    subjectN = vcfBitsSubjectCount(vBitsList);
    chromN   = vBitsSubjectChromCount(vBitsList);

    // Create haplotype bitmaps from variant bitmaps (vBitList will be sorted!)
    struct haploBits *hBitsList = haplotypeBitsCreate(he, vcff, gp->name, &vBitsList);
    if (hBitsList != NULL)
        {
        hapCount = slCount(hBitsList);

        hapSet = haploSetBeginForGene(he,gp);
        hapSet->haplos = haplotypesHarvest(he, vcff, hapSet, vBitsList, hBitsList, hapCount);
        assert(hapCount == slCount(hapSet->haplos));

        // Need to add a reference?
        // Will include an empty bitmap and a count of homozygous or haploid
        int refHomozygous = vcfVariantBitsReferenceOccurs(vcff, vBitsList, TRUE); // TRUE: homoHap
        hapSet->refHap = haplotypeForReferenceGenome(he,hapSet,vBitsList,refHomozygous,chromN);
        variantListsReorganize(vBitsList); // Reorganizes shared variant memory
        }
    }

if (hapSet == NULL && !excludeRefOnly)
    {
    hapSet = haploSetBeginForGene(he,gp);
    hapSet->refHap = haplotypeForReferenceGenome(he,hapSet,NULL,subjectN,chromN);
    }

// Each haplotype struct should contain all pertinent stuff to describe the gene allele.
// At this point there is no more need of vcff->records, vBits, hBits or elmTree.  
// Since all of those are allocated on vcff->reusePool, simply flushing records will
// free them all.  However, it will not set pointers to NULL so be careful!
vcfFileFlushRecords(vcff);

if (hapSet == NULL)
    return NULL;

// Now we can score the members of this list
haploSetCalcScores(he,hapSet,subjectN,chromN);

if (verboseLevel() >= 2) // verbose timing at every step
    haploSetsPrintStats(he, stderr,gp->name,hapSet, 1);

if (verboseLevel() >= 2) // verbose timing at every step
    {
    verboseTime(2, "%s: %s:%d-%d exons:%-2d bases:%-4d variants:%-3d non-reference haplotypes:%d\n",
                hapSet->commonId, hapSet->chrom, hapSet->chromStart, hapSet->chromEnd,
                gp->exonCount, basesCovered, variantCount, hapCount);
    }

return hapSet;
}

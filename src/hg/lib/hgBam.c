/* bamFile -- interface to binary alignment format files using Heng Li's samtools lib. */

#include "common.h"
#include "hdb.h"
#include "hgBam.h"

#ifdef USE_BAM
#include "htmshell.h"
#include "samAlignment.h"

char *bamFileNameFromTable(struct sqlConnection *conn, char *table, char *bamSeqName)
/* Return file name from table.  If table has a seqName column, then grab the 
 * row associated with bamSeqName (which can be e.g. '1' not 'chr1' if that is the
 * case in the bam file). */
{
boolean checkSeqName = (sqlFieldIndex(conn, table, "seqName") >= 0);
if (checkSeqName && bamSeqName == NULL)
    errAbort("bamFileNameFromTable: table %s has seqName column, but NULL seqName passed in",
	     table);
char query[512];
if (checkSeqName)
    safef(query, sizeof(query), "select fileName from %s where seqName = '%s'",
	  table, bamSeqName);
else
    safef(query, sizeof(query), "select fileName from %s", table);
char *fileName = sqlQuickString(conn, query);
if (fileName == NULL && checkSeqName)
    {
    if (startsWith("chr", bamSeqName))
	safef(query, sizeof(query), "select fileName from %s where seqName = '%s'",
	      table, bamSeqName+strlen("chr"));
    else
	safef(query, sizeof(query), "select fileName from %s where seqName = 'chr%s'",
	      table, bamSeqName);
    fileName = sqlQuickString(conn, query);
    }
if (fileName == NULL)
    {
    if (checkSeqName)
	errAbort("Missing fileName for seqName '%s' in %s table", bamSeqName, table);
    else
	errAbort("Missing fileName in %s table", table);
    }
return fileName;
}

struct ffAli *bamToFfAli(const bam1_t *bam, struct dnaSeq *target, int targetOffset,
			 boolean useStrand, char **retQSeq)
/* Convert from bam to ffAli format.  If retQSeq is non-null, set it to the 
 * query sequence into which ffAli needle pointers point. (Adapted from psl.c's pslToFfAli.) */
{
struct ffAli *ffList = NULL, *ff;
const bam1_core_t *core = &bam->core;
boolean isRc = useStrand && bamIsRc(bam);
DNA *needle = (DNA *)bamGetQuerySequence(bam, useStrand);
if (retQSeq)
    *retQSeq = needle;
if (isRc)
    reverseComplement(target->dna, target->size);
DNA *haystack = target->dna;
unsigned int *cigarPacked = bam1_cigar(bam);
int tStart = targetOffset, qStart = 0, i;
// If isRc, need to go through the CIGAR ops backwards, but sequence offsets still count up.
int iStart = isRc ? (core->n_cigar - 1) : 0;
int iIncr = isRc ? -1 : 1;
for (i = iStart;  isRc ? (i >= 0) : (i < core->n_cigar);  i += iIncr)
    {
    char op;
    int size = bamUnpackCigarElement(cigarPacked[i], &op);
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
	    AllocVar(ff);
	    ff->left = ffList;
	    ffList = ff;
	    ff->nStart = needle + qStart;
	    ff->nEnd = ff->nStart + size;
	    ff->hStart = haystack + tStart - targetOffset;
	    ff->hEnd = ff->hStart + size;
	    tStart += size;
	    qStart += size;
	    break;
	case 'I': // inserted in query
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	    qStart += size;
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tStart += size;
	    break;
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("bamToFfAli: unrecognized CIGAR op %c -- update me", op);
	}
    }
ffList = ffMakeRightLinks(ffList);
ffCountGoodEnds(ffList);
return ffList;
}


struct bamToSamHelper
/* Helper structure to help get sam alignments out of a BAM file */
    {
    struct lm *lm;	/* Local memory pool to allocate into */
    char *chrom;	/* Chromosome name */
    struct dyString *dy;	/* Dynamic temporary string */
    samfile_t *samFile;	/* Open sam/bam file header. */
    struct samAlignment *samList;	/* List of alignments. */
    };

static void addToChars(char *s, int size, char amount)
/* Add amount to each char in s of given size. */
{
int i;
for (i=0; i<size; ++i)
    s[i] += amount;
}

static boolean isAllSameChar(char *s, int size, char c)
/* Return TRUE if first size chars of s are 0 */
{
int i;
for (i=0; i<size; ++i)
    if (s[i] != c)
	return FALSE;
return TRUE;
}

int bamAddOneSamAlignment(const bam1_t *bam, void *data)
/* bam_fetch() calls this on each bam alignment retrieved.  Translate each bam
 * into a samAlignment. */
{
struct bamToSamHelper *helper = (struct bamToSamHelper *)data;
struct lm *lm = helper->lm;
struct samAlignment *sam;
lmAllocVar(lm, sam);
const bam1_core_t *core = &bam->core;
struct dyString *dy = helper->dy;
sam->qName = lmCloneString(lm, bam1_qname(bam));
sam->flag = core->flag;
if (helper->chrom != NULL)
    sam->rName = helper->chrom;
else
    sam->rName = lmCloneString(lm, helper->samFile->header->target_name[core->tid]);
sam->pos = core->pos + 1;
sam->mapQ = core->qual;
dyStringClear(dy);
bamUnpackCigar(bam, dy);
sam->cigar = lmCloneStringZ(lm, dy->string, dy->stringSize);
if (core->mtid >= 0)
    {
    if (core->tid == core->mtid)
	sam->rNext = "=";
    else
	sam->rNext = lmCloneString(lm, helper->samFile->header->target_name[core->mtid]);
    }
else
    sam->rNext = "*";
sam->pNext = core->mpos + 1;
sam->tLen = core->isize;
sam->seq = lmAlloc(lm, core->l_qseq + 1);
bamUnpackQuerySequence(bam, FALSE, sam->seq);
char *bamQual = (char *)bam1_qual(bam);
if (isAllSameChar(bamQual, core->l_qseq, -1))
    sam->qual = "*";
else
    {
    sam->qual = lmCloneStringZ(lm, bamQual, core->l_qseq);
    addToChars(sam->qual, core->l_qseq, 33);
    }
dyStringClear(dy);
bamUnpackAux(bam, dy);
sam->tagTypeVals = lmCloneStringZ(lm, dy->string, dy->stringSize);
slAddHead(&helper->samList, sam);
return 0;
}

struct samAlignment *bamFetchSamAlignment(char *fileOrUrl, char *chrom, int start, int end,
	struct lm *lm)
/* Fetch region as a list of samAlignments - which is more or less an unpacked
 * bam record.  Results is allocated out of lm, since it tends to be large... */
{
struct bamToSamHelper helper;
helper.lm = lm;
helper.chrom = chrom;
helper.dy = dyStringNew(0);
helper.samList = NULL;
char posForBam[256];
safef(posForBam, sizeof(posForBam), "%s:%d-%d", chrom, start, end);
bamFetch(fileOrUrl, posForBam, bamAddOneSamAlignment, &helper, &helper.samFile);
dyStringFree(&helper.dy);
slReverse(&helper.samList);
return helper.samList;
}

struct samAlignment *bamReadNextSamAlignments(samfile_t *fh, int count, struct lm *lm)
/* Read next count alignments in SAM format, allocated in lm.  May return less than
 * count at end of file. */
{
/* Set up helper. */
struct bamToSamHelper helper;
helper.lm = lm;
helper.chrom = NULL;
helper.dy = dyStringNew(0);
helper.samFile = fh;
helper.samList = NULL;

/* Loop through calling our own fetch function */
int i;
bam1_t *b = bam_init1();
for (i=0; i<count; ++i)
    {
    if (samread(fh, b) < 0)
       break;
    bamAddOneSamAlignment(b, &helper);
    }
bam_destroy1(b);

/* Clean up and go home. */
dyStringFree(&helper.dy);
slReverse(&helper.samList);
return helper.samList;
}

#else
// If we're not compiling with samtools, make stub routines so compile won't fail:

char *bamFileNameFromTable(struct sqlConnection *conn, char *table, char *bamSeqName)
/* Return file name from table.  If table has a seqName column, then grab the
 * row associated with bamSeqName (which is not nec. in chromInfo, e.g.
 * bam file might have '1' not 'chr1'). */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamFileNameFromTable");
return NULL;
}

struct samAlignment *bamFetchSamAlignment(char *fileOrUrl, char *chrom, int start, int end,
	struct lm *lm)
/* Fetch region as a list of samAlignments - which is more or less an unpacked
 * bam record.  Results is allocated out of lm, since it tends to be large... */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamFetchSamAlignment");
return NULL;
}

struct samAlignment *bamReadNextSamAlignments(samfile_t *fh, int count, struct lm *lm)
/* Read next count alignments in SAM format, allocated in lm.  May return less than
 * count at end of file. */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamReadNextSamAlignments");
return NULL;
}

struct ffAli *bamToFfAli(const bam1_t *bam, struct dnaSeq *target, int targetOffset,
			 boolean useStrand, char **retQSeq)
/* Convert from bam to ffAli format.  If retQSeq is non-null, set it to the
 * query sequence into which ffAli needle pointers point. */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamToFfAli");
return NULL;
}

#endif//ndef USE_BAM

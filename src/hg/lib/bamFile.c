/* bam -- interface to binary alignment format files using Heng Li's samtools lib. */

#ifdef USE_BAM

#include "common.h"
#include "htmshell.h"
#include "hdb.h"
#include "hgConfig.h"
#include "udc.h"
#include "bamFile.h"

static char const rcsid[] = "$Id: bamFile.c,v 1.12 2009/11/03 00:33:45 angie Exp $";

static boolean ignoreStrand = FALSE;

void bamIgnoreStrand()
/* Change the behavior of this lib to disregard item strand. 
 * If called, this should be called before any other bam functions. */
{
ignoreStrand = TRUE;
}

char *bamFileNameFromTable(char *db, char *table, char *bamSeqName)
/* Return file name from table.  If table has a seqName column, then grab the 
 * row associated with bamSeqName (which is not nec. in chromInfo, e.g. 
 * bam file might have '1' not 'chr1'). */
{
struct sqlConnection *conn = hAllocConn(db);
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
if (fileName == NULL)
    {
    if (checkSeqName)
	errAbort("Missing fileName for seqName '%s' in %s table", bamSeqName, table);
    else
	errAbort("Missing fileName in %s table", table);
    }
hFreeConn(&conn);
return fileName;
}

static char *samtoolsFileName(char *fileOrUrl)
/* If udcFuse is configured, and we have a URL, convert it into a filename in
 * the udcFuse filesystem for use by samtools.  Thus samtools will think it's
 * working on a local file, but udcFuse will pass access requests to udc and
 * we'll get the benefits of sparse-file local caching and https support.
 * udcFuse needs us to open udc files before invoking udcFuse paths, so open
 * both the .bam and .bai (index) URLs with udc here.  
 * If udcFuse is not configured, or fileOrUrl is not an URL, just pass through fileOrUrl. */
{
char *udcFuseRoot = cfgOption("udcFuse.mountPoint");
char *protocol = NULL, *afterProtocol = NULL, *colon = NULL;
udcParseUrl(fileOrUrl, &protocol, &afterProtocol, &colon);
if (udcFuseRoot != NULL && afterProtocol != NULL)
    {
    struct dyString *dy = dyStringNew(0);
    dyStringPrintf(dy, "%s/%s/%s", udcFuseRoot, protocol, afterProtocol);
    char *bamFileName = dyStringCannibalize(&dy);
    if (!fileExists(bamFileName))
	{
	struct udcFile *udcf = udcFileMayOpen(fileOrUrl, NULL);
	if (udcf != NULL)
	    {
	    udcFileClose(&udcf);
	    if (!fileExists(bamFileName))
		{
		warn("Cannot find %s -- remount udcFuse?", bamFileName);
		freeMem(bamFileName);
		return cloneString(fileOrUrl);
		}
	    // Look for index file: xxx.bam.bai or xxx.bai.
	    int urlLen = strlen(fileOrUrl), fLen = strlen(bamFileName);
	    char *indexUrl = needMem(urlLen+5), *indexFileName = needMem(fLen+5);
	    safef(indexUrl, urlLen+5, "%s.bai", fileOrUrl);
	    safef(indexFileName, fLen+5, "%s.bai", bamFileName);
	    udcf = udcFileMayOpen(indexUrl, NULL);
	    if (udcf == NULL)
		{
		if (endsWith(fileOrUrl, ".bam"))
		    {
		    strcpy(indexUrl+urlLen-1, "i");
		    strcpy(indexFileName+fLen-1, "i");
		    udcf = udcFileMayOpen(indexUrl, NULL);
		    if (udcf == NULL)
			errAbort("Cannot find BAM index file (%s.bai or %s)", fileOrUrl, indexUrl);
		    udcFileClose(&udcf);
		    }
		else
		    errAbort("Cannot find BAM index file for \"%s\"", fileOrUrl);
		}
	    freeMem(indexUrl);
	    freeMem(indexFileName);
	    }
	else
	    errAbort("Failed to read BAM file \"%s\"", fileOrUrl);
	}
    return bamFileName;
    }
return cloneString(fileOrUrl);
}

boolean bamFileExists(char *fileOrUrl)
/* Return TRUE if we can successfully open the bam file and its index file. */
{
char *bamFileName = samtoolsFileName(fileOrUrl);
samfile_t *fh = samopen(bamFileName, "rb", NULL);
if (fh != NULL)
    {
    // When file is an URL, this caches the index file in addition to validating:
    bam_index_t *idx = bam_index_load(bamFileName);
    samclose(fh);
    if (idx == NULL)
	{
	warn("bamFileExists: failed to read index corresponding to %s", bamFileName);
	return FALSE;
	}
    freeMem(idx);
    return TRUE;
    }
return FALSE;
}

void bamFetch(char *fileOrUrl, char *position, bam_fetch_f callbackFunc, void *callbackData)
/* Open the .bam file, fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData. 
 * Note: if sequences in .bam file don't begin with "chr" but cart position does, pass in 
 * cart position + strlen("chr") to match the .bam file sequence names. */
{
char *bamFileName = samtoolsFileName(fileOrUrl);
samfile_t *fh = samopen(bamFileName, "rb", NULL);
if (fh == NULL)
    errAbort("samopen(%s, \"rb\") returned NULL", bamFileName);

int chromId, start, end;
int ret = bam_parse_region(fh->header, position, &chromId, &start, &end);
if (ret != 0)
    // If the bam file does not cover the current chromosome, OK
    return;

bam_index_t *idx = bam_index_load(bamFileName);
if (idx == NULL)
    errAbort("bam_index_load(%s) failed.", bamFileName);
ret = bam_fetch(fh->x.bam, idx, chromId, start, end, callbackData, callbackFunc);
if (ret != 0)
    errAbort("bam_fetch(%s, %s (chromId=%d) failed (%d)", bamFileName, position, chromId, ret);
samclose(fh);
}

boolean bamIsRc(const bam1_t *bam)
/* Return TRUE if alignment is on - strand.  If bamIgnoreStrand has been called,
 * then this always returns FALSE. */
{
const bam1_core_t *core = &bam->core;
return (core->flag & BAM_FREVERSE) && !ignoreStrand;
}

char *bamGetQuerySequence(const bam1_t *bam)
/* Return the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so here we rev-comp it back to restore the original query sequence. */
{
const bam1_core_t *core = &bam->core;
char *qSeq = needMem(core->l_qseq + 1);
uint8_t *s = bam1_seq(bam);
int i;
for (i = 0; i < core->l_qseq; i++)
    qSeq[i] = bam_nt16_rev_table[bam1_seqi(s, i)];
if (bamIsRc(bam))
    reverseComplement(qSeq, core->l_qseq);
return qSeq;
}

UBYTE *bamGetQueryQuals(const bam1_t *bam)
/* Return the base quality scores encoded in bam as an array of ubytes. */
{
const bam1_core_t *core = &bam->core;
int qLen = core->l_qseq;
UBYTE *arr = needMem(qLen);
boolean isRc = bamIsRc(bam);
UBYTE *qualStr = bam1_qual(bam);
int i;
for (i = 0;  i < qLen;  i++)
    {
    int offset = isRc ? (qLen - 1 - i) : i;
    arr[offset] = (qualStr[0] == 255) ? 255 : qualStr[i];
    }
return arr;
}

char *bamGetCigar(const bam1_t *bam)
/* Return a BAM-enhanced CIGAR string, decoded from the packed encoding in bam. */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
struct dyString *dyCigar = dyStringNew(min(8, core->n_cigar*4));
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigarPacked[i], &op);
    dyStringPrintf(dyCigar, "%d", n);
    dyStringAppendC(dyCigar, op);
    }
return dyStringCannibalize(&dyCigar);
}

void bamShowCigarEnglish(const bam1_t *bam)
/* Print out cigar in English e.g. "20 (mis)Match, 1 Deletion, 3 (mis)Match" */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigarPacked[i], &op);
    if (i > 0)
	printf(", ");
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
	    printf("%d (mis)Match", n);
	    break;
	case 'I': // inserted in query
	    printf("%d Insertion", n);
	    break;
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	    printf("%d Skipped", n);
	    break;
	case 'D': // deleted from query
	    printf("%d Deletion", n);
	    break;
	case 'N': // long deletion from query (intron as opposed to small del)
	    printf("%d deletioN", n);
	    break;
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	    printf("%d Hard clipped query", n);
	    break;
	case 'P': // P="silent deletion from padded reference sequence"
	    printf("%d Padded / silent deletion", n);
	    break;
	default:
	    errAbort("bamShowCigarEnglish: unrecognized CIGAR op %c -- update me", op);
	}
    }
}

static void descFlag(unsigned flag, unsigned bitMask, char *desc, boolean makeRed,
	      boolean *retFirst)
/* Describe a flag bit (or multi-bit mask) if it is set in flag. */
{
if ((flag & bitMask) == bitMask) // *all* bits in bitMask are set in flag
    {
    if (!*retFirst)
	printf(" | ");
    printf("<span%s>(<TT>0x%02x</TT>) %s</span>",
	   (makeRed ? " style='color: red'" : ""), bitMask, desc);
    *retFirst = FALSE;
    }
}

void bamShowFlagsEnglish(const bam1_t *bam)
/* Print out flags in English, e.g. "Mate is on '-' strand; Properly paired". */
{
const bam1_core_t *core = &bam->core;
unsigned flag = core->flag;
boolean first = TRUE;
descFlag(flag, BAM_FDUP, "Optical or PCR duplicate", TRUE, &first);
descFlag(flag, BAM_FQCFAIL, "QC failure", TRUE, &first);
descFlag(flag, BAM_FSECONDARY, "Not primary alignment", TRUE, &first);
descFlag(flag, BAM_FREAD2, "Read 2 of pair", FALSE, &first);
descFlag(flag, BAM_FREAD1, "Read 1 of pair", FALSE, &first);
descFlag(flag, BAM_FMREVERSE, "Mate is on '-' strand", FALSE, &first);
descFlag(flag, BAM_FREVERSE, "Read is on '-' strand", FALSE, &first);
descFlag(flag, BAM_FMUNMAP, "Mate is unmapped", TRUE, &first);
if (flag & BAM_FUNMAP)
    errAbort("Read is unmapped (what is it doing here?!?)");
descFlag(flag, (BAM_FPROPER_PAIR | BAM_FPAIRED), "Properly paired", FALSE, &first);
if ((flag & BAM_FPAIRED) && !(flag & BAM_FPROPER_PAIR))
    descFlag(flag, BAM_FPAIRED, "Not properly paired", TRUE, &first);
}

int bamGetTargetLength(const bam1_t *bam)
/* Tally up the alignment's length on the reference sequence from
 * bam's packed-int CIGAR representation. */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
int tLength=0;
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigarPacked[i], &op);
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
	    tLength += n;
	    break;
	case 'I': // inserted in query
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tLength += n;
	    break;
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("bamGetTargetLength: unrecognized CIGAR op %c -- update me", op);
	}
    }
return tLength;
}

struct ffAli *bamToFfAli(const bam1_t *bam, struct dnaSeq *target, int targetOffset)
/* Convert from bam to ffAli format.  (Adapted from psl.c's pslToFfAli.) */
{
struct ffAli *ffList = NULL, *ff;
const bam1_core_t *core = &bam->core;
boolean isRc = bamIsRc(bam);
DNA *needle = (DNA *)bamGetQuerySequence(bam);
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

bam1_t *bamClone(const bam1_t *bam)
/* Return a newly allocated copy of bam. */
{
// Using typecasts to get around compiler complaints about bam being const:
bam1_t *newBam = cloneMem((void *)bam, sizeof(*bam));
newBam->data = cloneMem((void *)bam->data, bam->data_len*sizeof(bam->data[0]));
return newBam;
}

void bamShowTags(const bam1_t *bam)
/* Print out tags in HTML: bold key, no type indicator for brevity. */
{
// adapted from part of bam.c bam_format1:
uint8_t *s = bam1_aux(bam);
while (s < bam->data + bam->data_len)
    {
    uint8_t type, key[2];
    key[0] = s[0]; key[1] = s[1];
    s += 2; type = *s; ++s;
    printf(" <B>%c%c</B>:", key[0], key[1]);
    if (type == 'A') { printf("%c", *s); ++s; }
    else if (type == 'C') { printf("%u", *s); ++s; }
    else if (type == 'c') { printf("%d", *s); ++s; }
    else if (type == 'S') { printf("%u", *(uint16_t*)s); s += 2; }
    else if (type == 's') { printf("%d", *(int16_t*)s); s += 2; }
    else if (type == 'I') { printf("%u", *(uint32_t*)s); s += 4; }
    else if (type == 'i') { printf("%d", *(int32_t*)s); s += 4; }
    else if (type == 'f') { printf("%g", *(float*)s); s += 4; }
    else if (type == 'd') { printf("%lg", *(double*)s); s += 8; }
    else if (type == 'Z' || type == 'H')
	{
	while (*s) putc(*s++, stdout);
	++s;
	}
    }
putc('\n', stdout);
}

char *bamGetTagString(const bam1_t *bam, char *tag, char *buf, size_t bufSize)
/* If bam's tags include the given 2-character tag, place the value into 
 * buf (zero-terminated, trunc'd if nec) and return a pointer to buf,
 * or NULL if tag is not present. */
{
if (tag == NULL)
    errAbort("NULL tag passed to bamGetTagString");
if (! (isalpha(tag[0]) && isalnum(tag[1]) && tag[2] == '\0'))
    errAbort("bamGetTagString: invalid tag '%s'", htmlEncode(tag));
char *val = NULL;
// adapted from part of bam.c bam_format1:
uint8_t *s = bam1_aux(bam);
while (s < bam->data + bam->data_len)
    {
    uint8_t type, key[2];
    key[0] = s[0]; key[1] = s[1];
    s += 2; type = *s; ++s;
    if (key[0] == tag[0] && key[1] == tag[1])
	{
	if (type == 'A') { snprintf(buf, bufSize, "%c", *s);}
	else if (type == 'C') { snprintf(buf, bufSize, "%u", *s); }
	else if (type == 'c') { snprintf(buf, bufSize, "%d", *s); }
	else if (type == 'S') { snprintf(buf, bufSize, "%u", *(uint16_t*)s); }
	else if (type == 's') { snprintf(buf, bufSize, "%d", *(int16_t*)s); }
	else if (type == 'I') { snprintf(buf, bufSize, "%u", *(uint32_t*)s); }
	else if (type == 'i') { snprintf(buf, bufSize, "%d", *(int32_t*)s); }
	else if (type == 'f') { snprintf(buf, bufSize, "%g", *(float*)s); }
	else if (type == 'd') { snprintf(buf, bufSize, "%lg", *(double*)s); }
	else if (type == 'Z' || type == 'H') strncpy(buf, (char *)s, bufSize);
	else buf[0] = '\0';
	buf[bufSize-1] = '\0'; // TODO: is this nec?? see man pages
	val = buf;
	break;
	}
    else
	{
	if (type == 'A' || type == 'C' || type == 'c') { ++s; }
	else if (type == 'S' || type == 's') { s += 2; }
	else if (type == 'I' || type == 'i' || type == 'f') { s += 4; }
	else if (type == 'd') { s += 8; }
	else if (type == 'Z' || type == 'H')
	    {
	    while (*s++);
	    }
	}
    }
return val;
}

#endif//def USE_BAM

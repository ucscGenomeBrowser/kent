/* bamFile -- interface to binary alignment format files using Heng Li's samtools lib. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "portable.h"
#include "bamFile.h"
#include "htmshell.h"
#include "htslib/cram.h"
#include "udc.h"
#include "psl.h"

#include <limits.h>

void cramCheckRefs(samfile_t *fh, char *refUrl, char *cacheDir)
/* Check if all CRAM reference sequences are locally available.  If any are
 * missing, create pending-download request files and errAbort with a message
 * telling the user to refresh.  Uses the new cram_check_required_refs() API
 * to pre-check before reading rather than discovering failures mid-decode. */
{
if (fh->format.format != cram || cacheDir == NULL)
    return;

/* Set REF_CACHE so htslib's ref resolution finds our cache dir */
setenv("REF_CACHE", cacheDir, 1);

cram_fd *cfd = fh->fp.cram;
cram_ref_check_t *chk = cram_check_required_refs(cfd, NULL, cacheDir);
if (chk == NULL || chk->n_missing == 0)
    {
    cram_ref_check_free(chk);
    return;
    }

/* Use first missing ref for the error message (matches old behavior) */
char *md5String = chk->refs[0].md5;

char errorFile[4096];
char pendingFile[4096];
safef(errorFile, sizeof errorFile, "%s/error/%s", cacheDir, md5String);

FILE *downFile;
if ((downFile = fopen(errorFile, "r")) != NULL)
    {
    char errorBuf[4096];
    mustGetLine(downFile, errorBuf, sizeof errorBuf);
    fclose(downFile);
    cram_ref_check_free(chk);
    errAbort("cannot find reference %s.  Error: %s", md5String, errorBuf);
    }

/* Create pending download requests for all missing refs */
safef(pendingFile, sizeof pendingFile, "%s/pending/", cacheDir);
makeDirsOnPath(pendingFile);

for (int i = 0; i < chk->n_missing; i++)
    {
    char *md5 = chk->refs[i].md5;
    safef(pendingFile, sizeof pendingFile, "%s/pending/%s", cacheDir, md5);
    downFile = fopen(pendingFile, "w");
    if (downFile == NULL)
        {
        cram_ref_check_free(chk);
        errAbort("cannot find reference %s.  Cannot create file %s.", md5, pendingFile);
        }
    if (refUrl != NULL)
        fprintf(downFile, refUrl, md5);
    else
        fprintf(downFile, "http://www.ebi.ac.uk:80/ena/cram/md5/%s", md5);
    fprintf(downFile, "\n");
    fclose(downFile);
    }

char server[4096];
if (refUrl != NULL)
    safef(server, sizeof server, refUrl, md5String);
else
    safef(server, sizeof server, "http://www.ebi.ac.uk:80/ena/cram/md5/%s", md5String);

cram_ref_check_free(chk);
errAbort("Downloading reference from %s. "
         "Please refresh screen to check download status.", server);
}

static bam_index_t *tryIndexCandidates(samfile_t *sam, char *fileOrUrl, const char *ext)
/* Try loading index by appending ext, then by replacing the suffix with ext.
 * Uses sam_index_load3 with flags=0 so remote indexes are read through the
 * UDC hfile scheme handler instead of htslib downloading a local copy into
 * the (non-writable) CWD. */
{
char indexName[4096];
bam_index_t *ret = NULL;

safef(indexName, sizeof indexName, "%s%s", fileOrUrl, ext);
if ((ret = sam_index_load3(sam, fileOrUrl, indexName, 0)) != NULL)
    return ret;

safef(indexName, sizeof indexName, "%s", fileOrUrl);
char *lastDot = strrchr(indexName, '.');
if (lastDot && (lastDot - indexName) + strlen(ext) < sizeof indexName)
    {
    strcpy(lastDot, ext);
    ret = sam_index_load3(sam, fileOrUrl, indexName, 0);
    }
return ret;
}

static bam_index_t *bamOpenIndexGivenUrl(samfile_t *sam, char *fileOrUrl, char *baiFileOrUrl)
/* If fileOrUrl has a valid accompanying .bai/.crai file, parse and return the index;
 * otherwise return NULL. baiFileOrUrl can be NULL.
 * The difference to bamOpenIndex is that the URL/filename of the index file can be specified. */
{
if (baiFileOrUrl != NULL)
    return sam_index_load3(sam, fileOrUrl, baiFileOrUrl, 0);

const char *ext = (sam->format.format == cram) ? ".crai" : ".bai";
return tryIndexCandidates(sam, fileOrUrl, ext);
}


static void bamCloseIdx(bam_index_t **pIdx)
/* Free unless already NULL. */
{
if (pIdx != NULL && *pIdx != NULL)
    {
    free(*pIdx); // Not freeMem, freez etc -- sam just uses malloc/calloc.
    *pIdx = NULL;
    }
}

static bam_index_t *bamOpenIdx(samfile_t *sam, char *fileOrUrl)
/* If fileOrUrl has a valid accompanying .bai/.crai file, parse and return the index;
 * otherwise return NULL. */
{
return bamOpenIndexGivenUrl(sam, fileOrUrl, NULL);
}

boolean bamFileExists(char *fileOrUrl)
/* Return TRUE if we can successfully open the bam file and its index file.
 * NOTE: this doesn't give enough diagnostics */
{
char *bamFileName = fileOrUrl;
samfile_t *fh = samopen(bamFileName, "rb", NULL);
if (fh != NULL)
    {
    bam_index_t *idx = bamOpenIdx(fh, bamFileName);
    samclose(fh);
    if (idx == NULL)
	{
	warn("bamFileExists: failed to read index corresponding to %s", bamFileName);
	return FALSE;
	}
    bamCloseIdx(&idx);
    bamClose(&fh);
    return TRUE;
    }
return FALSE;
}

samfile_t *bamOpen(char *fileOrUrl, char **retBamFileName)
/* Return an open bam file or errAbort (should be named bamMustOpen).
 * Return parameter if NON-null will return the file name (vestigial; long ago
 * there was a plan to use udcFuse filenames instead of URLs) */
{
char *bamFileName = fileOrUrl;
if (retBamFileName != NULL)
    *retBamFileName = bamFileName;

#ifdef BAM_VERSION
// suppress too verbose messages in samtools >= 0.1.18; see redmine #6491
// This variable didn't exist in older versions of samtools (where BAM_VERSION wasn't defined).
bam_verbose = 1;
#endif

samfile_t *fh = samopen(bamFileName, "rb", NULL);
// Check both fh and fh->header; non-NULL fh can have NULL header if header doesn't parse!
if (fh == NULL)
    {
    boolean usingUrl = (strstr(fileOrUrl, "tp://") || strstr(fileOrUrl, "https://"));
    struct dyString *urlWarning = dyStringNew(0);
    if (usingUrl && fh == NULL)
	{
	dyStringAppend(urlWarning,
		       ". If you are able to access the URL with your web browser, "
		       "please try reloading this page.");
	}
    errAbort("Failed to open %s%s", fileOrUrl, urlWarning->string);
    }
return fh;
}

samfile_t *bamMustOpenLocal(char *fileName, char *mode, void *extraHeader)
/* Open up sam or bam file or die trying.  The mode parameter is 
 *    "r" - open SAM to read
 *    "rb" - open BAM to read
 *    "w" - open SAM to write
 *    "wb" - open BAM to write
 * The extraHeader is generally NULL in the read case, and the write case
 * contains a pointer to a bam_header_t with information about the header.
 * The implementation is just a wrapper around samopen from the samtools library
 * that aborts with error message if there's a problem with the open. */
{
samfile_t *sf = samopen(fileName, mode, extraHeader);
if (sf == NULL)
    errnoAbort("Couldn't open %s.\n", fileName);
return sf;
}

void bamClose(samfile_t **pSamFile)
/* Close down a samfile_t */
{
if (pSamFile != NULL)
    {
    samclose(*pSamFile);
    *pSamFile = NULL;
    }
}

void bamFileAndIndexMustExist(char *fileOrUrl, char *baiFileOrUrl)
/* Open both a bam file and its accompanying index or errAbort; this is what it
 * takes for diagnostic info to propagate up through errCatches in calling code. */
{
samfile_t *bamF = bamOpen(fileOrUrl, NULL);
bam_index_t *idx = bamOpenIndexGivenUrl(bamF, fileOrUrl, baiFileOrUrl);
if (idx == NULL)
    {
    const char *ext = (bamF->format.format == cram) ? ".crai" : ".bai";
    errAbort("failed to read index file (%s) corresponding to %s", ext, fileOrUrl);
    }
bamCloseIdx(&idx);
bamClose(&bamF);
}

void bamFetchAlreadyOpen(samfile_t *samfile, bam_hdr_t *header,  bam_index_t *idx, char *bamFileName, 
			 char *position, bam_fetch_f callbackFunc, void *callbackData)
/* With the open bam file, return items the same way with the callbacks as with bamFetch() */
/* except in this case use an already-open bam file and index (use bam_index_load and free() for */
/* the index). It seems a little strange to pass the filename in with the open bam, but */
/* it's just used to report errors. */
{
bam1_t *b;
AllocVar(b);
hts_itr_t *iter = sam_itr_querys(idx, header, position);
if (iter == NULL && startsWith("chr", position))
    iter = sam_itr_querys(idx, header, position + strlen("chr"));
// Special case to support SARS-CoV-2: If header uses a RefSeq or GenBank accession instead of
// our "NC_045512v2", go with that.
if (iter == NULL && header->n_targets == 1 && startsWith("NC_045512v2", position) &&
    (startsWith("NC_045512", header->target_name[0]) ||
     startsWith("MN908947", header->target_name[0])))
    {
    char *colon = strrchr(position, ':');
    char customPos[512];
    if (colon)
        safef(customPos, sizeof customPos, "%s%s", header->target_name[0], colon);
    else
        safecpy(customPos, sizeof customPos, header->target_name[0]);
    iter = sam_itr_querys(idx, header, customPos);
    }

if (iter == NULL)
    return;
int result;
while ((result = sam_itr_next(samfile, iter, b)) >= 0)
    callbackFunc(b, callbackData, header);
}

void bamAndIndexFetchPlus(char *fileOrUrl, char *baiFileOrUrl, char *position, bam_fetch_f callbackFunc, void *callbackData,
		 samfile_t **pSamFile, char *refUrl, char *cacheDir)
/* Open the BAM/CRAM file with the index specified by baiFileOrUrl.
 * baiFileOrUrl can be NULL and defaults to <fileOrUrl>.bai (BAM) or <fileOrUrl>.crai (CRAM).
 * Fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData.
 * This handles BAM files with "chr"-less sequence names, e.g. from Ensembl. 
 * The pSamFile parameter is optional.  If non-NULL it will be filled in, just for
 * the benefit of the callback function, with the open samFile.  */
{
char *bamFileName = NULL;
samfile_t *fh = bamOpen(fileOrUrl, &bamFileName);
if (fh->format.format == cram)
    {
    if (cacheDir == NULL)
        errAbort("CRAM cache dir hg.conf variable (cramRef) must exist for CRAM support");
    setenv("REF_CACHE", cacheDir, 1);
    }
bam_hdr_t *header = sam_hdr_read(fh);
if (pSamFile != NULL)
    *pSamFile = fh;
cramCheckRefs(fh, refUrl, cacheDir);
bam_index_t *idx = bamOpenIndexGivenUrl(fh, bamFileName, baiFileOrUrl);
if (idx == NULL)
    warn("bam_index_load(%s) failed.", bamFileName);
else
    {
    bamFetchAlreadyOpen(fh, header, idx, bamFileName, position, callbackFunc, callbackData);
    bamCloseIdx(&idx);
    }
bamClose(&fh);
}

static int bamGetTargetCount(char *fileOrUrl)
/* Return the number of target sequences in a bam or cram file. */
{
htsFile *htsF = hts_open(fileOrUrl, "r");
bam_hdr_t *header = sam_hdr_read(htsF);
int tCount = sam_hdr_nref(header);
bam_hdr_destroy(header);
hts_close(htsF);
return tCount;
}

long long bamFileItemCount(char *fileOrUrl, char *baiFileOrUrl)
/* Return the total number of mapped items across all sequences in fileOrUrl, using index file.
 * If baiFileOrUrl is NULL, the index file is assumed to be fileOrUrl.bai (BAM) or fileOrUrl.crai (CRAM).
 * NOTE: not all bam index files include mapped item counts, so this may return 0 even for large
 * bam.  As of May 2019, our copy of hts_idx_get_stat does not support cram indexes
 * (perhaps they never include counts?), so this always returns 0 for cram. */
{
long long itemCount = 0;
hts_idx_t *idx = NULL;
if (isNotEmpty(baiFileOrUrl))
    idx = hts_idx_load3(fileOrUrl, baiFileOrUrl, HTS_FMT_BAI, 0);
else
    {
    int format = endsWith(fileOrUrl, ".cram") ? HTS_FMT_CRAI : HTS_FMT_BAI;
    idx = hts_idx_load3(fileOrUrl, NULL, format, 0);
    }
if (idx == NULL)
    warn("bamFileItemCount: hts_idx_load(%s) failed.", baiFileOrUrl ? baiFileOrUrl : fileOrUrl);
else
    {
    int tCount = bamGetTargetCount(fileOrUrl);
    int tid;
    for (tid = 0;  tid < tCount;  tid++)
        {
        uint64_t mapped, unmapped;
        int ret = hts_idx_get_stat(idx, tid, &mapped, &unmapped);
        if (ret == 0)
            itemCount += mapped;
        // ret is -1 if counts are unavailable.
        }
    hts_idx_destroy(idx);
    idx = NULL;
    }
return itemCount;
}

void bamFetchPlus(char *fileOrUrl, char *position, bam_fetch_f callbackFunc, void *callbackData,
		 samfile_t **pSamFile, char *refUrl, char *cacheDir)
{
bamAndIndexFetchPlus(fileOrUrl, NULL, position, callbackFunc, callbackData, pSamFile, refUrl, cacheDir);
}

void bamFetch(char *fileOrUrl, char *position, bam_fetch_f callbackFunc, void *callbackData,
		 samfile_t **pSamFile)
{
bamFetchPlus(fileOrUrl, position, callbackFunc, callbackData, pSamFile,
    NULL, NULL);
}

boolean bamIsRc(const bam1_t *bam)
/* Return TRUE if alignment is on - strand. */
{
const bam1_core_t *core = &bam->core;
return (core->flag & BAM_FREVERSE);
}

void bamGetSoftClipping(const bam1_t *bam, int *retLow, int *retHigh, int *retClippedQLen)
/* If retLow is non-NULL, set it to the number of "soft-clipped" (skipped) bases at
 * the beginning of the query sequence and quality; likewise for retHigh at end.
 * For convenience, retClippedQLen is the original query length minus soft clipping
 * (and the length of the query sequence that will be returned). */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
char op;
int n = bamUnpackCigarElement(cigarPacked[0], &op);
int low = (op == 'S') ? n : 0;
n = bamUnpackCigarElement(cigarPacked[core->n_cigar-1], &op);
int high = (op == 'S') ? n : 0;
if (retLow != NULL)
    *retLow = low;
if (retHigh != NULL)
    *retHigh = high;
if (retClippedQLen != NULL)
    *retClippedQLen = (core->l_qseq - low - high);
}


void bamUnpackQuerySequence(const bam1_t *bam, boolean useStrand, char *qSeq)
/* Fill in qSeq with the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so if useStrand is given we rev-comp it back to restore the original query 
 * sequence. */
{
const bam1_core_t *core = &bam->core;
int qLen = core->l_qseq;
uint8_t *packedQSeq = bam1_seq(bam);
int i;
for (i = 0; i < qLen; i++)
    qSeq[i] = bam_nt16_rev_table[bam1_seqi(packedQSeq, i)];
qSeq[i] = '\0';
if (useStrand && bamIsRc(bam))
    reverseComplement(qSeq, qLen);
}

char *bamGetQuerySequence(const bam1_t *bam, boolean useStrand)
/* Allocate and return the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so if useStrand is given we rev-comp it back to restore the original query 
 * sequence. */
{
const bam1_core_t *core = &bam->core;
int qLen = core->l_qseq;
char *qSeq = needMem(qLen+1);
bamUnpackQuerySequence(bam, useStrand, qSeq);
return qSeq;
}

UBYTE *bamGetQueryQuals(const bam1_t *bam, boolean useStrand)
/* Return the base quality scores encoded in bam as an array of ubytes. */
{
const bam1_core_t *core = &bam->core;
int qLen = core->l_qseq;
UBYTE *arr = needMem(qLen);
boolean isRc = useStrand && bamIsRc(bam);
UBYTE *qualStr = bam1_qual(bam);
int i;
for (i = 0;  i < core->l_qseq;  i++)
    {
    int offset = isRc ? (qLen - 1 - i) : i;
    arr[i] = (qualStr[0] == 255) ? 255 : qualStr[offset];
    }
return arr;
}

void bamUnpackCigar(const bam1_t *bam, struct dyString *dyCigar)
/* Unpack CIGAR string into dynamic string */
{
unsigned int *cigarPacked = bam1_cigar(bam);
const bam1_core_t *core = &bam->core;
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigarPacked[i], &op);
    dyStringPrintf(dyCigar, "%d", n);
    dyStringAppendC(dyCigar, op);
    dyStringAppendC(dyCigar, ' ');
    }
}

char *bamGetCigar(const bam1_t *bam)
/* Return a BAM-enhanced CIGAR string, decoded from the packed encoding in bam. */
{
const bam1_core_t *core = &bam->core;
struct dyString *dyCigar = dyStringNew(min(8, core->n_cigar*4));
bamUnpackCigar(bam, dyCigar);
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
	case '=': // match
	    printf("%d Match", n);
	    break;
	case 'X': // mismatch
	    printf("%d Mismatch", n);
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
	case '=': // match
	case 'X': // mismatch
	    tLength += n;
	    break;
	case 'I': // inserted in query
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tLength += n;
	    break;
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("bamGetTargetLength: unrecognized CIGAR op %c -- update me", op);
	}
    }
return tLength;
}

bam1_t *bamClone(const bam1_t *bam)
/* Return a newly allocated copy of bam. */
{
// Using typecasts to get around compiler complaints about bam being const:
bam1_t *newBam = cloneMem((void *)bam, sizeof(*bam));
newBam->data = cloneMem((void *)bam->data, bam->data_len*sizeof(bam->data[0]));
return newBam;
}

static inline int typeSize(int x)
/* Return size of basic BAM types. */
{
if (x == 'C' || x == 'c' || x == 'A') return 1;
else if (x == 'S' || x == 's') return 2;
else if (x == 'I' || x == 'i' || x == 'f' || x == 'F') return 4;
else return 0;
}      

static void printType(uint8_t type, uint8_t **str)
/* Print out a string that has a value of the given type. */
{
uint8_t *s = *str;
if (type == 'A') { printf("%c", *s); ++s; }
else if (type == 'C') { printf("%u", *s); ++s; }
else if (type == 'c') { printf("%d", *(int8_t*)s); ++s; }
else if (type == 'S') { printf("%u", *(uint16_t*)s); s += 2; }
else if (type == 's') { printf("%d", *(int16_t*)s); s += 2; }
else if (type == 'I') { printf("%u", *(uint32_t*)s); s += 4; }
else if (type == 'i') { printf("%d", *(int32_t*)s); s += 4; }
else if (type == 'f') { printf("%g", *(float*)s); s += 4; }
else if (type == 'd') { printf("%lg", *(double*)s); s += 8; }
else if (type == 'B')
    {
    uint8_t subType =  *s++;
    int count =  *(int32_t*)s;
    s += 4;

    int ii;
    for (ii=0; ii < count; ii++)
        {
        printType(subType, &s);
        if (ii < count - 1)
            printf(",");
        }
    }
else if (type == 'Z' || type == 'H')
    {
    htmTextOut(stdout, (char *)s);
    s += strlen((char *)s) + 1;
    }
else
    errAbort("missing BAM type %c", type);
*str = s;
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
    printType(type, &s);
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
	else if (type == 'B') 
            {
            // 5 is for type byte and a following int32
            s += 5 + typeSize(*(s)) * (*(int32_t*)((s)+1));
            }
	else if (type == 'Z' || type == 'H')
	    {
	    while (*s++);
	    }
	}
    }
return val;
}

void bamUnpackAux(const bam1_t *bam, struct dyString *dy)
/* Unpack the tag:type:val part of bam into dy */
{
// adapted from part of bam.c bam_format1:
uint8_t *s = bam1_aux(bam);
boolean firstTime = TRUE;
while (s < bam->data + bam->data_len)
    {
    if (firstTime)
        firstTime = FALSE;
    else
        dyStringAppendC(dy, '\t');
    dyStringAppendC(dy, *s++);
    dyStringAppendC(dy, *s++);
    dyStringAppendC(dy, ':');
    dyStringAppendC(dy, s[0]);
    dyStringAppendC(dy, ':');
    uint8_t type = *s++;
    if (type == 'A') { dyStringPrintf(dy, "%c", *s); ++s; }
    else if (type == 'C') { dyStringPrintf(dy, "%u", *s); ++s; }
    else if (type == 'c') { dyStringPrintf(dy, "%d", *s); ++s; }
    else if (type == 'S') { dyStringPrintf(dy, "%u", *(uint16_t*)s); s += 2; }
    else if (type == 's') { dyStringPrintf(dy, "%d", *(int16_t*)s); s += 2; }
    else if (type == 'I') { dyStringPrintf(dy, "%u", *(uint32_t*)s); s += 4; }
    else if (type == 'i') { dyStringPrintf(dy, "%d", *(int32_t*)s); s += 4; }
    else if (type == 'f') { dyStringPrintf(dy, "%g", *(float*)s); s += 4; }
    else if (type == 'd') { dyStringPrintf(dy, "%lg", *(double*)s); s += 8; }
    else if (type == 'Z' || type == 'H')
	{
	dyStringAppend(dy, (char *)s);
	s += strlen((char *)s) + 1;
	}
    }
}

struct psl *bamToPslUnscored2(const bam1_t *bam, const bam_hdr_t *hdr, int inclHardClipped)
/* Translate BAM's numeric CIGAR encoding into PSL sufficient for cds.c (just coords,
 * no scoring info).  If inclHardClipped is True, the size of the hard-clipped regions
 * are include in the PSL.  This is required for supplementary alignments to have the
 * correct query sizes and offsets.
 */
{
const bam1_core_t *core = &bam->core;

if (core->tid == -1)
    return NULL;

struct psl *psl;
AllocVar(psl);
boolean isRc = (core->flag & BAM_FREVERSE);
psl->strand[0] = isRc ? '-' : '+';
psl->qName = cloneString(bam1_qname(bam));
psl->tName = cloneString(hdr->target_name[core->tid]);
unsigned blockCount = 0;
unsigned *blockSizes, *qStarts, *tStarts;
AllocArray(blockSizes, core->n_cigar);
AllocArray(qStarts, core->n_cigar);
AllocArray(tStarts, core->n_cigar);
int tPos = core->pos, qPos = 0, qLength = 0;
unsigned int *cigar = bam1_cigar(bam);
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigar[i], &op);
    switch (op)
	{
	case 'X': // mismatch (gapless aligned block)
	case '=': // match (gapless aligned block)
	case 'M': // match or mismatch (gapless aligned block)
	    blockSizes[blockCount] = n;
	    qStarts[blockCount] = qPos;
	    tStarts[blockCount] = tPos;
	    blockCount++;
	    tPos += n;
	    qPos += n;
	    qLength += n;
            if (op == 'X')
               psl->misMatch += n;
            else
               psl->match += n;
	    break;
	case 'I': // inserted in query
	    qPos += n;
	    qLength += n;
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tPos += n;
	    break;
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	    qPos += n;
	    qLength += n;
	    break;
	case 'H': // note query bases not stored in record's query sequence ("hard clipping")
            if (inclHardClipped)
                {
                qPos += n;
                qLength += n;
                }
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("bamToPsl: unrecognized CIGAR op %c -- update me", op);
	}
    }

if (blockCount == 0)
    {
    pslFree(&psl);
    return NULL;
    }

psl->tSize = hdr->target_len[core->tid];
psl->tStart = tStarts[0];
psl->tEnd = tStarts[blockCount-1] + blockSizes[blockCount-1];
psl->qSize = qLength;
psl->qStart = qStarts[0];
psl->qEnd = qStarts[blockCount-1] + blockSizes[blockCount-1];
if (isRc)
    reverseIntRange(&psl->qStart, &psl->qEnd, psl->qSize);
psl->blockCount = blockCount;
psl->blockSizes = blockSizes;
psl->qStarts = qStarts;
psl->tStarts = tStarts;
pslComputeInsertCounts(psl);
return psl;
}

struct psl *bamToPslUnscored(const bam1_t *bam, const bam_hdr_t *hdr)
/* Translate BAM's numeric CIGAR encoding into PSL sufficient for cds.c (just coords,
 * no scoring info) */
{
return bamToPslUnscored2(bam, hdr, FALSE);
}


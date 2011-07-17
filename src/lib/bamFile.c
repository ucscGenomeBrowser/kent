/* bamFile -- interface to binary alignment format files using Heng Li's samtools lib. */

#include "common.h"
#include "portable.h"
#include "bamFile.h"
#ifdef USE_BAM
#include "htmshell.h"
#include "udc.h"

#ifndef KNETFILE_HOOKS
static char *getSamDir()
/* Return the name of a trash dir for samtools to run in (it creates files in current dir)
 * and make sure the directory exists. */
{
static char *samDir = NULL;
char *dirName = "samtools";
if (samDir == NULL)
    {
    mkdirTrashDirectory(dirName);
    size_t len = strlen(trashDir()) + 1 + strlen(dirName) + 1;
    samDir = needMem(len);
    safef(samDir, len, "%s/%s", trashDir(), dirName);
    }
return samDir;
}
#endif//ndef KNETFILE_HOOKS

boolean bamFileExists(char *fileOrUrl)
/* Return TRUE if we can successfully open the bam file and its index file. */
{
char *bamFileName = fileOrUrl;
samfile_t *fh = samopen(bamFileName, "rb", NULL);
boolean usingUrl = TRUE; 
usingUrl = (strstr(fileOrUrl, "tp://") || strstr(fileOrUrl, "https://"));
if (fh != NULL)
    {
#ifndef KNETFILE_HOOKS
    // When file is an URL, this caches the index file in addition to validating:
    // Since samtools's url-handling code saves the .bai file to the current directory,
    // chdir to a trash directory before calling bam_index_load, then chdir back.
    char *runDir = getCurrentDir();
    char *samDir = getSamDir();
    if (usingUrl)
	setCurrentDir(samDir);
#endif//ndef KNETFILE_HOOKS
    bam_index_t *idx = bam_index_load(bamFileName);
#ifndef KNETFILE_HOOKS
    if (usingUrl)
	setCurrentDir(runDir);
#endif//ndef KNETFILE_HOOKS
    samclose(fh);
    if (idx == NULL)
	{
	warn("bamFileExists: failed to read index corresponding to %s", bamFileName);
	return FALSE;
	}
    free(idx); // Not freeMem, freez etc -- sam just uses malloc/calloc.
    return TRUE;
    }
return FALSE;
}

samfile_t *bamOpen(char *fileOrUrl, char **retBamFileName)
/* Return an open bam file, dealing with FUSE caching if need be. 
 * Return parameter if NON-null will return the file name after FUSing */
{
char *bamFileName = fileOrUrl;
if (retBamFileName != NULL)
    *retBamFileName = bamFileName;
samfile_t *fh = samopen(bamFileName, "rb", NULL);
if (fh == NULL)
    {
    boolean usingUrl = (strstr(fileOrUrl, "tp://") || strstr(fileOrUrl, "https://"));
    struct dyString *urlWarning = dyStringNew(0);
    if (usingUrl)
	{
	dyStringAppend(urlWarning,
		       ". If you are able to access the URL with your web browser, "
		       "please try reloading this page.");
	}
    errAbort("Failed to open %s%s", fileOrUrl, urlWarning->string);
    }
return fh;
}

void bamClose(samfile_t **pSamFile)
/* Close down a samefile_t */
{
if (pSamFile != NULL)
    {
    samclose(*pSamFile);
    *pSamFile = NULL;
    }
}

void bamFetchAlreadyOpen(samfile_t *samfile, bam_index_t *idx, char *bamFileName, 
			 char *position, bam_fetch_f callbackFunc, void *callbackData)
/* With the open bam file, return items the same way with the callbacks as with bamFetch() */
/* except in this case use an already-open bam file and index (use bam_index_load and free() for */
/* the index). It seems a little strange to pass the filename in with the open bam, but */
/* it's just used to report errors. */
{
int chromId, start, end;
int ret = bam_parse_region(samfile->header, position, &chromId, &start, &end);
if (ret != 0 && startsWith("chr", position))
    ret = bam_parse_region(samfile->header, position+strlen("chr"), &chromId, &start, &end);
if (ret != 0)
    // If the bam file does not cover the current chromosome, OK
    return;
ret = bam_fetch(samfile->x.bam, idx, chromId, start, end, callbackData, callbackFunc);
if (ret != 0)
    warn("bam_fetch(%s, %s (chromId=%d) failed (%d)", bamFileName, position, chromId, ret);
}

void bamFetch(char *fileOrUrl, char *position, bam_fetch_f callbackFunc, void *callbackData,
		 samfile_t **pSamFile)
/* Open the .bam file, fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData.
 * This handles BAM files with "chr"-less sequence names, e.g. from Ensembl. 
 * The pSamFile parameter is optional.  If non-NULL it will be filled in, just for
 * the benefit of the callback function, with the open samFile.  */
{
char *bamFileName = NULL;
samfile_t *fh = bamOpen(fileOrUrl, &bamFileName);
boolean usingUrl = TRUE;
usingUrl = (strstr(fileOrUrl, "tp://") || strstr(fileOrUrl, "https://"));
if (pSamFile != NULL)
    *pSamFile = fh;
#ifndef KNETFILE_HOOKS
// Since samtools' url-handling code saves the .bai file to the current directory,
// chdir to a trash directory before calling bam_index_load, then chdir back.
char *runDir = getCurrentDir();
char *samDir = getSamDir();
if (usingUrl)
    setCurrentDir(samDir);
#endif//ndef KNETFILE_HOOKS
bam_index_t *idx = bam_index_load(bamFileName);
#ifndef KNETFILE_HOOKS
if (usingUrl)
    setCurrentDir(runDir);
#endif//ndef KNETFILE_HOOKS
if (idx == NULL)
    warn("bam_index_load(%s) failed.", bamFileName);
else
    {
    bamFetchAlreadyOpen(fh, idx, bamFileName, position, callbackFunc, callbackData);
    free(idx); // Not freeMem, freez etc -- sam just uses malloc/calloc.
    }
bamClose(&fh);
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
	htmTextOut(stdout, (char *)s);
	s += strlen((char *)s) + 1;
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

#else
// If we're not compiling with samtools, make stub routines so compile won't fail:

boolean bamFileExists(char *bamFileName)
/* Return TRUE if we can successfully open the bam file and its index file. */
{
warn(COMPILE_WITH_SAMTOOLS, "bamFileExists");
return FALSE;
}

samfile_t *bamOpen(char *fileOrUrl, char **retBamFileName)
/* Return an open bam file */
{
warn(COMPILE_WITH_SAMTOOLS, "bamOpenUdc");
return FALSE;
}

void bamClose(samfile_t **pSamFile)
/* Close down a samefile_t */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamClose");
}

void bamFetch(char *fileOrUrl, char *position, bam_fetch_f callbackFunc, void *callbackData,
	      samfile_t **pSamFile)
/* Open the .bam file, fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData.
 * This handles BAM files with "chr"-less sequence names, e.g. from Ensembl.
 * The pSamFile parameter is optional.  If non-NULL it will be filled in, just for
 * the benefit of the callback function, with the open samFile.  */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamFetch");
}

boolean bamIsRc(const bam1_t *bam)
/* Return TRUE if alignment is on - strand. */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamIsRc");
return FALSE;
}

void bamGetSoftClipping(const bam1_t *bam, int *retLow, int *retHigh, int *retClippedQLen)
/* If retLow is non-NULL, set it to the number of "soft-clipped" (skipped) bases at
 * the beginning of the query sequence and quality; likewise for retHigh at end.
 * For convenience, retClippedQLen is the original query length minus soft clipping
 * (and the length of the query sequence that will be returned). */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamGetSoftClipping");
}

char *bamGetQuerySequence(const bam1_t *bam, boolean useStrand)
/* Return the nucleotide sequence encoded in bam.  The BAM format
 * reverse-complements query sequence when the alignment is on the - strand,
 * so if useStrand is given we rev-comp it back to restore the original query
 * sequence. */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamGetQuerySequence");
return NULL;
}

UBYTE *bamGetQueryQuals(const bam1_t *bam, boolean useStrand)
/* Return the base quality scores encoded in bam as an array of ubytes. */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamGetQueryQuals");
return NULL;
}

char *bamGetCigar(const bam1_t *bam)
/* Return a BAM-enhanced CIGAR string, decoded from the packed encoding in bam. */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamGetCigar");
return NULL;
}

void bamShowCigarEnglish(const bam1_t *bam)
/* Print out cigar in English e.g. "20 (mis)Match, 1 Deletion, 3 (mis)Match" */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamShowCigarEnglish");
}

void bamShowFlagsEnglish(const bam1_t *bam)
/* Print out flags in English, e.g. "Mate is on '-' strand; Properly paired". */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamShowFlagsEnglish");
}

int bamGetTargetLength(const bam1_t *bam)
/* Tally up the alignment's length on the reference sequence from
 * bam's packed-int CIGAR representation. */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamGetTargetLength");
return 0;
}

bam1_t *bamClone(const bam1_t *bam)
/* Return a newly allocated copy of bam. */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamClone");
return NULL;
}

void bamShowTags(const bam1_t *bam)
/* Print out tags in HTML: bold key, no type indicator for brevity. */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamShowTags");
}

char *bamGetTagString(const bam1_t *bam, char *tag, char *buf, size_t bufSize)
/* If bam's tags include the given 2-character tag, place the value into
 * buf (zero-terminated, trunc'd if nec) and return a pointer to buf,
 * or NULL if tag is not present. */
{
errAbort(COMPILE_WITH_SAMTOOLS, "bamGetTagString");
return NULL;
}

#endif//ndef USE_BAM

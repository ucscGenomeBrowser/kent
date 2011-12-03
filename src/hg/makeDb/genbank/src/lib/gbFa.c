/* FASTA I/O for genbank update.  See gbFa.h for why this exists. */
#include "common.h"
#include "errabort.h"
#include "gbFileOps.h"
#include "gbFa.h"


/* 16kb seems like a good size by experiment */
#define FA_STDIO_BUFSIZ (16*1024)
#define SEQ_LINE_LEN 50

struct gbFa* gbFaOpen(char *fileName, char* mode)
/* open a fasta file for reading or writing.  Uses atomic file
 * creation for write.*/
{
struct gbFa *fa;
AllocVar(fa);

strcpy(fa->fileName, fileName);
if (!(sameString(mode, "r") || sameString(mode, "w") || sameString(mode, "a")))
    errAbort("gbFaOpen only supports modes \"r\", \"w\" \"a\", got \"%s\"",
             mode);
fa->mode[0] = mode[0];

if (mode[0] == 'w')
    fa->fh = gbMustOpenOutput(fileName);
else
    fa->fh = gzMustOpen(fileName, mode);

if (mode[0] == 'a')
    {
    /* line number is wrong on append */
    fa->off = ftello(fa->fh);
    if (fa->off < 0)
        errnoAbort("can't get offset for append access for file: %s",
                   fileName);
    }
fa->fhBuf = needMem(FA_STDIO_BUFSIZ);
setbuffer(fa->fh, fa->fhBuf, FA_STDIO_BUFSIZ);

/* setup buffers if read access */
if (mode[0] == 'r')
    {
    fa->headerCap = 256;
    fa->headerBuf = needMem(fa->headerCap);
    fa->seqCap = 1024;
    fa->seqBuf = needMem(fa->seqCap);
    }
return fa;
}

void gbFaClose(struct gbFa **faPtr)
/* close a fasta file, check for any undetected I/O errors. */
{
struct gbFa *fa = *faPtr;
if (fa != NULL)
    {
    if (ferror(fa->fh))
        errAbort("%s error on %s", ((fa->mode[0] != 'r') ? "write" : "read"),
                 fa->fileName);
    if (fa->mode[0] == 'w')
        gbOutputRename(fa->fileName, &fa->fh);
    else
        gzClose(&fa->fh);
    freez(&fa->headerBuf);
    freez(&fa->seqBuf);
    freez(&fa->fhBuf);
    freez(faPtr);  /* NULLs var */
    }
}

static unsigned expandHeader(struct gbFa *fa)
/* expand or initially allocate header memory, return new capacity */
{
unsigned newSize = 2*fa->headerCap;
fa->headerBuf = needMoreMem(fa->headerBuf, fa->headerCap, newSize);
fa->headerCap = newSize;
return fa->headerCap;
}

boolean gbFaReadNext(struct gbFa *fa)
/* read the next fasta record header. The sequence is not read until
 * gbFaGetSeq is called */
{
boolean atBOLN = TRUE; /* always stops after a line */
char c, *next;
unsigned iHdr = 0, hdrCap = fa->headerCap;
off_t off = fa->off;

fa->seq = NULL;

/* find next header */
while (((c = getc_unlocked(fa->fh)) != EOF) && !((c == '>') && atBOLN))
    {
    off++;
    atBOLN = (c == '\n');
    }
fa->recOff = off; /* offset of '>' */
fa->off = ++off; /* count '>' */
if (c == EOF)
    return FALSE;

/* read header */
while ((c = getc_unlocked(fa->fh)) != EOF)
    {
    off++;
    if (iHdr == hdrCap)
        hdrCap = expandHeader(fa);
    fa->headerBuf[iHdr++] = c;
    if (c == '\n')
        break; /* got it */
}
fa->off = off;
if (c == EOF)
    errAbort("premature EOF in %s", fa->fileName);
fa->headerBuf[iHdr-1] = '\0';  /* wack newline */

next = fa->headerBuf;
fa->id = next;
next = skipToSpaces(next);
if (next != NULL)
    {
    *next++ = '\0';
    fa->comment = trimSpaces(next);
    }
else
    fa->comment = "";  /* empty string */
return TRUE;
}

static unsigned expandSeq(struct gbFa *fa)
/* expand or initially allocate seq memory, return new capacity */
{
unsigned newSize = 2*fa->seqCap;
fa->seqBuf = needMoreMem(fa->seqBuf, fa->seqCap, newSize);
fa->seqCap = newSize;
return fa->seqCap;
}

static void faReadSeq(struct gbFa *fa)
/* read next sequence into buffer */
{
char c;
unsigned iSeq = 0, seqCap = fa->seqCap;
off_t off = fa->off;

/* read seq */
while (((c = getc_unlocked(fa->fh)) != EOF) && (c != '>'))
    {
    off++;
    if (iSeq == seqCap)
        seqCap = expandSeq(fa);
    if (!isspace(c))
        fa->seqBuf[iSeq++] = c;
    }
if (c == '>')
    ungetc(c, fa->fh);
fa->off = off;  /* '>' was put back, so it's not counted */
fa->seqLen = iSeq;
if (iSeq == seqCap)
    seqCap = expandSeq(fa);
fa->seqBuf[iSeq] = '\0';
fa->seq = fa->seqBuf;
}

char* gbFaGetSeq(struct gbFa *fa)
/* Get the sequence for the current record, reading it if not already
 * buffered */
{
if (fa->seq == NULL)
    faReadSeq(fa);
return fa->seq;
}

void gbFaWriteSeq(struct gbFa *fa, char *id, char *comment, char *seq, int len)
/* write a sequence, comment maybe null, len is the length to write, or -1
 * for all of seq. */
{
int cnt = 0, iSeq = 0;
if (len < 0)
    len = strlen(seq);
fa->recOff = fa->off;

/* write header */
fputc('>', fa->fh);
fputs(id, fa->fh);
cnt += strlen(id) + 1;
if (comment != NULL)
    {
    fputc(' ', fa->fh);
    fputs(comment, fa->fh);
    cnt += strlen(comment) + 1;
    }
fputc('\n', fa->fh);
cnt++;

/* write seq */
while (iSeq < len)
    {
    int wrSize = ((len - iSeq) > SEQ_LINE_LEN) ? SEQ_LINE_LEN : (len - iSeq);
    fwrite(seq+iSeq, 1, wrSize, fa->fh);
    fputc('\n', fa->fh);
    cnt += wrSize + 1;
    iSeq += wrSize;
    }

if (ferror(fa->fh))
    errnoAbort("write failed: %s", fa->fileName);
fa->off += cnt;
}

static void copyHeader(struct gbFa *fa, struct gbFa *inFa, char *newHdr)
/* copy or repelace  header a fa header */
{
unsigned writeLen = 0;

/* write header */
fputc('>', fa->fh);
if (newHdr != NULL)
    {
    fputs(newHdr, fa->fh);
    writeLen += strlen(newHdr);
    }
else
    {
    fputs(inFa->id, fa->fh);
    writeLen += strlen(inFa->id);
    if (inFa->comment[0] != '\0')
        {
        fputc(' ', fa->fh);
        fputs(inFa->comment, fa->fh);
        writeLen += strlen(inFa->comment) + 1;
        }
    }
fputc('\n', fa->fh);
writeLen += 2;  /* > an \n */
fa->off += writeLen;
}

static void copySeq(struct gbFa *fa, struct gbFa *inFa)
/* copy the sequence part of the record */
{
unsigned writeCnt = 0;
int len;
char *seqp = gbFaGetSeq(inFa);

for(len = inFa->seqLen; (len >= SEQ_LINE_LEN);
    len -= SEQ_LINE_LEN, seqp += SEQ_LINE_LEN)
    {
    fwrite(seqp, 1, SEQ_LINE_LEN, fa->fh);
    fputc('\n', fa->fh);
    writeCnt += SEQ_LINE_LEN + 1;
    }
if (len > 0)
    {
    fwrite(seqp, 1, len, fa->fh);
    fputc('\n', fa->fh);
    writeCnt += len + 1;
    }
fa->off += writeCnt;
}

void gbFaWriteFromFa(struct gbFa *fa, struct gbFa *inFa, char *newHdr)
/* Write a fasta sequence that is buffered gbFa object.  If newHdr is not
 * null, then it is a replacement header, not including the '>' or '\n'.  If
 * null, the header in the inGbFa is used as-is.
 */
{
fa->recOff = fa->off;

copyHeader(fa, inFa, newHdr);
copySeq(fa, inFa);

if (ferror(fa->fh))
    errnoAbort("write failed: %s", fa->fileName);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


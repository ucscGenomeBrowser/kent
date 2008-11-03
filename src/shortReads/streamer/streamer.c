/* streamer - Splat like program that indexes reads instead of genome. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "twoBit.h"
#include "dnaLoad.h"

static char const rcsid[] = "$Id: streamer.c,v 1.1 2008/11/03 19:06:30 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "streamer - Splat like program that indexes reads instead of genome\n"
  "usage:\n"
  "   streamer XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void packedToHexes(UBYTE *packedDna, int *hexes, int bases)
/* Convert given number of bases of packed DNA into hexes. */
{
int unpackOps = (bases+11)/12;
while (--unpackOps >= 0)
    {
    *hexes++ = (packedDna[0] << 4) + (packedDna[1]>>4);
    *hexes++ = ((packedDna[1] & 15) << 8) + packedDna[2];
    packedDna += 3;
    }
}

void unpackNoGap0(UBYTE *packedDna, int *retBefore1, int *retBefore2, 
	int *retMiddle, int *retAfter1, int *retAfter2)
/* Unpack with no gap when on even byte boundary. */
{
/* 01 23 45 67   01 23 ^ 45 67   01 23 45 67 */
*retBefore1 = (packedDna[0] << 4) + (packedDna[1] >> 4);
*retBefore2 = ((packedDna[1] & 0xF) << 8) + packedDna[2];
*retMiddle = (packedDna[3]<<16) + (packedDna[4]<<8) + packedDna[5];
*retAfter1 = (packedDna[6] << 4) + (packedDna[7] >> 4);
*retAfter2 = ((packedDna[7] & 0xF) << 8) + packedDna[8];
}

void unpackNoGap1(UBYTE *packedDna, int *retBefore1, int *retBefore2, 
	int *retMiddle, int *retAfter1, int *retAfter2)
/* Unpack with no gap when offset by 1 into first byte. */
{
/* 0              1              2            3              4            5            6              7              8            9  */
/* 01 ^ 23 45 67  01 23 45 ^ 67  01 23 45 67  01 ^ 23 45 67  01 23 56 78  01 23 45 67  01 ^ 23 45 67  01 23 45 ^ 67  01 23 45 67  01 ^ 23 45 67 */
*retBefore1 = ((packedDna[0]&0x3F) << 6) | (packedDna[1] >> 2);
*retBefore2 = ((packedDna[1]&3) << 10) | (packedDna[2] << 2) | (packedDna[3] >> 6);
*retMiddle = ((packedDna[3]&0x3F) << 18) | (packedDna[4]<<10) | (packedDna[5] << 2) | ((packedDna[6] & 0xC0) >> 6);
*retAfter1 = ((packedDna[6]&0x3F)<<6) | (packedDna[7] >> 2);
*retAfter2 = ((packedDna[7]&3)<<10) | (packedDna[8] << 2) | (packedDna[9] >> 6);
}

void unpackNoGap2(UBYTE *packedDna, int *retBefore1, int *retBefore2, 
	int *retMiddle, int *retAfter1, int *retAfter2)
/* Unpack with no gap when offset by 2 into first byte. */
{
/* 0              1               2            3              4            5            6              7               8            9  */
/* 01 23 ^ 45 67  01 23 45 67  ^  01 23 45 67  01 23 ^ 45 67  01 23 56 78  01 23 45 67  01 23 ^ 45 67  01 23 45 67  ^  01 23 45 67  01 23 ^ 45 67 */
*retBefore1 = ((packedDna[0] & 0xF) << 8) | packedDna[1];
*retBefore2 = (packedDna[2] << 4) | (packedDna[3] >> 4);
*retMiddle = ((packedDna[3] & 0xF) << 20) | (packedDna[4] << 12) | (packedDna[5] << 4) | (packedDna[6] >> 4);
*retAfter1 = ((packedDna[6] & 0xF) << 8) | packedDna[7];
*retAfter2 = (packedDna[8] << 4) | (packedDna[0] >> 4);
}

void unpackNoGap3(UBYTE *packedDna, int *retBefore1, int *retBefore2, 
	int *retMiddle, int *retAfter1, int *retAfter2)
/* Unpack with no gap when offset by 2 into first byte. */
{
/* 0              1             2              3              4            5            6              7             8              9  */
/* 01 23 45 ^ 67  01 23 45 67   01 ^ 23 45 67  01 23 45 ^ 67  01 23 56 78  01 23 45 67  01 23 45 ^ 67  01 23 45 67   01 ^ 23 45 67  01 23 45 ^ 67 */
*retBefore1 = ((packedDna[0]&3) << 10) | (packedDna[1] << 2) | (packedDna[2] >> 6);
*retBefore2 = ((packedDna[2]&0x3f) << 6) | (packedDna[3] >> 1);
*retMiddle = ((packedDna[3]&3) << 22) | (packedDna[4] << 14) | (packedDna[5] << 6) | (packedDna[6] >> 2);
*retAfter1 = ((packedDna[6]&3)<<10) | (packedDna[7] << 2) | (packedDna[8] >> 6);
*retAfter2 = ((packedDna[8]&0x3f) << 6) | (packedDna[0] >> 6);
}


void streamThroughTwoBit(struct twoBit *twoBit)
/* Stream through a single packed file. */
{
/* Our data as 12 middle bases, plus two 6mers before and two after - 24 in total. */
int middle;
int before1, before2, after1, after2;
int seqBytes = (twoBit->size + 3)/4;
int lastByte = seqBytes - 10;
int i;
UBYTE *packedDna = twoBit->data;
for (i=0; i<lastByte; ++i)
    {
    unpackNoGap0(packedDna, &before1, &before2, &middle, &after1, &after2);
    unpackNoGap1(packedDna, &before1, &before2, &middle, &after1, &after2);
    unpackNoGap2(packedDna, &before1, &before2, &middle, &after1, &after2);
    unpackNoGap3(packedDna, &before1, &before2, &middle, &after1, &after2);
    packedDna += 1;
    }


#ifdef SOON
                  /* No Gap */
/* 0   1   2   3   4   5   6   7   8   9   A   B    */	/* Byte offsets */
/* 012301230123012301230123012301230123012301230123 */	/* Base offsets. */
/* 012345670123456701234567012345670123456701234567 */
/* B1    B2    M           A1    A2     */

                  /* One Base Gap */
/* 0   1   2   3   4   5   6   7   8   9   A   B    */	/* Byte offsets */
/* 012301230123012301230123012301230123012301230123 */	/* Base offsets. */
/* 012345012345 0123456789AB 012345012345 */
/* B1    B2     M            A1    A2     */

                  /* Two Base Gap */
/* 0   1   2   3   4   5   6   7   8   9   A   B    */	/* Byte offsets */
/* 012301230123012301230123012301230123012301230123 */	/* Base offsets. */
/* 012345012345  0123456789AB  012345012345 */
/* B1    B2      M             A1    A2     */

                  /* Three Base Gap */
/* 0   1   2   3   4   5   6   7   8   9   A   B    */	/* Byte offsets */
/* 012301230123012301230123012301230123012301230123 */	/* Base offsets. */
/* 012345012345   0123456789AB   012345012345 */
/* B1    B2       M              A1    A2     */

int maxGap = 3;

int before1ByteOffset=0;
int before1BitOffset=0;
int before2ByteOffset=1;
int before2BitOffset=2;
int midByteOffset, midBitOffset
switch (maxGap)
    {
    case 0:
       midByteOffset = 3;
       midBitOffset = 0;
       after1ByteOffset = 6;
       after1BitOffset = 0;
       after2ByteOffset=7;
       after2BitOffset=2
       break;
    case 1:
       midByteOffset = 3;
       midBitOffset = 1;
       after1ByteOffset = 6;
       after1BitOffset = 2
       after2ByteOffset = 8;
       after2BitOffset = 0;
       break;
    case 2:
       midByteOffset = 3;
       midBitOffset = 2;
       after1ByteOffset = 7;
       after1BitOffset = 0
       after2ByteOffest = 8;
       after2BitOffset = 2;
       break;
    case 3:
       midByteOffset = 3;
       midBitOffset = 3;
       after1ByteOffset = 7;
       after1BitOffset = 2;
       after2ByteOffset = 9;
       after2BitOffset = 0;
       break;
    default:
       errAbort("Can't handle gap size %d, just 0 to 3\n", maxGap);
       midByteOffset = 0;	/* Keep compiler from complaining */
       midBitOffset = 0;	/* Keep compiler from complaining */
       after1ByteOffset = after1BitOffset = 0;
       after2ByteOffset = after2BitOffset = 0;
       break;
    }

#endif /* SOON */
}

void streamer(char *input)
/* streamer - Splat like program that indexes reads instead of genome. */
{
uglyTime(NULL);
struct twoBitFile *tbf = twoBitOpen(input);
uglyTime("twoBitOpen");
struct twoBitIndex *tbi;
for (tbi = tbf->indexList; tbi != NULL; tbi = tbi->next)
    {
    struct twoBit *twoBit = twoBitOneFromFile(tbf, tbi->name);
    uglyTime("read %s", tbi->name);
    streamThroughTwoBit(twoBit);
    uglyTime("Streamed through %s", tbi->name);
    twoBitFree(&twoBit);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
streamer(argv[1]);
return 0;
}

#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"


/* Search for strings of Ns in sequence. */
static void findNRanges(char *dirname, char *chromname) 
{
char fullpath[512];
FILE *f;
struct dnaSeq *seq;
int nibSize;
int endBuf;
int bufSize = 16384;
int i;
int basesInBuf;
char c;
int block = 0;
char *dna;
char *posPtr;
int pos;
char *myindex, *myindex2;
boolean matchRegion = FALSE;
int startpos = 0, endpos = 0;
char blockname[512];
int blockcount = 0;
int size = 0;

strcpy(fullpath, "");
strcat(fullpath, dirname);
strcat(fullpath, chromname);
strcat(fullpath, ".nib");

nibOpenVerify(fullpath, &f, &nibSize);
for (i=0; i < nibSize; i = endBuf)
    {
    endBuf = i + bufSize;
    if (endBuf >= nibSize) endBuf = nibSize;
    basesInBuf = endBuf - i;
    seq = nibLdPart(fullpath, f, nibSize, i, basesInBuf);
    // printf("%s\n", seq->dna);
    posPtr = seq->dna;
    pos = 0;
    while (*posPtr != '\0') {
      if (*posPtr == 'n') {
        if (!matchRegion) {
          startpos = (block * bufSize) + pos + 1;
          matchRegion = TRUE;
        }
      } else {
        if (matchRegion) {
          blockcount++;
          strcpy(blockname, "");
          sprintf(blockname, "N%d", blockcount);
          endpos = (block * bufSize) + pos;
          size = endpos - startpos;
          if (size >= 1000) 
          {
              printf("%s\t%d\t%d\t%s\n", chromname, startpos, endpos, blockname);
          }
          matchRegion = FALSE;
        }
      }
      pos++;
      posPtr++;
    }
        
    freeDnaSeq(&seq);
    block++;
    }
fclose(f);
}

void usage() 
{
    printf("nibCheck -- report N regions 100 bases or greater.\n"
           "usage:\n"
           "   nibCheck chromname\n");
}

int main(int argc, char *argv[])
{
  char chromname[512];
  if (argc < 2) {
    usage();
    return -1;
  }
  strcpy(chromname, "");
  strcat(chromname, argv[1]);
  findNRanges("/gbdb/panTro1/nib/", chromname);
  return 0;
}


/* qaGold - make quality score file for golden path in a contig. */
#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "linefile.h"
#include "dnaseq.h"
#include "fa.h"
#include "qaSeq.h"
#include "hCommon.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "qaGold - make quality score file for golden path in a contig\n"
  "usage:\n"
  "   qaGold contigDir ooVersion\n");
}

enum {
  qaBad = 0,
  qaFinished = 99,
  qaUnknown = 100,
  };

int qaOutPerLine = 20;
int qaOutCharIx = 0;

void outputQaVal(FILE *f, int qaVal)
/* Print out one qa value to file. */
{
if (qaVal >= 100)
   fprintf(f, " ? ");
else
   fprintf(f, "%2d ", qaVal);
if (++qaOutCharIx >= qaOutPerLine)
   {
   fprintf(f, "\n");
   qaOutCharIx = 0;
   }
if (ferror(f))
    {
    perror("Error writing qa file\n");
    errAbort("\n");
    }
}

void repeatQaOut(FILE *f, int qaVal, int count)
/* Output same value count times. */
{
int i;
for (i=0; i<count; ++i)
    outputQaVal(f, qaVal);
}

struct qaInfo
/* QaSeq and some extra info. */
   {
   struct qaInfo *next;         /* Next in list. */
   struct qaSeq *qa;            /* Actual quality scores. */
   bool isFin;			/* True if finished. */
   };

struct qaInfo *nextQaInfo(FILE *f, boolean isSwapped, boolean isFin)
/* Read next qa from file. */
{
struct qaInfo *qi;
struct qaSeq *qa;

if ((qa = qacReadNext(f, isSwapped)) == NULL)
    return NULL;
AllocVar(qi);
qi->qa = qa;
qi->isFin = isFin;
return qi;
}


void qaGold(char *contigDir, int ooVersion)
/* qaGold - make quality score file for golden path in a contig. */
{
char *faNameBuf, **faNames;
int faCount, i;
char dir[256], cloneName[128], extension[64];
char fileName[512];
char acc[64];
char *faName, *qacName;
struct hash *qaHash = newHash(0);
FILE *f = NULL;
struct lineFile *lf;
char *line, *words[16];
int lineSize, wordCount;
boolean isSwapped;
boolean isFin;
struct qaInfo *qiList = NULL, *qi;
struct qaSeq *qa;

/* Load in all qa's the correspond to fa's in geno.lst. */
sprintf(fileName, "%s/geno.lst", contigDir);
readAllWords(fileName, &faNames, &faCount, &faNameBuf);
if (faCount == 0)
    return;
for (i=0; i<faCount; ++i)
    {
    faName = faNames[i];
    splitPath(faName, dir, cloneName, extension);
    if (!startsWith("NT_", cloneName))
	{
	isFin = endsWith(dir, "fin/fa/");
	qacName = qacPathFromFaPath(faName);
	if (isFin && !fileExists(qacName))
	    {
	    struct lineFile *lf = lineFileOpen(faName, TRUE);
	    struct dnaSeq seq;
            ZeroVar(&seq);

	    while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
	        {
		AllocVar(qi);
		slAddHead(&qiList, qi);
		qi->qa = qaMakeConstant(seq.name, seq.size, qaFinished);
		qi->isFin = TRUE;
		hashAddUnique(qaHash, seq.name, qi);
		}
	    }
	else
	    {
	    f = qacOpenVerify(qacName, &isSwapped);
	    while ((qi = nextQaInfo(f, isSwapped, isFin)) != NULL)
		{
		slAddHead(&qiList, qi);
		hashAddUnique(qaHash, qi->qa->name, qi);
		}
	    carefulClose(&f);
	    }
	}
    }
slReverse(&qiList);

/* Write qa file that corresponds to whole contig to
 * standard output. */
f = stdout;
sprintf(fileName, "%s/gold.%d.noNt", contigDir, ooVersion);
lf = lineFileOpen(fileName, TRUE);
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    int size, start, end;
    char strand;
    if (wordCount == 8 && sameWord(words[4], "N"))
       {
       size = lineFileNeedNum(lf, words, 5);
       repeatQaOut(f, qaBad, size);
       }
    else if (wordCount == 9)
       {
       boolean isFin = FALSE;
       if (lf->lineIx == 1)		/* Write header. */
           fprintf(f, ">%s\n", words[0]);
       if (startsWith("NT_", words[5]))
	   {
           isFin = TRUE;
	   qi = NULL;
	   qa = NULL;
	   }
       else
	   {
	   qi = hashMustFindVal(qaHash, words[5]);
	   qa = qi->qa;
	   isFin = qi->isFin;
	   }
       start = lineFileNeedNum(lf, words, 6) - 1;
       end = lineFileNeedNum(lf, words, 7);
       size = end-start;
       strand = words[8][0];
       if (isFin)
	   {
           repeatQaOut(f, 99, size);
	   }
       else if (qaIsGsFake(qa))
           repeatQaOut(f, 100, size);
       else
           {
	   if (strand == '+')
	       {
	       for (i=start; i<end; ++i)
	           {
		   outputQaVal(f, qa->qa[i]);
		   }
	       }
	   else
	       {
	       for (i=end-1; i>=start; --i)
	           {
		   outputQaVal(f, qa->qa[i]);
		   }
	       }
	   }
       }
    else
       {
       errAbort("Malformed line %d of %s\n", lf->lineIx, lf->fileName);
       }
    }
if (qaOutCharIx != 0)
   fprintf(f, "\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
if (!isdigit(argv[2][0]))
    usage();
qaGold(argv[1], atoi(argv[2]));
return 0;
}

/* elandToSplat - Convert eland output to splat output.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: elandToSplat.c,v 1.2 2008/11/05 05:50:05 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "elandToSplat - Convert eland output to splat output.\n"
  "usage:\n"
  "   elandToSplat in.vmf out.splat\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void outputMappings(char *readName, char *seq, char *mappings, struct lineFile *lf, FILE *f)
/* Parse out mappings, and output a splat line for each one. */
/* Mappings look like: 
   18_21_22.fa/chr18:17714750R2,18_21_22.fa/chr21:10039036F2,17066364R1,39312480R2,18_21_22.fa/chr22:20454541R1
   or just
   chr22.fa:10039036F2,17066364R1 */
{
if (readName[0] == '>')
    readName += 1;
char *chrom = NULL;
char *s, *e;
int seqLen = strlen(seq);
int commaCount = countChars(mappings, ',');
for (s = mappings; s != NULL && s[0] != 0; s = e)
    {
    e = strchr(s, ',');
    if (e != NULL)
      *e++ = 0;
    char *colon = strchr(s, ':');
    if (colon != NULL)
       {
       chrom = s;
       char *slash = strchr(s, '/');
       *colon = 0;
       if (slash != NULL)
	   chrom = slash+1;
       else
	   chopSuffix(chrom);
       s = colon + 1;
       }
    char strand = '+';
    char *endNum = NULL;
    if ((endNum = strchr(s, 'F')) != NULL)
       *endNum = 0;
    else if ((endNum = strchr(s, 'R')) != NULL)
       {
       *endNum = 0;
       strand = '-';
       }
    else
       errAbort("Expecting a 'R' or 'F' after number line %d of %s\n", lf->lineIx, lf->fileName);
    if (chrom == NULL)
       errAbort("Expecting chromosome line %d of %s\n", lf->lineIx, lf->fileName);
    int start = sqlUnsigned(s);
    fprintf(f, "%s\t%d\t%d\t%s\t%d\t%c\t%s\n", 
    	chrom, start, start+seqLen, seq, 1000/(commaCount+1), strand, readName);
    }
}

void elandToSplat(char *vmfIn, char *splatOut)
/* elandToSplat - Convert eland output to splat output.. */
{
struct lineFile *lf = lineFileOpen(vmfIn, TRUE);
FILE *f = mustOpen(splatOut, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *words[5];
    int wordCount = chopLine(line, words);
    if (wordCount != 3 && wordCount != 4)
       errAbort("Expecting 3 or 4 columns line %d of %s, got %d", lf->lineIx, lf->fileName, wordCount);
    if (wordCount == 4)
        {
	char *readName = words[0];
	char *seq = words[1];
	char *mappings = words[2];
	outputMappings(words[0], words[1], words[3], lf, f);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
elandToSplat(argv[1], argv[2]);
return 0;
}

/* fixEnsGtf - fix space vs. tab and start/stop codon problems in Ensemble .gtf file. */
#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "fixEnsGtf - fix space vs. tab and start/stop codon problems in Ensemble .gtf file\n"
  "usage:\n"
  "   fixEnsGtf out.gtf in.gtf(s)\n");
}

void fixLine(struct lineFile *lf, char *line, FILE *f)
/* Fix up a single line. */
{
char *group;            /* Last. */
char *words[8];		/* First words. */
int i;
char fixStart[16], fixEnd[16];
char *type, *strand;

/* Pass through comments. */
if (line[0] == '#')
    {
    fprintf(f, "%s\n", line);
    return;
    }

/* Find the start of the "group" field. */
group = line;
for (i=0; i<8; ++i)
    {
    group = skipToSpaces(group);
    if (group == NULL)
       errAbort("Expecting at least 9 fields line %d of %s\n", lf->lineIx, lf->fileName);
    group = skipLeadingSpaces(group);
    }

/* Truncate initial string before group field and chop it up. */
group[-1] = 0;
chopLine(line, words);

#ifdef FLAKY    /* This doesn't fix all problems, we'll just ignore start/stop_codons. */
/* Fix up start and stop codons. */
type = words[2];
strand = words[6];
if (sameString(type, "start_codon") && sameString(strand, "-"))
    {
    sprintf(fixStart, "%d", atoi(words[3])-3);
    sprintf(fixEnd, "%d", atoi(words[4])-3);
    words[3] = fixStart;
    words[4] = fixEnd;
    }
else if (sameString(type, "stop_codon"))
    {
    /* Start and end reversed on both strands. */
    int start = atoi(words[4]);
    int end = atoi(words[3]);
    if (sameString(strand, "-"))
        {
	start += 3;
	end += 3;
	}
    sprintf(fixStart, "%d", start);
    sprintf(fixEnd, "%d", end);
    words[3] = fixStart;
    words[4] = fixEnd;
    }
#endif /* FLAKY */

/* Skip start/stop codons.  Code will then assume all exons are CDS. */
type = words[2];
if (sameString(type, "start_codon") || sameString(type, "stop_codon"))
    return;

/* Write fixed output. */
for (i=0; i<8; ++i)
    fprintf(f, "%s\t", words[i]);
fprintf(f, "%s\n", group);
}

void fixEnsGtf(char *outName, int inCount, char *inFiles[])
/* fixEnsGtf - fix space vs. tab and start/stop codon problems in Ensemble .gtf file. */
{
FILE *f = mustOpen(outName, "w");
struct lineFile *lf;
int fileIx;
int lineSize;
char *line;

for (fileIx = 0; fileIx < inCount; ++fileIx)
    {
    lf = lineFileOpen(inFiles[fileIx], TRUE);
    printf("Processing %s\n", lf->fileName);
    while (lineFileNext(lf, &line, &lineSize))
        {
	fixLine(lf, line, f);
	}
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
fixEnsGtf(argv[1], argc-2, argv+2);
return 0;
}

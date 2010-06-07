/* formatFastaSeq - reformats sequence file removing numbering at beginning */
#include "common.h"
#include "linefile.h"
#include "hash.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "formatFastaSeq - reformats FASTA format sequence file removing numbering \n"
  "at beginning of lines and concatenating sequence into one line for each \n"
  "sequence record. \n"
  "usage:\n"
  "   formatFastSeq seqFile outFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

boolean isDna(char c)
{
return (strchr("atgcnATGCN", c)) != NULL;
}

void formatFastaSeq(char *inFile, char *outFile)
/* formatFastaSeq - reformat FASTA sequences in a file */
{
struct lineFile *seqFile;
FILE *formatSeqFile;
char *line, *words[9], *dnaString = NULL, *prev = NULL;
int lineSize, wordCount, seqCount = 0;
boolean foundNext = FALSE;

/* open FASTA sequence file */
seqFile = lineFileOpen(inFile, TRUE);
formatSeqFile = mustOpen(outFile, "w");

while (lineFileNext (seqFile, &line, &lineSize))
    {
    if (startsWith(">", line))
        {
        /* found new sequence */
        seqCount++;
        /* print out previously concatenated sequence */
        if (dnaString != NULL)
            {
            fprintf(formatSeqFile, "%s", dnaString);
            free(dnaString);
            }
        dnaString = NULL;
        /* print current FASTA header line */
        if (seqCount != 1)
            fprintf(formatSeqFile, "\n");
        fprintf(formatSeqFile, "%s\n", line);
        foundNext = TRUE;
        }
    else
       {
       wordCount = chopLine(line, words);

       if (wordCount > 0)
           {
           /* check if first char of this word is a digit */
           if (isdigit(*words[0]))
               { 
               foundNext = FALSE;
               /* check if second word is DNA sequence */
               if (isDna(*words[1]) && (!foundNext))
                   {
                   if (dnaString != NULL)
                       {
                       prev = cloneString(dnaString);
                       dnaString = addSuffix(prev, words[1]);
                       }
                   else 
                       {
                       dnaString = cloneString(words[1]);
                       }
                   }
               }
           else
               fprintf(formatSeqFile, "%s", line);
           }
       }
    }
/* print out final line */
if (dnaString != NULL)
    {
    fprintf(formatSeqFile, "%s", dnaString);
    free(dnaString);
    }
fprintf(formatSeqFile, "\n");
fprintf(stdout, "There are %d sequences in the FASTA file.\n", seqCount);
lineFileClose(&seqFile);
carefulClose(&formatSeqFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
formatFastaSeq(argv[1], argv[2]);
return 0;
}

/* 
checkAgpAndFa - take a .agp file and a .fa file and validate
that they are in synch.
*/

#include "common.h"
#include "options.h"
#include "linefile.h"
#include "hash.h"
#include "fa.h"
#include "twoBit.h"
#include "agpFrag.h"
#include "agpGap.h"


/* Command line option variables */
char *exclude=NULL;

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"exclude", OPTION_STRING},
    {NULL, 0}
};

/* Make the arguments global for convenience: */
char *agpFile = NULL;
char *faFile = NULL;

void usage()
/* 
Explain usage and exit. 
*/
{
    fflush(stdout);
    errAbort(
      "\ncheckAgpAndFa - takes a .agp file and .fa file and ensures that they are in synch\n"
      "usage:\n\n"
      "   checkAgpAndFa in.agp in.fa\n\n"
      "options:\n"
      "   -exclude=seq - Ignore seq (e.g. chrM for which we usually get\n"
      "                  sequence from GenBank but don't have AGP)\n"
      "in.fa can be a .2bit file.  If it is .fa then sequences must appear\n"
      "in the same order in .agp and .fa.\n"
      "\n");
}

boolean isExcluded(char *seqName)
/* Return true if seqName is -exclude'd. */
{
return exclude != NULL && sameString(seqName, exclude);
}

void getNextSeqFromFa(char *agpName, DNA **retDna, int *retSize, char **retName)
/* Get the next sequence in the FASTA file, if any.  If -exclude is given,
 * skip over that sequence.  Complain if sequence name is not what we expect 
 * from AGP, or if it appears that one input has ended before the other. */
{
static struct lineFile *lfFa = NULL;
boolean gotSeq = FALSE;

if (lfFa == NULL)
    lfFa = lineFileOpen(faFile, TRUE);

gotSeq = faSpeedReadNext(lfFa, retDna, retSize, retName);
if (gotSeq && isExcluded(*retName))
    gotSeq = faSpeedReadNext(lfFa, retDna, retSize, retName);
if (!gotSeq && agpName != NULL)
    {
    fflush(stdout);
    errAbort("Unexpected end of FASTA input %s when looking "
	     "for AGP sequence %s", faFile, agpName);
    }
if (gotSeq && agpName == NULL)
    {
    fflush(stdout);
    errAbort("FASTA file %s has sequence %s that is not present in "
	     "AGP file %s", faFile, *retName, agpFile);
    }
if (gotSeq && !sameString(agpName, *retName))
    {
    fflush(stdout);
    errAbort("Expecting to find sequence %s at line %d of %s, but got %s.  "
	     "When input is FASTA, sequences must appear in exactly the same "
	     "order as in AGP.",
	     agpName, lfFa->lineIx, faFile, *retName);
    }
}

void getNextSeqFromTwoBit(char *agpName, DNA **retDna, int *retSize,
			  char **retName)
/* Fetch agpName from twoBit.  Complain if it's not there.  If agpName is NULL
 * then AGP is done -- complain if any sequences not -exclude'd are in twoBit
 * but not in AGP. */
{
static struct twoBitFile *tbf = NULL;
static struct hash *seqNameHash = NULL;

if (tbf == NULL)
    {
    tbf = twoBitOpen(faFile);
    seqNameHash = hashNew(20);
    }

if (agpName != NULL)
    {
    struct dnaSeq *ds = twoBitReadSeqFragLower(tbf, agpName, 0, 0);
    /* That will errAbort if agpName is not in the 2bit. */ 
    *retDna = ds->dna;
    *retSize = ds->size;
    *retName = ds->name;
    hashAddUnique(seqNameHash, ds->name, NULL);
    /* Free the struct -- members will be freed below. */
    freeMem(ds);
    }
else
    {
    /* End of AGP has been reached.  See if any sequences (not -exclude'd) 
     * have been skipped over. */
    struct slName *seqNameList = twoBitSeqNames(faFile);
    struct slName *sn = NULL;
    for (sn = seqNameList;  sn != NULL;  sn = sn->next)
	{
	if (hashLookup(seqNameHash, sn->name) == NULL && !isExcluded(sn->name))
	    {
	    fflush(stdout);
	    errAbort("twoBit file %s has sequence %s that is not present in "
		     "AGP file %s", faFile, sn->name, agpFile);
	    }
	}
    slNameFreeList(&seqNameList);
    }
}

void getNextSeq(char *agpName, DNA **retDna, int *retSize, char **retName)
/* Get next sequence named in AGP, or if agpName is NULL (end of AGP), 
 * make sure there are no extra sequences missing from AGP.  
 * Complain if there's an inconsistency. */
{
if (twoBitIsFile(faFile))
    getNextSeqFromTwoBit(agpName, retDna, retSize, retName);
else
    getNextSeqFromFa(agpName, retDna, retSize, retName);
}

boolean containsOnlyChar(char *string, int offset, int strLength, char searchChar)
/* 
Ensure that the searched string only contains a certain set char, like 'n', for example.

param string - the string to examine
param offset - the starting offset at which to begin examination
param strLength - the number of chars to examine up from the offset
param searchChar - the char to search for in the string

returns true if this string contains only the searchChar
returns false if any other char is in this string 
*/
{
int charIndex = 0;
int stringEnd = offset + strLength;

for (charIndex = offset; charIndex < stringEnd; charIndex++)
    {
    if (searchChar != string[charIndex])
	{
        printf("Bad char %c found at index %d\n", string[charIndex], charIndex);
	return FALSE;
	}
    }
printf("Contains %d Ns\n", strLength);
return TRUE;
}

boolean containsOnlyChars(char *string, int offset, int strLength, char *searchCharList, int numSearchChars)
/* 
Ensure that the searched string only contains a certain set of chars, like 'acgtACGT',
for example.

param string - the string to examine
param offset - the starting offset at which to begin examination
param strLength - the number of chars to examine up from the offset
param searchCharList - the set of chars to search for in the string
param numSearchChars - the size of the searchCharList containing the searched-for chars

returns true if this string contains only the searchChars
returns false if any other char is in this string 
*/
{
int charIndex = 0;
int currentChar = 0;
int numMismatches = 0;
int stringEnd =  offset + strLength;

for(charIndex = offset; charIndex < stringEnd; charIndex++)
    {
    numMismatches = 0;
    for (currentChar = 0; currentChar < numSearchChars; currentChar++)
        {
        if (searchCharList[currentChar] != string[charIndex])
 	    {
	    ++numMismatches;
	    }
        /* If there was not even a single match then return FALSE */
	if(numMismatches == numSearchChars)
	    {
	    printf("Bad char %c found at index %d\n", string[charIndex], charIndex);
	    return FALSE;
	    }
	}
    }

return TRUE;
}

boolean agpMatchesFaEntry(struct agpFrag *agp, int offset, char *dna, int seqSize, char *seqName)
/* Return TRUE if the agp and fasta entries agree, FALSE otherwise. */
{
int fragSize = (agp->chromEnd - agp->chromStart);
boolean result = FALSE;

if (agp->chromEnd > seqSize)
    {
    printf("agp chromEnd (%d) > seqSize (%d)\n", agp->chromEnd, seqSize);
    return FALSE;
    }

if (sameString(agp->chrom, seqName))
    {
    if (agp->type[0] == 'N' || agp->type[0] == 'U')
        {
        printf("FASTA gap entry\n");
        result =  containsOnlyChar(dna, offset, fragSize, 'n');
        }
    else 
	{
        printf("FASTA sequence entry\n");
	/* Sometimes there are ambiguous characters so can't do this: */
	/* containsOnlyChars(dna, offset, fragSize, "acgt", 4); */
	result = TRUE;
	}
    }

return result;
}

struct agpFrag *agpFragOrGapLoadCheck(struct lineFile *lfAgp, char *words[],
				      int wordCount)
/* Read in and check the next line of AGP.  It may be a gap or a fragment 
 * line.  If it is a gap, cast it to a fragment and return (only coords will
 * be used by caller). */
{
struct agpFrag *agpFrag = NULL;
if (words[4][0] != 'N' && words[4][0] != 'U')
    {
    lineFileExpectWords(lfAgp, 9, wordCount);
    agpFrag = agpFragLoad(words);
    /* file is 1-based but agpFragLoad() now assumes 0-based: */
    agpFrag->chromStart -= 1;
    agpFrag->fragStart  -= 1;
    agpFragOutput(agpFrag, stdout, ' ', '\n');
    if (agpFrag->chromEnd - agpFrag->chromStart != 
	agpFrag->fragEnd - agpFrag->fragStart)
	{
	fflush(stdout);
	errAbort("1Sizes don't match in %s and %s line %d of %s\n",
		 agpFrag->chrom, agpFrag->frag, lfAgp->lineIx,
		 lfAgp->fileName);
	}
    if (agpFrag->chromEnd - agpFrag->chromStart <= 0)
	{
	fflush(stdout);
	errAbort("Size is %d in %s and %s line %d of %s\n",
		 agpFrag->chromEnd - agpFrag->chromStart,
		 agpFrag->chrom, agpFrag->frag, lfAgp->lineIx,
		 lfAgp->fileName);
	}
    }
else
    {
    struct agpGap *agpGap = agpGapLoad(words);
    int gapSize = -1;
    agpGap->chromStart--;
    gapSize = (agpGap->chromEnd - agpGap->chromStart);

    if (gapSize != agpGap->size)
	{
	fflush(stdout);
	errAbort("2Sizes don't match in %s, calculated size %d, size %d, "
		 "line %d of %s\n",
		 agpGap->chrom, gapSize, agpGap->size,
		 lfAgp->lineIx, lfAgp->fileName);
	}   
    agpFrag = (struct agpFrag*) agpGap;
    }
return agpFrag;
}

void checkAgpAndFa()
/* Read the .agp file and make sure that it agrees with the .fa file. */
{
struct lineFile *lfAgp = lineFileOpen(agpFile, TRUE);
char *line = NULL;
char *words[9];
int wordCount = 0;
char *agpName = NULL, *prevAgpSeqName = NULL;
struct agpFrag *agpFrag = NULL;
int dnaOffset = -1;
int seqSize = 0;
char *seqName = NULL;
DNA *dna = NULL;

while (lineFileNextReal(lfAgp, &line))
    {
    wordCount = chopLine(line, words);
    if (wordCount < 5)
	{
	fflush(stdout);
	errAbort("Bad line %d of %s\n", lfAgp->lineIx, lfAgp->fileName);
	}
    agpName = words[0];
    if (prevAgpSeqName == NULL || ! sameString(prevAgpSeqName, agpName))
	{
	if (prevAgpSeqName != NULL && dnaOffset != seqSize)
	    {
	    fflush(stdout);
	    errAbort("AGP for %s ends at %d but sequence size is %d",
		     prevAgpSeqName, dnaOffset, seqSize);
	    }
	getNextSeq(agpName, &dna, &seqSize, &seqName);
	dnaOffset = 0;
	printf("\n\nAnalyzing data for sequence: %s, size: %d, "
	       "dnaOffset = %d\n\n",
	       seqName, seqSize, dnaOffset);
	freez(&prevAgpSeqName);
	prevAgpSeqName = cloneString(agpName);
	}

    printf("\nLoop: %s, dnaOffset=%d, seqSize=%d\n",
	   agpName, dnaOffset, seqSize);
    agpFrag = agpFragOrGapLoadCheck(lfAgp, words, wordCount);
    if (dnaOffset != 0 && agpFrag->chromStart != dnaOffset)
	{
	fflush(stdout);
	errAbort("%s start %d doesn't match previous end %d, line %d of %s",
		 agpFrag->chrom, agpFrag->chromStart, dnaOffset, lfAgp->lineIx,
		 lfAgp->fileName);
	}
   
    /* If the agp entry is ignoring gaps at the start of the sequence 
       then we need to fake an agp entry and check the fake entry first 
       against the fasta entry */
    if (0 == dnaOffset && 0 != agpFrag->chromStart)
	{
	int origStart = agpFrag->chromStart;
	int origEnd = agpFrag->chromEnd;
	char origType[2];
	agpFrag->chromStart = 0;
	agpFrag->chromEnd = origStart;	
	strcpy(origType, agpFrag->type);
	strcpy(agpFrag->type, "N");
 
	if (!agpMatchesFaEntry(agpFrag, dnaOffset, dna, seqSize, seqName))
	    {
	    fflush(stdout);
	    errAbort("Invalid Fasta file entry\n");
	    }

	agpFrag->chromStart = origStart;
	agpFrag->chromEnd = origEnd;
	strcpy(agpFrag->type, origType);
	}

    dnaOffset = agpFrag->chromStart;                

    printf("agpFrag->chromStart: %d, agpFrag->chromEnd: %d, dnaOffset: %d\n",
	   agpFrag->chromStart, agpFrag->chromEnd, dnaOffset);
    if (!agpMatchesFaEntry(agpFrag, dnaOffset, dna, seqSize, seqName))
	{
	printf("Invalid Agp or Fasta file entry for sequence %s\n",
	       agpFrag->chrom);
	fflush(stdout);
	errAbort("agpMatchesFaEntry failed; exiting\n");
	}
    else 
	{
	printf("Valid Fasta file entry\n");
	}

    dnaOffset = agpFrag->chromEnd;
    }  

if (prevAgpSeqName == NULL)
    {
    fflush(stdout);
    errAbort("Empty AGP input");
    }
if (dnaOffset != seqSize)
    {
    fflush(stdout);
    errAbort("AGP for %s ends at %d but sequence size is %d",
	     prevAgpSeqName, dnaOffset, seqSize);
    }

/* Check for extra FASTA sequences not described by AGP: */
getNextSeq(NULL, &dna, &seqSize, &seqName);

/* Magic phrase to look for at the end of stdout: */
printf("All AGP and FASTA entries agree - both files are valid\n");
}

int main(int argc, char *argv[])
/* 
Process command line then delegate  main work to checkAgpAndFa().
*/
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
exclude = optionVal("exclude", exclude);
agpFile = argv[1];
faFile = argv[2];
checkAgpAndFa();
return 0;
}

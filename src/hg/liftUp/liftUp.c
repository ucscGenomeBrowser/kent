/* liftUp - change coordinates of .psl, .agp, or .out file
 * to parent coordinate system. */
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "hash.h"
#include "psl.h"
#include "genePred.h"
#include "genePredReader.h"
#include "xAli.h"
#include "rmskOut.h"
#include "chromInserts.h"
#include "axt.h"
#include "chain.h"
#include "chainNet.h"
#include "liftUp.h"
#include "options.h"
#include "verbose.h"
#include "xa.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: liftUp.c,v 1.47 2008/09/07 18:12:46 braney Exp $";

boolean isPtoG = TRUE;  /* is protein to genome lift */
boolean nohead = FALSE;	/* No header for psl files? */
boolean nosort = FALSE;	/* Don't sort files */
boolean ignoreVersions = FALSE;  /* drop NCBI versions */
boolean extGenePred = FALSE;  /* load extended genePred */
int dots=0;	/* Put out I'm alive dot now and then? */
int gapsize = 0;        /* optional non-default gap size */

void usage()
/* Explain usage and exit. */
{
errAbort(
 "liftUp - change coordinates of .psl, .agp, .gap, .gl, .out, .gff, .gtf .bscore \n"
 ".tab .gdup .axt .chain .net, genePred, .wab, .bed, or .bed8 files to parent\n"
 "coordinate system.\n"
 "\n"
 "usage:\n"
 "   liftUp [-type=.xxx] destFile liftSpec how sourceFile(s)\n"
 "The optional -type parameter tells what type of files to lift\n"
 "If omitted the type is inferred from the suffix of destFile\n"
 "Type is one of the suffixes described above.\n"
 "DestFile will contain the merged and lifted source files,\n"
 "with the coordinates translated as per liftSpec.  LiftSpec\n"
 "is tab-delimited with each line of the form:\n"
 "   offset oldName oldSize newName newSize\n"
 "LiftSpec may optionally have a sixth column specifying + or - strand,\n"
 "but strand is not supported for all input types.\n"
 "The 'how' parameter controls what the program will do with\n"
 "items which are not in the liftSpec.  It must be one of:\n"
 "   carry - Items not in liftSpec are carried to dest without translation\n"
 "   drop  - Items not in liftSpec are silently dropped from dest\n"
 "   warn  - Items not in liftSpec are dropped.  A warning is issued\n"
 "   error - Items not in liftSpec generate an error\n"
 "If the destination is a .agp file then a 'large inserts' file\n"
 "also needs to be included in the command line:\n"
 "   liftUp dest.agp liftSpec how inserts sourceFile(s)\n"
 "This file describes where large inserts due to heterochromitin\n"
 "should be added. Use /dev/null and set -gapsize if there's not inserts file.\n"
 "\n"
 "options:\n"
 "   -nohead  No header written for .psl files\n"
 "   -dots=N Output a dot every N lines processed\n"
 "   -pslQ  Lift query (rather than target) side of psl\n"
 "   -axtQ  Lift query (rather than target) side of axt\n"
 "   -chainQ  Lift query (rather than target) side of chain\n"
 "   -netQ  Lift query (rather than target) side of net\n"
 "   -wabaQ  Lift query (rather than target) side of waba alignment\n"
 "   	(waba lifts only work with query side at this time)\n"
 "   -nosort Don't sort bed, gff, or gdup files, to save memory\n"
 "   -gapsize change contig gapsize from default\n"
 "   -ignoreVersions - Ignore NCBI-style version number in sequence ids of input files\n"
 "   -extGenePred lift extended genePred\n"
 );
}

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"nohead", OPTION_BOOLEAN},
    {"nosort", OPTION_BOOLEAN},
    {"isPtoG", OPTION_BOOLEAN},
    {"dots", OPTION_INT},
    {"gapsize", OPTION_INT},
    {"type", OPTION_STRING},
    {"pslQ", OPTION_BOOLEAN},
    {"pslq", OPTION_BOOLEAN},
    {"axtQ", OPTION_BOOLEAN},
    {"axtq", OPTION_BOOLEAN},
    {"chainQ", OPTION_BOOLEAN},
    {"chainq", OPTION_BOOLEAN},
    {"netQ", OPTION_BOOLEAN},
    {"netq", OPTION_BOOLEAN},
    {"wabaQ", OPTION_BOOLEAN},
    {"wabaq", OPTION_BOOLEAN},
    {"ignoreVersions", OPTION_BOOLEAN},
    {"extGenePred", OPTION_BOOLEAN},
    {NULL, 0}
};

enum how
/* how to handle items missing from liftSpec */
{
    warnMissing,  /* warn if items are missing */
    silentDrop,   /* silently drop items. */
    carryMissing, /* carry missing items untranslated. */
    errorMissing  /* error if items are missing */
};
enum how how = warnMissing;


char *rmChromPrefix(char *s)
/* Remove chromosome prefix if any. */
{
char *e = strchr(s, '/');
if (e != NULL)
    return e+1;
else
    return s;
}

void rmChromPart(struct liftSpec *list)
/* Turn chrom/ctg to just ctg . */
{
struct liftSpec *el;
for (el = list; el != NULL; el = el->next)
    {
    el->oldName = rmChromPrefix(el->oldName);
    }
}

struct liftSpec *findLift(struct hash *liftHash, char *oldName, struct lineFile *lf)
/* Find lift or  return NULL.  lf parameter is just for error reporting. */
{
static int warnLeft = 10;	/* Only issue 10 warnings. */
char *verDot = ignoreVersions ? strrchr(oldName, '.') : NULL;
if (verDot != NULL)
    *verDot = '\0';
struct hashEl *hel = hashLookup(liftHash, oldName);
if (verDot != NULL)
    *verDot = '.';
if (hel == NULL)
    {
    if (how == warnMissing)
	{
	if (warnLeft > 0)
	    {
	    --warnLeft;
	    if (lf != NULL)
		warn("%s isn't in liftSpec file line %d of %s", oldName, lf->lineIx, lf->fileName);
	    else
		warn("%s isn't in liftSpec", oldName);
	    }
	}
    else if (how == errorMissing)
        {
        if (lf != NULL)
            errAbort("%s isn't in liftSpec file line %d of %s", oldName, lf->lineIx, lf->fileName);
        else
            errAbort("%s isn't in liftSpec", oldName);
        }
    return NULL;
    }
return hel->val;
}


void skipLines(struct lineFile *lf, int count)
/* Skip a couple of lines. */
{
int i, lineSize;
char *line;

for (i=0; i<count; ++i)
    lineFileNext(lf, &line, &lineSize);
}

int numField(char **words, int field, int skip, struct lineFile *lf)
/* Read field from words as number.  lf parameter just for error reporting. */
{
char *s = words[field] + skip;
bool sign = FALSE;
int val;

if (s[0] == '-')
    {
    sign = TRUE;
    ++s;
    }
if (!isdigit(s[0]))
    errAbort("Expecting number field %d line %d of %s", field+1, lf->lineIx, lf->fileName);
val = atoi(s);
if (sign)
    val = -val;
return val;
}

char flipStrand(char strand)
/* Turn + to - and vice versa. */
{
return (strand == '-' ? '+' : '-');
}

void cantHandleSpecRevStrand(struct liftSpec *spec)
/* abort if spec has a minus strand and we don't (yet) support that. */
{
if (spec && spec->strand == '-')
    errAbort("Can't handle lifts with - strands for this input type");
}

void liftOut(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift up coordinates in .out file.  Add offset to id (15th) column to 
 * maintain non-overlapping id ranges for different input files. */
{
FILE *dest = mustOpen(destFile, "w");
char *source;
int i;
struct lineFile *lf;
int lineSize, wordCount;
char *line, *words[32];
char *s;
int begin, end, left;
char leftString[18];
int highestIdSoFar = 0, idOffset = 0;
char idStr[32];
struct liftSpec *spec;
char *newName;

rmskOutWriteHead(dest);
for (i=0; i<sourceCount; ++i)
    {
    source = sources[i];
    verbose(1, "Lifting %s\n", source);
    if (!fileExists(source))
	{
	warn("%s does not exist\n", source);
	continue;
	}
    lf = lineFileOpen(source, TRUE);
    if (!lineFileNext(lf, &line, &lineSize))
	{
	warn("%s is empty\n", source);
	lineFileClose(&lf);
	continue;
	}
    if (startsWith("There were no", line))
	{
	lineFileClose(&lf);
	continue;
	}
    skipLines(lf, 2);
    while (lineFileNext(lf, &line, &lineSize))
	{
	wordCount = chopLine(line, words);
	// 16 becomes 17, new field in RMasker June23 '03 - Hiram
	if (wordCount < 14 || wordCount > 17)
	    errAbort("Expecting 14-17 words (found %d) line %d of %s", wordCount, lf->lineIx, lf->fileName);
	if (wordCount >= 15)
	    {
	    if (words[14][0] == '*')
	    	{
		warn("Warning: 15th column has * (should be a numeric ID).\n");
	    	idStr[0] = '\0';
		}
	    else
	    	{
	    	int numId = sqlUnsigned(words[14]) + idOffset;
	    	if (numId > highestIdSoFar)
		    highestIdSoFar = numId;
	    	safef(idStr, sizeof(idStr), "%d", numId);
		}
	    }
	else
	    idStr[0] = '\0';
	begin = numField(words, 5, 0, lf);
	end = numField(words, 6, 0, lf);
	s = words[7];
	if (s[0] != '(')
	    errAbort("Expecting parenthesized field 8 line %d of %s", lf->lineIx, lf->fileName);
	left = numField(words, 7, 1, lf);
	spec = findLift(liftHash, words[4], lf);
	if (spec == NULL) 
	    {
	    if (how == carryMissing)
	        newName = words[4];
	    else
		continue;
	    }
	else
	    {
	    cantHandleSpecRevStrand(spec);
	    begin += spec->offset;
	    end += spec->offset;
	    left = spec->newSize - end;
	    newName = spec->newName;
	    }
	sprintf(leftString, "(%d)", left);
	fprintf(dest, 
	  "%5s %5s %4s %4s  %-9s %7d %7d %9s %1s  %-14s %-19s %6s %4s %6s %6s\n",
	  words[0], words[1], words[2], words[3], newName,
	  begin, end, leftString,
	  words[8], words[9], words[10], words[11], words[12], words[13], idStr);
	}
	lineFileClose(&lf);
	idOffset = highestIdSoFar;
    }
if (ferror(dest))
    errAbort("error writing %s", destFile);
fclose(dest);
}

void freePslOrXa(struct psl *psl, boolean isXa)
/* Free a psl that may be extended. */
{
if (isXa)
    {
    struct xAli *xa = (struct xAli *)psl;
    xAliFree(&xa);
    }
else
    pslFree(&psl);
}

void doDots(int *pDotMod)
/* Output a dot every now and then. */
{
if (dots > 0)
    {
    if (--*pDotMod <= 0)
	{
        verboseDot();
	*pDotMod = dots;
	}
    }
}

void liftPsl(char *destFile, struct hash *liftHash, int sourceCount, char *sources[],
	boolean querySide, boolean isExtended)
/* Lift up coordinates in .psl file. */
{
FILE *dest = mustOpen(destFile, "w");
char *source;
int i,j;
struct lineFile *lf;
struct psl *psl;
struct xAli *xa = NULL;
unsigned *starts;
unsigned *blockSizes;
struct liftSpec *spec;
int offset;
int blockCount;
char *seqName;
int dotMod = dots;
int seqSize;
int strandChar = (querySide ? 0 : 1);

if (!nohead)
    pslWriteHead(dest);
for (i=0; i<sourceCount; ++i)
    {
    source = sources[i];
    if (!fileExists(source))
	{
	warn("%s doesn't exist!", source);
	continue;
	}
    verbose(1, "Lifting %s\n", source);
    lf = pslFileOpenWithMeta(source, dest);
    for (;;)
        {
	if (isExtended)
	    {
	    xa = xAliNext(lf);
	    psl = (struct psl *)xa;
	    }
	else
	    psl = pslNext(lf);
	if (psl == NULL)
	    break;
	boolean isProt = pslIsProtein(psl);

	doDots(&dotMod);
	if (querySide)
	    seqName = psl->qName;
	else
	    seqName = psl->tName;
	spec = findLift(liftHash, seqName, lf);
	if (spec == NULL)
	    {
	    if (how != carryMissing)
	        {
		freePslOrXa(psl, isExtended);
		continue;
		}
	    }
	else
	    {
	    offset = spec->offset;
	    blockSizes = psl->blockSizes;
	    if (querySide)
	        {
		if (!isPtoG)
		    {
		    cantHandleSpecRevStrand(spec);
		    psl->qStart += offset;
		    psl->qEnd += offset;
		    }
		else
		    {
		    psl->match *= 3;
		    psl->misMatch *= 3;
		    if (spec->strand == '-')
			{
			int tmp = psl->qEnd;
			psl->qEnd = psl->qStart;
			psl->qStart = tmp;
			psl->qStart *= -3;
			psl->qEnd *= -3;
			psl->qStart += offset;
			psl->qEnd += offset;
			}
		    else if (spec->strand == '+')
			{
			psl->qStart *= 3;
			psl->qStart += offset;
			psl->qEnd *= 3;
			psl->qEnd += offset;
			}
		    }
		starts = psl->qStarts;
		seqSize = psl->qSize;
		}
	    else
	        {
		if (spec->strand == '-')
		    reverseIntRange(&psl->tStart, &psl->tEnd, psl->tSize);
		psl->tStart += offset;
		psl->tEnd += offset;
		starts = psl->tStarts;
		seqSize = psl->tSize;
		}
	    blockCount = psl->blockCount;
	    if (isPtoG && (spec->strand == '-'))
	        {
		psl->strand[strandChar] = spec->strand;
		for (j=0; j<blockCount; ++j)
		    {
		    starts[j] *= -3;
		    starts[j] += offset;
		    starts[j] = spec->newSize - starts[j];
		    }
		}
	    else if (isPtoG && (spec->strand == '+'))
	        {
		psl->strand[strandChar] = spec->strand;
		for (j=0; j<blockCount; ++j)
		    {
		    starts[j] *= 3;
		    starts[j] += offset;
		    }
		}
	    else /* mRNA case. */
		{
		if (spec->strand == '+')
		    {
		    if (psl->strand[strandChar] == '-')
			{
			for (j=0; j<blockCount; ++j)
			    {
			    int tr = seqSize - starts[j];
			    tr += offset;
			    starts[j] = spec->newSize - tr;
			    }
			}
		    else
			{
			for (j=0; j<blockCount; ++j)
			    starts[j] += offset;
			}
		    }
		else
		    {
		    if (isProt)
			{
			/* if it's protein, we can't reverse the query */
			if (psl->strand[strandChar] == '-')
			    {
			    for (j=0; j<blockCount; ++j)
				starts[j] += offset;
			    }
			else
			    {
			    for (j=0; j<blockCount; ++j)
				{
				int tr = seqSize - starts[j];
				tr += offset;
				starts[j] = spec->newSize - tr;
				}
			    }
			psl->strand[strandChar] = 
			    flipStrand(psl->strand[strandChar]);
			}
		    else
			{
			if (psl->strand[strandChar] == '-')
			     errAbort("Can't handle all these minus strands! line %d",lf->lineIx);
			else
			    {
			    for (j=0; j<blockCount; ++j)
				{
				psl->tStarts[j] = psl->tSize - 
				    (psl->tStarts[j] + blockSizes[j]) + offset;
				psl->qStarts[j] = psl->qSize - 
				    (psl->qStarts[j] + blockSizes[j]);	/* no offset. */
				}
			    psl->strand[1-strandChar] = 
				flipStrand(psl->strand[1-strandChar]);
			    reverseUnsigned(blockSizes, blockCount);
			    reverseUnsigned(psl->qStarts, blockCount);
			    reverseUnsigned(psl->tStarts, blockCount);
			    }
			}
		    }
		}

	    if (isPtoG)
		for (j=0; j<blockCount; ++j)
		    blockSizes[j] *= 3;
	    if (querySide)
	        {
		psl->qSize = spec->newSize;
		psl->qName = spec->newName;
		}
	    else
	        {
		psl->tSize = spec->newSize;
		psl->tName = spec->newName;
		}
	    }
	if (isExtended)
	    {
	    xAliTabOut(xa, dest);
	    }
	else
	    {
	    pslTabOut(psl, dest);
	    }
	if (querySide)
	    psl->qName = seqName;
	else
	    psl->tName = seqName;
	freePslOrXa(psl, isExtended);
	}
    lineFileClose(&lf);
    if (dots)
        verbose(1, "\n");
    }
if (ferror(dest))
    errAbort("error writing %s", destFile);
fclose(dest);
}

void liftAxt(char *destFile, struct hash *liftHash, 
	int sourceCount, char *sources[], boolean querySide)
/* Lift up coordinates in .axt file. */
{
FILE *f = mustOpen(destFile, "w");
int sourceIx;
int dotMod = dots;

for (sourceIx = 0; sourceIx < sourceCount; ++sourceIx)
    {
    char *source = sources[sourceIx];
    struct lineFile *lf = lineFileOpen(source, TRUE);
    struct axt *axt;
    lineFileSetMetaDataOutput(lf, f);
    verbose(1, "Lifting %s\n", source);
    while ((axt = axtRead(lf)) != NULL)
        {
	struct liftSpec *spec;
	struct axt a = *axt;
	char *seqName;
	if (querySide)
	    seqName = a.qName;
	else
	    seqName = a.tName;
	spec = findLift(liftHash, seqName, lf);
	if (spec == NULL)
	    {
	    if (how != carryMissing)
	        {
		axtFree(&axt);
		continue;
		}
	    }
	else
	    {
	    int offset;
	    char strand = (querySide ? a.qStrand : a.tStrand);
	    cantHandleSpecRevStrand(spec);
	    if (strand == '-')
		{
		int ctgEnd = spec->offset + spec->oldSize;
		offset = spec->newSize - ctgEnd;
		}
	    else
		offset = spec->offset;
	    if (querySide)
	        {
		a.qStart += offset;
		a.qEnd += offset;
		a.qName = spec->newName;
		}
	    else
	        {
		a.tStart += offset;
		a.tEnd += offset;
		a.tName = spec->newName;
		if (strand == '-')
                    warn("Target minus strand, please double check results.");
                }
            }
        axtWrite(&a, f);
        axtFree(&axt);
        doDots(&dotMod);
        }
    lineFileClose(&lf);
    if (dots)
        verbose(1, "\n");
    }
}

void liftChain(char *destFile, struct hash *liftHash, 
        int sourceCount, char *sources[], boolean querySide)
/* Lift up coordinates in .chain file. */
{
FILE *f = mustOpen(destFile, "w");
int sourceIx;
int dotMod = dots;

for (sourceIx = 0; sourceIx < sourceCount; ++sourceIx)
    {
    char *source = sources[sourceIx];
    struct lineFile *lf = lineFileOpen(source, TRUE);
    struct chain *chain;
    lineFileSetMetaDataOutput(lf, f);
    verbose(1, "Lifting %s\n", source);
    while ((chain = chainRead(lf)) != NULL)
	{
	struct liftSpec *spec;
	char *seqName = querySide ? chain->qName : chain->tName;
	spec = findLift(liftHash, seqName, lf);
	if (spec == NULL)
	    {
	    if (how != carryMissing)
		{
		chainFree(&chain);
		continue;
		}
	    }
	else
	    {
	    struct cBlock *b = NULL;
	    int offset = spec->offset;
	    if (spec->strand == '-')
		{
		if (querySide)
		    {
		    int qSpan = chain->qEnd - chain->qStart;
		    if (chain->qStrand == '-')
		        chain->qStart += spec->offset;
		    else
		        {
			chain->qStart = spec->newSize - spec->offset 
				- (chain->qSize - chain->qStart);
			}
		    chain->qEnd = chain->qStart + qSpan;
		    chain->qStrand = flipStrand(chain->qStrand);
		    freeMem(chain->qName);
		    chain->qName = cloneString(spec->newName);
		    chain->qSize = spec->newSize;
		    /* We don't need to mess with the blocks here
		     * since they are all relative to the start. */
	            }
		else
		    {
		    /* We try and keep the target strand positive, so we end up
		     * flipping in both target and query and flipping the target
		     * strand. */
		    reverseIntRange(&chain->qStart, &chain->qEnd, chain->qSize);
		    reverseIntRange(&chain->tStart, &chain->tEnd, chain->tSize);
		    chain->qStrand = flipStrand(chain->qStrand);

		    /* Flip around blocks and add offset. */
		    for (b=chain->blockList;  b != NULL;  b=b->next)
			{
			reverseIntRange(&b->qStart, &b->qEnd, chain->qSize);
			reverseIntRange(&b->tStart, &b->tEnd, chain->tSize);
			b->tStart += offset;
			b->tEnd   += offset;
			}
		    slReverse(&chain->blockList);

		    /* On target side add offset as well and update name and size. */
		    chain->tStart += offset;
		    chain->tEnd   += offset;
		    freeMem(chain->tName);
		    chain->tName = cloneString(spec->newName);
		    chain->tSize = spec->newSize;
		    }
		}
	    else
		{
		if (querySide)
		    {
		    if (chain->qStrand == '-')
			offset = spec->newSize - (spec->offset + spec->oldSize);
		    freeMem(chain->qName);
		    chain->qName = cloneString(spec->newName);
		    chain->qSize = spec->newSize;
		    chain->qStart += offset;
		    chain->qEnd   += offset;
		    for (b=chain->blockList;  b != NULL;  b=b->next)
			{
			b->qStart += offset;
			b->qEnd   += offset;
			}
		    }
		else
		    {
		    freeMem(chain->tName);
		    chain->tName = cloneString(spec->newName);
		    chain->tSize = spec->newSize;
		    chain->tStart += offset;
		    chain->tEnd   += offset;
		    for (b=chain->blockList;  b != NULL;  b=b->next)
			{
			b->tStart += offset;
			b->tEnd   += offset;
			}
		    }
		}
	    }
	chainWrite(chain, f);
	chainFree(&chain);
	doDots(&dotMod);
	}
    lineFileClose(&lf);
    if (dots)
        verbose(1, "\n");
    }
}

void liftFillsT(struct cnFill *fillList, struct liftSpec *spec)
    /* Lift target coords in each element of fillList and recursively descend to 
     * children of each element if necessary. */
{
    struct cnFill *fill;
    for (fill=fillList;  fill != NULL;  fill=fill->next)
        {
	cantHandleSpecRevStrand(spec);
        fill->tStart += spec->offset;
        if (fill->children != NULL)
            liftFillsT(fill->children, spec);
        }
}

void liftFillsQ(struct cnFill *fillList, struct hash *nameHash,
        struct hash *liftHash, struct lineFile *lf)
/* Lift query coords in each element of fillList and recursively descend to 
 * children of each element if necessary.  lf is only for err reporting. */
{
    struct cnFill *fill;
    for (fill=fillList;  fill != NULL;  fill=fill->next)
        {
        struct liftSpec *spec = findLift(liftHash, fill->qName, lf);
        if (spec == NULL)
            {
            if (how != carryMissing)
                {
                errAbort("Can't lift fill->qName %s and don't support dropping lifts",
                        fill->qName);
                }
            }
        else
            {
	    cantHandleSpecRevStrand(spec);
            fill->qName = spec->newName;
            fill->qStart += spec->offset;
            }
        // nameHash needs to be completely rebuilt with all strings it contained 
        // before (see chainNet.c::cnFillFromLine), regardless of whether we're 
        // changing their values:
        fill->qName = hashStoreName(nameHash, fill->qName);
        fill->type  = hashStoreName(nameHash, fill->type);

        if (fill->children != NULL)
            liftFillsQ(fill->children, nameHash, liftHash, lf);
        }
}


void liftNet(char *destFile, struct hash *liftHash, 
        int sourceCount, char *sources[], boolean querySide)
/* Lift up coordinates in .net file. */
{

    FILE *f = mustOpen(destFile, "w");
    int sourceIx;
    int dotMod = dots;

    for (sourceIx = 0; sourceIx < sourceCount; ++sourceIx)
        {
        char *source = sources[sourceIx];
        struct lineFile *lf = lineFileOpen(source, TRUE);
        struct chainNet *net;
        lineFileSetMetaDataOutput(lf, f);
        verbose(1, "Lifting %s\n", source);
        while ((net = chainNetRead(lf)) != NULL)
            {
            if (querySide)
                {
                struct hash *newNameHash = hashNew(6);
                liftFillsQ(net->fillList, newNameHash, liftHash, lf);
                hashFree(&(net->nameHash));
                net->nameHash = newNameHash;
                }
            else
                {
                struct liftSpec *spec = findLift(liftHash, net->name, lf);
                if (spec == NULL)
                    {
                    if (how != carryMissing)
                        {
                        chainNetFree(&net);
                        continue;
                        }
                    }
                else
                    {
                    freeMem(net->name);
                    net->name = cloneString(spec->newName);
                    net->size = spec->newSize;
                    liftFillsT(net->fillList, spec);
                    }
                }
            chainNetWrite(net, f);
            chainNetFree(&net);
            doDots(&dotMod);
            }
        lineFileClose(&lf);
        if (dots)
            verbose(1, "\n");
        }
}


void liftWab(char *destFile, struct hash *liftHash, 
        int sourceCount, char *sources[], boolean querySide)
/* Lift up coordinates in .wab file. */
{
FILE *f = mustOpen(destFile, "w");
int sourceIx;

for (sourceIx = 0; sourceIx < sourceCount; ++sourceIx)
    {
    struct xaAli *xa;
    char *source = sources[sourceIx];
    FILE *in = mustOpen(source, "r");
    while ((xa = xaReadNext(in, FALSE)) != NULL)
	{
	char *seqName = querySide ? xa->query : xa->target;
	struct liftSpec *spec = findLift(liftHash, seqName, NULL);
	int offset;
	if (spec == NULL)
	    {
	    verbose(0,"name:\t%s\n", xa->name);
	    verbose(0,"query:\t%s\n", xa->query);
	    verbose(0,"qStart,qEnd:\t%d,%d\n", xa->qStart,xa->qEnd);
	    verbose(0,"qStrand:\t%c\n", xa->qStrand);
	    verbose(0,"target:\t%s\n", xa->target);
	    verbose(0,"tStart,tEnd:\t%d,%d\n", xa->tStart,xa->tEnd);
	    verbose(0,"tStrand:\t%c\n", xa->tStrand);
	    verbose(0,"milliScore:\t%d\n", xa->milliScore);
	    verbose(0,"symCount:\t%d\n", xa->symCount);
	    errAbort("Can not find lift spec for %s", seqName);
	    }
	if (querySide)
	    {
	    cantHandleSpecRevStrand(spec);
	    offset = spec->offset;
	    xa->qStart += offset;
	    xa->qEnd += offset;
	fprintf(f, "%s align %d.%d%% of %d %s.fa %s:%d-%d %c %s:%d-%d %c\n",
    xa->name, xa->milliScore/10, xa->milliScore%10, xa->symCount,
    spec->newName, spec->newName, xa->qStart, xa->qEnd, xa->qStrand,
    xa->target, xa->tStart, xa->tEnd, xa->tStrand);
	    }
	else
	    {
	    errAbort("Sorry, lift for WABA target not yet implemented");
	    }
	mustWrite(f, xa->qSym, xa->symCount);
	fputc('\n', f);
	mustWrite(f, xa->tSym, xa->symCount);
	fputc('\n', f);
	mustWrite(f, xa->hSym, xa->symCount);
	fputc('\n', f);

	}
    carefulClose(&in);
    }
}

void malformedAgp(struct lineFile *lf)
    /* Report error in .agp. */
{
    errAbort("Bad line %d in %s\n", lf->lineIx, lf->fileName);
}

void liftAgp(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
    /* Lift up coordinates in .agp file. */
{
    FILE *dest = mustOpen(destFile, "w");
    char *source;
    int i;
    struct lineFile *lf;
    int lineSize, wordCount;
    char *line, *words[32];
    char *s;
    struct liftSpec *spec;
    int start = 0;
    int end = 0;
    int ix = 0;
    char newDir[256], newName[128], newExt[64];
    struct bigInsert *bi;
    struct chromInserts *chromInserts;
    struct hash *insertHash = newHash(8);
    struct hash *contigsHash = newHash(10);
    boolean firstContig = TRUE;
    char lastContig[256];
    char *contig;
    int lastEnd = 0;

    if (sourceCount < 2)
        usage();

    if (how == carryMissing)
        warn("'carry' doesn't work for .agp files, ignoring");

    splitPath(destFile, newDir, newName, newExt);

    /* Read in inserts file and process it. */
    chromInsertsRead(sources[0], insertHash);
    chromInserts = hashFindVal(insertHash, newName);

    strcpy(lastContig, "");
    for (i=1; i<sourceCount; ++i)
        {
        source = sources[i];
        verbose(1, "Lifting %s\n", source);
        lf = lineFileMayOpen(source, TRUE);
        if (lf != NULL)
            {
            while (lineFileNext(lf, &line, &lineSize))
                {
                /* Check for comments and just pass them through. */
                s = skipLeadingSpaces(line);
                if (s[0] == '#')
                    {
                    fprintf(dest, "%s\n", line);
                    continue;
                    }
                /* Parse line, adjust offsets, write */
                wordCount = chopLine(line, words);
                if (wordCount != 8 && wordCount != 9)
                    malformedAgp(lf);
                contig = words[0];
                if (!sameString(contig, lastContig))
                    {
                    char *gapType = "contig";
                    char *ctg = rmChromPrefix(contig);
                    int gapSize = chromInsertsGapSize(chromInserts, 
                            ctg, firstContig);
                    if (hashLookup(contigsHash, contig))
                        errAbort("Contig repeated line %d of %s", lf->lineIx, lf->fileName);
                    hashAdd(contigsHash, contig, NULL);
                    if (gapSize != 0)
                        {
                        if ((bi = bigInsertBeforeContig(chromInserts, ctg)) != NULL)
                            {
                            gapType = bi->type;
                            }
                        fprintf(dest, "%s\t%d\t%d\t%d\tN\t%d\t%s\tno\n",
                                newName, end+1, end+gapSize, ++ix, gapSize, gapType);
                        }
                    firstContig = FALSE;
                    strcpy(lastContig, contig);
                    }
                spec = findLift(liftHash, contig, lf);
		cantHandleSpecRevStrand(spec);
                start = numField(words, 1, 0, lf) + spec->offset;
                end = numField(words, 2, 0, lf) + spec->offset;
                if (end > lastEnd) lastEnd = end;
                if (!sameString(newName, spec->newName))
                    errAbort("Mismatch in new name between %s and %s", newName, spec->newName);
                fprintf(dest, "%s\t%d\t%d\t%d\t%s\t%s\t%s\t%s",
                        newName, start, end, ++ix,
                        words[4], words[5], words[6], words[7]);
                if (wordCount == 9)
                    fprintf(dest, "\t%s", words[8]);
                fputc('\n', dest);
                }
            lineFileClose(&lf);
            if (dots)
                verbose(1, "\n");
            }
        }
    if (chromInserts != NULL)
        {
        if ((bi = chromInserts->terminal) != NULL)
            {
            fprintf(dest, "%s\t%d\t%d\t%d\tN\t%d\t%s\tno\n",
                    newName, lastEnd+1, lastEnd+bi->size, ++ix, bi->size, bi->type);
            }
        }
    if (ferror(dest))
        errAbort("error writing %s", destFile);
    fclose(dest);
}

void liftGap(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
    /* Lift up coordinates in .gap file (just the gaps from .agp).  Negative strad allowed */
{
    FILE *dest = mustOpen(destFile, "w");
    char *source;
    int i;
    struct lineFile *lf;
    int lineSize, wordCount;
    char *line, *words[32];
    char *s;
    struct liftSpec *spec;
    int start = 0;
    int end = 0;
    int ix = 0;
    char newDir[256], newName[128], newExt[64];
    char lastContig[256];
    char *contig;
    int lastEnd = 0;
    int fragStart, fragEnd;

    if (how == carryMissing)
        warn("'carry' doesn't work for .gap files, ignoring");

    splitPath(destFile, newDir, newName, newExt);

    strcpy(lastContig, "");
    for (i=0; i<sourceCount; ++i)
        {
        source = sources[i];
        verbose(1, "Lifting %s\n", source);
        lf = lineFileMayOpen(source, TRUE);
        if (lf != NULL)
            {
            while (lineFileNext(lf, &line, &lineSize))
                {
                /* Check for comments and just pass them through. */
                s = skipLeadingSpaces(line);
                if (s[0] == '#')
                    {
                    fprintf(dest, "%s\n", line);
                    continue;
                    }
                /* Parse line, adjust offsets, write */
                wordCount = chopLine(line, words);
                if (wordCount != 8 && wordCount != 9)
                    malformedAgp(lf);
                if (words[4][0] != 'N' && words[4][0] != 'U')
                    errAbort("Found non-gap in .gap file: %s", words[4]);
                contig = words[0];
                spec = findLift(liftHash, contig, lf);
                start = fragStart = numField(words, 1, 0, lf);
                end = fragEnd = numField(words, 2, 0, lf);
                end = fragEnd;
                if (spec->strand == '-')
                    {
                    start = spec->oldSize - fragEnd + 1;
                    end = spec->oldSize - fragStart + 1;
                }
            start += spec->offset;
            end += spec->offset;
	    if (end > lastEnd) lastEnd = end;
	    fprintf(dest, "%s\t%d\t%d\t%d\t%s\t%s\t%s\t%s",
		    spec->newName, start, end, ++ix,
		    words[4], words[5], words[6], words[7]);
	    if (wordCount == 9)
		fprintf(dest, "\t%s", words[8]);
	    fputc('\n', dest);
	    }
	lineFileClose(&lf);
        if (dots)
            verbose(1, "\n");
	}
    }
if (ferror(dest))
    errAbort("error writing %s", destFile);
fclose(dest);
}

boolean liftGenePredObj(struct hash *liftHash, struct genePred* gp, struct lineFile* lf)
/* lift a genePred  */
{
int iExon;
struct liftSpec *spec = findLift(liftHash, gp->chrom, lf);
if (spec == NULL)
    return ((how == carryMissing) ? TRUE : FALSE);

cantHandleSpecRevStrand(spec);
gp->txStart += spec->offset;
gp->txEnd += spec->offset;
/* sometimes no cds is indicated by zero, sometimes by setting both to
 * txEnd.  Don't lift zero case */
if (!((gp->cdsStart == 0) && (gp->cdsEnd == 0)))
    {
    gp->cdsStart += spec->offset;
    gp->cdsEnd += spec->offset;
    }
for (iExon = 0; iExon < gp->exonCount; iExon++)
    {
    gp->exonStarts[iExon] += spec->offset;
    gp->exonEnds[iExon] += spec->offset;
    }
freez(&gp->chrom);
gp->chrom = cloneString(spec->newName);
return TRUE;
}

void liftGenePred(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift a genePred files. */
{
char *row[GENEPRED_NUM_COLS];
struct lineFile* lf;
FILE* dest = mustOpen(destFile, "w");
int iSrc;

for (iSrc = 0; iSrc < sourceCount; iSrc++)
    {
    verbose(1, "Lifting %s\n", sources[iSrc]);
    lf = lineFileOpen(sources[iSrc], TRUE);
    while (lineFileChopNextTab(lf, row, ArraySize(row)))
        {
        struct genePred* gp = genePredLoad(row);
        if (liftGenePredObj(liftHash, gp, lf))
            genePredTabOut(gp, dest);
        genePredFree(&gp);
        }
    lineFileClose(&lf);
    if (dots)
        verbose(1, "\n");
    }

carefulClose(&dest);
}

void liftGenePredExt(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift a genePred files. */
{
char *row[GENEPREDX_NUM_COLS];
struct lineFile* lf;
FILE* dest = mustOpen(destFile, "w");
int iSrc;
int colCount;

for (iSrc = 0; iSrc < sourceCount; iSrc++)
    {
    verbose(1, "Lifting %s\n", sources[iSrc]);
    lf = lineFileOpen(sources[iSrc], TRUE);
    while ((colCount = lineFileChopNextTab(lf, row, ArraySize(row))))
        {
        struct genePred* gp = genePredExtLoad(row, colCount);
        if (liftGenePredObj(liftHash, gp, lf))
            genePredTabOut(gp, dest);
        genePredFree(&gp);
        }
    lineFileClose(&lf);
    if (dots)
        verbose(1, "\n");
    }

carefulClose(&dest);
}

struct bedInfo
/* Info on a line of a bed file. */
    {
    struct bedInfo *next;	/* Next in list. */
    char *chrom;  /* Chromosome (not allocated here). */
    int start;    /* Start position. */
    int end;      /* End position. */
    char line[1]; /* Rest of line - allocated at run time. */
    };

int bedInfoCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct bedInfo *a = *((struct bedInfo **)va);
const struct bedInfo *b = *((struct bedInfo **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

int max3(int a, int b, int c)
/* Return max of a,b,c */
{
int m = max(a,b);
if (c > m) m = c;
return m;
}

void liftTabbed(char *destFile, struct hash *liftHash, 
   int sourceCount, char *sources[],
   int ctgWord, int startWord, int endWord, 
   boolean doubleLift, int ctgWord2, int startWord2, int endWord2,
   int startOffset, int strandWord)
/* Generic lift a tab-separated file with contig, start, and end fields.
 * If doubleLift is TRUE, also lift second set of coordinated.*/
{
int minFieldCount = max3(startWord, endWord, ctgWord) + 1;
int wordCount, lineSize;
char *words[128], *line, *source;
struct lineFile *lf;
FILE *f = mustOpen(destFile, "w");
int i,j;
int start = 0;
int end = 0;
int start2 = 0;
int end2 = 0;
char *contig, *chrom = NULL, *chrom2 = NULL;
struct liftSpec *spec;
static char buf[1024*16];
char *s;
int len;
struct bedInfo *biList = NULL, *bi;
boolean anyHits = FALSE;

if (doubleLift)
   {
   int min2 = max3(ctgWord2, startWord2, endWord2);
   minFieldCount = max(minFieldCount, min2);
   }
for (i=0; i<sourceCount; ++i)
    {
    source = sources[i];
    lf = lineFileOpen(source, TRUE);
    verbose(1, "Lifting %s\n", source);
    while (lineFileNext(lf, &line, &lineSize))
	{
	if (line[0] == '#')
	    continue;
	wordCount = chopTabs(line, words);
	if (wordCount == 0)
	    continue;
	if (wordCount < minFieldCount)
	   errAbort("Expecting at least %d words line %d of %s", 
	   	minFieldCount, lf->lineIx, lf->fileName); 
	contig = words[ctgWord];
	contig = rmChromPrefix(contig);
	if (startWord >= 0)
	    start = lineFileNeedNum(lf, words, startWord);
	if (endWord >= 0)
	    end = lineFileNeedNum(lf, words, endWord);
	spec = findLift(liftHash, contig, lf);
	if (spec == NULL)
	    {
	    if (how == carryMissing)
		chrom = cloneString(contig);
	    else
		continue;
	    }
	else
	    {
	    chrom = spec->newName;
	    if (spec->strand == '-')
		{
		int s = start - startOffset,  e = end;
		start = spec->oldSize - e + startOffset;
		end = spec->oldSize - s;
		if (strandWord >= 0 && strandWord < wordCount)
		    {
		    char strand = words[strandWord][0];
		    if (strand == '+')
		        words[strandWord] = "-";
		    else if (strand == '-')
		        words[strandWord] = "+";
		    }
		}
	    start += spec->offset;
	    end += spec->offset;
	    }
	if (doubleLift)
	    {
	    contig = words[ctgWord2];
	    start2 = lineFileNeedNum(lf, words, startWord2);
	    end2 = lineFileNeedNum(lf, words, endWord2);
	    spec = findLift(liftHash, contig, lf);
	    if (spec == NULL)
		{
		if (how == carryMissing)
		    chrom2 = cloneString(contig);
		else
		    errAbort("Couldn't find second contig in lift file at line %d of %s\n", lf->lineIx, lf->fileName);
		}
	    else
		{
		cantHandleSpecRevStrand(spec);
		chrom2 = spec->newName;
		start2 += spec->offset;
		end2 += spec->offset;
		}
	    }
	anyHits = TRUE;
	s = buf;
	for (j=0; j<wordCount; ++j)
	    {
	    if (s + 128 >= buf + sizeof(buf))
	        errAbort("Line %d too long in %s", lf->lineIx, lf->fileName);
	    if (j != 0)
		*s++ = '\t';
	    if (j == ctgWord)
		s += sprintf(s, "%s", chrom);
	    else if (j == startWord)
	        s += sprintf(s, "%d", start);
	    else if (j == endWord)
	        s += sprintf(s, "%d", end);
	    else if (doubleLift && j == ctgWord2)
		s += sprintf(s, "%s", chrom2);
	    else if (doubleLift && j == startWord2)
	        s += sprintf(s, "%d", start2);
	    else if (doubleLift && j == endWord2)
	        s += sprintf(s, "%d", end2);
	    else
	        s += sprintf(s, "%s", words[j]);
	    }
	*s = 0;
        if (nosort)
            {
            fprintf(f, "%s\n", buf);
            }
        else
            {
            len = s-buf;
            bi = needMem(sizeof(*bi) + len);
            bi->chrom = chrom;
            bi->start = start;
            bi->end = end;
            memcpy(bi->line, buf, len);
            slAddHead(&biList, bi);
            }
	}
    lineFileClose(&lf);
    if (dots)
        verbose(1, "\n");
    }
if (!nosort)
    {
    slSort(&biList, bedInfoCmp);
    for (bi = biList; bi != NULL; bi = bi->next)
        {
        fprintf(f, "%s\n", bi->line);
        }
    }
if (ferror(f))
    errAbort("error writing %s", destFile);
fclose(f);
if (!anyHits)
   errAbort("No lines lifted!");
}

void liftBed(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift Browser Extensible Data file.  This is a tab
 * separated file where first three fields are 
 * seq, start, end.  This also sorts the result. */
{
liftTabbed(destFile, liftHash, sourceCount, sources, 0, 1, 2, FALSE, 0, 0, 0, 0, 5);
}

void liftBed8(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift BED8, getting the thickStart, and thickEnd fields. */
{
/* uses same contig for both pairs of coords */
liftTabbed(destFile, liftHash, sourceCount, sources, 0, 1, 2, TRUE, 0, 6, 7, 0, 5);
}

void liftGff(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift up coordinates of a .gff or a .gtf file. */
{
liftTabbed(destFile, liftHash, sourceCount, sources, 0, 3, 4, FALSE, 0, 0, 0, 1, 6);
}

void liftGdup(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift up coordinates of a .gdup. */
{
liftTabbed(destFile, liftHash, sourceCount, sources, 0, 1, 2, TRUE, 6, 7, 8, 1, -1);
}

void liftBScore(char *destFile, struct hash *liftHash, int sourceCount, char *sources[], boolean querySide)
/* Lift up coordinates of a .bscore */
{
if (querySide)
    liftTabbed(destFile, liftHash, sourceCount, sources, 1, 2, 3, FALSE, 0, 0, 0, 0, 1);
else
    liftTabbed(destFile, liftHash, sourceCount, sources, 4, 5, 6, FALSE, 0, 0, 0, 0, 1);
}

void liftBlast(char *destFile, struct hash *liftHash, int sourceCount, char *sources[])
/* Lift up coordinates of a .gdup. */
{
liftTabbed(destFile, liftHash, sourceCount, sources, 1, 8, 9, FALSE, 0, 0, 0, 1, -1);
}


char *contigInDir(char *name, char dirBuf[256])
/* Figure out which contig we're in by the file name. It should be the directory
 * before us. */
{
char root[128], ext[64]; 
char *s, *contig;
int len;

verbose(2,"#\tname: %s\n", name);
splitPath(name, dirBuf, root, ext);
len = strlen(dirBuf);
if (len == 0)
    errAbort("Need contig directory in file name to lift .gl files\n");
if (dirBuf[len-1] == '/')
    dirBuf[len-1] = 0;
verbose(2,"#\tdirBuf: %s\n", dirBuf);
if ((s = strrchr(dirBuf, '/')) != NULL)
    contig = s+1;
else
    contig = dirBuf;
verbose(2,"#\ts: %s\n", s);
return contig;
} 

void liftGl(char *destFile, struct hash *liftHash, int sourceCount, char *sources[]) 
/* Lift up coordinates in .gl file. */ 
{ 
char dirBuf[256], chromName[256];
int i; 
char *source; 
char *contig; 
FILE *dest = mustOpen(destFile, "w"); 
struct lineFile *lf = NULL;
int lineSize, wordCount;
char *line, *words[32];
struct liftSpec *spec;
int offset;

if (how == carryMissing)
    warn("'carry' doesn't work for .gl files, ignoring");
for (i=0; i<sourceCount; ++i)
    {
    source = sources[i];
    verbose(1, "Processing %s\n", source);
    contig = contigInDir(source, dirBuf);
    verbose(2,"#\tcontig: %s, source: %s, dirBuf: %s\n", contig, source, dirBuf);
    if (!startsWith("ctg", contig) &&
	!startsWith("NC_", contig) &&
	!startsWith("NT_", contig) &&
	!startsWith("NG_", contig))
        {
	sprintf(chromName, "chr%s", contig);
	contig = chromName;
    verbose(2,"#\tcontig: %s, chromName: %s\n", contig, chromName);
	}
    spec = findLift(liftHash, contig, lf);
    if (spec == NULL)
        continue;
    cantHandleSpecRevStrand(spec);
    offset = spec->offset;
    lf = lineFileMayOpen(source, TRUE);
    if (lf == NULL)
        {
	warn("%s doesn't exist, skipping", source);
	continue;
	}
    while (lineFileNext(lf, &line, &lineSize))
	{
	int s, e;
	if ((wordCount = chopLine(line, words)) != 4)
	    errAbort("Bad line %d of %s", lf->lineIx, lf->fileName);
	s = atoi(words[1]);
	e = atoi(words[2]);
	fprintf(dest, "%s\t%d\t%d\t%s\n", words[0], s+offset, e+offset, words[3]);
	}
    lineFileClose(&lf);
    if (dots)
        verbose(1, "\n");
    }
}

void liftUp(char *destFile, char *liftFile, char *howSpec, int sourceCount, char *sources[])
/* liftUp - change coordinates of .psl, .agp, or .out file
 * to parent coordinate system. */
{
struct liftSpec *lifts = NULL;
struct hash *liftHash;
char *destType = optionVal("type", destFile);

if (sameWord(howSpec, "carry"))
    how = carryMissing;
else if (sameWord(howSpec, "warn"))
    how = warnMissing;
else if (sameWord(howSpec, "drop"))
    how = silentDrop;
else if (sameWord(howSpec, "error"))
    how = errorMissing;
else
    usage();
if (how == carryMissing && sameString("/dev/null", liftFile))
    verbose(1, "Carrying input -- ignoring /dev/null liftFile\n");
else
    {
    lifts = readLifts(liftFile);
    verbose(1, "Got %d lifts in %s\n", slCount(lifts), liftFile);
    }

if (endsWith(destType, ".out"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, FALSE);
    liftOut(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".pslx") || endsWith(destType, ".xa") || endsWith(destType, ".psl"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, TRUE);
    liftPsl(destFile, liftHash, sourceCount, sources, 
    	optionExists("pslQ") || optionExists("pslq"), !endsWith(destType, ".psl") );
    }
else if (endsWith(destType, ".agp"))
    {
    liftHash = hashLift(lifts, FALSE);
    liftAgp(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".gap"))
    {
    liftHash = hashLift(lifts, TRUE);
    liftGap(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".gl"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, FALSE);
    liftGl(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".gff") || endsWith(destType, ".gtf"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, TRUE);
    liftGff(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".gdup"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, FALSE);
    liftGdup(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".bed"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, TRUE);
    liftBed(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".bed8"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, TRUE);
    liftBed8(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".gp") || endsWith(destType, ".genepred"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, TRUE);
    if (extGenePred)
        liftGenePredExt(destFile, liftHash, sourceCount, sources);
    else
        liftGenePred(destFile, liftHash, sourceCount, sources);
    }
else if (endsWith(destType, ".bscore"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, TRUE);
    liftBScore(destFile, liftHash, sourceCount, sources,
    	optionExists("pslQ") || optionExists("pslq"));
    }
else if (endsWith(destType, ".tab"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, TRUE);
    liftBlast(destFile, liftHash, sourceCount, sources);
    }
else if (strstr(destType, "gold"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, FALSE);
    liftAgp(destFile, liftHash, sourceCount, sources);
    }
else if (strstr(destType, ".axt"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, FALSE);
    liftAxt(destFile, liftHash, sourceCount, sources, 	
    	optionExists("axtQ") || optionExists("axtq"));
    }
else if (strstr(destType, ".chain"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, TRUE);
    liftChain(destFile, liftHash, sourceCount, sources, 	
    	optionExists("chainQ") || optionExists("chainq"));
    }
else if (strstr(destType, ".net"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, FALSE);
    liftNet(destFile, liftHash, sourceCount, sources, 	
    	optionExists("netQ") || optionExists("netq"));
    }
else if (strstr(destType, ".wab"))
    {
    rmChromPart(lifts);
    liftHash = hashLift(lifts, FALSE);
    liftWab(destFile, liftHash, sourceCount, sources,
    	optionExists("wabaQ") || optionExists("wabaq"));
    }
else 
    {
    errAbort("Unknown file suffix for %s\n", destType);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
nohead = optionExists("nohead");
nosort = optionExists("nosort");
isPtoG = optionExists("isPtoG");
dots = optionInt("dots", dots);
gapsize = optionInt("gapsize", gapsize);
extGenePred = optionExists("extGenePred");
ignoreVersions = optionExists("ignoreVersions");
if (gapsize !=0)
    chromInsertsSetDefaultGapSize(gapsize);
if (argc < 5)
    usage();
liftUp(argv[1], argv[2], argv[3], argc-4, argv+4);
return 0;
}

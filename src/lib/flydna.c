/* flydna.c - routines for accessing fly genome and cDNA sequences. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "snof.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "nt4.h"
#include "cda.h"
#include "wormdna.h"
#include "flydna.h"


static char *chromNames[] = {"adh"};
static char *ntFileNames[] = {"c:/biodata/fly/chrom/adh.nt"};

void flyLoadNt4Genome(struct nt4Seq ***retNt4Seq, int *retNt4Count)
/* Load up entire packed fly genome into memory. */
{
struct nt4Seq **pSeq;
int i;

pSeq = needMem(ArraySize(ntFileNames) * sizeof(pSeq[0]) );
for (i=0; i<ArraySize(ntFileNames); ++i)
	{
	pSeq[i] = loadNt4(ntFileNames[i], chromNames[i]);
	}
*retNt4Seq = pSeq;
*retNt4Count = ArraySize(ntFileNames);
}

void flyFreeNt4Genome(struct nt4Seq ***pNt4Seq)
/* Free up packed fly genome. */
{
struct nt4Seq **pSeq;
int i;

if ((pSeq = *pNt4Seq) == NULL)
	return;
for (i=0; i<ArraySize(ntFileNames); ++i)
	freeNt4(&pSeq[i]);
freez(pNt4Seq);
}

void flyChromNames(char ***retNames, int *retNameCount)
/* Get list of fly chromosome names. */
{
*retNames = chromNames;
*retNameCount = ArraySize(chromNames);
}

void flyFaCommentIntoInfo(char *faComment, struct wormCdnaInfo *retInfo)
/* Process line from .fa file containing information about cDNA into binary
 * structure. */
{
if (retInfo)
    {
    char *s;
    zeroBytes(retInfo, sizeof(*retInfo));
    /* Separate out first word and use it as name. */
    s = strchr(faComment, ' ');
    if (s != NULL)
	    *s++ = 0;
    retInfo->name = faComment+1;
    retInfo->motherString = faComment;
	s = strrchr(retInfo->name, '.');
	retInfo->orientation = '+';
	if (s != NULL)
		retInfo->orientation = (s[1] == '3' ? '-' : '+');
    }
}


boolean flyCdnaSeq(char *name, struct dnaSeq **retDna, struct wormCdnaInfo *retInfo)
/* Get a single fly cDNA sequence. Optionally (if retInfo is non-null) get additional
 * info about the sequence. */
{
long offset;
char *faComment;
char **pFaComment = (retInfo == NULL ? NULL : &faComment);
static struct snof *cdnaSnof = NULL;
static FILE *cdnaFa;

if (cdnaSnof == NULL)
	cdnaSnof = snofMustOpen("c:/biodata/fly/cDna/allcdna");
if (cdnaFa == NULL)
	cdnaFa = mustOpen("c:/biodata/fly/cDna/allcdna.fa", "rb");
if (!snofFindOffset(cdnaSnof, name, &offset))
    return FALSE;
fseek(cdnaFa, offset, SEEK_SET);
if (!faReadNext(cdnaFa, name, TRUE, pFaComment, retDna))
    return FALSE;
flyFaCommentIntoInfo(faComment, retInfo);
return TRUE;
}


char *flyFeaturesDir()
/* Return the features directory. (Includes trailing slash.) */
{
return "C:/biodata/fly/features/";
}

FILE *flyOpenGoodAli()
/* Opens good alignment file and reads signature. 
 * (You can then cdaLoadOne() it.) */
{
return cdaOpenVerify("C:/biodata/fly/cDNA/good.ali");
}

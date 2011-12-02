/* wormDna - Stuff for finding worm DNA and annotation features.
 * This is pretty much the heart of the cobbled-together 'database'
 * behind the intronerator. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "gdf.h"
#include "nt4.h"
#include "snof.h"
#include "wormdna.h"
#include "cda.h"
#include "sig.h"
#include "dystring.h"


static char *jkwebDir = NULL;

static char *cdnaDir = NULL;
static char *featDir = NULL;
static char *nt4Dir = NULL;
static char *sangerDir = NULL;
static char *genieDir = NULL;
static char *xenoDir = NULL;

static void getDirs()
/* Look up the directories where our data is stored. */
{
if (jkwebDir == NULL)
    {
    char buf[512];
    
    /* Look up directory where directory pointer files are stored
     * in environment string if it's there. */
    if ((jkwebDir = getenv("JKWEB")) == NULL)
        jkwebDir = "";

    sprintf(buf, "%scdna.dir", jkwebDir);
    firstWordInFile(buf, buf, sizeof(buf));
    cdnaDir = cloneString(buf);

    sprintf(buf, "%sfeat.dir", jkwebDir);
    firstWordInFile(buf, buf, sizeof(buf));
    featDir = cloneString(buf);

    sprintf(buf, "%snt4.dir", jkwebDir);
    firstWordInFile(buf, buf, sizeof(buf));
    nt4Dir = cloneString(buf);

    sprintf(buf, "%ssanger/", featDir); 
    sangerDir = cloneString(buf);

    sprintf(buf, "%sgenie/", featDir);
    genieDir = cloneString(buf);

    sprintf(buf, "%sxeno.dir", jkwebDir);
    firstWordInFile(buf, buf, sizeof(buf));
    xenoDir = cloneString(buf);
    }
}

char *wormFeaturesDir()
/* Return the features directory. (Includes trailing slash.) */
{
getDirs();
return featDir;
}

char *wormChromDir()
/* Return the directory with the chromosomes. */
{
getDirs();
return nt4Dir;
}

char *wormCdnaDir()
/* Return directory with cDNA data. */
{
getDirs();
return cdnaDir;
}

char *wormSangerDir()
/* Return directory with Sanger specific gene predictions. */
{
getDirs();
return sangerDir;
}

char *wormGenieDir()
/* Return directory with Genie specific gene predictions. */
{
getDirs();
return genieDir;
}

char *wormXenoDir()
/* Return directory with cross-species alignments. */
{
getDirs();
return xenoDir;
}

static char *chromIds[] = {"i", "ii", "iii", "iv", "v", "x", "m", };

void wormChromNames(char ***retNames, int *retNameCount)
/* Get list of worm chromosome names. */
{
*retNames = chromIds;
*retNameCount = ArraySize(chromIds);
}

int wormChromIx(char *name)
/* Return index of worm chromosome. */
{
return stringIx(name, chromIds);
}

char *wormChromForIx(int ix)
/* Given ix, return worm chromosome official name. */
{
assert(ix >= 0 && ix <= ArraySize(chromIds));
return chromIds[ix];
}

char *wormOfficialChromName(char *name)
/* This returns a pointer to our official string for the chromosome name.
 * (This allows some routines to do direct pointer comparisons rather
 * than string comparisons.) */
{
int ix = wormChromIx(name);
if (ix < 0) return NULL;
return chromIds[ix];
}


static struct snof *cdnaSnof = NULL;
static FILE *cdnaFa = NULL;

static void wormCdnaCache()
/* Set up to read cDNAs */
{
getDirs();
if (cdnaSnof == NULL)
    {
    char buf[512];

    sprintf(buf, "%s%s", cdnaDir, "allcdna");
    cdnaSnof = snofMustOpen(buf);
    sprintf(buf, "%s%s", cdnaDir, "allcdna.fa");
    cdnaFa = mustOpen(buf, "rb");
    }
}

void wormCdnaUncache()
/* Tear down structure for reading cDNAs. */
{
snofClose(&cdnaSnof);
carefulClose(&cdnaFa);
freez(&cdnaDir);
}

void wormFreeCdnaInfo(struct wormCdnaInfo *ci)
/* Free the mother string in the cdnaInfo.  (The info structure itself normally isn't
 * dynamically allocated. */
{
freeMem(ci->motherString);
zeroBytes(ci, sizeof(*ci));
}

static char *realInfoString(char *s)
/* Returns NULL if s is just "?", the NULL placeholder. */
{
if (s[0] == '?' && s[1] == 0) return NULL;
return s;
}

static void parseRestOfCdnaInfo(char *textInfo, struct wormCdnaInfo *retInfo)
/* Parse text info string into a binary structure retInfo. */
{
int wordCount;
char *words[32];
char *s;

wordCount = chopString(textInfo, "|", words, ArraySize(words));
if (wordCount < 8)
    errAbort("Expecting at least 8 fields in cDNA database, got %d", wordCount);
if ((s = realInfoString(words[0])) != NULL)
    retInfo->orientation = s[0];
retInfo->gene = realInfoString(words[1]);
retInfo->product = realInfoString(words[2]);
if ((s = realInfoString(words[3])) != NULL)
    {
    char *parts[2];
    int partCount;
    partCount = chopString(s, ".", parts, ArraySize(parts));
    if (partCount == 2)
        {
        retInfo->knowStart = retInfo->knowEnd = TRUE;
        if (parts[0][0] == '<')
            {
            retInfo->knowStart = FALSE;
            parts[0] += 1;
            }
        if (parts[1][0] == '>')
            {
            retInfo->knowEnd = FALSE;
            parts[1] += 1;
            }
        retInfo->cdsStart = atoi(parts[0]);
        retInfo->cdsEnd = atoi(parts[1]);
        }
    }
if ((s = realInfoString(words[4])) != NULL)
    {
    if (sameString("embryo", s))
        retInfo->isEmbryonic = TRUE;
    else if (sameString("adult", s))
        retInfo->isAdult = TRUE;
    }
if ((s = realInfoString(words[5])) != NULL)
    {
    if (sameString("herm", s))
        retInfo->isHermaphrodite = TRUE;
    else if (sameString("male", s))
        retInfo->isMale = TRUE;
    }

if ((s = realInfoString(words[6])) != NULL)
    {
    /* Reserved. Unused currently */
    }
retInfo->description = realInfoString(words[7]);
}

void wormFaCommentIntoInfo(char *faComment, struct wormCdnaInfo *retInfo)
/* Process line from .fa file containing information about cDNA into binary
 * structure. */
{
if (retInfo)
    {
    char *s;
    zeroBytes(retInfo, sizeof(*retInfo));
    /* Separate out first word and use it as name. */
    s = strchr(faComment, ' ');
    if (s == NULL)
        errAbort("Expecting lots of info, just got %s", faComment);
    *s++ = 0;
    retInfo->name = faComment+1;
    retInfo->motherString = faComment;

    parseRestOfCdnaInfo(s, retInfo);
    }
}

boolean wormCdnaInfo(char *name, struct wormCdnaInfo *retInfo)
/* Get info about cDNA sequence. */
{
char commentBuf[512];
char *comment;
long offset;

wormCdnaCache();
if (!snofFindOffset(cdnaSnof, name, &offset))
    return FALSE;
fseek(cdnaFa, offset, SEEK_SET);
mustGetLine(cdnaFa, commentBuf, sizeof(commentBuf));
if (commentBuf[0] != '>')
    errAbort("Expecting line starting with > in cDNA fa file.\nGot %s", commentBuf);
comment = cloneString(commentBuf);
wormFaCommentIntoInfo(comment, retInfo);
return TRUE;
}

boolean wormCdnaSeq(char *name, struct dnaSeq **retDna, struct wormCdnaInfo *retInfo)
/* Get a single worm cDNA sequence. Optionally (if retInfo is non-null) get additional
 * info about the sequence. */
{
long offset;
char *faComment;
char **pFaComment = (retInfo == NULL ? NULL : &faComment);

wormCdnaCache();
if (!snofFindOffset(cdnaSnof, name, &offset))
    return FALSE;
fseek(cdnaFa, offset, SEEK_SET);
if (!faReadNext(cdnaFa, name, TRUE, pFaComment, retDna))
    return FALSE;
wormFaCommentIntoInfo(faComment, retInfo);
return TRUE;
}

struct wormFeature *newWormFeature(char *name, char *chrom, int start, int end, char typeByte)
/* Allocate a new feature. */
{
int size = sizeof(struct wormFeature) + strlen(name);
struct wormFeature *feat = needMem(size);
feat->chrom = chrom;
feat->start = start;
feat->end = end;
feat->typeByte = typeByte;
strcpy(feat->name, name);
return feat;
}


static struct wormFeature *scanChromOffsetFile(char *dir, char *suffix, 
    bits32 signature, int nameOffset, char *chromId, int start, int end,
    int addEnd)
/* Scan a chrom.pgo or chrom.cdo file for names of things that are within
 * range. */
{
FILE *f;
char fileName[512];
bits32 sig, nameSize, entryCount;
int entrySize;
int *entry;
char *name;
bits32 i;
struct wormFeature *list = NULL, *el;
char *typePt;
char typeByte;

sprintf(fileName, "%s%s%s", dir, chromId, suffix);
f = mustOpen(fileName, "rb");
mustReadOne(f, sig);
if (sig != signature)
    errAbort("Bad signature on %s", fileName);
mustReadOne(f, entryCount);
mustReadOne(f, nameSize);
entrySize = nameSize + nameOffset;
entry = needMem(entrySize + 1);
name = (char *)entry;
name += nameOffset;
typePt = name-1;
for (i=0; i<entryCount; ++i)
    {
    mustRead(f, entry, entrySize);
    if (entry[0] > end)
        break;
    if (entry[1] < start)
        continue;
    typeByte = *typePt;
    el = newWormFeature(name, chromId, entry[0], entry[1]+addEnd, typeByte);
    slAddHead(&list, el);
    }
slReverse(&list);
fclose(f);
freeMem(entry);
return list;
}

struct wormFeature *wormCdnasInRange(char *chromId, int start, int end)
/* Get all cDNAs that overlap the range. freeDnaSeqList the returned
 * list when you are through with it. */
{
/* This routine looks through the .CDO files made by cdnaOff
 */
getDirs();
return scanChromOffsetFile(cdnaDir, ".cdo", cdoSig, 2*sizeof(int)+1, 
    chromId, start, end, 0);
}

struct wormFeature *wormSomeGenesInRange(char *chromId, int start, int end, char *gdfDir)
/* Get info on genes that overlap range in a particular set of gene predictions. */
{
return scanChromOffsetFile(gdfDir, ".pgo", pgoSig, 2*sizeof(int)+1,
    chromId, start, end, 0);
}

struct wormFeature *wormGenesInRange(char *chromId, int start, int end)
/* Get names of all genes that overlap the range. */
{
/* This routine looks through the .PGO files made by makePgo
 */
getDirs();
return wormSomeGenesInRange(chromId, start, end, sangerDir);
}

struct wormFeature *wormCosmidsInRange(char *chromId, int start, int end)
/* Get names of all genes that overlap the range. */
{
/* This routine looks through the .COO files made by makePgo
 */
getDirs();
return scanChromOffsetFile(featDir, ".coo", pgoSig, 2*sizeof(int)+1,
    chromId, start, end, 1);
}

FILE *wormOpenGoodAli()
/* Opens good alignment file and reads signature. 
 * (You can then cdaLoadOne() it.) */
{
char fileName[512];
getDirs();
sprintf(fileName, "%sgood.ali", cdnaDir);
return cdaOpenVerify(fileName);
}

struct cdaAli *wormCdaAlisInRange(char *chromId, int start, int end)
/* Return list of cdna alignments that overlap range. */
{
struct cdaAli *list = NULL, *el;
char fileName[512];
FILE *ixFile, *aliFile;
bits32 sig;
int s, e;
long fpos;

aliFile = wormOpenGoodAli();

sprintf(fileName, "%s%s.alx", cdnaDir, chromId);
ixFile = mustOpen(fileName, "rb");
mustReadOne(ixFile, sig);
if (sig != alxSig)
    errAbort("Bad signature on %s", fileName);

for (;;)
    {
    if (!readOne(ixFile, s))
        break;
    mustReadOne(ixFile, e);
    mustReadOne(ixFile, fpos);
    if (e <= start)
        continue;
    if (s >= end)
        break;
    AllocVar(el);
    fseek(aliFile, fpos, SEEK_SET);
    el = cdaLoadOne(aliFile);
    if (el == NULL)
        errAbort("Truncated cdnaAli file");
    slAddHead(&list, el);
    }
slReverse(&list);
fclose(aliFile);
fclose(ixFile);
return list;
}

boolean nextWormCdnaAndInfo(struct wormCdnaIterator *it, struct dnaSeq **retSeq, 
    struct wormCdnaInfo *retInfo)
/* Return next sequence and associated info from database. */
{
char *faComment;

if (!faReadNext(it->faFile, "unknown", TRUE, &faComment, retSeq))
    return FALSE;
wormFaCommentIntoInfo(faComment, retInfo);
return TRUE;
}

struct dnaSeq *nextWormCdna(struct wormCdnaIterator *it)
/* Return next sequence in database */
{
return faReadOneDnaSeq(it->faFile, "unknown", TRUE);
}

boolean wormSearchAllCdna(struct wormCdnaIterator **retSi)
/* Set up to search entire database or worm cDNA */
{
char buf[512];
struct wormCdnaIterator *it;

it = needMem(sizeof(*it));
getDirs();
sprintf(buf, "%s%s", cdnaDir, "allcdna.fa");
it->faFile = mustOpen(buf, "rb");
*retSi = it;
return TRUE;
}

void freeWormCdnaIterator(struct wormCdnaIterator **pIt)
/* Free up iterator returned by wormSearchAllCdna() */
{
struct wormCdnaIterator *it = *pIt;
if (it != NULL)
    {
    carefulClose(&it->faFile);
    freez(pIt);
    }
}

static boolean isAllAlpha(char *s)
/* Returns TRUE if every character in string is a letter. */
{
char c;
while ((c = *s++) != 0)
    {
    if (!isalpha(c)) return FALSE;
    }
return TRUE;
}

static boolean isAllDigit(char *s)
/* Returns TRUE if every character in string is a digit. */
{
char c;
while ((c = *s++) != 0)
    {
    if (!isdigit(c)) return FALSE;
    }
return TRUE;
}

boolean wormIsOrfName(char *in)
/* Check to see if the input is formatted correctly to be
 * an ORF. */
{
return strchr(in, '.') != NULL;
}

boolean wormIsGeneName(char *name)
/* See if it looks like a worm gene name - that is
 *   abc-12
 * letters followed by a dash followed by a number. */
{
char buf[128];
int partCount;
strncpy(buf, name, sizeof(buf));
partCount = chopString(buf, "-", NULL, 0);
if (partCount == 2)
    {
    char *parts[2];
    chopString(buf, "-", parts, 2);
    return isAllAlpha(parts[0]) && isAllDigit(parts[1]);
    }
else
    {
    return FALSE;
    }
}

struct slName *wormGeneToOrfNames(char *name)
/* Returns list of cosmid.N type ORF names that are known by abc-12 type name. */
{
struct slName *synList = NULL;
char synFileName[512];
FILE *synFile;
char lineBuf[128];
int nameLen = strlen(name);

/* genes are supposed to be lower case. */
tolowers(name);

/* Open synonym file and loop through each line of it */
sprintf(synFileName, "%ssyn", wormFeaturesDir());
if ((synFile = fopen(synFileName, "r")) == NULL)
    errAbort("Can't find synonym file '%s'. (errno: %d)\n", synFileName, errno);
while (fgets(lineBuf, ArraySize(lineBuf), synFile))
    {
    /* If first part of line matches chop up line. */
    if (strncmp(name, lineBuf, nameLen) == 0)
	{
	char *syns[32];
	int count;
	count = chopString(lineBuf, whiteSpaceChopper, syns, ArraySize(syns));

	/* Looks like we got a synonym.  Add all the aliases. */
	if (strcmp(name, syns[0]) == 0)
	    {
	    int i;
	    for (i=1; i<count; ++i)
                slAddTail(&synList, newSlName(syns[i]));
	    break;
	    }
	}
    }
fclose(synFile);
return synList;
}

char *wormGeneFirstOrfName(char *geneName)
/* Return first ORF synonym to gene. */
{
struct slName *synList = wormGeneToOrfNames(geneName);
char *name;
if (synList == NULL)
    return NULL;
name = cloneString(synList->name);
slFreeList(&synList);
return name;
}

boolean wormGeneForOrf(char *orfName, char *geneNameBuf, int bufSize)
/* Look for gene type (unc-12 or something) synonym for cosmid.N name. */
{
FILE *f;
char fileName[512];
char lineBuf[512];
int nameLen = strlen(orfName);
boolean ok = FALSE;

sprintf(fileName, "%sorf2gene", wormFeaturesDir());
f = mustOpen(fileName, "r");
while (fgets(lineBuf, sizeof(lineBuf), f))
    {
    if (strncmp(lineBuf, orfName, nameLen) == 0 && lineBuf[nameLen] == ' ')
        {
        char *words[2];
        int wordCount;
        wordCount = chopLine(lineBuf, words);
        assert((int)strlen(words[1]) < bufSize);
        strncpy(geneNameBuf, words[1], bufSize);
        ok = TRUE;
        break;
        }
    }
fclose(f);
return ok;
}

boolean wormInfoForGene(char *orfName, struct wormCdnaInfo *retInfo)
/* Return info if any on ORF, or NULL if none exists. freeMem() return value. */
{
FILE *f;
char fileName[512];
char lineBuf[512];
int nameLen;
char *info = NULL;
char *synName = NULL;
int lineCount = 0;

/* Make this one work for orfs as well as gene names */
if (wormIsGeneName(orfName))
    {
    synName = wormGeneFirstOrfName(orfName);
    if (synName != NULL)
        orfName = synName;
    }
sprintf(fileName, "%sorfInfo", wormFeaturesDir());
nameLen = strlen(orfName);
f = mustOpen(fileName, "r");
while (fgets(lineBuf, sizeof(lineBuf), f))
    {
    ++lineCount;
    if (strncmp(lineBuf, orfName, nameLen) == 0 && lineBuf[nameLen] == ' ')
        {
        info = cloneString(lineBuf);
        break;
        }
    }
freeMem(synName);
fclose(f);
if (info == NULL)
    return FALSE;
wormFaCommentIntoInfo(info, retInfo);
return TRUE;;
}

boolean getWormGeneDna(char *name, DNA **retDna, boolean upcExons)
/* Get the DNA associated with a gene.  Optionally upper case exons. */
{
struct gdfGene *g;
struct slName *syn = NULL;
long lstart, lend;
int start, end;
int dnaSize;
DNA *dna;
struct wormGdfCache *gdfCache;

/* Translate biologist type name to cosmid.N name */
if (wormIsGeneName(name))
    {
    syn = wormGeneToOrfNames(name);
    if (syn != NULL)
        name = syn->name;
    }
if (strncmp(name, "g-", 2) == 0)
    gdfCache = &wormGenieGdfCache;
else
    gdfCache = &wormSangerGdfCache;
if ((g = wormGetSomeGdfGene(name, gdfCache)) == NULL)
    return FALSE;
gdfGeneExtents(g, &lstart, &lend);
start = lstart;
end = lend;
/* wormClipRangeToChrom(chromIds[g->chromIx], &start, &end); */
dnaSize = end-start;
*retDna = dna = wormChromPart(chromIds[g->chromIx], start, dnaSize);

gdfOffsetGene(g, -start);
if (g->strand == '-')
    {
    reverseComplement(dna, dnaSize);
    gdfRcGene(g, dnaSize);
    }
if (upcExons)
    {
    int i;
    struct gdfDataPoint *pt = g->dataPoints;
    for (i=0; i<g->dataCount; i += 2)
        {
        toUpperN(dna + pt[i].start, pt[i+1].start - pt[i].start);
        }
    }
gdfFreeGene(g);
return TRUE;
}

boolean getWormGeneExonDna(char *name, DNA **retDna)
/* Get the DNA associated with a gene, without introns.  */
{
struct gdfGene *g;
struct slName *syn = NULL;
long lstart, lend;
int start, end;
int dnaSize;
DNA *dna;
int i;
struct gdfDataPoint *pt = NULL;
struct wormGdfCache *gdfCache;
struct dyString *dy = newDyString(1000);
/* Translate biologist type name to cosmid.N name */
if (wormIsGeneName(name))
    {
    syn = wormGeneToOrfNames(name);
    if (syn != NULL)
        name = syn->name;
    }
if (strncmp(name, "g-", 2) == 0)
    gdfCache = &wormGenieGdfCache;
else
    gdfCache = &wormSangerGdfCache;
if ((g = wormGetSomeGdfGene(name, gdfCache)) == NULL)
    return FALSE;
gdfGeneExtents(g, &lstart, &lend);
start = lstart;
end = lend;
/*wormClipRangeToChrom(chromIds[g->chromIx], &start, &end);*/
dnaSize = end-start;
dna = wormChromPart(chromIds[g->chromIx], start, dnaSize);

gdfOffsetGene(g, -start);
if (g->strand == '-')
    {
    reverseComplement(dna, dnaSize);
    gdfRcGene(g, dnaSize);
    }
pt = g->dataPoints;
for (i=0; i<g->dataCount; i += 2)
    {
    dyStringAppendN(dy, (dna+pt[i].start), (pt[i+1].start - pt[i].start));
    }
*retDna = cloneString(dy->string);
dyStringFree(&dy);
gdfFreeGene(g);
return TRUE;
}

static void makeChromFileName(char *chromId, char *buf)
{
getDirs();
sprintf(buf, "%s%s.nt4", nt4Dir, chromId);
}

void wormLoadNt4Genome(struct nt4Seq ***retNt4Seq, int *retNt4Count)
/* Load up entire packed worm genome into memory. */
{
int count = ArraySize(chromIds);
struct nt4Seq **nt4s = needMem(count*sizeof(*nt4s));
int i;
char fileName[512];

for (i=0; i<count; ++i)
    {
    makeChromFileName(chromIds[i], fileName);
    nt4s[i] = loadNt4(fileName, chromIds[i]);
    }
*retNt4Seq = nt4s;
*retNt4Count = count;
}

void wormFreeNt4Genome(struct nt4Seq ***pNt4Seq)
/* Free up packed worm genome. */
{
struct nt4Seq **seqs;
int i;
if ((seqs = *pNt4Seq) == NULL)
    return;
for (i=0; i<ArraySize(chromIds); ++i)
    freeNt4(&seqs[i]);
freez(pNt4Seq);
}

int wormChromSize(char *chrom)
/* Return size of worm chromosome. */
{
static int sizes[ArraySize(chromIds)];
int ix;
int size;

if ((ix = wormChromIx(chrom)) < 0)
    errAbort("%s isn't a chromosome", chrom);
size = sizes[ix];

/* If we don't know it already have to get it from file. */
if (size == 0)
    {
    char fileName[512];
    makeChromFileName(chromIds[ix], fileName);
    size = sizes[ix] = nt4BaseCount(fileName);
    }
return size;
}


DNA *wormChromPart(char *chromId, int start, int size)
/* Return part of a worm chromosome. */
{
char fileName[512];
makeChromFileName(chromId, fileName);
return nt4LoadPart(fileName, start, size);
}

DNA *wormChromPartExonsUpper(char *chromId, int start, int size)
/* Return part of a worm chromosome with exons in upper case. */
{
DNA *dna = wormChromPart(chromId, start, size);
struct wormFeature *geneFeat = wormGenesInRange(chromId, start, start+size);
struct wormFeature *feat;

for (feat = geneFeat; feat != NULL; feat = feat->next)
    {
    char *name = feat->name;
    if (!wormIsNamelessCluster(name))
        {
        struct gdfGene *gene = wormGetGdfGene(name);
        gdfUpcExons(gene, feat->start, dna, size, start);
        gdfFreeGene(gene);
        }
    }
slFreeList(&geneFeat);
return dna;
}

void wormClipRangeToChrom(char *chrom, int *pStart, int *pEnd)
/* Make sure that we stay inside chromosome. */
{
int chromEnd = wormChromSize(chrom);
int temp;

/* Swap ends if reversed. */
if (*pStart > *pEnd)
    {
    temp = *pEnd;
    *pEnd = *pStart;
    *pStart = temp;
    }
/* Generally speaking try to slide the range covered by
 * start-end inside the chromosome rather than just
 * truncating an end. */
if (*pStart < 0)
    {
    *pEnd -= *pStart;
    *pStart = 0;
    }
if (*pEnd > chromEnd)
    {
    *pStart -= *pEnd - chromEnd;
    *pEnd = chromEnd;
    }
/* This handles case where the range is larger than the chromosome. */
if (*pStart < 0)
    *pStart = 0;
}

boolean wormParseChromRange(char *in, char **retChromId, int *retStart, int *retEnd)
/* Chop up a string representation of a range within a chromosome and put the
 * pieces into the return variables. Return FALSE if it isn't formatted right. */
{
char *words[5];
int wordCount;
char *chromId;
char buf[128];

strncpy(buf, in, sizeof(buf));
wordCount = chopString(buf, "- \t\r\n:", words, ArraySize(words));
if (wordCount != 3)
    return FALSE;
chromId = wormOfficialChromName(words[0]);
if (chromId == NULL)
    return FALSE;
if (!isdigit(words[1][0]) || !isdigit(words[2][0]))
    return FALSE;
*retChromId = chromId;
*retStart = atoi(words[1]);
*retEnd = atoi(words[2]);
wormClipRangeToChrom(chromId, retStart, retEnd);
return TRUE;
}

boolean wormIsChromRange(char *in)
/* Check to see if the input is formatted correctly to be
 * a range of a chromosome. */
{
char *chromId;
int start, end;
boolean ok;
ok =  wormParseChromRange(in, &chromId, &start, &end);
return ok;
}

boolean wormFixupOrfName(char *name)
/* Turn something into a proper cosmid.# style name. Return FALSE if it can't be done. */
{
char *dot = strrchr(name, '.');
if (dot == NULL)
    return FALSE;
toUpperN(name, dot-name);   /* First part always upper case. */
if (!isdigit(dot[1]))          /* Nameless cluster - just leave following digits be. */
    return TRUE;
else
    tolowers(dot+1);        /* Suffix is lower case. */
return TRUE;
}

boolean wormIsAltSplicedName(char *name)
/* Is name in right form to be an isoform? */
{
char *dot = strrchr(name, '.');
if (dot == NULL)
    return FALSE;
if (!isdigit(dot[1]))
    return FALSE;
return isalpha(dot[strlen(dot)-1]);
}

static void makeIsoformBaseName(char *name)
{
if (wormIsAltSplicedName(name))
    name[strlen(name)-1] = 0;
}

static boolean findAltSpliceRange(char *name, struct snof *snof, FILE *f, 
    char **retChrom, int *retStart, int *retEnd, char *retStrand)
/* Return range of chromosome covered by a gene and all of it's isoforms. */
{
char baseName[64];
char bName[64];
int snIx, maxIx;
int start = 0x7fffffff;
int end = -start;
char lineBuf[128];
char *words[3];
int wordCount;
int baseNameSize;

strcpy(baseName, name);
makeIsoformBaseName(baseName);
baseNameSize = strlen(baseName);
if (!snofFindFirstStartingWith(snof, baseName, baseNameSize, &snIx))
    return FALSE;
maxIx = snofElementCount(snof);
for (;snIx < maxIx; ++snIx)
    {
    long offset;
    char *geneName;

    snofNameOffsetAtIx(snof, snIx, &geneName, &offset);
    if (strncmp(geneName, baseName, baseNameSize) != 0)
        break;
    strcpy(bName, geneName);
    makeIsoformBaseName(bName);
    if (sameString(baseName, bName))
        {
        int s, e;
        fseek(f, offset, SEEK_SET);
        mustGetLine(f, lineBuf, sizeof(lineBuf));
        wordCount = chopLine(lineBuf, words);
        assert(wordCount == 3);
        wormParseChromRange(words[0], retChrom, &s, &e);
        *retStrand = words[1][0];
        if (start > s)
            start = s;
        if (end < e)
            end = e;
        }
    }
*retStart = start;
*retEnd = end;
return TRUE;
}


boolean wormGeneRange(char *name, char **retChrom, char *retStrand, int *retStart, int *retEnd)
/* Return chromosome position of a chrom range, gene, ORF, cosmid, or nameless cluster. */
{
static struct snof *c2gSnof = NULL, *c2cSnof = NULL;
static FILE *c2gFile = NULL, *c2cFile = NULL;
long offset;
char fileName[512];
struct slName *syn = NULL;
boolean ok;

if (wormIsChromRange(name))
    {
    *retStrand = '.';
    return wormParseChromRange(name, retChrom, retStart, retEnd);
    }

getDirs();

/* Translate biologist type name to cosmid.N name */
if (wormIsGeneName(name))
    {
    syn = wormGeneToOrfNames(name);
    if (syn != NULL)
	{
        name = syn->name;
	}
    }
if (wormFixupOrfName(name)) /* See if ORF, and if so make nice. */
    {
    if (c2gSnof == NULL)
        {
        sprintf(fileName, "%sc2g", featDir);
        c2gSnof = snofMustOpen(fileName);
        sprintf(fileName, "%sc2g", featDir);
        c2gFile = mustOpen(fileName, "rb");
        }
    ok = findAltSpliceRange(name, c2gSnof, c2gFile, retChrom, retStart, retEnd, retStrand);
    }
else    /* Lets say it's a cosmid. */
    {
    char lineBuf[128];
    char *words[3];
    int wordCount;
    touppers(name);
    if (c2cSnof == NULL)
        {
        sprintf(fileName, "%sc2c", featDir);
        c2cSnof = snofMustOpen(fileName);
        sprintf(fileName, "%sc2c", featDir);
        c2cFile = mustOpen(fileName, "rb");
        }
    if (!snofFindOffset(c2cSnof, name, &offset) )
        return FALSE;
    fseek(c2cFile, offset, SEEK_SET);
    mustGetLine(c2cFile, lineBuf, sizeof(lineBuf));
    wordCount = chopLine(lineBuf, words);
    assert(wordCount == 3);
    assert(strcmp(words[2], name) == 0);
    assert(wormIsChromRange(words[0]));
    *retStrand = words[1][0];
    ok = wormParseChromRange(words[0], retChrom, retStart, retEnd);
    }
slFreeList(&syn);
return ok;
}

boolean wormIsNamelessCluster(char *name)
/* Returns true if name is of correct format to be a nameless cluster. */
{
char *e = strrchr(name, '.');
if (e == NULL)
    return FALSE;
if (e[1] != 'N')
    return FALSE;
if (!isdigit(e[2]))
    return FALSE;
return TRUE;
}

DNA *wormGetNamelessClusterDna(char *name)
/* Get DNA associated with nameless cluster */
{
char *chrom;
int start, end;
char strand;
if (!wormGeneRange(name, &chrom, &strand, &start, &end))
    errAbort("Can't find %s in database", name);
return wormChromPart(chrom, start, end-start);
}

struct wormGdfCache wormSangerGdfCache = {&sangerDir,NULL,NULL};
struct wormGdfCache wormGenieGdfCache = {&genieDir,NULL,NULL};
struct wormGdfCache *defaultGdfCache = &wormSangerGdfCache;


static void wormCacheSomeGdf(struct wormGdfCache *cache)
/* Cache one gene prediction set. */
{
if (cache->snof == NULL)
    {
    char fileName[512];
    char *dir;
    bits32 sig;
    getDirs();
    dir = *(cache->pDir);
    sprintf(fileName, "%sgenes", dir);
    cache->snof = snofMustOpen(fileName);
    sprintf(fileName, "%sgenes.gdf", dir);
    cache->file = mustOpen(fileName, "rb");
    mustReadOne(cache->file, sig);
    if (sig != glSig)
        errAbort("%s is not a good file", fileName);
    }
}

#if 0 /* unused */
static void wormCacheGdf()
/* Set up for fast access to GDF file entries. */
{
wormCacheSomeGdf(defaultGdfCache);
}
#endif

void wormUncacheSomeGdf(struct wormGdfCache *cache)
/* Uncache some gene prediction set. */
{
snofClose(&cache->snof);
carefulClose(&cache->file);
}

void wormUncacheGdf()
/* Free up resources associated with fast GDF access. */
{
wormUncacheSomeGdf(defaultGdfCache);
}

struct gdfGene *wormGetSomeGdfGene(char *name, struct wormGdfCache *cache)
/* Get a single gdfGene of given name. */
{
long offset;

wormCacheSomeGdf(cache);
if (!snofFindOffset(cache->snof, name, &offset) )
    return NULL;
fseek(cache->file, offset, SEEK_SET);
return gdfReadOneGene(cache->file);
}

struct gdfGene *wormGetGdfGene(char *name)
/* Get a single gdfGene of given name. */
{
return wormGetSomeGdfGene(name, defaultGdfCache);
}

struct gdfGene *wormGetSomeGdfGeneList(char *baseName, int baseNameSize, struct wormGdfCache *cache)
/* Get all gdfGenes that start with a given name. */
{
int snIx;
int maxIx;
struct snof *snof;
FILE *f;
struct gdfGene *list = NULL, *el;

wormCacheSomeGdf(cache);
snof = cache->snof;
f = cache->file;
if (!snofFindFirstStartingWith(snof, baseName, baseNameSize, &snIx))
    return NULL;

maxIx = snofElementCount(snof);
for (;snIx < maxIx; ++snIx)
    {
    long offset;
    char *geneName;

    snofNameOffsetAtIx(snof, snIx, &geneName, &offset);
    if (strncmp(geneName, baseName, baseNameSize) != 0)
        break;
    fseek(f, offset, SEEK_SET);
    el = gdfReadOneGene(f);
    slAddTail(&list, el);
    }
slReverse(&list);
return list;
}

struct gdfGene *wormGetGdfGeneList(char *baseName, int baseNameSize)
/* Get all gdfGenes that start with a given name. */
{
return wormGetSomeGdfGeneList(baseName, baseNameSize, defaultGdfCache);
}

struct gdfGene *wormGdfGenesInRange(char *chrom, int start, int end, 
    struct wormGdfCache *geneFinder)
/* Get list of genes in range according to given gene finder. */
{
char *dir = NULL;
struct gdfGene *gdfList = NULL, *gdf;
struct wormFeature *nameList, *name;

if (geneFinder == &wormSangerGdfCache)
    dir = wormSangerDir();
else if (geneFinder == &wormGenieGdfCache)
    dir = wormGenieDir();
else
    errAbort("Unknown geneFinder line %d of %s", __LINE__, __FILE__);

nameList = wormSomeGenesInRange(chrom, start, end, dir);
for (name = nameList; name != NULL; name = name->next)
    {
    char *n = name->name;
    if (!wormIsNamelessCluster(n))
        {
        gdf = wormGetSomeGdfGene(n, geneFinder);
        slAddHead(&gdfList, gdf);
        }
    }
slFreeList(&nameList);
slReverse(&gdfList);
return gdfList;
}



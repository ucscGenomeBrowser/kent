/* Handle details pages for maf tracks and axt tracks. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "hgc.h"
#include "maf.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "genePred.h"

/* Javascript to help make a selection from a drop-down
 * go back to the server. */
static char *autoSubmit = "onchange=\"document.gpForm.submit();\"";

struct nameAndLabel
/* Store track name and label. */
   {
   struct nameAndLabel *next;
   char *name;	/* Name (not allocated here) */
   char *label; /* Label (not allocated here) */
   };

static int nameAndLabelCmp(const void *va, const void *vb)
/* Compare to sort on label. */
{
const struct nameAndLabel *a = *((struct nameAndLabel **)va);
const struct nameAndLabel *b = *((struct nameAndLabel **)vb);
return strcmp(a->label, b->label);
}

static char *findLabel(struct nameAndLabel *list, char *label)
/* Try to find label in list. Return NULL if it's
 * not there. */
{
struct nameAndLabel *el;
for (el = list; el != NULL; el = el->next)
    {
    if (sameString(el->label, label))
        return label;
    }
return NULL;
}

static char *genePredDropDown(char *varName)
/* Make gene-prediction drop-down().  Return track name of
 * currently selected one.  Return NULL if no gene tracks. */
{
struct trackDb *curTrack = NULL;
char *cartTrack = cartOptionalString(cart, varName);
struct hashEl *trackList, *trackEl;
char *selectedName = NULL;
struct nameAndLabel *nameList = NULL, *name;
char *trackName = NULL;

/* Make alphabetized list of all genePred track names. */
trackList = hashElListHash(trackHash);
for (trackEl = trackList; trackEl != NULL; trackEl = trackEl->next)
    {
    struct trackDb *tdb = trackEl->val;
    char *dupe = cloneString(tdb->type);
    char *type = firstWordInLine(dupe);
    if (sameString(type, "genePred"))
	{
	AllocVar(name);
	name->name = tdb->tableName;
	name->label = tdb->shortLabel;
	slAddHead(&nameList, name);
	}
    freez(&dupe);
    }
slSort(&nameList, nameAndLabelCmp);

/* No gene tracks - not much we can do. */
if (nameList == NULL)
    {
    slFreeList(&trackList);
    return NULL;
    }

/* Try to find current track - from cart first, then
 * knownGenes, then refGenes. */
if (cartTrack != NULL)
    selectedName = findLabel(nameList, cartTrack);
if (selectedName == NULL)
    selectedName = findLabel(nameList, "Known Genes");
if (selectedName == NULL)
    selectedName = findLabel(nameList, "RefSeq Genes");
if (selectedName == NULL)
    selectedName = nameList->name;

/* Make drop-down list. */
    {
    int nameCount = slCount(nameList);
    char **menu;
    int i;

    AllocArray(menu, nameCount);
    for (name = nameList, i=0; name != NULL; name = name->next, ++i)
	{
	menu[i] = name->label;
	}
    cgiMakeDropListFull(varName, menu, menu, 
    	nameCount, selectedName, autoSubmit);
    freez(&menu);
    }

/* Convert to track name */
for (name = nameList; name != NULL; name = name->next)
    {
    if (sameString(selectedName, name->label))
        trackName = name->name;
    }
        

/* Clean up and return. */
slFreeList(&nameList);
slFreeList(&trackList);
return trackName;
}


static void mafPrettyHeader(FILE *f, struct mafAli *maf)
/* Write out summary. */
{
struct mafComp *mc;
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    char dbOnly[128];
    char *chrom;
    int s = mc->start;
    int e = s + mc->size;
    safef(dbOnly, sizeof(dbOnly), "%s", mc->src);
    chrom = chopPrefix(dbOnly);
    
    /* skip repeats, labeled as "rep" */
    if (sameString("rep", dbOnly))
        continue;    
        
    if (mc->strand == '-')
        reverseIntRange(&s, &e, mc->srcSize);
    fprintf(f, "%s %s (%s) ", 
    	hOrganism(dbOnly), hFreezeFromDb(dbOnly), dbOnly);
    linkToOtherBrowser(dbOnly, chrom, s, e);
    fprintf(f, "%s:%d-%d</A>, strand %c, size %d\n",
    	chrom, s+1, e, mc->strand, mc->size);
    }
}

static void blueCapWrite(FILE *f, char *s, int size)
/* Write with capital letters in blue. */
{
boolean isBlue = FALSE;
int i;
for (i=0; i<size; ++i)
    {
    char c = s[i];
    if (isupper(c))
        {
	if (!isBlue)
	    {
	    fprintf(f, "<FONT COLOR=\"#0000FF\">");
	    isBlue = TRUE;
	    }
	}
    else if (islower(c))
        {
	if (isBlue)
	    {
	    fprintf(f, "</FONT>");
	    isBlue = FALSE;
	    }
	}
    fprintf(f, "%c", c);
    }
if (isBlue)
    fprintf(f, "</FONT>");
}

static void mafPrettyBody(FILE *f, struct mafAli *maf, int lineSize)
/* Print MAF base by base with line-breaks. */
{
int srcChars = 0;
struct mafComp *mc;
int i, lineStart, lineEnd;

/* Figure out length of source field. */
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    int len = strlen(mc->src);
    if (srcChars < len)
        srcChars = len;
    }

for (lineStart = 0; lineStart < maf->textSize; lineStart = lineEnd)
    {
    lineEnd = lineStart + lineSize;
    if (lineEnd >= maf->textSize)
        lineEnd = maf->textSize;
    for (mc = maf->components; mc != NULL; mc = mc->next)
        {
	fprintf(f, "%-*s ", srcChars, mc->src);
	blueCapWrite(f, mc->text + lineStart, lineEnd - lineStart);
	fprintf(f, "\n");
	}
    fprintf(f, "\n");
    }
}



static void mafPrettyOut(FILE *f, struct mafAli *maf, int lineSize)
/* Output MAF in human readable format. */
{
mafPrettyHeader(f, maf);
fprintf(f, "\n");
mafPrettyBody(f, maf, lineSize);
}

static void mafLowerCase(struct mafAli *maf)
/* Lower case letters in maf. */
{
struct mafComp *mc;
for (mc = maf->components; mc != NULL; mc = mc->next)
    tolowers(mc->text);
}

static boolean findAliRange(char *ali, int aliSize, int start, int end,
	int *retStart, int *retEnd)
/* Convert start/end in sequence coordinates to alignment
 * coordinates (that include dashes).  Return FALSE if
 * no intersection. */
{
int i, baseIx=0;
char c;
int rStart = 0, rEnd = 0;

if (start >= end)
    return FALSE;
for (i=0; i<aliSize; ++i)
    {
    c = ali[i];
    if (c != '-')
        {
	if (baseIx == start)
	    rStart = i;
	++baseIx;
	if (baseIx == end)
	    {
	    rEnd = i+1;
	    break;
	    }
	}
    }
if (rStart >= rEnd)
    return FALSE;
*retStart = rStart;
*retEnd = rEnd;
return TRUE;
}

static void capAliRange(char *ali, int aliSize, int start, int end)
/* Capitalize bases between start and end ignoring inserts. */
{
int s, e;

if (findAliRange(ali, aliSize, start, end, &s, &e))
    toUpperN(ali+s, e-s);
}

static void capAliTextOnTrack(struct mafAli *maf,
	char *db, char *chrom, 
	char *track, boolean onlyCds)
/* Capitalize exons in alignment. */
{
int rowOffset;
struct sqlConnection *conn = sqlConnect(db);
struct mafComp *selfMc = maf->components, *mc;
int start = selfMc->start;
int end = start + selfMc->size;
struct sqlResult *sr = hRangeQuery(conn, track, chrom, start, end, 
		NULL, &rowOffset);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = genePredLoad(row+rowOffset);
    int i;
    for (i=0; i<gp->exonCount; ++i)
        {
	int s = gp->exonStarts[i];
	int e = gp->exonEnds[i];
	if (onlyCds)
	    {
	    if (s < gp->cdsStart) s = gp->cdsStart;
	    if (e > gp->cdsEnd) e = gp->cdsEnd;
	    }
	if (s < start) s = start;
	if (e > end) e = end;
	if (findAliRange(selfMc->text, maf->textSize, s-start, e-start, &s, &e))
	    {
	    for (mc = maf->components; mc != NULL; mc = mc->next)
		toUpperN(mc->text + s, e-s);
	    }
	}
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

static void capMafOnTrack(struct mafAli *maf, char *track, boolean onlyCds)
/* Capitalize parts of maf that correspond to exons according
 * to given gene prediction track.  */
{
char dbOnly[64];
char *chrom;
struct mafComp *mc = maf->components;
strncpy(dbOnly, mc->src, sizeof(dbOnly));
chrom = chopPrefix(dbOnly);
capAliTextOnTrack(maf, dbOnly, chrom, track, onlyCds);
}

static struct mafAli *mafOrAxtLoadInRegion(struct sqlConnection *conn, 
	struct trackDb *tdb, char *chrom, int start, int end,
	char *axtOtherDb)
/* Load mafs from region, either from maf or axt file. */
{
if (axtOtherDb != NULL)
    {
    struct hash *qSizeHash = hChromSizeHash(axtOtherDb);
    struct mafAli *mafList = axtLoadAsMafInRegion(conn, tdb->tableName,
    	chrom, start, end,
	database, axtOtherDb, hChromSize(chrom), qSizeHash);
    hashFree(&qSizeHash);
    return mafList;
    }
else
    return mafLoadInRegion(conn, tdb->tableName, chrom, start, end);
}

static char *codeAll[] = {
	"coding",
	"all",
};

static void mafOrAxtClick(struct sqlConnection *conn, struct trackDb *tdb, char *axtOtherDb)
/* Display details for MAF or AXT tracks. */
{
if (winEnd - winStart > 30000)
    {
    printf("Zoom so that window is 30,000 bases or less to see base-by-base alignments\n");
    }
else
    {
    struct mafAli *mafList, *maf, *subList = NULL;
    int aliIx = 0, realCount = 0;
    char dbChrom[64];
    char *capTrack;

    mafList = mafOrAxtLoadInRegion(conn, tdb, seqName, winStart, winEnd, 
    	axtOtherDb);
    safef(dbChrom, sizeof(dbChrom), "%s.%s", database, seqName);
    for (maf = mafList; maf != NULL; maf = maf->next)
        {
	struct mafAli *subset = mafSubset(maf, dbChrom, winStart, winEnd);
	if (subset != NULL)
	    {
	    struct mafComp *mc;
	    /* Reformat MAF if needed so that sequence from current
	     * database is the first component and on the
	     * plus strand. */
	    mafMoveComponentToTop(subset, dbChrom);
	    if (subset->components->strand == '-')
		mafFlipStrand(subset);
	    subset->score = mafScoreMultiz(subset);
	    slAddHead(&subList, subset);
	    ++realCount;
	    }
	}
    slReverse(&subList);
    mafAliFreeList(&mafList);
    if (subList != NULL)
	{
	char *codeVarName = "hgc.multiCapCoding";
	char *codeVarVal = cartUsualString(cart, codeVarName, "coding");
	boolean onlyCds = sameWord(codeVarVal, "coding");
	puts("<FORM ACTION=\"/cgi-bin/hgc\" NAME=\"gpForm\" METHOD=\"GET\">");
	cartSaveSession(cart);
	cgiContinueHiddenVar("g");
	printf("Capitalize ");
	cgiMakeDropListFull(codeVarName, codeAll, codeAll, 
	    ArraySize(codeAll), codeVarVal, autoSubmit);
	printf("exons based on: ");
	capTrack = genePredDropDown("hgc.multiCapTrack");
	printf("<BR>\n");
	printf("</FORM>\n");
	printf("<TT><PRE>");
	for (maf = subList; maf != NULL; maf = maf->next)
	    {
	    mafLowerCase(maf);
	    if (capTrack != NULL)
	    	capMafOnTrack(maf, capTrack, onlyCds);
	    printf("<B>Alignment %d of %d in window, score %0.1f</B>\n",
		    ++aliIx, realCount, maf->score);
	    mafPrettyOut(stdout, maf, 70);
	    }
	mafAliFreeList(&subList);
	}
    else
	{
        printf("No multiple alignment in browser window");
	}
    printf("</PRE></TT>");
    }
}

void genericMafClick(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start)
/* Display details for MAF tracks. */
{
mafOrAxtClick(conn, tdb, NULL); 
}

void genericAxtClick(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start, char *otherDb)
/* Display details for AXT tracks. */
{
mafOrAxtClick(conn, tdb, otherDb); 
}


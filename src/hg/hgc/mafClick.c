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

int nameAndLabelCmp(const void *va, const void *vb)
/* Compare to sort on label. */
{
const struct nameAndLabel *a = *((struct nameAndLabel **)va);
const struct nameAndLabel *b = *((struct nameAndLabel **)vb);
return strcmp(a->label, b->label);
}

char *findLabel(struct nameAndLabel *list, char *label)
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
    if (mc->strand == '-')
        reverseIntRange(&s, &e, mc->srcSize);
    linkToOtherBrowser(dbOnly, chrom, s, e);
    fprintf(f, "%s:%d-%d</A>, strand %c, size %d\n",
    	mc->src, s+1, e, mc->strand, mc->size);
    }
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
	mustWrite(f, mc->text + lineStart, lineEnd - lineStart);
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

static void capAliRange(char *ali, int aliSize, int start, int end)
/* Capitalize bases between start and end ignoring inserts. */
{
int i, baseIx=0;
char c;

if (start >= end)
    return;
for (i=0; i<aliSize; ++i)
    {
    c = ali[i];
    if (c != '-')
        {
	if (baseIx >= start)
	    ali[i] = toupper(c);
	++baseIx;
	if (baseIx >= end)
	    break;
	}
    }
}

static void capAliTextOnTrack(char *ali, int aliSize, 
	char *db, char *chrom, int start, int end, 
	char *track)
/* Capitalize exons in alignment. */
{
int rowOffset;
struct sqlConnection *conn = sqlConnect(db);
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
	if (s < start) s = start;
	if (e > end) e = end;
	capAliRange(ali, aliSize, s - start, e - start);
	}
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

static void capMafOnTrack(struct mafAli *maf, char *track)
/* Capitalize parts of maf that correspond to exons according
 * to given gene prediction track.  */
{
char dbOnly[64];
char *chrom;
struct mafComp *mc;
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    strncpy(dbOnly, mc->src, sizeof(dbOnly));
    chrom = chopPrefix(dbOnly);
    if (sameString(dbOnly, database))
        {
	int start = mc->start;
	int end = start + mc->size;
	if (mc->strand == '-')
	    {
	    reverseIntRange(&start, &end, mc->srcSize);
	    reverseComplement(mc->text, maf->textSize);
	    }
	capAliTextOnTrack(mc->text, maf->textSize, dbOnly, 
		chrom, start, end, track);
	if (mc->strand == '-')
	    {
	    reverseComplement(mc->text, maf->textSize);
	    }
	}
    }
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


static void mafOrAxtClick(struct sqlConnection *conn, struct trackDb *tdb, char *axtOtherDb)
/* Display details for MAF or AXT tracks. */
{
if (winEnd - winStart > 20000)
    {
    printf("Zoom so that window is 20000 bases or less to see base-by-base alignments\n");
    }
else
    {
    struct mafAli *mafList, *maf, *subList = NULL;
    int aliIx = 0, realCount = 0;
    char dbChrom[64];
    struct slName *dbList = NULL, *dbEl;
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
	    subset->score = mafScoreMultiz(subset);
	    mafMoveComponentToTop(subset, dbChrom);
	    slAddHead(&subList, subset);

	    /* Get a list of all databases used in any maf. */
	    for (mc = subset->components; mc != NULL; mc = mc->next)
	        {
		char dbOnly[64];
		strncpy(dbOnly, mc->src, sizeof(dbOnly));
		chopPrefix(dbOnly);
		slNameStore(&dbList, dbOnly);
		}
	    ++realCount;
	    }
	}
    slReverse(&subList);
    slReverse(&dbList);
    mafAliFreeList(&mafList);
    if (subList != NULL)
	{
	printf("Alignments between");
	for (dbEl = dbList; dbEl != NULL; dbEl = dbEl->next)
	    {
	    char *org = hOrganism(dbEl->name);
	    tolowers(org);
	    if (dbEl != dbList)
	       {
	       if (dbEl->next == NULL)
		   printf(" and");
	       else
		   printf(",");
	       }
	    printf(" %s", org);
	    freez(&org);
	    }
	printf(".  The versions of each genome used are:");
	printf("<UL>\n");
	for (dbEl = dbList; dbEl != NULL; dbEl = dbEl->next)
	    {
	    char *db = dbEl->name;
	    char *org = hOrganism(db);
	    char *freeze = hFreezeFromDb(db);
	    printf("<LI><B>%s</B> - %s (%s)", org, freeze, db);
	    freez(&org);
	    freez(&freeze);
	    }
	printf("</UL>\n");
	puts("<FORM ACTION=\"/cgi-bin/hgc\" NAME=\"gpForm\" METHOD=\"GET\">");
	cartSaveSession(cart);
	cgiContinueHiddenVar("g");
	printf("Capitalize exons based on: ");
	capTrack = genePredDropDown("hgc.multiCapTrack");
	printf("<BR>\n");
	printf("</FORM>\n");
	printf("<TT><PRE>");
	for (maf = subList; maf != NULL; maf = maf->next)
	    {
	    mafLowerCase(maf);
	    if (capTrack != NULL)
	    	capMafOnTrack(maf, capTrack);
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


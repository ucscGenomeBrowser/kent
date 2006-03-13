/* Handle details pages for maf tracks and axt tracks. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "hgc.h"
#include "maf.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "genePred.h"
#include "botDelay.h"
#include "hgMaf.h"
#include "hui.h"
#include "hCommon.h"

static char const rcsid[] = "$Id: mafClick.c,v 1.35 2006/03/13 23:22:26 braney Exp $";

#define ADDEXONCAPITAL

/* Javascript to help make a selection from a drop-down
 * go back to the server. */
static char *autoSubmit = "onchange=\"document.gpForm.submit();\"";

static void blueCapWrite(FILE *f, char *s, int size, char *r)
/* Write capital letters in blue. */
{
boolean isBlue = FALSE;
int i;
for (i=0; i<size; ++i)
    {
      if (r!=NULL && s[i]==r[i])
	fprintf(f, ".");
      else
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
    }

if (isBlue)
    fprintf(f, "</FONT>");
}

void initSummaryLine(char *summaryLine, int size, int val)
/* Fill summary line with stars and null terminate */
{
int i;
for (i = 0; i < size; i++)
    summaryLine[i] = val;
summaryLine[i] = 0;
}

void updateSummaryLine(char *summaryLine, char *referenceText,
                                        char *alignText, int size)
/* Blank out columns in the summary line where this alignment
 * differs from the reference */
{
int i;
for (i=0; i<size; i++)
    {
    if (alignText[i] != referenceText[i])
        summaryLine[i] = ' ';
    }
}

static void mafPrettyOut(FILE *f, struct mafAli *maf, int lineSize, 
	boolean onlyDiff, int blockNo)
{
int ii, ch;
int srcChars = 0;
struct mafComp *mc;
int lineStart, lineEnd;
char *summaryLine = needMem(lineSize+1);
char *referenceText;
int startChars, sizeChars, srcSizeChars;
boolean haveInserts = FALSE;
struct mafComp *masterMc = maf->components;

startChars = sizeChars = srcSizeChars = 0;

for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    /* Figure out length of source (species) field. */
    /*if (mc->size != 0)*/
	{
	char dbOnly[128];
	int len;
	char *chrom, *org;

	memset(dbOnly, 0, sizeof(dbOnly));
	safef(dbOnly, sizeof(dbOnly), "%s", mc->src);
	chrom = chopPrefix(dbOnly);

	if ((org = hOrganism(dbOnly)) == NULL)
	    len = strlen(dbOnly);
	else
	    len = strlen(org);
	if (srcChars < len)
	    srcChars = len;

	len = digitsBaseTen(mc->start);
	if (startChars < len)
	    startChars = len;
	len = digitsBaseTen(mc->size);
	if (sizeChars < len)
	    sizeChars = len;
	len = digitsBaseTen(mc->srcSize);
	if (srcSizeChars < len)
	    srcSizeChars = len;

	if (mc->text && (mc->rightStatus == MAF_INSERT_STATUS) && (masterMc->start + masterMc->size < winEnd))
	    haveInserts = TRUE;

#ifdef REVERSESTRAND
	/* complement bases if hgTracks is on reverse strand */
	if (mc->size && cartCgiUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE))
	    complement(mc->text, maf->textSize);
#endif
	}
    }
/* first sequence in the alignment */
referenceText = maf->components->text;

for (lineStart = 0; lineStart < maf->textSize; lineStart = lineEnd)
    {
    int size;
    lineEnd = lineStart + lineSize;
    if (lineEnd >= maf->textSize)
        lineEnd = maf->textSize;
    size = lineEnd - lineStart;
    initSummaryLine(summaryLine, size, '*');
    for (mc = maf->components; mc != NULL; mc = mc->next)
        {
	char dbOnly[128], *chrom;
	int s = mc->start;
	int e = s + mc->size;
	char *org;
	char *revComp = "";
	char strand = mc->strand;
	struct dyString *dy = newDyString(512);
#ifdef REVERSESTRAND
	if (cartCgiUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE))
	    strand = (strand == '+') ? '-' : '+';
#endif
	if (strand == '-') revComp = "&hgSeq.revComp=on";

	dyStringClear(dy);

	safef(dbOnly, sizeof(dbOnly), "%s", mc->src);
	chrom = chopPrefix(dbOnly);
	if ((org = hOrganism(dbOnly)) == NULL)
	    org = dbOnly;
	
	if (mc->strand == '-')
	    reverseIntRange(&s, &e, mc->srcSize);


	if (mc->text != NULL)
	    {
	    if ((lineStart == 0) && (hDbIsActive(dbOnly)))
		{
		dyStringPrintf(dy, "%s Browser %s:%d-%d %c %*dbps",hOrganism(dbOnly),chrom, s+1, e, mc->strand,sizeChars, mc->size);
		linkToOtherBrowserTitle(dbOnly, chrom, s, e, dy->string);
		dyStringClear(dy);
		dyStringPrintf(dy, "Get %s DNA %s:%d-%d %c %*dbps",hOrganism(dbOnly),chrom, s+1, e, mc->strand,sizeChars, mc->size);
		fprintf(f, "B</A> ");
		printf("<A TITLE=\"%s\" TARGET=\"_blank\" HREF=\"%s?o=%d&g=getDna&i=%s&c=%s&l=%d&r=%d&db=%s%s\">D</A> ",  dy->string,hgcName(),
		   s, cgiEncode(chrom),
		   chrom, s, e, dbOnly, revComp);
		}
	    else
		{
		fprintf(f, "    ");
		}

	    dyStringClear(dy);
	    dyStringPrintf(dy, "%s:%d-%d %c %*dbps",chrom, s+1, e, mc->strand,sizeChars, mc->size);
	    fprintf(f, "<A TITLE=\"%s\"> %*s </A> ", dy->string, srcChars, org);

	    updateSummaryLine(summaryLine, referenceText + lineStart, 
				    mc->text + lineStart, size);
	    blueCapWrite(f, mc->text + lineStart, size, 
			 (onlyDiff && mc != maf->components) ? referenceText + lineStart : NULL);
	    fprintf(f, "\n");
	    }
	else
	    {
	    if (((mc->leftStatus == MAF_CONTIG_STATUS) && (mc->rightStatus == MAF_CONTIG_STATUS) )
	    || ((mc->leftStatus == MAF_INSERT_STATUS) && (mc->rightStatus == MAF_INSERT_STATUS) )
	    || ((mc->leftStatus == MAF_MISSING_STATUS) && (mc->rightStatus == MAF_MISSING_STATUS) ))
		{
		if ((lineStart == 0) && (hDbIsActive(dbOnly)))
		    {
		    int s = mc->start;
		    int e = s + mc->rightLen;
		    struct dyString *dy = newDyString(512);

		    if (mc->strand == '-')
			reverseIntRange(&s, &e, mc->srcSize);

		    dyStringPrintf(dy, "%s Browser %s:%d-%d %c %d bps Unaligned",hOrganism(dbOnly),chrom, s+1, e, mc->strand, e-s);
		    linkToOtherBrowserTitle(dbOnly, chrom, s, e, dy->string);
		    
		    dyStringClear(dy);
		    dyStringPrintf(dy, "Get %s DNA %s:%d-%d %c %d bps Unaligned",hOrganism(dbOnly),chrom, s+1, e, mc->strand, e-s);
		    fprintf(f,"B</A> ");

		    printf("<A TITLE=\"%s\" TARGET=\"_blank\" HREF=\"%s?o=%d&g=getDna&i=%s&c=%s&l=%d&r=%d&db=%s%s\">D</A>  ", dy->string,  hgcName(),
		       s, cgiEncode(chrom),
		       chrom, s, e, dbOnly,revComp);
		    }
		else
		    fprintf(f, "     ");
		initSummaryLine(summaryLine, size, ' ');
		dyStringClear(dy);
		dyStringPrintf(dy, "%s:%d-%d %c %*dbps",chrom, s+1, e, mc->strand,sizeChars, mc->size);
		fprintf(f, "<A TITLE=\"%s\">%*s</A>  ", dy->string, srcChars, org);
		ch = '-';
		switch(mc->rightStatus)
		    {
		    case MAF_INSERT_STATUS:
			ch = '=';
			break;
		    case MAF_MISSING_STATUS:
			ch = 'N';
			break;
		    case MAF_CONTIG_STATUS:
			ch = '-';
			break;
		    }
		for(ii=lineStart; ii < lineEnd ; ii++)
		    fputc(ch,f);
		fprintf(f,"\n");
		}
	    }
	}
#ifdef ADDMATCHLINE
    if (lineStart == 0)
	fprintf(f, "    %-*s %s\n", srcChars, "", summaryLine);
    else
	fprintf(f, "%-*s %s\n", srcChars, "", summaryLine);
#else
    fprintf(f, "\n");
#endif
    }

if (haveInserts)
    {
    fprintf(f, "<B>Inserts between block %d and %d in window</B>\n",blockNo, blockNo+1);
    for (mc = maf->components; mc != NULL; mc = mc->next)
	{
	char dbOnly[128], *chrom;
	int s = mc->start + mc->size;
	int e = s + mc->rightLen;
	char *org;

	if (mc->text == NULL)
	    continue;

	if (mc->strand == '-')
	    reverseIntRange(&s, &e, mc->srcSize);

	safef(dbOnly, sizeof(dbOnly), "%s", mc->src);
	chrom = chopPrefix(dbOnly);

	if ((org = hOrganism(dbOnly)) == NULL)
	    org = dbOnly;

	if (mc->rightStatus == MAF_INSERT_STATUS)
	    {
	    if (hDbIsActive(dbOnly))
		{
		char *revComp = "";
		char strand = mc->strand;
#ifdef REVERSESTRAND
		if (cartCgiUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE))
		    strand = (strand == '+') ? '-' : '+';
#endif
		if (strand == '-') revComp = "&hgSeq.revComp=on";

		linkToOtherBrowser(dbOnly, chrom, s, e);
		fprintf(f,"B");
		fprintf(f, "</A>");
		fprintf(f, " ");

		printf("<A TARGET=\"_blank\" HREF=\"%s?o=%d&g=getDna&i=%s&c=%s&l=%d&r=%d&db=%s%s\">D</A> ",  hgcName(),
		   s, cgiEncode(chrom),
		   chrom,  s, e, dbOnly,revComp);
		}
	    else
		fprintf(f, "    ");

	    fprintf(f, "%*s %dbp\n", srcChars, org,mc->rightLen);
	    }
	}
    fprintf(f, "\n");
    }
freeMem(summaryLine);

}

static void mafLowerCase(struct mafAli *maf)
/* Lower case letters in maf. */
{
struct mafComp *mc;
for (mc = maf->components; mc != NULL; mc = mc->next)
    if (mc->size != 0)
	tolowers(mc->text);
}

#ifdef ADDEXONCAPITAL
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
		if (mc->text)
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
#endif

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

#ifdef ADDEXONCAPITAL
static char *codeAll[] = {
	"coding",
	"all",
};
#endif

static char *showAll[] = {
	"all",
	"diff",
};

static void mafOrAxtClick(struct sqlConnection *conn, struct trackDb *tdb, char *axtOtherDb)
/* Display details for MAF or AXT tracks. */
{
hgBotDelay();
if (winEnd - winStart > 30000)
    {
    printf("Zoom so that window is 30,000 bases or less to see base-by-base alignments\n");
    }
else
    {
    struct mafAli *mafList, *maf, *subList = NULL;
    int aliIx = 0, realCount = 0;
    char dbChrom[64];
    char option[128];
    char *capTrack;
    char *wigTable = trackDbSetting(tdb, "wiggle");
    struct hash *speciesOffHash = NULL;
    char *speciesOrder = NULL;
    char *speciesTarget = trackDbSetting(tdb, SPECIES_TARGET_VAR);
    char buffer[1024];
    int useTarg = FALSE;
    int useIrowChains = FALSE;

    safef(option, sizeof(option), "%s.%s", tdb->tableName, MAF_CHAIN_VAR);
    if (cartCgiUsualBoolean(cart, option, FALSE) && 
	trackDbSetting(tdb, "irows") != NULL)
	    useIrowChains = TRUE;

    safef(buffer, sizeof(buffer), "%s.vis",tdb->tableName);
    if (useIrowChains)
	{
	if (!cartVarExists(cart, buffer) && (speciesTarget != NULL))
	    useTarg = TRUE;
	else
	    {
	    char *val;

	    val = cartUsualString(cart, buffer, "useCheck");
	    useTarg = sameString("useTarg",val);
	    }
	}

    mafList = mafOrAxtLoadInRegion(conn, tdb, seqName, winStart, winEnd, 
    	axtOtherDb);
    safef(dbChrom, sizeof(dbChrom), "%s.%s", database, seqName);
    
    safef(option, sizeof(option), "%s.speciesOrder", tdb->tableName);
    speciesOrder = cartUsualString(cart, option, NULL);
    if (speciesOrder == NULL)
	speciesOrder = trackDbSetting(tdb, "speciesOrder");

    for (maf = mafList; maf != NULL; maf = maf->next)
        {
        int mcCount = 0;
        struct mafComp *mc;
        struct mafAli *subset;
        struct mafComp *nextMc;

        /* remove empty components and configured off components
         * from MAF, and ignore
         * the entire MAF if all components are empty 
         * (solely for gap annotation) */

	if (!useTarg)
	    {
	    for (mc = maf->components->next; mc != NULL; mc = nextMc)
		{
		char buf[64];
                char *organism;
		mafSrcDb(mc->src, buf, sizeof buf);
                organism = hOrganism(buf);
                if (!organism)
                    organism = buf;
		nextMc = mc->next;
		safef(option, sizeof(option), "%s.%s", tdb->tableName, buf);
		if (!cartUsualBoolean(cart, option, TRUE))
		    {
		    if (speciesOffHash == NULL)
			speciesOffHash = newHash(4);
		    hashStoreName(speciesOffHash, organism);
		    }
		if (!cartUsualBoolean(cart, option, TRUE))
		    slRemoveEl(&maf->components, mc);
		else
		    mcCount++;
		}
	    }
        if (mcCount == 0)
            continue;

	if (speciesOrder)
	    {
	    int speciesCt;
	    char *species[256];
	    struct mafComp **newOrder, *mcThis;
	    int i;

	    mcCount = 0;
	    speciesCt = chopLine(cloneString(speciesOrder), species);
	    newOrder = needMem((speciesCt + 1) * sizeof (struct mafComp *));
	    newOrder[mcCount++] = maf->components;

	    for (i = 0; i < speciesCt; i++)
		{
		if ((mcThis = mafMayFindCompPrefix(maf, species[i], "")) == NULL)
		    continue;
		newOrder[mcCount++] = mcThis;
		}

	    maf->components = NULL;
	    for (i = 0; i < mcCount; i++)
		{
		newOrder[i]->next = 0;
		slAddHead(&maf->components, newOrder[i]);
		}

	    slReverse(&maf->components);
	    }
	subset = mafSubset(maf, dbChrom, winStart, winEnd);
	if (subset != NULL)
	    {
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
	char *showVarName = "hgc.showMultiBase";
	char *showVarVal = cartUsualString(cart, showVarName, "all");
	boolean onlyDiff = sameWord(showVarVal, "diff");
#ifdef ADDEXONCAPITAL
	char *codeVarName = "hgc.multiCapCoding";
	char *codeVarVal = cartUsualString(cart, codeVarName, "coding");
	boolean onlyCds = sameWord(codeVarVal, "coding");
#endif

	if (wigTable)
	    {
	    char *chrom = cartCgiUsualString(cart, "c", "chr7");
	    char other[128];
	    safef(other, ArraySize(other), "%d", winStart);
	    printf("<P><B>See also: </B>");
	    printf("<A HREF=\"%s&g=%s&i=%s&c=%s&l=%d&r=%d&o=%s&db=%s&parentWigMaf=%s\" TARGET=\"_blank\">",
		hgcPathAndSettings(), wigTable, wigTable, chrom, winStart,
			winEnd, other, database, tdb->tableName);
	    printf("Alignment score statistics</A></P>\n");
	    }

#ifdef ADDEXONCAPITAL
	puts("<FORM ACTION=\"/cgi-bin/hgc\" NAME=\"gpForm\" METHOD=\"GET\">");
	cartSaveSession(cart);
	cgiContinueHiddenVar("g");
	cgiContinueHiddenVar("c");
	printf("Capitalize ");
	cgiMakeDropListFull(codeVarName, codeAll, codeAll, 
	    ArraySize(codeAll), codeVarVal, autoSubmit);
	printf("exons based on ");
	capTrack = genePredDropDown(cart, trackHash, 
                                        "gpForm", "hgc.multiCapTrack");
#endif
	printf("show ");
	cgiMakeDropListFull(showVarName, showAll, showAll, 
	    ArraySize(showAll), showVarVal, autoSubmit);
	printf("bases");
	printf("<BR>\n");
	printf("</FORM>\n");

#ifdef REVERSESTRAND
        /* notify if bases are complemented (hgTracks is on reverse strand) */
        if (cartCgiUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE))
            puts("<EM>Alignment displayed on reverse strand</EM><BR>");
#endif
	puts("Place cursor over species for alignment detail. Click on 'B' to link to browser ");
	puts("for aligned species, click on 'D' to get DNA for aligned species.<BR>");

	printf("<TT><PRE>");

        /* notify if species removed from alignment */
        if (speciesOffHash)
            {
            char *species;
            struct hashCookie hc = hashFirst(speciesOffHash);
            puts("<B>Components not displayed:</B> ");
            while ((species = hashNextName(&hc)) != NULL)
                printf("%s ", species);
            puts("<BR>");
            }


	for (maf = subList; maf != NULL; maf = maf->next)
	    {
	    mafLowerCase(maf);
#ifdef ADDEXONCAPITAL
	    if (capTrack != NULL)
	    	capMafOnTrack(maf, capTrack, onlyCds);
#endif
	    printf("<B>Alignment block %d of %d in window, %d - %d, %d bps </B>\n",
		++aliIx,realCount,maf->components->start + 1,maf->components->start + maf->components->size, 
		maf->components->size);
	    mafPrettyOut(stdout, maf, 70,onlyDiff, aliIx);
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


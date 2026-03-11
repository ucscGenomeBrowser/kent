/* Handle details pages for maf tracks and axt tracks. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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
#include "hubConnect.h"
#include "trackHub.h"
#include "chromAlias.h"

extern boolean issueBotWarning;

#define ADDEXONCAPITAL

/* Javascript to help make a selection from a drop-down
 * go back to the server. */
static char *autoSubmit = "document.gpForm.submit();";

static void blueCapWrite(FILE *f, char *s, int size, char *r)
/* Write capital letters in blue. */
{
boolean isBlue = FALSE;
int i;
for (i=0; i<size; ++i)
    {
    if (r!=NULL && s[i]==r[i])
	fprintf(f, " ");
    else
	{
	char c = s[i];
	if (isupper(c))
            {
            if (!isBlue)
                {
                fprintf(f, "<span style='color:#0000FF;'>");
                isBlue = TRUE;
                }
            }
	else if (islower(c))
            {
            if (isBlue)
                {
                fprintf(f, "</span>");
                isBlue = FALSE;
                }
            }
	fprintf(f, "%c", c);
	}
    }

if (isBlue)
    fprintf(f, "</span>");
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

void mafPrettyOut(FILE *f, struct mafAli *maf, int lineSize,
                  boolean onlyDiff, int blockNo, struct hash *labelHash)
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
	char *org;

	memset(dbOnly, 0, sizeof(dbOnly));
	safef(dbOnly, sizeof(dbOnly), "%s", mc->src);
	chopPrefix(dbOnly);

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
	struct dyString *dy = dyStringNew(512);
#ifdef REVERSESTRAND
	if (cartCgiUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE))
	    strand = (strand == '+') ? '-' : '+';
#endif
	if (strand == '-') revComp = "&hgSeq.revComp=on";

	dyStringClear(dy);

	safef(dbOnly, sizeof(dbOnly), "%s", mc->src);
	chrom = chopPrefix(dbOnly);
        if ((labelHash == NULL) || ((org = hashFindVal(labelHash, dbOnly)) == NULL))
            {
            if ((org = hOrganism(dbOnly)) == NULL)
                org = dbOnly;
            }

	if (mc->strand == '-')
	    reverseIntRange(&s, &e, mc->srcSize);


	if (mc->text != NULL)
	    {
            if (lineStart == 0)
		{
		if (hDbIsActive(dbOnly))
		    {
		    dyStringPrintf(dy, "%s Browser %s:%d-%d %c %*dbps",hOrganism(dbOnly),chrom, s+1, e, mc->strand,sizeChars, mc->size);
		    linkToOtherBrowserTitle(dbOnly, chrom, s, e, dy->string);
		    dyStringClear(dy);
		    fprintf(f, "B</A> ");
		    }
		else
		    fprintf(f, "  ");

                if (hDbExists(dbOnly))
                    {
                    dyStringPrintf(dy, "Get %s DNA %s:%d-%d %c %*dbps",hOrganism(dbOnly),chrom, s+1, e, mc->strand,sizeChars, mc->size);
                    printf("<A TITLE=\"%s\" TARGET=\"_blank\" HREF=\"%s?o=%d&g=getDna&i=%s&c=%s&l=%d&r=%d&db=%s%s\">D</A> ",  dy->string,hgcName(),
                       s, cgiEncode(chrom),
                       chrom, s, e, dbOnly, revComp);
                    }
                else
                    fprintf(f, "  ");
                }
            else
                {
                fprintf(f, "    ");
                }

            dyStringClear(dy);
            dyStringPrintf(dy, "%s:%d-%d %c %*dbps",chrom, s+1, e, mc->strand,sizeChars, mc->size);
            fprintf(f, "<A TITLE=\"%s\"> %*s </A> ", dy->string, srcChars, org);

            updateSummaryLine(summaryLine, referenceText + lineStart, mc->text + lineStart, size);
            blueCapWrite(f, mc->text + lineStart, size,
			 (onlyDiff && mc != maf->components) ? referenceText + lineStart : NULL);
	    fprintf(f, "\n");
	    }
	else
	    {
	    if (((mc->leftStatus == MAF_CONTIG_STATUS) && (mc->rightStatus == MAF_CONTIG_STATUS) )
	    || ((mc->leftStatus == MAF_TANDEM_STATUS) && (mc->rightStatus == MAF_TANDEM_STATUS) )
	    || ((mc->leftStatus == MAF_INSERT_STATUS) && (mc->rightStatus == MAF_INSERT_STATUS) )
	    || ((mc->leftStatus == MAF_MISSING_STATUS) && (mc->rightStatus == MAF_MISSING_STATUS) ))
		{
                if (lineStart == 0)
		    {
		    int s = mc->start;
		    int e = s + mc->rightLen;
		    struct dyString *dy = dyStringNew(512);

		    if (mc->strand == '-')
			reverseIntRange(&s, &e, mc->srcSize);

		    if ( hDbIsActive(dbOnly))
			{
			dyStringPrintf(dy, "%s Browser %s:%d-%d %c %d bps Unaligned",hOrganism(dbOnly),chrom, s+1, e, mc->strand, e-s);
			linkToOtherBrowserTitle(dbOnly, chrom, s, e, dy->string);

			fprintf(f,"B</A> ");
			dyStringClear(dy);
			}
		    else
			fprintf(f,"  ");

                    if (hDbExists(dbOnly))
                        {
                        dyStringPrintf(dy, "Get %s DNA %s:%d-%d %c %d bps Unaligned",hOrganism(dbOnly),chrom, s+1, e, mc->strand, e-s);

                        printf("<A TITLE=\"%s\" TARGET=\"_blank\" HREF=\"%s?o=%d&g=getDna&i=%s&c=%s&l=%d&r=%d&db=%s%s\">D</A>  ", dy->string,  hgcName(),
                           s, cgiEncode(chrom),
                           chrom, s, e, dbOnly,revComp);
                        }
                    else
                        fprintf(f, "  ");
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
		    case MAF_TANDEM_STATUS:
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

        if ((labelHash == NULL) || ((org = hashFindVal(labelHash, dbOnly)) == NULL))
            {
            if ((org = hOrganism(dbOnly)) == NULL)
                org = dbOnly;
            }

	if (mc->rightStatus == MAF_INSERT_STATUS)
	    {
	    char *revComp = "";
	    if (hDbIsActive(dbOnly))
		{
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

		}
	    else
		fprintf(f, "  ");

            if (hDbExists(dbOnly))
                {
                printf("<A TARGET=\"_blank\" HREF=\"%s?o=%d&g=getDna&i=%s&c=%s&l=%d&r=%d"
                       "&db=%s%s\">D</A> ",  hgcName(), s, cgiEncode(chrom), chrom,  s,
                       e, dbOnly,revComp);
                }
            else
		fprintf(f, "  ");
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

static void mafStripRefGaps(struct mafAli *maf)
/* Remove columns where the reference (first component) has a gap character.
 * These are insertions in non-reference species that should be collapsed
 * when displaying in reference coordinates. */
{
struct mafComp *mc;
struct mafComp *ref = maf->components;
if (ref == NULL || ref->text == NULL)
    return;
int textSize = maf->textSize;

/* Build boolean array of columns to keep (where ref is not a gap) */
bool *keep = needMem(textSize);
int newSize = 0;
int ii;
for (ii = 0; ii < textSize; ii++)
    {
    if (ref->text[ii] != '-')
        {
        keep[ii] = TRUE;
        newSize++;
        }
    }

if (newSize == textSize)
    {
    freeMem(keep);
    return;  /* nothing to strip */
    }

/* Compact all component texts in place */
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    if (mc->text == NULL)
        continue;
    int jj = 0;
    for (ii = 0; ii < textSize; ii++)
        if (keep[ii])
            mc->text[jj++] = mc->text[ii];
    mc->text[jj] = '\0';
    }
maf->textSize = newSize;
freeMem(keep);
}

static struct mafAli *mafOrAxtLoadInRegion2(struct sqlConnection *conn,struct sqlConnection *conn2,
                                            struct trackDb *tdb, char *chrom, int start, int end,
                                            char *axtOtherDb, char *file)
{
if (axtOtherDb != NULL)
    {
    struct hash *qSizeHash = hChromSizeHash(axtOtherDb);
    struct mafAli *mafList = axtLoadAsMafInRegion(conn, tdb->table,
            chrom, start, end, database, axtOtherDb, hChromSize(database, chrom), qSizeHash);
    hashFree(&qSizeHash);
    return mafList;
    }
else
    return mafLoadInRegion2(conn, conn2, tdb->table, chrom,
	start, end, file);
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

static void conservationStatsLink(struct trackDb *tdb,
                                    char *label, char *table)
/* write link that to display statistics of phastCons table */
{
char *chrom = cartCgiUsualString(cart, "c", "chr7");
printf("<A HREF=\"%s&g=%s&i=%s&c=%s&l=%d&r=%d&o=%d&db=%s"
        "&parentWigMaf=%s\" TARGET=\"_blank\">%s</A>",
        hgcPathAndSettings(), table, table, chrom,
winStart, winEnd, winStart, database, tdb->track, label);
}

static void mafOrAxtClick2(struct sqlConnection *conn, struct sqlConnection *conn2, struct trackDb *tdb, char *axtOtherDb, char *fileName)
/* Display details for MAF or AXT tracks. */
{
if (issueBotWarning)
    {
    char *ip = getenv("REMOTE_ADDR");
    botDelayMessage(ip, botDelayMillis);
    }
if (winEnd - winStart > 30000)
    {
    printf("Zoom so that window is 30,000 bases or less to see alignments and conservation statistics\n");
    }
else
    {
    struct mafAli *maf = NULL;
    char option[128];
    char *capTrack;
    struct consWiggle *consWig, *consWiggles;
    struct hash *speciesOffHash = NULL;
    char *speciesOrder = NULL;
    char *speciesTarget = trackDbSetting(tdb, SPECIES_TARGET_VAR);
    char buffer[1024];
    int useTarg = FALSE;
    int useIrowChains = FALSE;
    struct hash *labelHash = mafGetLabelHash(tdb);
    struct slName *orderList = NULL;

    safef(option, sizeof(option), "%s.%s", tdb->track, MAF_CHAIN_VAR);
    if (cartCgiUsualBoolean(cart, option, FALSE) &&
	trackDbSetting(tdb, "irows") != NULL)
	    useIrowChains = TRUE;

    safef(buffer, sizeof(buffer), "%s.vis",tdb->track);
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

    /* Determine species order from trackDb settings, matching hgTracks logic */
    char *speciesGroup = trackDbSetting(tdb, SPECIES_GROUP_VAR);
    char *speciesUseFile = trackDbSetting(tdb, SPECIES_USE_FILE);
    speciesOrder = trackDbSetting(tdb, SPECIES_ORDER_VAR);

    /* Check cart override for speciesOrder */
    safef(option, sizeof(option), "%s.speciesOrder", tdb->track);
    char *cartOrder = cartUsualString(cart, option, NULL);
    if (cartOrder != NULL)
	speciesOrder = cartOrder;

    if (speciesUseFile)
	speciesOrder = cartGetOrderFromFile(database, cart, speciesUseFile);

    /* Build hash of species that default to off */
    char *speciesOff = trackDbSetting(tdb, SPECIES_DEFAULT_OFF_VAR);
    struct hash *defaultOffHash = NULL;
    if (speciesOff)
        {
        char *offSpecies[2048];
        int offCt = chopLine(cloneString(speciesOff), offSpecies);
        defaultOffHash = newHash(5);
        int ii;
        for (ii = 0; ii < offCt; ii++)
            hashAdd(defaultOffHash, offSpecies[ii], NULL);
        }

    /* Build orderList of selected species for mafFrag, matching
     * newSpeciesItems() logic in wigMafTrack.c */
    if (speciesOrder || speciesGroup)
        {
        char *groups[1000];
        char sGroup[2048];
        int groupCt = 1;
        int group;

        if (speciesGroup)
            groupCt = chopLine(cloneString(speciesGroup), groups);

        /* Add reference database as first in list */
        slNameAddHead(&orderList, database);

        for (group = 0; group < groupCt; group++)
            {
            char *species[2048];
            int speciesCt;
            if (groupCt != 1 || !speciesOrder)
                {
                safef(sGroup, sizeof sGroup, "%s%s",
                                        SPECIES_GROUP_PREFIX, groups[group]);
                speciesOrder = trackDbRequiredSetting(tdb, sGroup);
                }
            speciesCt = chopLine(cloneString(speciesOrder), species);

            int ii;
            for (ii = 0; ii < speciesCt; ii++)
                {
                boolean defaultOn = (defaultOffHash == NULL
                    || hashLookup(defaultOffHash, species[ii]) == NULL);
                if (useTarg || cartUsualBooleanClosestToHome(cart, tdb,
                                            FALSE, species[ii], defaultOn))
                    {
                    slNameAddTail(&orderList, species[ii]);
                    }
                else
                    {
                    if (speciesOffHash == NULL)
                        speciesOffHash = newHash(4);
                    char *organism = hOrganism(species[ii]);
                    if (!organism)
                        organism = species[ii];
                    hashStoreName(speciesOffHash, organism);
                    }
                }
            }
        }

    /* Load stitched alignment using mafFrag approach */
    if (sameString(tdb->type, "bigMaf"))
        {
        char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
        struct bbiFile *bbi = bigBedFileOpenAlias(bigDataUrl, chromAliasFindAliases);
        maf = hgBigMafFrag(database, bbi, seqName, winStart, winEnd, '+', NULL, orderList);
        bbiFileClose(&bbi);
        }
    else if (axtOtherDb == NULL && fileName == NULL)
        {
        /* Regular MAF from database */
        maf = hgMafFrag(database, tdb->table, seqName, winStart, winEnd, '+', NULL, orderList);
        }
    else
        {
        /* AXT or MAF with external file - load blocks, then stitch */
        struct mafAli *mafList = mafOrAxtLoadInRegion2(conn, conn2, tdb, seqName,
                                    winStart, winEnd, axtOtherDb, fileName);
        maf = hgMafFragFromMafList(database, seqName, winStart, winEnd, '+',
                                    mafList, NULL, orderList);
        }

    /* Remove insertion columns (where reference has gaps) */
    if (maf != NULL)
        mafStripRefGaps(maf);

    if (maf != NULL)
	{
	char *showVarName = "hgc.showMultiBase";
	char *showVarVal = cartUsualString(cart, showVarName, "all");
	boolean onlyDiff = sameWord(showVarVal, "diff");
#ifdef ADDEXONCAPITAL
	char *codeVarName = "hgc.multiCapCoding";
	char *codeVarVal = cartUsualString(cart, codeVarName, "coding");
	boolean onlyCds = sameWord(codeVarVal, "coding");
#endif
        /* add links for conservation score statistics */
        consWiggles = wigMafWiggles(database, tdb);
        int wigCount = slCount(consWiggles);
        if (wigCount == 1)
            {
            conservationStatsLink(tdb, "Conservation score statistics", consWiggles->table);
            }
        else if (wigCount > 1)
            {
            /* multiple wiggles. List all that have been turned on with
             * checkboxes */

            /* Scan for cart variables -- do any exist, are any turned on ? */
            boolean wigSet = FALSE;
            boolean wigOn = FALSE;
            for (consWig = consWiggles; consWig != NULL;
                        consWig = consWig->next)
                {
                char *wigVarSuffix = NULL;
                (void)wigMafWiggleVar(tdb->track, consWig, &wigVarSuffix);
                if (cartVarExistsAnyLevel(cart, tdb, FALSE, wigVarSuffix))
                    {
                    wigSet = TRUE;
                    if (cartBooleanClosestToHome(cart, tdb, FALSE, wigVarSuffix))
                        wigOn = TRUE;
                    }
                }
            /* If there are no cart vars, turn on the first (default) wig */
            if (!wigSet)
                {
                char *prefix = tdb->track; // use when setting things to the cart
                if (tdbIsContainerChild(tdb))
                    prefix = tdbGetContainer(tdb)->track;

                cartSetBoolean(cart, wigMafWiggleVar(prefix, consWiggles, NULL), TRUE);
                wigOn = TRUE;
                }
            if (wigOn)
                {
                boolean first = TRUE;
                for (consWig = consWiggles; consWig != NULL;
                            consWig = consWig->next)
                    {
                    if (first)
                        {
                        printf("Conservation score statistics:");
                        first = FALSE;
                        }
                    char *wigVarSuffix = NULL;
                    (void)wigMafWiggleVar(tdb->track, consWig, &wigVarSuffix);
                    if (cartUsualBooleanClosestToHome(cart, tdb, FALSE, wigVarSuffix,FALSE))
                        {
                        printf("&nbsp;&nbsp;");
                        subChar(consWig->uiLabel, '_', ' ');
                        conservationStatsLink(tdb,
                            consWig->uiLabel, consWig->table);
                        }
                    }
                }
            }
        puts("</P>\n");

        /* no alignment to display when in visibilities where only wiggle is shown */
        char *vis = cartOptionalString(cart, tdb->track);
        if (vis)
            {
            enum trackVisibility tv = hTvFromStringNoAbort(vis);
            if (tv == tvSquish || tv == tvDense)
                return;
            }

#ifdef ADDEXONCAPITAL
	puts("<FORM ACTION=\"../cgi-bin/hgc\" NAME=\"gpForm\" METHOD=\"GET\">");
	cartSaveSession(cart);
	cgiContinueHiddenVar("g");
	cgiContinueHiddenVar("c");
	cgiContinueHiddenVar("i");
	cgiContinueHiddenVar("o");
	cgiContinueHiddenVar("t");
	cgiContinueHiddenVar("l");
	cgiContinueHiddenVar("r");
	printf("Capitalize ");
        cgiMakeDropListFull(codeVarName, codeAll, codeAll,
	    ArraySize(codeAll), codeVarVal, "change", autoSubmit);
	printf("exons based on ");
        capTrack = genePredDropDown(cart, trackHash,
                                       "gpForm", "hgc.multiCapTrack");
#endif
	printf("show ");
        cgiMakeDropListFull(showVarName, showAll, showAll,
	    ArraySize(showAll), showVarVal, "change", autoSubmit);
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

	mafLowerCase(maf);
#ifdef ADDEXONCAPITAL
	if (capTrack != NULL)
	    capMafOnTrack(maf, capTrack, onlyCds);
#endif
        printf("<B>Alignment %d - %d, %d bps </B>\n",
               maf->components->start + 1,
               maf->components->start + maf->components->size,
               maf->components->size);
        mafPrettyOut(stdout, maf, 70, onlyDiff, 1, labelHash);
	mafAliFree(&maf);
	}
    else
	{
        printf("No multiple alignment in browser window");
	}
    printf("</PRE></TT>");
    slNameFreeList(&orderList);
    }
}

static void mafOrAxtClick(struct sqlConnection *conn, struct trackDb *tdb, char *axtOtherDb)
{
struct sqlConnection *conn2 = NULL;
if (!(isHubTrack(tdb->track) || trackHubDatabase(database)))
    conn2 = hAllocConn(database);
// MAF file location is optionally in trackDb
char *mafFile = hashFindVal(tdb->settingsHash, "mafFile");

mafOrAxtClick2(conn, conn2, tdb, axtOtherDb, mafFile);

hFreeConn(&conn2);
}

void customMafClick(struct sqlConnection *conn, struct sqlConnection *conn2,
    struct trackDb *tdb)
{
struct hash *settings = tdb->settingsHash;
char *fileName;
if ((fileName = hashFindVal(settings, "mafFile")) == NULL)
    errAbort("cannot find custom maf file setting");
mafOrAxtClick2(conn, conn2, tdb, NULL, fileName);
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


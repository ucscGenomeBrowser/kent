/* regMotif.c - stuff for displaying details of regulatory
 * motifs and the like. */

#include "common.h"
#include "hash.h"
#include "dnaseq.h"
#include "jksql.h"
#include "hCommon.h"
#include "hdb.h"
#include "web.h"
#include "portable.h"
#include "cart.h"
#include "trackDb.h"
#include "hgc.h"
#include "dnaMotif.h"
#include "transRegCode.h"
#include "transRegCodeProbe.h"

static void printProbRow(char *label, float *p, int pCount)
/* Print one row of a probability profile. */
{
int i;
printf("%s ", label);
for (i=0; i < pCount; ++i)
    printf("%5.2f ", p[i]);
printf("\n");
}

static void printSpacedDna(char *dna, int size)
/* Print string with spaces between each letter. */
{
int i;
printf("  ");
for (i=0; i<size; ++i)
    printf("  %c   ", dna[i]);
}

static void printConsensus(struct dnaMotif *motif)
/* Print motif - use bold caps for strong letters, then
 * caps, then small letters, then .'s */
{
int i, size = motif->columnCount;
char c;
float best;
printf("  ");
for (i=0; i<size; ++i)
    {
    c = 'a';
    best = motif->aProb[i];
    if (motif->cProb[i] > best)
	{
	best = motif->cProb[i];
	c = 'c';
	}
    if (motif->gProb[i] > best)
	{
	best = motif->gProb[i];
	c = 'g';
	}
    if (motif->tProb[i] > best)
	{
	best = motif->tProb[i];
	c = 't';
	}
    printf("  ");
    if (best >= 0.90)
	printf("<B>%c</B>", toupper(c));
    else if (best >= 0.75)
        printf("%c", toupper(c));
    else if (best >= 0.50)
        printf("%c", tolower(c));
    else if (best >= 0.40)
        printf("<FONT COLOR=\"#A0A0A0\">%c</FONT>", tolower(c));
    else
        printf("<FONT COLOR=\"#A0A0A0\">.</FONT>");
    printf("   ");
    }
}

static struct dnaMotif *loadDnaMotif(char *motifName, char *motifTable)
/* Load dnaMotif from table. */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
struct dnaMotif *motif;
sprintf(query, "name = '%s'", motifName);
motif = dnaMotifLoadWhere(conn, motifTable, query);
hFreeConn(&conn);
return motif;
}

static void motifHitSection(struct dnaSeq *seq, struct dnaMotif *motif)
/* Print out section about motif. */
{
webNewSection("Motif:");
printf("<PRE>");
if (motif != NULL)
    {
    struct tempName pngTn;
    dnaMotifMakeProbabalistic(motif);
    makeTempName(&pngTn, "logo", ".png");
    dnaMotifToLogoPng(motif, 47, 140, NULL, "../trash", pngTn.forCgi);
    printf("   ");
    printf("<IMG SRC=\"%s\" BORDER=1>", pngTn.forHtml);
    printf("\n");
    }
if (seq != NULL)
    {
    touppers(seq->dna);
    printSpacedDna(seq->dna, seq->size);
    printf("this occurence\n");
    }
if (motif != NULL)
    {
    printConsensus(motif);
    printf("motif consensus\n");
    printProbRow("A", motif->aProb, motif->columnCount);
    printProbRow("C", motif->cProb, motif->columnCount);
    printProbRow("G", motif->gProb, motif->columnCount);
    printProbRow("T", motif->tProb, motif->columnCount);
    }
printf("</PRE>");
}

void doTriangle(struct trackDb *tdb, char *item, char *motifTable)
/* Display detailed info on a regulatory triangle item. */
{
int start = cartInt(cart, "o");
struct dnaSeq *seq = NULL;
struct dnaMotif *motif = loadDnaMotif(item, motifTable);
char *table = tdb->tableName;
int rowOffset = hOffsetPastBin(seqName, table);
char query[256];
struct sqlResult *sr;
char **row;
struct bed *hit = NULL;
struct sqlConnection *conn = hAllocConn();

cartWebStart(cart, "Regulatory Motif Info");
genericBedClick(conn, tdb, item, start, 6);

sprintf(query, 
	"select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	table, item, seqName, start);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    hit = bedLoadN(row + rowOffset, 6);
sqlFreeResult(&sr);

if (hit != NULL)
    {
    int i;
    seq = hDnaFromSeq(hit->chrom, hit->chromStart, hit->chromEnd, dnaLower);
    if (hit->strand[0] == '-')
	reverseComplement(seq->dna, seq->size);
    }
motifHitSection(seq, motif);
printTrackHtml(tdb);
}

void doTransRegCode(struct trackDb *tdb, char *item, char *motifTable)
/* Display detailed info on a transcriptional regulatory code item. */
{
struct dnaMotif *motif = loadDnaMotif(item, motifTable);
int start = cartInt(cart, "o");
struct dnaSeq *seq = NULL;
char *table = tdb->tableName;
int rowOffset = hOffsetPastBin(seqName, table);
char query[256];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = hAllocConn();
struct transRegCode *trc = NULL;

cartWebStart(cart, "Regulatory Code Info");
sprintf(query, 
	"select * from %s where  name = '%s' and chrom = '%s' and chromStart = %d",
	table, item, seqName, start);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    trc = transRegCodeLoad(row+rowOffset);
sqlFreeResult(&sr);

if (trc != NULL)
    {
    char strand[2];
    seq = hDnaFromSeq(trc->chrom, trc->chromStart, trc->chromEnd, dnaLower);
    strand[0] = dnaMotifBestStrand(motif, seq->dna);
    strand[1] = 0;
    if (strand[0] == '-')
        reverseComplement(seq->dna, seq->size);
    printf("<B>Name:</B> %s<BR>\n", trc->name);
    printf("<B>CHIP/CHIP Evidence:</B> %s<BR>\n", trc->chipEvidence);
    printf("<B>Species conserved in:</B> %d of 2<BR>\n", trc->consSpecies);
    printf("<B>Bit Score of Motif Hit:</B> %4.2f<BR>\n", 
    	dnaMotifBitScore(motif, seq->dna));
    printPosOnChrom(trc->chrom, trc->chromStart, trc->chromEnd, strand, TRUE, trc->name);
    }
motifHitSection(seq, motif);
printTrackHtml(tdb);
}

static double motifScoreHere(char *chrom, int start, int end,
	char *motifName, char *motifTable)
/* Return score of motif at given position. */
{
double score;
struct dnaSeq *seq = hDnaFromSeq(chrom, start, end, dnaLower);
struct dnaMotif *motif = loadDnaMotif(motifName, motifTable);
char strand = dnaMotifBestStrand(motif, seq->dna);
if (strand == '-')
    reverseComplement(seq->dna, seq->size);
score = dnaMotifBitScore(motif, seq->dna);
dnaMotifFree(&motif);
dnaSeqFree(&seq);
return score;
}

static void findBestScoringMotifHit(char *motifTable, struct hash *trcHash, 
	char *name, struct transRegCode **retTrc, double *retScore, int *retCount)
/* Find best hit of named motif in hash. */
{
struct transRegCode *trc, *bestTrc = NULL;
double score, bestScore = -BIGNUM;
struct hashEl *el = hashLookup(trcHash, name);
int count = 0;
while (el != NULL)
    {
    trc = el->val;
    score = motifScoreHere(trc->chrom, trc->chromStart, trc->chromEnd,
    	name, motifTable);
    if (score > bestScore)
        {
	bestScore = score;
	bestTrc = trc;
	}
    el = el->next;
    ++count;
    }
*retTrc = bestTrc;
*retScore = bestScore;
*retCount = count;
}

static void colLabel(char *label, int columns)
/* Print out label of given width. */
{
printf("<TH BGCOLOR=#%s", HG_COL_TABLE);
if (columns > 1)
    printf(" COLSPAN=%d", columns);
printf(">%s</TH>", label);
}

struct tfData
/* Data associated with one transcription factor. */
   {
   struct tfData *next;
   char *name;	/* Transcription factor name. */
   struct slName *conditionList;	/* List of growth conditions. */
   struct transRegCode *trcList;	/* List of binding sites. */
   };

int tfDataCmpName(const void *va, const void *vb)
/* Compare two tfData names. */
{
const struct tfData *a = *((struct tfData **)va);
const struct tfData *b = *((struct tfData **)vb);
return strcmp(a->name, b->name);
}

void doTransRegCodeProbe(struct trackDb *tdb, char *item, 
	char *codeTable, char *motifTable)
/* Display detailed info on a CHIP/CHIP probe from transRegCode experiments. */
{
char query[256];
struct sqlResult *sr;
char **row;
int rowOffset = hOffsetPastBin(seqName, tdb->tableName);
struct sqlConnection *conn = hAllocConn();
struct transRegCodeProbe *probe = NULL;
struct transRegCode *trc;

cartWebStart(cart, "CHIP/CHIP Probe Info");
safef(query, sizeof(query), "select * from %s where name = '%s'",
	tdb->tableName, item);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    probe = transRegCodeProbeLoad(row+rowOffset);
sqlFreeResult(&sr);
if (probe != NULL)
    {
    struct tfData *tfList = NULL, *tf;
    struct hash *tfHash = newHash(0);
    struct transRegCode *trc;
    int i;

    /* Print basic info. */
    printf("<B>Name:</B> %s<BR>\n", probe->name);
    printPosOnChrom(probe->chrom, probe->chromStart, probe->chromEnd, 
    	NULL, TRUE, probe->name);
    printf("<BR>\n");

    /* Make up list of all transcriptionFactors. */
    for (i=0; i<probe->tfCount; ++i)
        {
	/* Parse out factor and condition. */
	char *tfName = probe->tfList[i];
	char *condition = strchr(tfName, '_');
	if (condition != NULL)
	    *condition++ = 0;
	else
	    condition = "n/a";
	tf = hashFindVal(tfHash, tfName);
	if (tf == NULL)
	    {
	    AllocVar(tf);
	    hashAddSaveName(tfHash, tfName, tf, &tf->name);
	    slAddHead(&tfList, tf);
	    }
	slNameAddHead(&tf->conditionList, condition);
	}
    slSort(&tfList, tfDataCmpName);

    /* Fold in motif hits in region. */
    if (sqlTableExists(conn, codeTable))
        {
	sr = hRangeQuery(conn, codeTable, 
		probe->chrom, probe->chromStart, probe->chromEnd,
		"chipEvidence != 'none'", &rowOffset);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    trc = transRegCodeLoad(row+rowOffset);
	    tf = hashFindVal(tfHash, trc->name);
	    if (tf != NULL)
		slAddTail(&tf->trcList, trc);
	    }
	sqlFreeResult(&sr);
	}

    /* Print info on individual transcription factors. */
    webNewSection("Experiments with Significant Enrichment After IP");
    if (tfList == NULL)
        printf("No significant immunoprecipitation of this probe.");
    else
        {
	hTableStart();
	printf("<TR>");
	colLabel("Transcription", 1);
	colLabel("Growth", 1);
	colLabel("Motif Information", 3);
	printf("</TR>\n");
	printf("<TR>");
	colLabel("Factor", 1);
	colLabel("Conditions", 1);
	colLabel("Hits", 1);
	colLabel("Scores", 1);
	colLabel("Conservation (2 max)", 1);
	printf("</TR>\n");

	for (tf = tfList; tf != NULL; tf = tf->next)
	    {
	    struct slName *cond;
	    printf("<TR>");

	    /* Parse out factor and growth condition and print. */
	    printf("<TD>%s</TD>", tf->name);
	    printf("<TD>");
	    slSort(&tf->conditionList, slNameCmp);
	    for (cond = tf->conditionList; cond != NULL; cond = cond->next)
	        {
		if (cond != tf->conditionList)
		    printf(", ");
		printf("%s", cond->name);
	         }
	    printf("</TD>");


	    /* Print motif info. */
	    if (tf->trcList == NULL)
	        printf("<TD>0</TD><TD>n/a</TD><TD>n/a</TD>\n");
	    else
	        {
		printf("<TD>%d</TD>", slCount(tf->trcList));
		/* Print scores. */
		printf("<TD>");
		for (trc = tf->trcList; trc != NULL; trc = trc->next)
		    {
		    double score;
		    if (trc != tf->trcList)
		        printf(", ");
		    score = motifScoreHere(
		    	trc->chrom, trc->chromStart, trc->chromEnd,
			trc->name, motifTable);
		    printf("%3.1f", score);
		    }
		printf("</TD><TD>");
		for (trc = tf->trcList; trc != NULL; trc = trc->next)
		    {
		    if (trc != tf->trcList)
		        printf(", ");
		    printf("%d", trc->consSpecies);
		    }
		printf("</TD>");
		}
	    printf("</TR>\n");
	    }
	hTableEnd();
	}
    printf("<BR>\n");
    transRegCodeProbeFree(&probe);
    }
hFreeConn(&conn);
}


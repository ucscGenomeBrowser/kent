/* regMotif.c - stuff for displaying details of regulatory
 * motifs and the like. */

#include "common.h"
#include "hash.h"
#include "dnaseq.h"
#include "hdb.h"
#include "web.h"
#include "portable.h"
#include "cart.h"
#include "trackDb.h"
#include "hgc.h"
#include "dnaMotif.h"
#include "transRegCode.h"

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



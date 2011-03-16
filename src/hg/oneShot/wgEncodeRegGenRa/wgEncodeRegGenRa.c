/* wgEncodeRegGenRa - Help generate some of the wgEncodeReg.ra file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wgEncodeRegGenRa - Help generate some of the wgEncodeReg.ra file.\n"
  "usage:\n"
  "   wgEncodeRegGenRa now\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct markInfo
/* Info on a histone mark or the like. */
    {
    char *part;		/* Part as used in file and track names */
    char *name;		/* Human readable name */
    char *blurb;	/* Short description */
    char *vis;		/* Visibility */
    };


struct markInfo marks[] = {
    { "H3k4me1", "H3K4Me1", "Often Found Near Regulatory Elements", "full", },
    { "H3k4me3", "H3K4Me3", "Often Found Near Promoters", "full", },
    { "H3k27ac", "H3K27Ac", "Often Found Near Regulatory Elements", "hide", },
};

struct cellInfo
/* Info on a cell line */
    {
    char *part;		/* Part as used in file and track names */
    char *name;		/* Human readable name */
    int r,g,b;		/* Color */
    };

struct cellInfo cells[] = {
    { "Gm12878", "GM12878", 255,128,128, },
    { "H1hesc", "H1-hESC", 255,212,128, },
    { "Hmec", "HMEC", 120,235,120, },
    { "Hsmm", "HSMM", 120,235,204, },
    { "Huvec", "HUVEC", 128,212,255, },
    { "K562", "K562", 128,128,255, },
    { "Nhlf", "NHLF", 255,128,212, },
};

void wgEncodeRegGenRa(char *now)
/* wgEncodeRegGenRa - Help generate some of the wgEncodeReg.ra file. */
{
struct sqlConnection *conn = sqlConnect("hg19");
int i;
for (i=0; i<ArraySize(marks); ++i)
    {
    struct markInfo *mark = &marks[i];
    printf("    track wgRegMark%s\n", mark->part);
    printf("    container multiWig\n");
    printf("    noInherit on\n");
    printf("    type bigWig 0 10000\n");
    printf("    superTrack wgEncodeReg full\n");
    printf("    shortLabel Overlayed %s\n", mark->name);
    printf("    longLabel %s Mark (%s) from ENCODE\n", mark->name, mark->blurb);
    printf("    configurable on\n");
    printf("    visibility %s\n", mark->vis);
    printf("    viewLimits 0:50\n");
    printf("    maxHeightPixels 100:30:11\n");
    printf("    aggregate transparentOverlay\n");
    printf("    priority 1.%d\n", i+1);
    printf("    dragAndDrop subtracks\n");
    printf("\n");

    int j;
    for (j=0; j<ArraySize(cells); ++j)
        {
	struct cellInfo *cell = &cells[j];

	/* Get table, and see if it exists.  If so get info from it. */
	boolean tableExists = FALSE;
	int wigMax = 10000;
	char table[256];
	safef(table, sizeof(table), "wgEncodeBroadHistone%s%sStdSig", cell->part, mark->part);
	if (sqlTableExists(conn, table))
	    {
	    tableExists = TRUE;
	    char query[512];
	    safef(query, sizeof(query), "select fileName from %s", table);
	    char *fileName = sqlQuickString(conn, query);
	    struct bbiFile *bbi = bigWigFileOpen(fileName);
	    struct bbiSummaryElement sum = bbiTotalSummary(bbi);
	    wigMax = sum.maxVal;
	    bbiFileClose(&bbi);
	    }
	printf("\ttrack wgEncodeRegMark%s%s\n", marks->part, cell->part);
	printf("\ttable wgEncodeBroadHistone%sStdSig%s\n", cell->part, mark->part);
	if (!tableExists)
	    printf("\t#%s table doesn't exist\n", table);
	printf("\tshortLabel %s\n", cell->name);
	printf("\tlongLabel %s Mark (%s) on %s Cells from ENCODE\n",
		mark->name, mark->blurb, cell->name);
	printf("\tparent wgEncodeRegMark%s\n", marks->part);
	printf("\tcolor %d,%d,%d\n", cell->r, cell->g, cell->b);
	printf("\ttype bigWig 0 %d\n", wigMax);
	printf("\n");
	}
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
wgEncodeRegGenRa(argv[1]);
return 0;
}

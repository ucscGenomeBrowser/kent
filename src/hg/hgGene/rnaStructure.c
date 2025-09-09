/* rnaStructure - do section on 3' and 5' UTR structure. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include <limits.h>
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "rnaFold.h"
#include "hui.h"
#include "web.h"
#include "portable.h"
#include "hgGene.h"
#include "hgConfig.h"
#include "pipeline.h"


static void rnaTrashDirsInit(char **tables, int count)
/*	create trash directories if necessary */
{
for ( count--; count > -1; count--)
    mkdirTrashDirectory(tables[count]);
}

static boolean rnaStructureExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if tables exists and have our gene. */
{
if (sqlTableExists(conn, "foldUtr3") && 
	sqlRowExists(conn, "foldUtr3", "name", geneId))
    return TRUE;
if (sqlTableExists(conn, "foldUtr5") && 
	sqlRowExists(conn, "foldUtr5", "name", geneId))
    return TRUE;
return FALSE;
}

static void rnaStructurePrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out rnaStructure table. */
{
static boolean firstTime = TRUE;
static char *names[2] = 
	{"5' UTR", "3' UTR"};
static char *tables[2] = {"foldUtr5", "foldUtr3"};
int side;
char *tableName = cartUsualString(cart, hggType, NULL);
char *geneName = cartUsualString(cart, hggGene, NULL);

if (firstTime)
    {
    rnaTrashDirsInit(tables, ArraySize(tables));
    firstTime = FALSE;
    }

webPrintLinkTableStart();
webPrintLabelCell("Region");
webPrintLabelCell("Fold Energy");
webPrintLabelCell("Bases");
webPrintLabelCell("Energy/Base");
webPrintWideCenteredLabelCell("Display As", 3);

char *rnaPlotPath = cfgOptionDefault("rnaPlotPath", "../cgi-bin/RNAplot");

for (side = 0; side < ArraySize(names); ++side)
    {
    char *table = tables[side];
    struct sqlResult *sr;
    char query[256], **row;
    sqlSafef(query, sizeof(query), "select * from %s where name = '%s'",
    	table, geneId);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	struct rnaFold fold;
	int bases;
	char psName[2048], altPsName[2048];

	/* Load fold and save it as postScript. */
	rnaFoldStaticLoad(row, &fold);
	safef(psName, sizeof(psName), "%s/%s/%s_%s.ps", trashDir(), table, table, geneId);
        // newer versions of RNAplot add _ss.ps to the file name
        safef(altPsName, sizeof(altPsName), "%s/%s/%s_%s.ps_ss.ps", trashDir(), table, table, geneId);
        bool plotDone = FALSE;
	if (fileExists(psName) || fileExists(altPsName))
            plotDone = TRUE;
        else
	    {
	    FILE *f;

            if (!fileExists(rnaPlotPath))
                {
                plotDone = FALSE;
                }
            else
                {
                char *absoluteRnaPlotPath = NULL;
                absoluteRnaPlotPath = realpath(rnaPlotPath, NULL);
                char *plotCmd[] = {absoluteRnaPlotPath, NULL};
                char *curdir, outputDir[2048], psNameLocal[2048];
                curdir = getCurrentDir();
                safef(outputDir, sizeof(outputDir), "%s/%s/", trashDir(), table);
                setCurrentDir(outputDir); /* RNAplot only writes output to the current directory */
                struct pipeline *plStruct = pipelineOpen1(plotCmd, pipelineWrite | pipelineNoAbort, "/dev/null", NULL, 0);
                safef(psNameLocal, sizeof(psNameLocal), "%s_%s.ps", table, geneId);
                f = pipelineFile(plStruct);
                if (f != NULL)
                    {
                    fprintf(f, ">%s\n", psNameLocal);	/* This is used by RNAplot for the filename. */
                    fprintf(f, "%s\n%s\n", fold.seq, fold.fold);
                    plotDone = TRUE;
                    }
                pipelineClose(&plStruct);
                setCurrentDir(curdir); // return home
                free(absoluteRnaPlotPath);
                }
            }

        // newer versions of RNAplot add _ss.ps to the file name
        if (!fileExists(psName))
            safecpy(psName, sizeof(psName), altPsName);
            
	/* Print row of table, starting with energy terms . */
	hPrintf("</TR><TR>");
	bases = strlen(fold.seq);
	webPrintLinkCell(names[side]);
	webPrintLinkCellStart();
	hPrintf("%1.2f", fold.energy);
	webPrintLinkCellEnd();
	webPrintLinkCellStart();
	hPrintf("%d", bases);
	webPrintLinkCellEnd();
	webPrintLinkCellStart();
	hPrintf("%1.3f", fold.energy/bases);
	webPrintLinkCellEnd();

        if (plotDone)
            {
            /* Print link to png image. */
            webPrintLinkCellStart();
            hPrintf("<A HREF=\"%s?%s&%s=%s&%s=%s&%s=%s&%s=%s&%s=%s&%s=%d&%s=%d&%s=%s\" class=\"toc\" TARGET=_blank>",
                geneCgi, cartSidUrlString(cart), 
                hggType, tableName,
                hggGene, geneName,
                hggMrnaFoldRegion, table,
                hggMrnaFoldPs, psName,
                hggChrom, curGeneChrom,
                hggStart, curGeneStart,
                hggEnd, curGeneEnd,
                hggDoRnaFoldDisplay, "picture");
            hPrintf(" Picture ");
            hPrintf("</A>");
            webPrintLinkCellEnd();

            /* Print link to PostScript. */
            webPrintLinkCellStart();
            hPrintf("<A HREF=\"%s\" class=\"toc\">", psName);
            hPrintf(" PostScript ");
            hPrintf("</A>");
            webPrintLinkCellEnd();
            }

	/* Print link to text. */
	webPrintLinkCellStart();
	hPrintf("<A HREF=\"%s?%s&%s=%s&%s=%s&%s=%s&%s=%s&%s=%d&%s=%d&%s=%s\" class=\"toc\" TARGET=_blank>",
	    geneCgi, cartSidUrlString(cart), 
            hggType, tableName,
            hggGene, geneName,
	    hggMrnaFoldRegion, table,
            hggChrom, curGeneChrom,
            hggStart, curGeneStart,
            hggEnd, curGeneEnd,
	    hggDoRnaFoldDisplay, "text");
	hPrintf(" Text ");
	hPrintf("</A>");
	webPrintLinkCellEnd();
	}
    sqlFreeResult(&sr);
    }
webPrintLinkTableEnd();
hPrintf("<BR>The RNAfold program from the ");
hPrintf("<A HREF=\"http://www.tbi.univie.ac.at/~ivo/RNA/\" TARGET=_blank>");
hPrintf("Vienna RNA Package</A> is used to perform the ");
hPrintf("secondary structure predictions and folding calculations. ");
hPrintf("The estimated folding energy is in kcal/mol.  The more ");
hPrintf("negative the energy, the more secondary structure the RNA ");
hPrintf("is likely to have.");
}

struct section *rnaStructureSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create rnaStructure section. */
{
struct section *section = sectionNew(sectionRa, "rnaStructure");
if (section != NULL)
    {
    section->exists = rnaStructureExists;
    section->print = rnaStructurePrint;
    }
return section;
}

struct rnaFold *loadFold(struct sqlConnection *conn,
	char *table, char *name)
/* Load named fold from table. */
{
struct rnaFold *fold = NULL;
struct sqlResult *sr;
char query[256], **row;
sqlSafef(query, sizeof(query), "select * from %s where name = '%s'",
    table, name);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    fold = rnaFoldLoad(row);
sqlFreeResult(&sr);
return fold;
}

void doRnaFoldDisplay(struct sqlConnection *conn, char *geneId, char *geneName)
/* Show RNA folding somehow. */
{
char *table = cartString(cart, hggMrnaFoldRegion);
char *how = cartString(cart, hggDoRnaFoldDisplay);
struct rnaFold *fold = loadFold(conn, table, geneId);

if (fold == NULL)
    {
    warn("Couldn't load %s from %s", geneId, table);
    return;
    }
if (sameString(how, "text"))
    {
    hPrintf("<TT><PRE>");
    hPrintf("%s\n%s (%1.2f)\n", fold->seq, fold->fold, fold->energy);
    hPrintf("</PRE></TT>");
    }
else if (sameString(how, "picture"))
    {
    char *psFile = cartString(cart, hggMrnaFoldPs);

    // Sanity check the psFile variable
    // It should point to a .ps file that exists in the trash/foldUtr5 or foldUtr3 directory.
    // That means it should match trashDir()/foldUtr[35]/foldUtr[35]_.*\.ps as a regex,
    // and that the file should exist.

    if (!endsWith(psFile, ".ps"))
        {
        warn("Invalid file provided for creating of RNA structure pdf/image");
        return;
        }
    char sanCheck[4096];
    safef(sanCheck, sizeof(sanCheck), "%s/foldUtr5/foldUtr5_", trashDir());
    if (!startsWith(sanCheck, psFile))
        {
        safef(sanCheck, sizeof(sanCheck), "%s/foldUtr3/foldUtr3_", trashDir());
        if (!startsWith(sanCheck, psFile))
            {
            warn("Invalid file provided for creating of RNA structure pdf/image");
            return;
            }
        }
    if (!fileExists(psFile))
        {
        warn("Indicated postscript file for RNA structure pdf/image generation does not exist.");
        return;
        }

    char *rootName = cloneString(psFile);
    char pngName[256];
    char pdfName[256];

    chopSuffix(rootName);
    safef(pngName, sizeof(pngName), "%s.png", rootName);
    safef(pdfName, sizeof(pngName), "%s.pdf", rootName);
    hPrintf("<H2>%s (%s) %s energy %1.2f</H2>\n", 
    	geneName, geneId, table, fold->energy);
    if (!fileExists(pdfName))
        {
        char *command[] = { "ps2pdf", psFile, pdfName, NULL};
        struct pipeline *pl = pipelineOpen1(command, pipelineWrite | pipelineNoAbort, "/dev/null", NULL, 0);
        int sysRet = pipelineWait(pl);
        if (sysRet != 0)
            errAbort("System call returned %d for:\n  %s", sysRet, pipelineDesc(pl));
        }
    hPrintf("Click <A HREF=\"%s\">here for PDF version</A><BR>", pdfName);
    if (!fileExists(pngName))
        {
        char outputBuf[1024];
        safef(outputBuf, sizeof outputBuf, "-sOutputFile=%s", pngName);
        char *command[] = { "gs","-sDEVICE=png16m", outputBuf,"-dBATCH","-dNOPAUSE","-q", psFile, NULL};
        struct pipeline *pl = pipelineOpen1(command, pipelineWrite | pipelineNoAbort, "/dev/null", NULL, 0);
        int sysRet = pipelineWait(pl);
        if (sysRet != 0)
            errAbort("System call returned %d for:\n  %s", sysRet, pipelineDesc(pl));
        }
    hPrintf("<IMG SRC=\"%s\">", pngName);
    }
}


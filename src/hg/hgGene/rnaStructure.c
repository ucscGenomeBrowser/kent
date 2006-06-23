/* rnaStructure - do section on 3' and 5' UTR structure. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "rnaFold.h"
#include "hui.h"
#include "portable.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: rnaStructure.c,v 1.8 2006/06/23 21:43:24 hiram Exp $";

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

if (firstTime)
    {
    rnaTrashDirsInit(tables, ArraySize(tables));
    firstTime = FALSE;
    }

hPrintLinkTableStart();
hPrintLabelCell("Region");
hPrintLabelCell("Fold Energy");
hPrintLabelCell("Bases");
hPrintLabelCell("Energy/Base");
hPrintWideLabelCell("<CENTER>Display As</CENTER>", 3);
for (side = 0; side < ArraySize(names); ++side)
    {
    char *table = tables[side];
    struct sqlResult *sr;
    char query[256], **row;
    safef(query, sizeof(query), "select * from %s where name = '%s'",
    	table, geneId);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	struct rnaFold fold;
	int bases;
	char psName[128];

	/* Load fold and save it as postScript. */
	rnaFoldStaticLoad(row, &fold);
	safef(psName, sizeof(psName), "../trash/%s/%s_%s.ps", table, table, geneId);
	if (!fileExists(psName))
	    {
	    FILE *f;
	    f = popen("../cgi-bin/RNAplot", "w");
	    if (f != NULL)
	        {
		fprintf(f, ">%s\n", psName);	/* This tells where to put file. */
		fprintf(f, "%s\n%s\n", fold.seq, fold.fold);
		pclose(f);
		}
	    }

	/* Print row of table, starting with energy terms . */
	hPrintf("</TR><TR>");
	bases = strlen(fold.seq);
	hPrintLinkCell(names[side]);
	hPrintLinkCellStart();
	hPrintf("%1.2f", fold.energy);
	hPrintLinkCellEnd();
	hPrintLinkCellStart();
	hPrintf("%d", bases);
	hPrintLinkCellEnd();
	hPrintLinkCellStart();
	hPrintf("%1.3f", fold.energy/bases);
	hPrintLinkCellEnd();

	/* Print link to png image. */
	hPrintLinkCellStart();
	hPrintf("<A HREF=\"%s?%s&%s=%s&%s=%s&%s=%s\" class=\"toc\" TARGET=_blank>",
	    geneCgi, cartSidUrlString(cart), 
	    hggMrnaFoldRegion, table,
	    hggMrnaFoldPs, psName,
	    hggDoRnaFoldDisplay, "picture");
	hPrintf(" Picture ");
	hPrintf("</A>");
	hPrintLinkCellEnd();

	/* Print link to PostScript. */
	hPrintLinkCellStart();
	hPrintf("<A HREF=\"%s\" class=\"toc\">", psName);
	hPrintf(" PostScript ");
	hPrintf("</A>");
	hPrintLinkCellEnd();

	/* Print link to text. */
	hPrintLinkCellStart();
	hPrintf("<A HREF=\"%s?%s&%s=%s&%s=%s\" class=\"toc\" TARGET=_blank>",
	    geneCgi, cartSidUrlString(cart), 
	    hggMrnaFoldRegion, table,
	    hggDoRnaFoldDisplay, "text");
	hPrintf(" Text ");
	hPrintf("</A>");
	hPrintLinkCellEnd();
	}
    sqlFreeResult(&sr);
    }
hPrintLinkTableEnd();
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
safef(query, sizeof(query), "select * from %s where name = '%s'",
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
	 char command[512];
	 safef(command, sizeof(command), "ps2pdf %s %s" , psFile, pdfName);
	 system(command);
	 }
    hPrintf("Click <A HREF=\"%s\">here for PDF version</A><BR>", pdfName);
    if (!fileExists(pngName))
         {
	 char command[512];
	 safef(command, sizeof(command),
	 	"gs -sDEVICE=png16m -sOutputFile=%s -dBATCH -dNOPAUSE -q %s"
		, pngName, psFile);
	 system(command);
	 }
    hPrintf("<IMG SRC=\"%s\">", pngName);
    }
}


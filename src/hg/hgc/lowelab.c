/* Lowe Lab additions for Archaea and Bacterial details pages*/
#include "common.h"
#include "obscure.h"
#include "hCommon.h"
#include "hash.h"
#include "bits.h"
#include "memgfx.h"
#include "portable.h"
#include "errabort.h"
#include "dystring.h"
#include "nib.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "seqOut.h"
#include "hdb.h"
#include "spDb.h"
#include "hui.h"
#include "hgRelate.h"
#include "htmlPage.h"
#include "psl.h"
#include "cogs.h"
#include "cogsxra.h"
#include "bed.h"
#include "xenalign.h"
#include "isochores.h"
#include "genePred.h"
#include "genePredReader.h"
#include "pepPred.h"
#include "wabAli.h"
#include "minGeneInfo.h"
#include "rnaGene.h"
#include "tRNAs.h"
#include "snoRNAs.h"
#include "gbRNAs.h"
#include "hgMaf.h"
#include "maf.h"
#include "hgConfig.h"
#include "web.h"
#include "dbDb.h"
#include "sample.h"
#include "axt.h"
#include "gbProtAnn.h"
#include "hgSeq.h"
#include "chain.h"
#include "chainNet.h"
#include "chainDb.h"
#include "chainToPsl.h"
#include "chainToAxt.h"
#include "netAlign.h"
#include "tigrCmrGene.h"
#include "jgiGene.h"
#include "lowelabPfamHit.h"
#include "lowelabArkinOperonScore.h"
#include "lowelabTIGROperonScore.h"
#include "sargassoSeaXra.h"
#include "codeBlastScore.h"
#include "codeBlast.h"
#include "tigrOperon.h"
#include "easyGene.h"
#include "llaInfo.h"
#include "blastTab.h"
#include "hdb.h"
#include "hgc.h"
#include "genbank.h"
#include "pseudoGeneLink.h"
#include "axtLib.h"
#include "tfbsCons.h"
#include "tfbsConsMap.h"
#include "tfbsConsSites.h"
#include "tfbsConsFactors.h"
#include "simpleNucDiff.h"
#include "botDelay.h"
#include "bed5FloatScore.h"
#include "bed6FloatScore.h"
#include "pscreen.h"
#include "gapCalc.h"
#include "chainConnect.h"
#include "ec.h"
#include "transMapClick.h"
#include "ccdsClick.h"
#include "memalloc.h"
#include "rnaHybridization.h"
#include "cddInfo.h"
#include "alignInfo.h"
#include "loweutils.h"
#include "cddDesc.h"
#include "arCOGs.h"
#include "arcogdesc.h"
#include "megablastInfo.h"
#include "geneTree.h"

/*#define LISTUI*/

static char const rcsid[] = "$Id: lowelab.c,v 1.50 2010/05/11 01:43:29 kent Exp $";

extern char *uniprotFormat;

static void goPrint( struct sqlConnection *conn, char *geneId, char *acc)
/* Print out GO annotations. */
{
struct sqlConnection *goConn = sqlConnect("go");
char query[512];
struct sqlResult *sr;
char **row;
static char *aspects[3] = {"F", "P", "C"};
static char *aspectNames[3] = {
    "Molecular Function",
    "Biological Process",
    "Cellular Component",
};
int aspectIx;
int termCount = 0;

if (sqlTableExists(goConn,"goaPart") &&
    sqlTableExists(goConn,"term"))
for (aspectIx = 0; aspectIx < ArraySize(aspects); ++aspectIx)
    {
    boolean hasFirst = FALSE;
    safef(query, sizeof(query),
          "select term.acc,term.name"
          " from goaPart,term"
          " where goaPart.dbObjectId = '%s'"
          " and goaPart.goId = term.acc"
          " and goaPart.aspect = '%s'"
          , acc, aspects[aspectIx]);
    sr = sqlGetResult(goConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
		char *goID = row[0];
		char *goTermName = row[1];
		if (!hasFirst)
        {
		    printf("<B>%s:</B><BR>", aspectNames[aspectIx]);
		    hasFirst = TRUE;
        }
        printf("<A HREF = \"");
		printf("http://godatabase.org/cgi-bin/go.cgi?view=details&depth=1&query=%s", goID);
		printf("\" TARGET=_blank>%s</A> %s<BR>\n", goID, goTermName);
		termCount++;
    }
    if (hasFirst)
        printf("<BR>");
    sqlFreeResult(&sr);
    }
	if (termCount == 0) printf("Not available<BR>\n");
	sqlDisconnect(&goConn);
}

void keggOtherGenes(struct sqlConnection *conn, char *geneId,
        char *table, char *mapId)
/* Print out genes linked to a kegg pathway mapId. */
{
char query[512], **row;
struct sqlResult *sr;
char *extraTable = "gbProtCodeXra";
char *keggTable = "keggPathway";
if (hTableExists(database, extraTable))
    {
    safef(query, sizeof(query),
            "select x.name, x.gene, x.product from %s k1, %s x  "
            "where k1.mapID = '%s' and "
            "k1.kgID = x.name ;"
            , keggTable,  extraTable, mapId );
    sr = sqlGetResult(conn, query);
    printf("<table>\n");
    while ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<tr><td>");
        hgcAnchorPosition(table,row[0]);
    printf("%s</A> <BR>\n",row[0]);
        if (differentString(row[0],row[1]) && differentString(row[1], "none"))
            printf("</td><td>%s</td><td>%s</td></tr>\n",
                row[1], row[2]);
        else
            printf("</td><td> </td><td>%s</td></tr>\n",
                row[2]);
        }
    sqlFreeResult(&sr);
    printf("</table>\n");
    }
}
void keggLink(struct sqlConnection *conn, char *geneId,
        char *table, char *title)
/* Print out kegg database link. */
{
char query[512], **row;
struct sqlResult *sr;
struct sqlConnection *conn2 = hAllocConn(database);
safef(query, sizeof(query),
    "select keggPathway.locusID,keggPathway.mapID,keggMapDesc.description"
    " from keggPathway,keggMapDesc"
    " where keggPathway.kgID='%s'"
    " and keggPathway.mapID = keggMapDesc.mapID"
    , geneId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("%s",title);
    printf("<A HREF=\"http://www.genome.ad.jp/dbget-bin/show_pathway?%s+%s\" TARGET=_blank>",
        row[1], row[0]);
    printf("%s</A> - %s<BR>", row[1], row[2]);
    printf("<B>Other Genes in Kegg Pathway: </b><BR>");
    keggOtherGenes(conn2, geneId, table, row[1]);
    printf("<BR>\n");
    }
sqlFreeResult(&sr);
}

int keggCount(struct sqlConnection *conn, char *geneId)
/* Count up number of hits. */
{
char query[256];
char *keggTable = "keggPathway";
if (!hTableExists(database, keggTable))
    return 0;
safef(query, sizeof(query),
    "select count(*) from %s where kgID='%s'", keggTable, geneId);
return sqlQuickNum(conn, query);
}

void modBaseAnchor(char *swissProtAcc)
/* Print out anchor to modBase. */
{
printf("<A HREF=\"http://salilab.org/modbase-cgi/model_search.cgi?searchkw=name&kword=%s\" TARGET=_blank>", swissProtAcc);
}

float computeGCContent(char* dna, int length)
{
    float percent = 0.0f;
    int count = 0;
    int i = 0;
    for (i = 0; i < length; i++)
    {
        if ((dna[i] == 'C') || (dna[i] == 'c') || (dna[i] == 'G') || (dna[i] == 'g'))
            count++;
    }
    percent = (float) count / (float) length * 100.0f;
    return percent;
}

int selfBlastpHitCount(struct sqlConnection *conn, char *geneId)
/* Count up number of hits. */
{
char query[512];
char *blastpHitsTable = "blastpHits";
if (!hTableExists(database, blastpHitsTable))
    return 0;
safef(query, sizeof(query),
    "select count(*) from %s where query = '%s' and target like '%s:%%' and target != '%s:%s'",
	blastpHitsTable, geneId, database, database, geneId);
return sqlQuickNum(conn, query);
}

struct blastTab* loadSelfBlastpHits(struct sqlConnection *conn, char* queryName, int self)
/* Load all blastp hits in the same genome of the given query gene into a list */
{
    char query[512];
    struct sqlResult *srBlastpHits = NULL;
    struct blastTab *list = NULL;
    struct blastTab *blastpHits;
    char **row;
    char blastpHitsTable[] = "blastpHits";

    if (hTableExists(database, blastpHitsTable))
    {
		if (self)
		{
			sprintf(query, "select * from %s where query = '%s' and target like '%s:%%'",
					blastpHitsTable, queryName, database);
		}
		else
		{
			sprintf(query, "select * from %s where query = '%s' and target like '%s:%%' and target != '%s:%s'",
					blastpHitsTable, queryName, database, database, queryName);
		}
        srBlastpHits = sqlGetResult(conn, query);
        while ((row = sqlNextRow(srBlastpHits)) != NULL)
        {
            blastpHits = blastTabLoad(row);
            slAddTail(&list, blastpHits);
        }
    }
    if (srBlastpHits != NULL)
        sqlFreeResult(&srBlastpHits);
    return list;
}

void printSelfHomologs(struct sqlConnection *conn, struct blastTab *blastpHitsList)
/* Print self homologs in refseq */
{
    char query[512];
    struct sqlResult *sr;
    char **row;
    char refSeq[50];
    char xraTable[50];
    char product[255];
    struct blastTab *blastpHits;
    struct minGeneInfo *ginfo;
    char *blastpTarget[2];
    char **buffer = NULL;
    boolean findTable = FALSE;
    unsigned int cdsStart = 0;
    unsigned int cdsEnd = 0;

    printf("<br>\n");
    printf("<b>Homologs within genome</b><BR>\n");

    /* Print table */
    printf("<table style='width:60%%; background-color:#%s;' border=0 cellpadding=1 cellspacing=0>", HG_COL_BORDER);
    printf("<tbody><tr><td>\n");
    printf("<table style='width:100%%; text-align:left; background-color:#%s;' border=1 cellpadding=2 cellspacing=2>\n", HG_COL_INSIDE);
    printf("<tbody>\n");

    /* Print table column heading */
    printf("<tr style=\"vertical-align: top;\">\n");
    printf("<td width=\"20%%\"><b>Gene</b></td>\n");
    printf("<td><b>Product</b></td>\n");
    printf("<td width=\"30%%\"><b>BlastP E-value</b></td>\n");
    printf("</tr>\n");

    blastpHits = blastpHitsList;
    while (blastpHits != NULL)
    {
        parseDelimitedString(blastpHits->target, ':', blastpTarget, 2);

		if (hTableExists(blastpTarget[0], "lookup"))
		{
			sprintf(query, "select lookupValue from %s.lookup where lookupCode = 'annotRev'", blastpTarget[0]);
			sr = sqlGetResult(conn, query);
			if ((row = sqlNextRow(sr)) != NULL)
			{
				strcpy(refSeq, row[0]);
				findTable = TRUE;
				sqlFreeResult(&sr);
			}
		}
		else if (hTableExists(blastpTarget[0], "refSeq"))
		{
			strcpy(refSeq, "refSeq");
			findTable = TRUE;
		}
		if (findTable)
		{
			sprintf(query, "select chrom, cdsStart, cdsEnd from %s where name = '%s'",
					refSeq, blastpTarget[1]);
			sr = sqlGetResult(conn, query);
			if ((row = sqlNextRow(sr)) != NULL)
			{
				cdsStart = strtoul(row[1], buffer, 10);
				cdsEnd = strtoul(row[2], buffer, 10);
		        printf("<tr style=\"vertical-align: top;\">\n");
				printf("<td><a href=\"hgTracks\?position=%s:%u-%u&db=%s\" TARGET=_blank>%s</a></td>\n",
					   row[0], cdsStart, cdsEnd, blastpTarget[0], blastpTarget[1]);
			}
			else
				printf("<td>%s</td>\n", blastpTarget[1]);
			sqlFreeResult(&sr);
		}
		else
			printf("<td>%s</td>\n", blastpTarget[1]);

		if (hTableExists(blastpTarget[0], "lookup"))
		{
			sprintf(query, "select lookupValue from %s.lookup where lookupCode = 'annotRevXra'", blastpTarget[0]);
			sr = sqlGetResult(conn, query);
			if ((row = sqlNextRow(sr)) != NULL)
			{
				strcpy(xraTable, row[0]);
				sqlFreeResult(&sr);
			}
			else
				strcpy(product, "N/A");

			sprintf(query, "select product from %s where name = '%s'", xraTable, blastpTarget[1]);
			sr = sqlGetResult(conn, query);
			if ((row = sqlNextRow(sr)) != NULL)
			{
				strcpy(product, row[0]);
				sqlFreeResult(&sr);
			}
			else
				strcpy(product, "N/A");
		}
		else
		{
			ginfo = getGbProtCodeInfo(conn, blastpTarget[0], blastpTarget[1]);
			if (ginfo != NULL && ginfo->product != NULL && differentString(ginfo->product,"none"))
				strcpy(product, ginfo->product);
			else
				strcpy(product, "N/A");
		}
		printf("<td>%s</td>\n", product);
		printf("<td style=\"text-align: right;\">%0.0e</td>\n", blastpHits->eValue);
		printf("</tr>\n");

        free(blastpTarget[0]);
        free(blastpTarget[1]);
        blastpHits = blastpHits->next;
    }
    /* Close table */
    printf("</tbody>\n");
    printf("</table>\n");
    printf("</td></tr></tbody>\n");
    printf("</table>\n");
}

int getGeneTree(struct sqlConnection *conn, char *geneId, char *treeFileName)
{
	int success = 0;
	char query[256];
	char *geneTreeTable = "geneTree";
	struct sqlResult *sr;
	char **row;
	struct geneTree *genetree;

	if (!hTableExists(database, geneTreeTable))
		return 0;
	safef(query, sizeof(query),
		"select * from %s where name = '%s'", geneTreeTable, geneId);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	{
		genetree = geneTreeLoad(row);
		if (!fileExists(treeFileName))
		{
			FILE *f;
			f = fopen(treeFileName, "w");
			if (f != NULL)
			{
				fprintf(f, "%s\n", genetree->tree);
				fclose(f);
				success = 1;
			}
		}
		else
			success = 1;
	}
	sqlFreeResult(&sr);
	geneTreeFree(&genetree);

	return success;
}

void doRefSeq(struct trackDb *tdb, char *item,
             char *pepTable, char *extraTable)
/* Handle click on gene track. */
{
struct minGeneInfo ginfo;
char query[256];
char query2[256];
char *gi = NULL;
struct sqlResult *sr, *sr2;
struct dnaSeq *sequence;
char **row, **row2;
char *words[16], *dupe = cloneString(tdb->type);
int wordCount, x, length;
int num = 0;
int hits = 0;
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2;
struct sqlConnection *spConn = NULL;
struct COG *COG=NULL;
struct COGXra *COGXra=NULL;
char *temparray[160];
char *giwords[5];
char *spAcc = NULL;
struct slName *el, *list;
char *table = tdb->table;
char *pdb = hPdbFromGdb(database);
struct genePred *gpList = NULL, *gp = NULL;
char tableName[64];
boolean hasBin;
int itemCount = 0;
int arcogCount = 0;
char genome[50] = "";
char clade[50] = "";
boolean hasArCOG;

char treeFileName[256];
char treeTmpPsFileName[256];
char treePsFileName[256];
char treePngFileName[256];
char treePdfFileName[256];
char command[512];
char buffer[512];
char searchTerm[256];

struct blastTab *blastpHitsList;

char pepTableName[64];
char extraTableName[64];

if (startsWith("annotRev", table))
{
    sprintf(pepTableName, "%s%s", table, pepTable);
    sprintf(extraTableName, "%s%s", table, extraTable);
}
else
{
    strcpy(pepTableName, pepTable);
    strcpy(extraTableName, extraTable);
}

spConn = sqlConnect( pdb);
genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
if (extraTableName != NULL && hTableExists(database, extraTableName))
{
    sprintf(query, "select * from %s where name = '%s'", extraTableName, item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
		minGeneInfoStaticLoad(row, &ginfo);
		gi = cloneString(ginfo.gi);
        if (ginfo.gene != NULL && differentString(ginfo.gene,"none"))
            printf("<B>Gene: </B>%s<BR>\n", ginfo.gene);
        if (ginfo.product != NULL && differentString(ginfo.product,"none"))
            medlineLinkedLine("Product", ginfo.product, ginfo.product);
        if (ginfo.note != NULL && differentString(ginfo.note,"none"))
            printf("<B>Note: </B>%s<BR>\n", ginfo.note);
        if (ginfo.protein != NULL && differentString(ginfo.protein,"none"))
            printf("<B>Protein: </B>%s<BR>\n", ginfo.protein);
        if (ginfo.ec != NULL && differentString(ginfo.ec,"none"))
        {
            printf("<B>EC (Enzyme Commission) number:</B> "
               "<A HREF=\"http://us.expasy.org/enzyme/%s\" "
           "TARGET=_BLANK>%s</A>\n", ginfo.ec, ginfo.ec);

            getEcHtml(ginfo.ec);
            medlineLinkedTermLine("EC PubMed Search", ginfo.ec, ginfo.ec, "EC/RN Number");
            printf(" <B>Brenda : </B> <A HREF=\"http://www.brenda.uni-koeln.de/php/result_flat.php4?ecno=%s&organism=\" "
           "TARGET=_BLANK>%s</A><BR>\n", ginfo.ec, ginfo.ec);
        }
    }
    sqlFreeResult(&sr);
}
/* lookup swissprot acc */
spAcc = uniProtFindPrimAccFromGene(item, database);
if (spAcc != NULL)
{
    printf("<B>UniProtKB:</B> ");
    printf("<A HREF=");
    printf(uniprotFormat, spAcc);
    if (spAcc == NULL)
    {
        printf(" TARGET=_blank>%s</A></B><BR>\n", item);
    }
    else
    {
        printf(" TARGET=_blank>%s</A></B><BR>\n", spAcc);
    }
}

/* print table of contents */
printf("<br><hr style='width:100%%; height:2px;'><span style='font-size:larger;'>\n");
printf("<b>[<a href=\"#positions\">Positions and Sequence</a>]&nbsp;&nbsp;&nbsp;\n");
printf("[<a href=\"#COG\">COG</a>]&nbsp;&nbsp;&nbsp;\n");
printf("[<a href=\"#GO\">Gene Ontology</a>]&nbsp;&nbsp;&nbsp;\n");
printf("[<a href=\"#domain\">Protein Domain and Structure Infomation</a>]&nbsp;&nbsp;&nbsp;\n");
printf("[<a href=\"#homology\">Gene Homology</a>]&nbsp;&nbsp;&nbsp;\n");
printf("[<a href=\"#pathway\">Pathway</a>]</b></span> <br>\n");
printf("<hr style=\"width: 100%%; height: 2px;\"><br>\n");

/* Positions and sequence */
printf("<table style=\"text-align: left; width: 99%%;\" border=\"1\" cellpadding=\"5\" cellspacing=\"0\">\n");
printf("<tbody><tr><td style=\"background-color:#eee9e9;\">\n");
printf("<a name=\"positions\"></a><b>Positions and Sequence</b><br></td></tr>\n");
printf("<tr><td>\n");

hFindSplitTable(database, seqName, table, tableName, &hasBin);
safef(query, sizeof(query), "name = \"%s\"", item);
gpList = genePredReaderLoadQuery(conn, tableName, query);
for (gp = gpList; gp != NULL; gp = gp->next)
{
    sequence = hDnaFromSeq(database, gp->chrom, gp->txStart, gp->txEnd, dnaUpper);
    if (sequence != NULL)
        printf("<B>GC content:</B> %0.2f%%<BR>\n", computeGCContent(sequence->dna, sequence->size));
}
geneShowPosAndLinks(item, item, tdb, pepTableName, "htcTranslatedProtein",
            "htcGeneMrna", "htcGeneInGenome", "Predicted mRNA");
genePredFreeList(&gpList);

printf("</td></tr></tbody></table><br>\n");

/* COG */
printf("<table style=\"text-align: left; width: 99%%;\" border=\"1\" cellpadding=\"5\" cellspacing=\"0\">\n");
printf("<tbody><tr><td style=\"background-color:#eee9e9;\">\n");
printf("<a name=\"COG\"></a><b>COG</b><br></td></tr>\n");
printf("<tr><td>\n");

/* cog description */
itemCount = 0;
if (hTableExists(database, "COG"))
{
    sprintf(query, "select * from COG where name = '%s'", item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
    COG = COGLoad(row);
    if(COG!=NULL)
    {
        length=chopString(COG->COG, "," , temparray, 999);
        for(x=0; x<length; x++)
        {
	        conn2 = hAllocConn(database);
	        sprintf(query2, "select * from COGXra where name = '%s'", temparray[x]);
                sr2 = sqlGetResult(conn2, query2);
	        while ((row2 = sqlNextRow(sr2)) != NULL)
            {
	            COGXra=COGXraLoad(row2);
	            if(COGXra!=NULL)
	              printf("<B>COG:</B> "
                 "<A HREF=\"http://www.ncbi.nlm.nih.gov/COG/grace/wiew.cgi?%s\"  target=\"_blank\" "
                 ">%s</A>&nbsp; "
                 "<A HREF=\"http://www.ncbi.nlm.nih.gov/COG/grace/wiew.cgi?fun=%s\"  target=\"_blank\" "
                 ">Code %s</A>&nbsp;\n",
                 COGXra->name, COGXra->name, COG->code,COG->code);
	            printf(" %s<BR>\n", COGXra->info);
				itemCount++;
            }
            sqlFreeResult(&sr2);
            hFreeConn(&conn2);
            }
        }
    }
    sqlFreeResult(&sr);
    //hFreeConn(&conn2);
}

/*
if (hTableExists(database, "arCOGs"))
{
    struct arCOGs *infoload = NULL;
    struct arcogdesc *description = NULL;
    int rowOffset = hOffsetPastBin(database, seqName, "arCOGs");
    //infoload = arCOGsLoad(row);

    row = NULL;
    sprintf(query, "select * from arCOGs where gene = '%s'", item);
    sr = sqlGetResult(conn, query);
    conn2 = hAllocConn(database);
    while ((row = sqlNextRow(sr)) != NULL)
    {
        infoload = arCOGsLoad(row+rowOffset);
        if(infoload!=NULL)
        {
            sprintf(query2, "select * from mgCommonDb.arcogdesc where name = '%s'",infoload->name );
            sr2 = sqlGetResult(conn2, query2);
            while ((row2 = sqlNextRow(sr2)) != NULL)
            {
                description=arcogdescLoad(row2);
                if(description!=NULL)
                {
                    printf("<B>arCOG:</B> %s Code %s",infoload->name, description->code);
                    printf("  %s<BR/>\n", description->description);
					itemCount++;
                }
            }
            sqlFreeResult(&sr2);
            hFreeConn(&conn2);
         }
     }
}
*/

arcogCount = 0;
hasArCOG = FALSE;
row = NULL;
sprintf(query, "show databases like 'arCogsDb'");
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
{
	hasArCOG = TRUE;
}
sqlFreeResult(&sr);

if (hasArCOG)
{
	/* Get species info */
	memset(genome, 0, 50);
	memset(clade, 0, 50);
	getGenomeClade(conn, database, genome, clade);

	sprintf(query, "select distinct a.arcog_id, a.anntation, c.class_id from arCogsDb.arcogDef a, arCogsDb.arcog b, arCogsDb.arcogFun c where a.arcog_id = b.arcog_id and a.arcog_id = c.arcog_id and db_name = '%s' and name = '%s'",
			database, item);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	{
		printf("<B>arCOG:</B> <A HREF=\"/arCOGsBrowser/#Tax_Tree;ArcogsId=%s\" target=\"_blank\">%s</A> <A HREF=\"/arCOGsBrowser/#MainAdvance;Genome=%s,FunId=%s,Limit=50,Index=0,Load=true\" target=\"_blank\">Code %s</A> ",
			   row[0], row[0], genome, row[2], row[2]);
		printf("  %s<BR/>\n", row[1]);
		arcogCount++;
		itemCount++;		
	}
	sqlFreeResult(&sr);
	if (arcogCount  > 0)
		printf("<A HREF=\"/arCOGsBrowser/#MainGene;Genome=%s,Gene=%s\" target=\"_blank\">arCOG Gene Annotation</A><BR/>", genome, item);
}

if (itemCount == 0) printf("Not available\n");
printf("</td></tr></tbody></table><br>\n");

/* GO */
printf("<table style=\"text-align: left; width: 99%%;\" border=\"1\" cellpadding=\"5\" cellspacing=\"0\">\n");
printf("<tbody><tr><td style=\"background-color:#eee9e9;\">\n");
printf("<a name=\"GO\"></a><b>Gene Ontology</b><br></td></tr>\n");
printf("<tr><td>\n");

/* print go terms */
goPrint( conn, item, spAcc);
printf("</td></tr></tbody></table><br>\n");

/* Protein domain and structure information */
printf("<table style=\"text-align: left; width: 99%%;\" border=\"1\" cellpadding=\"5\" cellspacing=\"0\">\n");
printf("<tbody><tr><td style=\"background-color:#eee9e9;\">\n");
printf("<a name=\"domain\"></a><b>Protein Domain and Structure Information</b><br></td></tr>\n");
printf("<tr><td>\n");

/* interpro domains */
list = spExtDbAcc1List(spConn, spAcc, "InterPro");
if (list != NULL)
{
    char query[256], **row, **row2;
    struct sqlResult *sr, *sr2;
    printf("<B>InterPro Domains: </B> ");
    printf("<A HREF=\"http://www.ebi.ac.uk/interpro/ISpy?mode=single&ac=%s\" TARGET=_blank>",
        spAcc);
    printf("Graphical view of domain structure</A><BR>");
    safef(query, sizeof(query),
        "select extAcc1,extAcc2 from extDbRef,extDb"
    " where extDbRef.acc = '%s'"
    " and extDb.val = 'Interpro' and extDb.id = extDbRef.extDb"
    , spAcc);
    sr = sqlGetResult(spConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char interPro[256];
        char *pdb = hPdbFromGdb(database);
        safef(interPro, 128, "%s.interProXref", pdb);
            if (hTableExists(database, interPro))
                {
                safef(query, sizeof(query),
                        "select description from %s where accession = '%s' and interProId = '%s'",
                        interPro, spAcc, row[0]);
                sr2 = sqlGetResult(conn, query);
                if ((row2 = sqlNextRow(sr2)) != NULL)
                    {
                    printf("<A HREF=\"http://www.ebi.ac.uk/interpro/IEntry?ac=%s\" TARGET=_blank>", row[0]);
                    printf("%s</A> - %s <BR>\n", row[0], row2[0]);
                    }
                sqlFreeResult(&sr2);
            }
            else
            {
                printf("<A HREF=\"http://www.ebi.ac.uk/interpro/IEntry?ac=%s\" TARGET=_blank>", row[0]);
                printf("%s</A> - %s<BR>\n", row[0], row[1]);
            }
    }
    printf("<BR>\n");
    slFreeList(&list);
}

/* pfam domains */
list = spExtDbAcc1List(spConn, spAcc, "Pfam");
if (list != NULL)
{
    printf("<B>Pfam Domains:</B><BR>");
    for (el = list; el != NULL; el = el->next)
    {
    char query[256];
    char *description;
    safef(query, sizeof(query), "select description from proteome.pfamDesc where pfamAC='%s'", el->name);
    description = sqlQuickString(spConn, query);
    if (description == NULL)
        {
        safef(query, sizeof(query),
        "select extDbRef.extAcc1 from extDbRef,extDb "
        "where extDbRef.acc = '%s' "
        "and extDbRef.extDb = extDb.id "
        "and extDb.val = '%s'"
        , spAcc,el->name);

        printf("%s\n", query);
        description = sqlQuickString(spConn, query);
        }
    if (description == NULL)
        description = cloneString("n/a");
    printf("<A HREF=\"http://pfam.sanger.ac.uk/family?acc=%s\" TARGET=_blank>",
        el->name);
    printf("%s</A> - %s<BR>\n", el->name, description);
    freez(&description);
    }
    slFreeList(&list);
    printf("<BR>\n");
}

list = spExtDbAcc1List(spConn, spAcc, "PDB");
if (list != NULL)
{
    char query[256], **row;
    struct sqlResult *sr;
    int column = 0, maxColumn=4, rowCount=0;
    printf("<B>Protein Data Bank (PDB) 3-D Structure</B><BR>");
    safef(query, sizeof(query),
        "select extAcc1,extAcc2 from extDbRef,extDb"
    " where extDbRef.acc = '%s'"
    " and extDb.val = 'PDB' and extDb.id = extDbRef.extDb"
    , spAcc);
    sr = sqlGetResult(spConn, query);
    printf("<TABLE><TR>\n");
    while ((row = sqlNextRow(sr)) != NULL)
        {
    if (++column > maxColumn)
        {
        printf("</TR><TR>");
        column = 1;
        if (rowCount == 0)
            {
        printf("<TD ALIGN=CENTER COLSPAN=4><I>To conserve bandwidth, only the images from the first %d structures are shown.</I>", maxColumn);
        printf("</TR><TR>");
        }
        ++rowCount;
        }
    printf("<TD>");
    printf("<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?pdbId=%s\" TARGET=_blank>", row[0]);
    if (rowCount < 1)
        printf("<IMG SRC=\"http://www.rcsb.org/pdb/images/%s_bio_r_80.jpg\"><BR>", row[0]);
    printf("%s</A> - %s<BR>\n", row[0], row[1]);
    printf("</TD>");
    }
    printf("</TR></TABLE>\n");
    printf("<BR>\n");
    slFreeList(&list);
    }

/* Do modBase link. */
    {
    printf("<B>ModBase Predicted Comparative 3D Structure on ");
    modBaseAnchor(spAcc);
    printf("%s", spAcc);
    printf("</A></B><BR>\n");
    printf("<TABLE><TR>");
    printf("<TD>");
    modBaseAnchor(spAcc);
    printf("\n<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s\"></A></TD>", spAcc);
    printf("<TD>");
    modBaseAnchor(spAcc);
    printf("\n<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s&axis=x&degree=90\"></A></TD>", spAcc);
    printf("<TD>");
    modBaseAnchor(spAcc);
    printf("\n<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s&axis=y&degree=90\"></A></TD>", spAcc);
    printf("</TR><TR>\n");
    printf("<TD ALIGN=CENTER>Front</TD>");
    printf("<TD ALIGN=CENTER>Top</TD>");
    printf("<TD ALIGN=CENTER>Side</TD>");
    printf("</TR></TABLE>\n");
    printf("<I>The pictures above may be empty if there is no "
            "ModBase structure for the protein.  The ModBase structure "
        "frequently covers just a fragment of the protein.  You may "
        "be asked to log onto ModBase the first time you click on the "
        "pictures. It is simplest after logging in to just click on "
        "the picture again to get to the specific info on that model.</I><p>");
}
printf("</td></tr></tbody></table><br>\n");

/* Gene Homology */
printf("<table style=\"text-align: left; width: 99%%;\" border=\"1\" cellpadding=\"5\" cellspacing=\"0\">\n");
printf("<tbody><tr><td style=\"background-color:#eee9e9;\">\n");
printf("<a name=\"homology\"></a><b>Gene Homology</b><br></td></tr>\n");
printf("<tr><td>\n");

/* ncbi blast hits */
hits = chopString(gi,":",giwords,sizeof(giwords));
if (hits > 0)
{
	printf("<B>NCBI Blast Hits:</B> "
	   "<A HREF=\"http://www.ncbi.nlm.nih.gov/sutils/blink.cgi?pid=%s\" "
	   "TARGET=_BLANK>%s</A><BR>\n", (hits == 2) ? giwords[1] : giwords[0],
	   (hits == 2) ? giwords[1] : giwords[0]);
}

/* Gene tree */
safef(treeFileName, sizeof(treeFileName), "../trash/geneTree/geneTree_%s.txt", item);
if (getGeneTree(conn, item, treeFileName))
{
	safef(treeTmpPsFileName, sizeof(treeTmpPsFileName), "../trash/geneTree/tmp_geneTree_%s.ps", item);
	safef(treePsFileName, sizeof(treePsFileName), "../trash/geneTree/geneTree_%s.ps", item);
	safef(treePngFileName, sizeof(treePngFileName), "../trash/geneTree/geneTree_%s.png", item);
	safef(treePdfFileName, sizeof(treePdfFileName), "../trash/geneTree/geneTree_%s.pdf", item);
	safef(searchTerm, sizeof(searchTerm), ".%s) show\n", item);

	if (!fileExists(treePsFileName))
	{
		safef(command, sizeof(command), "../bin/draw_tree -b %s | grep -v translate | sed '5 i 10 530 translate' | sed '6 i 0.33 0.33 scale' | sed 's/findfont 12/findfont 24/' > %s",
			  treeFileName, treeTmpPsFileName);
		mustSystem(command);

		FILE *fi = mustOpen(treeTmpPsFileName, "r");
		FILE *fo = mustOpen(treePsFileName, "w");
		if ((fi != NULL) && (fo != NULL))
		{
			mustGetLine(fi, buffer, 512);
			while (!sameWord(buffer, ""))
			{
				if (endsWith(buffer, searchTerm))
				{
					fprintf(fo, "255 0 0 setrgbcolor\n");
					fprintf(fo, "%s", buffer);
					fprintf(fo, "0 0 0 setrgbcolor\n");
				}
				else
					fprintf(fo, "%s", buffer);
				mustGetLine(fi, buffer, 512);
			}
			fclose(fo);
			fclose(fi);
		}
		safef(command, sizeof(command), "rm -f %s", treeTmpPsFileName);
		mustSystem(command);
	}

	if (!fileExists(treePngFileName))
	{
		safef(command, sizeof(command), "pstoimg -scale 0.4 -aaliastext -type png -density 300 -out %s %s",
			  treePngFileName, treePsFileName);
		mustSystem(command);
	}

	if (!fileExists(treePdfFileName))
	{
		safef(command, sizeof(command), "ps2pdf %s %s",
			  treePsFileName, treePdfFileName);
		mustSystem(command);
	}

	printf("<BR><B>Phylogenetic tree with homologous genes:</B> View as \n");
	printf("<A HREF=\"%s\" TARGET=_BLANK>Postscript</A>&nbsp;\n", treePsFileName);
	printf("<A HREF=\"%s\" TARGET=_BLANK>PNG</A>&nbsp;\n", treePngFileName);
	printf("<A HREF=\"%s\" TARGET=_BLANK>PDF</A>&nbsp;\n", treePdfFileName);
	printf("<A HREF=\"%s\" TARGET=_BLANK>Text</A>&nbsp;<BR>\n", treeFileName);

	printf("<BR>The 20 highest scoring homologous genes of this protein were retrieved by BLASTP from the genomes available in the UCSC microbial genome browser. ");
	printf("The alignments of the protein sequences, made by <A HREF=\"http://www.drive5.com/muscle/\" TARGET=_BLANK>MUSCLE</A>, ");
	printf("were used to construct the phylogenetic tree. This maximum likelihood unrooted tree was computed by using ");
	printf("<A HREF=\"http://www.atgc-montpellier.fr/phyml/\" TARGET=_BLANK>PHYML</A> with the Jones Taylor Thornton model of sequence evolution, ");
	printf("by including a Gamma-correction (eight categories of evolutionary rates, ");
	printf("an estimated alpha-parameter and an estimated proportion of invariant sites).<BR>");
}

/* Self Homologs */
if (selfBlastpHitCount(conn, item) > 0)
{
	blastpHitsList = loadSelfBlastpHits(conn, item, 0);
	printSelfHomologs(conn, blastpHitsList);
}
printf("</td></tr></tbody></table><br>\n");

/* Pathway */
printf("<table style=\"text-align: left; width: 99%%;\" border=\"1\" cellpadding=\"5\" cellspacing=\"0\">\n");
printf("<tbody><tr><td style=\"background-color:#eee9e9;\">\n");
printf("<a name=\"pathway\"></a><b>Pathway</b><br></td></tr>\n");
printf("<tr><td>\n");

/* kegg pathway links */
if (keggCount(conn, item) > 0)
{
    keggLink(conn, item, table, "<B>Kegg Pathway: </b><BR>");
}
else
	printf("Not available\n");
printf("</td></tr></tbody></table><br>\n");

/* Do SAM-T02 sub-section */
//doSamT02(spAcc, database);

printTrackHtml(tdb);
hFreeConn(&conn);
}


/*Code to display Sargasso Sea Information*/
void doSargassoSea(struct trackDb *tdb, char *trnaName)
{
struct bed *cb=NULL;
struct sargassoSeaXra *cbs=NULL, *cbs2, *list=NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char query[512];
char *dupe, *words[16];
char **row;
char tempstring[255]="";
int flag,  z, dashes, wordCount, rowOffset;
int start = cartInt(cart, "o"), num = 0, flag2=0;
float sequenceLength, dashlength=60;

genericHeader(tdb,trnaName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
genericBedClick(conn, tdb, trnaName, start, num);
rowOffset = hOffsetPastBin(database, seqName, tdb->table);

sprintf(query, "select * from %s where name = '%s'", tdb->table, trnaName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    cb=bedLoadN(row+1, 6);
sequenceLength=(cb->chromEnd - cb->chromStart);
if(sequenceLength<0){ sequenceLength=sequenceLength*-1;}
sequenceLength=sequenceLength/3;
dashlength=sequenceLength/60;

/*Query the database for the extrainfo file for sargassoSea*/
conn=hAllocConn(database);/*sqlConnect(dupe);*/
safef(tempstring, sizeof(tempstring),"select * from sargassoSeaXra where qname = '%s'", trnaName);
sr = sqlGetResult(conn, tempstring);

/*Load the required data from the database*/
while ((row = sqlNextRow(sr)) != NULL)
    {
    cbs=sargassoSeaXraLoad(row);
    slAddHead(&list, cbs);
    }
slReverse(&list);

flag=0;
flag2=0;

/*Print out table with Blast information*/
 printf("   </tbody>\n</table>\n<br><br>");

printf("<table cellpadding=\"2\" cellspacing=\"2\" border=\"1\" style=\"text-align: left; width: 100%%;\">");
printf(" <tbody>\n    <tr>\n");
printf("     <td style=\"vertical-align: top;\"><b>Organism</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Nucleotides \naligned begin</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Nucelotides \naligned end</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\"><b>NCBI Link</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Evalue</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Percent Identity</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Alignment Length</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Gap openings</b>");
printf("</td>\n    </tr>\n    <tr>");

flag=0;
for(cbs2=list;cbs2!=NULL;cbs2=cbs2->next)
    {
    printf("\n      \n      <td style=\"vertical-align: top;\">");
    printf("<a name=\"%i-desc\"></a>",cbs2->GI);
    printf("<a\nhref=\"#%i-align\">%s</a>",cbs2->GI,cbs2->species);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%i",cbs2->thisseqstart);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%i",cbs2->thisseqend);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("<a\nhref=\"http://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi?db=protein&amp;val=%i\">NCBI Link</a>",cbs2->GI);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%s",cbs2->evalue);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%.2f",cbs2->PI);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%i",cbs2->length);
    printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("%i",cbs2->gap);
    printf("<br></td>\n    </tr>\n");
    flag=0;
    }
/* printf("  <br><br></tbody>\n</table>  \n"); */



/*Print out the table for the alignments*/
printf("<table cellpadding=\"2\" cellspacing=\"2\" border=\"1\" style=\"width: 100%%;\">");
printf(" <tbody>\n    <tr>\n");
printf("     <td style=\"vertical-align: top;\"><b>Organism</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\"><b>Alignment</b>");
printf("</td>\n    </tr>\n    <tr>");
printf("     <td style=\"vertical-align: top;\">%s where each plus(+) is approx. %.1f amino acids", trnaName, dashlength);
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<code>\n");
for(z=0; z<60; z++)
{
    printf("+");
}
printf("</code>");

printf("</td>\n    </tr>\n    <tr>");
flag=0;
flag2=0;

for(cbs2=list;cbs2!=NULL;cbs2=cbs2->next)
    {
    printf("\n      </td>\n      <td style=\"vertical-align: top;\">");
    dashes=cbs2->queryseqstart-cbs2->queryseqend;
    if(dashes<0) dashes=dashes*-1;
    printf("<a name=\"%i-align\"></a>",cbs2->GI);
    printf("<a\nhref=\"#%i-desc\">%s</a>, %s",cbs2->GI, cbs2->species,cbs2->qName);
    printf("\n      </td>\n      <td style=\"vertical-align: top;\">");
    printf("<code>\n");
    dashes=dashes/dashlength;
    if(cbs2->queryseqstart>cbs2->queryseqend)
    for(z=0; z<((cbs2->queryseqend)/dashlength); z++)
        {
        printf("&nbsp;");
        }
    else
        for(z=0; z<((cbs2->queryseqstart)/dashlength); z++)
        {
        printf("&nbsp;");
        }
    if(dashes<1) printf("+");
    for(z=0; z<dashes; z++) printf("+");
    printf("</code>");
    printf("</td>\n    </tr>\n");
    flag=0;


    }


/*Free the data*/

hFreeConn(&conn);
sargassoSeaXraFree(&cbs);
printTrackHtml(tdb);
}


/*Function to print out full code name from code letter*/
void printCode(char code)
{
    switch(code)
    {
    case 'a':
        printf("Aerobe");
        break;
    case 'b':
        printf("Bacteria");
        break;
    case 'c':
        printf("Crenarchaea");
        break;
    case 'd':
        printf("Acidophile");
        break;
    case 'e':
        printf("Euryarchaea");
        break;
    case 'g':
        printf("Within Genus");
        break;
    case 'h':
        printf("Hyperthermophile");
        break;
    case 'm':
        printf("Methanogen");
        break;
    case 'n':
         printf("Anaerobe");
        break;
    case 'o':
        printf("Nanoarchaea");
        break;
    case 't':
        printf("Thermophile");
        break;
    case 'u':
        printf("Eukarya");
        break;
    case 'v':
        printf("Viral");
            break;
    case 'k':
        printf("Alkaliphile");
            break;
    case 'l':
        printf("Halophile");
            break;
    case 'r':
        printf("Facultative Aerobe");
            break;
    default:
        break;
       }
}


void doTrnaGenes(struct trackDb *tdb, char *trnaName)
{
struct tRNAs *trna;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char *dupe, *words[16];
char **row;
int wordCount;
int rowOffset;

char* chrom = cartString(cart, "c");

genericHeader(tdb,trnaName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);

rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sprintf(query, "select * from %s where chrom = '%s' and name = '%s'", tdb->table, chrom, trnaName);
sr = sqlGetResult(conn, query);
printf("<TABLE>\n");
while ((row = sqlNextRow(sr)) != NULL)
  {
    printf("<TR>\n");
    printf("<TD valign=top>\n");
    trna = tRNAsLoad(row+rowOffset);

    printf("<B>tRNA name: </B> %s<BR>\n",trna->name);
    printf("<B>tRNA Isotype: </B> %s<BR>\n",trna->aa);
    printf("<B>tRNA anticodon: </B> %s<BR>\n",trna->ac);
    printf("<B>tRNAscan-SE score: </B> %.2f<BR>\n",trna->trnaScore);
    printf("<B>Intron(s): </B> %s<BR>\n",trna->intron);
    printf("<B>Genomic size: </B> %d nt<BR>\n",trna->chromEnd-trna->chromStart);
    printf("<B>Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
       hgTracksPathAndSettings(), database, trna->chrom, trna->chromStart+1, trna->chromEnd);
    printf("%s:%d-%d</A><BR>\n", trna->chrom, trna->chromStart+1, trna->chromEnd);
    printf("<B>Strand:</B> %s<BR>\n", trna->strand);
    if (!sameString(trna->genomeUrl, ""))
      {
        printf("<BR><A HREF=\"%s\" TARGET=_blank>View summary of all genomic tRNA predictions</A><BR>\n",
               trna->genomeUrl);
        printf("<BR><A HREF=\"%s\" TARGET=_blank>View tRNA alignments</A><BR>\n", trna->trnaUrl);
      }

    if (trna->next != NULL)
      printf("<hr>\n");

    printf("</TD>\n");
    printf("<TD>\n");

    if (startsWith(trna->chrom, trna->name))
      printf("<img align=right src=\"../RNA-img/%s/%s-%s.gif\" alt='tRNA secondary structure for %s'>\n",
	     database,database,trna->name,trna->name);
    else
      printf("<img align=right src=\"../RNA-img/%s/%s-%s-%s.gif\" alt='tRNA secondary structure for %s'>\n",
	   database,database,trna->chrom,trna->name,trna->name);

    printf("</TD>");
    printf("</TR>");
  }
 printf("</TABLE>");
 sqlFreeResult(&sr);
 hFreeConn(&conn);
 printTrackHtml(tdb);
 tRNAsFree(&trna);
}

void doSnornaGenes(struct trackDb *tdb, char *snornaName)
{
struct snoRNAs *snorna;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char *dupe, *words[16];
char **row;
int wordCount;
int rowOffset;

genericHeader(tdb,snornaName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);

rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sprintf(query, "select * from %s where name = '%s'", tdb->table, snornaName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
  {
    snorna = snoRNAsLoad(row+rowOffset);

    printf("<B>sRNA name: </B> %s<BR>\n",snorna->name);
    printf("<B>Snoscan score: </B> %.2f<BR>\n",snorna->snoScore);
    printf("<B>HMM snoRNA score: </B> %.2f<BR>\n",snorna->hmmScore);
    printf("<B>Predicted targets: </B> %s<BR>\n",snorna->targetList);
    printf("<B>Predicted guide interactions:</B><pre>%s</pre>\n",snorna->guideStr);
    printf("<B>Possible sRNA homolog(s): </B> %s<BR>\n",snorna->orthologs);

    printf("<BR><B>Genomic size: </B> %d nt<BR>\n",snorna->chromEnd-snorna->chromStart);
    printf("<B>Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
       hgTracksPathAndSettings(), database, snorna->chrom, snorna->chromStart+1, snorna->chromEnd);
    printf("%s:%d-%d</A><BR>\n", snorna->chrom, snorna->chromStart+1, snorna->chromEnd);
    printf("<B>Strand:</B> %s<BR><BR>\n", snorna->strand);
    printf("<B>Snoscan program output: </B><BR><pre> %s</pre><BR>\n",snorna->snoscanOutput);
    if (snorna->next != NULL)
      printf("<hr>\n");
  }
 sqlFreeResult(&sr);
 hFreeConn(&conn);
 printTrackHtml(tdb);
 snoRNAsFree(&snorna);
}

void doGbRnaGenes(struct trackDb *tdb, char *gbRnaName)
{
struct gbRNAs *gbRna;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char *dupe, *words[16];
char **row;
int wordCount;
int rowOffset;

genericHeader(tdb,gbRnaName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);


rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sprintf(query, "select * from %s where name = '%s'", tdb->table, gbRnaName);
sr = sqlGetResult(conn, query);


while ((row = sqlNextRow(sr)) != NULL)
  {

    gbRna = gbRNAsLoad(row+rowOffset);

    printf("<B>Genbank ncRNA name: </B> %s<BR>\n",gbRna->name);
    printf("<B>Product Description/Note: </B> %s<BR>\n",gbRna->product);
    printf ("<B>Intron(s): </B> %s<BR>\n",gbRna->intron);

    printf("<BR><B>Genomic size: </B> %d nt<BR>\n",gbRna->chromEnd-gbRna->chromStart);
    printf("<B>Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
       hgTracksPathAndSettings(), database, gbRna->chrom, gbRna->chromStart+1, gbRna->chromEnd);
    printf("%s:%d-%d</A><BR>\n", gbRna->chrom, gbRna->chromStart+1, gbRna->chromEnd);
    printf("<B>Strand:</B> %s<BR>\n", gbRna->strand);

    if (gbRna->next != NULL)
      printf("<hr>\n");

  }
 sqlFreeResult(&sr);
 hFreeConn(&conn);
 printTrackHtml(tdb);
 gbRNAsFree(&gbRna);

}

void doEasyGenes(struct trackDb *tdb, char *egName)
{
struct easyGene *egList = NULL, *eg;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;

genericHeader(tdb,egName);
rowOffset = hOffsetPastBin(database, seqName, tdb->table);
sprintf(query, "select * from %s where name = '%s'", tdb->table, egName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    slAddTail(&egList,easyGeneLoad(row+rowOffset));
slReverse(&egList);
sqlFreeResult(&sr);
hFreeConn(&conn);
for (eg = egList; eg != NULL; eg = eg->next)
    {
    if (eg->genbank[0] == 'Y')
    printf("<span style='color:#FF0000;'>\n");
    else
    printf("<span style='color:#000000;'>\n");
    printf("<B>Item:</B> %s<BR>\n",eg->name);
    printf("<B>Feature identifier:</B> %s<BR>\n",eg->feat);
    printf("<B>Start codon:</B> %s<BR>\n",eg->startCodon);
    printf("<B>EasyGene descriptor:</B> %s<BR>\n",eg->descriptor);
    if (eg->R >= 0.001)
    printf("<B>R value:</B> %.3f<BR>\n",eg->R);
    else
    printf("<B>R value:</B> %.2e<BR>\n",eg->R);
    printf("<B>Log-odds :</B> %.1f<BR>\n",eg->logOdds);
    printf("<B>Frame:</B> %d<BR>\n",eg->frame);
    printf("<B>Swiss-Prot match:</B> %s<BR>\n",eg->swissProt);
    printf("<B>ORF identifier:</B> %s<BR>\n",eg->orf);
    printPos(eg->chrom, eg->chromStart, eg->chromEnd, eg->strand, TRUE, eg->name);
    printf("</span>\n");
    if (eg->next != NULL)
    printf("<hr>\n");
    }
printTrackHtml(tdb);
easyGeneFreeList(&egList);
}

void doCodeBlast(struct trackDb *tdb, char *trnaName)
{
struct pepPred *pp=NULL;
struct codeBlast *cb=NULL;
struct codeBlastScore *cbs=NULL, *cbs2, *list=NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char query[512];
char *dupe, *words[16];
char **row;
char tempstring[255]="";
int flag, z, dashes, wordCount, rowOffset, currentGI=0;
int start = cartInt(cart, "o"), num = 0, flag2=0;
float sequenceLength, dashlength=60;

genericHeader(tdb,trnaName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
genericBedClick(conn, tdb, trnaName, start, num);
rowOffset = hOffsetPastBin(database, seqName, tdb->table);

sprintf(query, "select * from %s where name = '%s'", tdb->table, trnaName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cb=codeBlastLoad(row);
    }

sequenceLength=(cb->chromEnd - cb->chromStart);
if(sequenceLength<0){ sequenceLength=sequenceLength*-1;}
sequenceLength=sequenceLength/3;
dashlength=sequenceLength/60;

conn=hAllocConn(database);/*sqlConnect(dupe);*/
sprintf(query, "select * from gbProtCodePep where name = '%s'", trnaName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    pp=pepPredLoad(row);
    }


/*Query the database for the extrainfo file for codeBlast*/
conn=hAllocConn(database);/*sqlConnect(dupe);*/
safef(tempstring, sizeof(tempstring), "select * from codeBlastScore where qname = '%s'", trnaName);

sr = sqlGetResult(conn, tempstring);

/*Load the required data from the database*/
while ((row = sqlNextRow(sr)) != NULL)
    {
    cbs=codeBlastScoreLoad(row);
    slAddHead(&list, cbs);
    }

if (pp!=NULL)
    printf(
"<br><a\nhref=\"http://www.ncbi.nlm.nih.gov/BLAST/Blast.cgi?ALIGNMENTS=250&ALIGNMENT_VIEW=Pairwise&AUTO_FORMAT=Semiauto&CDD_SEARCH=on&CLIENT=web&DATABASE=nr&DESCRIPTIONS=500&ENTREZ_QUERY=All+organisms&EXPECT=10&FILTER=L&FORMAT_BLOCK_ON_RESPAGE=None&FORMAT_ENTREZ_QUERY=All+organisms&FORMAT_OBJECT=Alignment&FORMAT_TYPE=HTML&GAPCOSTS=11+1&GET_SEQUENCE=on&I_THRESH=0.005&LAYOUT=TwoWindows&MASK_CHAR=0&MASK_COLOR=0&MATRIX_NAME=BLOSUM62&NCBI_GI=on&NEW_FORMATTER=on&PAGE=Proteins&PROGRAM=blastp&QUERY=%s&SERVICE=plain&SET_DEFAULTS=Yes&SET_DEFAULTS.x=25&SET_DEFAULTS.y=11&SHOW_LINKOUT=on&SHOW_OVERVIEW=on&WORD_SIZE=3&END_OF_HTTPGET=Yes\">Query NCBI Blast",pp->seq);

/*Print out table with Blast information*/
printf("   </tbody>\n</table>\n<br><br><table cellpadding=\"2\" cellspacing=\"2\" border=\"1\" style=\"text-align: left; width: 100%%;\">");
printf(" <tbody>\n    <tr>\n");
printf("     <td style=\"vertical-align: top;\"><b>Organism</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Phylogenetic/Functional Category</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Gene Name</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Product</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\"><b>NCBI Entry</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Evalue</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Percent Identity</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Alignment Length(AA)</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<b>Gap openings</b>");
printf("</td>\n    </tr>\n    <tr>");

flag=0;
for(cbs2=list;cbs2!=NULL;cbs2=cbs2->next)
    {
    if(sameString(trnaName, cbs2->qName))
        {
    if(flag==0)
        {
        currentGI=cbs2->GI;
        printf("\n      \n      <td style=\"vertical-align: top;\">");
        printf("<a name=\"%i-desc\"></a>",cbs2->GI);

        printf("<a\nhref=\"#%i-align\">%s</a>",cbs2->GI,cbs2->species);
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");

        flag=1;
        }

    if((cbs2->next!=NULL) && (currentGI== cbs2->GI) && (currentGI== cbs2->next->GI)  )
        {
        printCode(cbs2->code[0]);
            printf(", ");
        }

    else
        {

        printCode(cbs2->code[0]);

        /*See which database to link to*/
        if((sameString(cbs2->species, "Pyrococcus furiosus"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=pyrFur2&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Pyrococcus abyssi"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=pyrAby1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Pyrococcus horikoshii"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=pyrHor1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Thermococcus kodakaraensis"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=therKoda1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Sulfolobus solfataricus"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=sulSol1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Sulfolobus tokodaii"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=sulfToko1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Sulfolobus acidocaldarius"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=sulfAcid1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Pyrobaculum aerophilum"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=pyrAer1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Nanoarchaeum equitans"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=nanEqu1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Methanosarcina acetivorans"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=metAce1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Methanosarcina barkeri"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=methBark1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Methanosarcina mazei"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=methMaze1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Methanothermobacter thermautotrophicus"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=methTher1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Methanococcoides burtonii"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=methBurt1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Methanopyrus kandleri"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=methKand1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Haloarcula marismortui"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=halMar1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Halobacterium sp."))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=haloHalo1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Aeropyrum pernix"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=aerPer1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Archaeoglobus fulgidus"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=archFulg1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Thermoplasma acidophilum"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=therAcid1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Thermoplasma volcanium"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=therVolc1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Methanocaldococcus jannaschii"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=methJann1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Methanococcus maripaludis"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=metMar1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Methanosphaera stadtmanae"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=methStad1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Methanospirillum hungatei"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=methHung1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Aquifex aeolicus"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=aquiAeol1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Thermotoga maritima"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=therMari1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Escherichia coli K12"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=eschColi_K12_1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Bacillus subtilis"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=baciSubt1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else if((sameString(cbs2->species, "Shewanella oneidensis"))){
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"%s?db=shewOnei1&position=%s\">%s</a>",hgTracksName(),cbs2->name,cbs2->name);
        }
        else{
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("%s",cbs2->name);
        }
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("%s",cbs2->product);
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<a\nhref=\"http://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi?db=protein&amp;val=%i\">NCBI Link</a>",cbs2->GI);
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("%s",cbs2->evalue);
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("%.2f",cbs2->PI);
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("%i",cbs2->length);
        printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("%i",cbs2->gap);
        printf("<br></td>\n    </tr>\n");
        flag=0;

        }
        }
    }

    /*Print out the table for the alignments*/
printf("</td></td></td><br>\n<table cellpadding=\"2\" cellspacing=\"2\" border=\"1\" style=\"width: 100%%;\">");
printf(" <tbody>\n    <tr>\n");
printf("     <td style=\"vertical-align: top;\"><b>Organism</b>");
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\"><b>Alignment</b>");
printf("</td>\n    </tr>\n    <tr>");
printf("     <td style=\"vertical-align: top;\">%s where each plus(+) is approx. %f amino acids", trnaName, dashlength);
printf("<br>\n      </td>\n      <td style=\"vertical-align: top;\">");
printf("<code>\n");
for(z=0; z<60; z++)
{
    printf("+");
}
printf("</code>");

printf("</td>\n    </tr>\n    <tr>");
flag=0;
flag2=0;

for(cbs2=list;cbs2!=NULL;cbs2=cbs2->next)
    {
    if(sameString(trnaName, cbs2->qName));
        {
    if(flag==0)
        {
        currentGI=cbs2->GI;
        printf("\n      </td>\n      <td style=\"vertical-align: top;\">");
        flag=1;
        }
    if((cbs2->next!=NULL) && (currentGI== cbs2->GI) && (currentGI== cbs2->next->GI) )
        {
        }
    else
        {
        dashes=cbs2->seqend-cbs2->seqstart;
        if(cbs2->length<dashes/2){dashes=dashes/3;}
        if(dashes<0) dashes=dashes*-1;
        printf("<a name=\"%i-align\"></a>",cbs2->GI);
            printf("<a\nhref=\"#%i-desc\">%s</a>, %s",cbs2->GI, cbs2->species, cbs2->name);
        printf("\n      </td>\n      <td style=\"vertical-align: top;\">");
        printf("<code>\n");
        dashes=dashes/dashlength;
        if(cbs2->seqstart>cbs2->seqend)
        for(z=0; z<((cbs2->seqend)/dashlength); z++)
            {
            printf("&nbsp;");
            }
        else
        for(z=0; z<((cbs2->seqstart)/dashlength); z++)
            {
            printf("&nbsp;");
            }
        if(dashes<1) printf("+");
        for(z=0; z<dashes; z++) printf("+");
        printf("</code>");
        printf("</td>\n    </tr>\n");
        flag=0;

        }

        }

    }

printf("  <br><br></tbody>\n</table>  \n");
/*\printf("  </tbody>\n</table>  \n");
*/
/*Free the data*/

hFreeConn(&conn);
codeBlastScoreFree(&cbs);
codeBlastFree(&cb);
printTrackHtml(tdb);
}


void llDoCodingGenes(struct trackDb *tdb, char *item,
             char *pepTable, char *extraTable)
/* Handle click on gene track. */
{
struct minGeneInfo ginfo;
char query[256];
char query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
char *words[16], *dupe = cloneString(tdb->type);
int wordCount, x, length;
int start = cartInt(cart, "o"), num = 0;
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2;
struct COG *COG=NULL;
struct COGXra *COGXra=NULL;
char *temparray[160];

genericHeader(tdb, item);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
genericBedClick(conn, tdb, item, start, num);
if (pepTable != NULL && hTableExists(database, pepTable))
    {
    char *pepNameCol = sameString(pepTable, "gbSeq") ? "acc" : "name";
    conn = hAllocConn(database);
    /* simple query to see if pepName has a record in pepTable: */
    safef(query, sizeof(query), "select 0 from %s where %s = '%s'",
      pepTable, pepNameCol, item);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
    {
    hgcAnchorSomewhere("htcTranslatedProtein", item, pepTable, seqName);
    printf("Predicted Protein</A> <BR>\n");
    }
    sqlFreeResult(&sr);
    }
if (extraTable != NULL && hTableExists(database, extraTable))
    {
    conn = hAllocConn(database);
    sprintf(query, "select * from %s where name = '%s'", extraTable, item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
    minGeneInfoStaticLoad(row, &ginfo);
    printf("<B>Product: </B>%s<BR>\n", ginfo.product);
    printf("<B>Note: </B>%s<BR>\n", ginfo.note);
    }
    sqlFreeResult(&sr);
    }
if (hTableExists(database, "COG"))
    {
    conn = hAllocConn(database);
    sprintf(query, "select * from COG where name = '%s'", item);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
    COG = COGLoad(row);
    if(COG!=NULL)
        {
            length=chopString(COG->COG, "," , temparray, 999);
        for(x=0; x<length; x++)
        {
        conn2 = hAllocConn(database);
        sprintf(query2, "select * from COGXra where name = '%s'", temparray[x]);
                sr2 = sqlGetResult(conn2, query2);
        while ((row2 = sqlNextRow(sr2)) != NULL)
                {
            COGXra=COGXraLoad(row2);
            if(COGXra!=NULL)
                printf("<B>COG: </B>%s <B>INFO: </B>%s<BR>\n", COGXra->name, COGXra->info);
            }
            sqlFreeResult(&sr2);
            hFreeConn(&conn2);
            }
         }
     }
    }
printTrackHtml(tdb);
hFreeConn(&conn);
}

/*void doTigrOperons(struct trackDb *tdb, char *opName)*/
/* track handler for the TIGR operon predictions */
/*{
struct tigrOperon *op;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char *dupe, *words[16];
char **row;
int wordCount;
int start = cartInt(cart, "o"), num = 0;

genericHeader(tdb,opName);
dupe = cloneString(tdb->type);
wordCount = chopLine(dupe, words);
if (wordCount > 1)
    num = atoi(words[1]);
if (num < 3) num = 3;
genericBedClick(conn, tdb, opName, start, num);
sprintf(query, "select * from %sInfo where name = '%s'", tdb->table, opName);
sr = sqlGetResult(conn, query);*/
/* Make the operon table like on the TIGR web page. */
/*if ((row = sqlNextRow(sr)) != NULL)
    {
    int i,j;
    char *infos[30];
    op = tigrOperonLoad(row);
    chopCommas(op->info,infos);
    printf("<P>\n<TABLE BORDER=1 ALIGN=\"CENTER\">\n");
    for (i = 0; i <= op->size; i++)
    {
    printf("  <TR ALIGN=\"CENTER\">");
    for (j = 0; j <= op->size; j++)
        {
        printf("<TD>");
        if ((i == 0 || j == 0) && !(i == 0 && j == 0))
        (i > j) ? printf("%s",op->genes[i-1]) : printf("%s",op->genes[j-1]);
        else if (i + j > 0)
        {
        char *data = infos[((i-1)*op->size)+(j-1)];
        if (!sameString(data,"---"))
            {
            char *n, *conf;
            n = chopPrefixAt(data,'|');
            conf = data;
            printf("confidence = %.2f, n = %d",atof(conf),atoi(n));
            }
        else
            printf("%s",data);
        }
        printf("</TD>");
        }
    printf("</TR>\n");
    }
    printf("</TABLE>\n</P>\n");
    }
printTrackHtml(tdb);*/
/* clean up */
/*sqlFreeResult(&sr);
hFreeConn(&conn);
tigrOperonFree(&op);
}
*/

void doTigrCmrGene(struct trackDb *tdb, char *tigrName)
/* Handle the TIRG CMR gene track. */
{
  struct tigrCmrGene *tigr;
  char query[512];
  struct sqlConnection *conn = hAllocConn(database);
  struct sqlResult *sr;
  char *dupe, *words[16];
  char **row;
  int wordCount;
  int rowOffset;
  /* int start = cartInt(cart, "o"), num = 0; */

  genericHeader(tdb,tigrName);
  dupe = cloneString(tdb->type);
  wordCount = chopLine(dupe, words);

  rowOffset = hOffsetPastBin(database, seqName, tdb->table);
  sprintf(query, "select * from %s where name = '%s'", tdb->table, tigrName);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
      tigr = tigrCmrGeneLoad(row);
      if (tigr != NULL)
    {
      printf("<B>TIGR locus name: </B> %s<BR>\n",tigrName);
      printf("<B>TIGR gene description: </B> %s<BR>\n",tigr->tigrCommon);
      printf("<B>Alternate TIGR gene name: </B> ");
      if (strlen(tigr->tigrGene) >0) {
        printf("%s<BR>\n",tigr->tigrGene);
      }
      else {
        printf("None<BR>");
      }
      printf("<B>Genbank locus name: </B> %s<BR>\n",tigr->primLocus);
      printf("<B>Protein length: </B> %d aa<BR>\n",tigr->tigrPepLength);
      if (strlen(tigr->tigrECN)>0) {
        printf("<B>Enzyme comission number: </B> %s<BR>\n",tigr->tigrECN);
      }
      printf("<B>Main role: </B> %s<BR>\n",tigr->tigrMainRole);
      printf("<B>Subrole: </B> %s<BR>\n",tigr->tigrSubRole);
      if (tigr->tigrMw > 0) {
        printf("<B>Molecular weight: </B> %.2f Da<BR>\n",tigr->tigrMw);
      }
      if (tigr->tigrPi > 0) {
        printf("<B>Isoelectric point: </B> %.2f<BR>\n",tigr->tigrPi);
      }
      printf("<B>GC content: </B> %.2f %%<BR>\n",tigr->tigrGc);

      printf("<BR><B>Position:</B> "
         "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
         hgTracksPathAndSettings(), database, tigr->chrom, tigr->chromStart, tigr->chromEnd);
      printf("%s:%d-%d</A><BR>\n", tigr->chrom, tigr->chromStart, tigr->chromEnd);
      printf("<B>Strand:</B> %s<BR>\n", tigr->strand);
      printf("<B>Genomic size: </B> %d nt<BR>\n",tigr->tigrLength);
      if (tigr->next != NULL)
        printf("<hr>\n");
    }
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  printTrackHtml(tdb);
  tigrCmrGeneFree(&tigr);
}

void doJgiGene(struct trackDb *tdb, char *jgiName)
/* Handle the JGI gene track. */
{
  struct jgiGene *jgi;
  char query[512];
  struct sqlConnection *conn = hAllocConn(database);
  struct sqlResult *sr;
  char *dupe, *words[16];
  char **row;
  int wordCount;
  int rowOffset;

  genericHeader(tdb,jgiName);
  dupe = cloneString(tdb->type);
  wordCount = chopLine(dupe, words);

  rowOffset = hOffsetPastBin(database, seqName, tdb->table);
  sprintf(query, "select * from %s where name = '%s'", tdb->table, jgiName);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
      jgi = jgiGeneLoad(row+rowOffset);
      printf("<B>JGI locus name: </B> %s<BR>\n",jgiName);
      printf("<B>JGI gene symbol: </B> %s<BR>\n",jgi->jgiSymbol);
      printf("<B>JGI gene description: </B> %s<BR>\n",jgi->jgiDescription);
      printf("<B>JGI gene id:</B> "
             "<A HREF=\"http://img.jgi.doe.gov/cgi-bin/pub/main.cgi?section=GeneDetail&page=geneDetail&gene_oid=%s\" TARGET=_blank>",
             jgi->jgiGeneId);
      printf("%s</A><BR>\n", jgi->jgiGeneId);
      printf("<B>GC content: </B> %.0f %%<BR>\n",jgi->jgiGc);

      printf("<BR><B>Position:</B> "
             "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
             hgTracksPathAndSettings(), database, jgi->chrom, jgi->chromStart + 1, jgi->chromEnd);
      printf("%s:%d-%d</A><BR>\n", jgi->chrom, jgi->chromStart + 1, jgi->chromEnd);
      printf("<B>Strand:</B> %s<BR>\n", jgi->strand);
      printf("<B>Genomic size: </B> %d nt<BR>\n", (jgi->chromEnd - jgi->chromStart));
      if (jgi->next != NULL)
        printf("<hr>\n");
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  printTrackHtml(tdb);
  jgiGeneFree(&jgi);
}

void doPfamHit(struct trackDb *tdb, char *hitName)
/* Handle the Pfam hits track. */
{
  struct lowelabPfamHits *pfamHit;
  char query[512];
  struct sqlConnection *conn = hAllocConn(database);
  struct sqlConnection *spConn = NULL;
  struct sqlResult *sr;
  char *dupe, *words[16];
  char **row;
  int wordCount;
  int rowOffset;
  char *description;
  int start = cartInt(cart, "o");
  char *pdb = hPdbFromGdb(database);
  spConn = sqlConnect(pdb);

  genericHeader(tdb,hitName);
  dupe = cloneString(tdb->type);
  wordCount = chopLine(dupe, words);

  rowOffset = hOffsetPastBin(database, seqName, tdb->table);

  sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d", tdb->table, hitName,seqName,start);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
      pfamHit = lowelabPfamHitsLoad(row+rowOffset);

      safef(query, sizeof(query), "select description from proteome.pfamDesc where pfamAC='%s'", pfamHit->pfamAC);

    if (!sqlTableExists(spConn,"proteome.pfamDesc"))
        {
        safef(query, sizeof(query),
        "select extDbRef.extAcc1 from extDbRef,extDb "
        "where extDbRef.acc = '%s' "
        "and extDbRef.extDb = extDb.id "
        "and extDb.val = '%s'"
        , pfamHit->pfamAC,pfamHit->pfamID);
        }

    description = sqlQuickString(spConn, query);
    if (description == NULL)
        description = cloneString("n/a");

    printf("<A HREF=\"http://pfam.sanger.ac.uk/family?acc=%s\" TARGET=_blank>",
           pfamHit->pfamAC );
    printf("%s</A> - %s<BR><BR>\n", pfamHit->pfamAC, description);
    freez(&description);

    printf("<B>Domain assignment based on %d%% identity BlastP hit to Pfam-A SwissProt Sequence:</B><BR>\n"
           "<A HREF=\"http://www.expasy.org/uniprot/%s\">%s</A> (%s, %d%% full-length alignment) "
         "<BR><A HREF=\"http://www.sanger.ac.uk/cgi-bin/Pfam/swisspfamget.pl?name=%s\">[Pfam Domain Structure]</A><BR>\n",
         pfamHit->ident,pfamHit->swissAC,pfamHit->swissAC,pfamHit->protCoord,pfamHit->percLen,pfamHit->swissAC);

      printf("<BR><B>Position:</B> "
             "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
             hgTracksPathAndSettings(), database, pfamHit->chrom, pfamHit->chromStart + 1, pfamHit->chromEnd);
      printf("%s:%d-%d</A><BR>\n", pfamHit->chrom, pfamHit->chromStart + 1, pfamHit->chromEnd);
      printf("<B>Strand:</B> %s<BR>\n", pfamHit->strand);
      printf("<B>Genomic size: </B> %d nt<BR>\n", (pfamHit->chromEnd - pfamHit->chromStart));
      if (pfamHit->next != NULL)
        printf("<hr>\n");
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  printTrackHtml(tdb);
  lowelabPfamHitsFree(&pfamHit);
}

void doTigrOperons(struct trackDb *tdb, char *tigrOperonName)
/* Handle the TIGR operons track. */
{
    struct bed *tigrOperon;
    struct lowelabTIGROperonScore *tigrOperonScore;
    char query[512];
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char *dupe, *words[16];
    char **row;
    int wordCount;
    int rowOffset;
    int bedSize = 0;

    genericHeader(tdb, tigrOperonName);

    dupe = cloneString(tdb->type);
    wordCount = chopLine(dupe, words);
    if (wordCount > 1)
        bedSize = atoi(words[1]);
    if (bedSize < 3) bedSize = 3;

    rowOffset = hOffsetPastBin(database, seqName, tdb->table);
    sprintf(query, "select * from %s where name = '%s'", tdb->table, tigrOperonName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
        tigrOperon = bedLoadN(row+rowOffset, bedSize);
        printf("<B>Operon name: </B> %s<BR>\n",tigrOperonName);

        printf("<BR><B>Position:</B> "
               "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
               hgTracksPathAndSettings(), database, tigrOperon->chrom, tigrOperon->chromStart + 1, tigrOperon->chromEnd);
        printf("%s:%d-%d</A><BR>\n", tigrOperon->chrom, tigrOperon->chromStart + 1, tigrOperon->chromEnd);
        printf("<B>Strand:</B> %s<BR>\n", tigrOperon->strand);
        printf("<B>Genomic size: </B> %d nt<BR>\n", (tigrOperon->chromEnd - tigrOperon->chromStart));
        if (tigrOperon->next != NULL)
            printf("<hr>\n");
    }
    sqlFreeResult(&sr);

    printf("<br><B>OperonDB predicted gene pairs</B><br>\n");

    /* Print table */
    printf("<table style=\"width: 50%%;\" bgcolor=\"#%s\" border=\"0\" cellpadding=\"1\" cellspacing=\"0\">", HG_COL_BORDER);
    printf("<tbody><tr><td>\n");
    printf("<table style='width:100%%; text-align:left; background-color:#%s;' border=1 cellpadding=2 cellspacing=2>\n", HG_COL_INSIDE);
    printf("<tbody>\n");

    /* Print table column heading */
    printf("<tr style=\"vertical-align: top;\">\n");
    printf("<td width=\"25%%\"><b>Gene 1</b></td>\n");
    printf("<td width=\"25%%\"><b>Gene 2</b></td>\n");
    printf("<td width=\"25%%\"><b>Confidence</b></td>\n");
    printf("<td width=\"25%%\"><b>Number of Conserved Genomes</b></td>\n");
    printf("</tr>\n");

    sprintf(query, "select * from lowelabTIGROperonScore where name = '%s'", tigrOperonName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
        tigrOperonScore = lowelabTIGROperonScoreLoad(row);
        printf("<tr style=\"vertical-align: top;\">\n");

        printf("<td>%s</td>\n", tigrOperonScore->gene1);
        printf("<td>%s</td>\n", tigrOperonScore->gene2);
        printf("<td style=\"text-align: right;\">%d</td>\n", tigrOperonScore->confidence);
        printf("<td style=\"text-align: right;\"><A HREF=\"%s\">%d</A></td>\n", tigrOperonScore->ortholog_link, tigrOperonScore->ortholog);

        printf("</tr>\n");

        tigrOperonScore = tigrOperonScore->next;
    }
    sqlFreeResult(&sr);

    /* Close table */
    printf("</tbody>\n");
    printf("</table>\n");
    printf("</td></tr></tbody>\n");
    printf("</table>\n");

    printf("<br>Note:\n");
    printf("<br>Confidence - an estimation of the lower boundary of the probability that the two corresponding genes are located in the same operon\n");
    printf("<br>Number of Conserved Genomes - number of other genomes that have the same pair of genes located in the same directon (a set of consecutive genes on the same DNA strand)\n");

    bedFree(&tigrOperon);
    lowelabTIGROperonScoreFree(&tigrOperonScore);

    hFreeConn(&conn);
    printTrackHtml(tdb);
}

void doArkinOperons(struct trackDb *tdb, char *arkinOperonName)
/* Handle the Arkin operons track. */
{
    struct bed *arkinOperon;
    struct lowelabArkinOperonScore *arkinOperonScore;
    char query[512];
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char *dupe, *words[16];
    char **row;
    int wordCount;
    int rowOffset;
    int bedSize = 0;

    genericHeader(tdb, arkinOperonName);

    dupe = cloneString(tdb->type);
    wordCount = chopLine(dupe, words);
    if (wordCount > 1)
        bedSize = atoi(words[1]);
    if (bedSize < 3) bedSize = 3;

    rowOffset = hOffsetPastBin(database, seqName, tdb->table);
    sprintf(query, "select * from %s where name = '%s'", tdb->table, arkinOperonName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
        arkinOperon = bedLoadN(row+rowOffset, bedSize);
        printf("<B>Arkin operon name: </B> %s<BR>\n",arkinOperonName);

        printf("<BR><B>Position:</B> "
               "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
               hgTracksPathAndSettings(), database, arkinOperon->chrom, arkinOperon->chromStart + 1, arkinOperon->chromEnd);
        printf("%s:%d-%d</A><BR>\n", arkinOperon->chrom, arkinOperon->chromStart + 1, arkinOperon->chromEnd);
        printf("<B>Strand:</B> %s<BR>\n", arkinOperon->strand);
        printf("<B>Genomic size: </B> %d nt<BR>\n", (arkinOperon->chromEnd - arkinOperon->chromStart));
        if (arkinOperon->next != NULL)
            printf("<hr>\n");
    }
    sqlFreeResult(&sr);

    printf("<br><B>Arkin operon predicted gene pairs</B><br>\n");

    /* Print table */
    printf("<table style=\"width: 50%%;\" bgcolor=\"#%s\" border=\"0\" cellpadding=\"1\" cellspacing=\"0\">", HG_COL_BORDER);
    printf("<tbody><tr><td>\n");
    printf("<table style='width:100%%; text-align:left; background-color:#%s;' border=1 cellpadding=2 cellspacing=2>\n", HG_COL_INSIDE);
    printf("<tbody>\n");

    /* Print table column heading */
    printf("<tr style=\"vertical-align: top;\">\n");
    printf("<td width=\"25%%\"><b>Gene 1</b></td>\n");
    printf("<td width=\"25%%\"><b>Gene 2</b></td>\n");
    printf("<td width=\"25%%\"><b>Probability of the Same Operon</b></td>\n");
    printf("<td width=\"25%%\"><b>Gene neighbor score</b></td>\n");
    printf("</tr>\n");

    sprintf(query, "select * from lowelabArkinOperonScore where name = '%s'", arkinOperonName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
        arkinOperonScore = lowelabArkinOperonScoreLoad(row);
        printf("<tr style=\"vertical-align: top;\">\n");

        printf("<td>%s</td>\n", arkinOperonScore->gene1);
        printf("<td>%s</td>\n", arkinOperonScore->gene2);
        printf("<td style=\"text-align: right;\">%0.3f</td>\n", arkinOperonScore->prob);
        printf("<td style=\"text-align: right;\">%0.3f</td>\n", arkinOperonScore->gnMinus);

        printf("</tr>\n");

        arkinOperonScore = arkinOperonScore->next;
    }
    sqlFreeResult(&sr);

    /* Close table */
    printf("</tbody>\n");
    printf("</table>\n");
    printf("</td></tr></tbody>\n");
    printf("</table>\n");

    printf("<br>Note:\n");
    printf("<br>Probability of the Same Operon - Estimated probability that the pair is in the same operon. Values near 1 or 0 are confident predictions of being in the same operon or not, while values near 0.5 are low-confidence predictions.\n");
    printf("<br>Gene neighbor score - An indicator of conservation (gene neighbor score, surprisal subtracted). Positive scores indicate conservation, and scores above 5 are generally strongly indicative of operons\n");

    bedFree(&arkinOperon);
    lowelabArkinOperonScoreFree(&arkinOperonScore);

    hFreeConn(&conn);
    printTrackHtml(tdb);
}


struct bed * getBlastpTrackRecord(struct sqlConnection *conn, struct trackDb *tdb, char *targetName)
/* Get blastp track record clicked by user*/
{
    struct bed *blastpTrack = NULL;
    char *dupe, *words[16];
    int wordCount;
    int bedSize = 0;
    char query[512];
    struct sqlResult *sr;
    char **row;
    int rowOffset;

    int start = cartInt(cart, "o");
    int end = cartInt(cart, "t");
    char *chrom = cartString(cart, "c");

    dupe = cloneString(tdb->type);
    wordCount = chopLine(dupe, words);
    if (wordCount > 1)
        bedSize = atoi(words[1]);
    if (bedSize < 3) bedSize = 3;

    rowOffset = hOffsetPastBin(database, seqName, tdb->table);
    sprintf(query, "select distinct * from %s where name = '%s' and chrom = '%s' and chromStart = %d and chromEnd = %d",
            tdb->table, targetName, chrom, start, end);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        blastpTrack = bedLoadN(row+rowOffset, bedSize);

    freez(&dupe);
    sqlFreeResult(&sr);

    return blastpTrack;
}


void printQueryGeneInfo(struct sqlConnection *conn, struct bed *blastpTrack, char *queryName, unsigned int *querySeqLength, char *queryTable)
/* Get and print blastp query gene info */
{
    char query[512];
    struct sqlResult *srRefSeq;
    char **row;
    int geneCount;
    char **buffer = NULL;
    char *targetGeneName[2];
    struct minGeneInfo* ginfo;

    char blastpHits[] = "blastpHits";

    unsigned int queryStart = 0;
    unsigned int queryEnd = 0;

    parseDelimitedString(blastpTrack->name, ':', targetGeneName, 2);

    if (hTableExists(database, queryTable) && hTableExists(database, blastpHits))
    {
        /* Get query gene from refSeq */
        sprintf(query, "select count(*) from %s where chrom = '%s' and strand = '%s' and cdsStart <= %u and cdsEnd >= %u",
                queryTable, blastpTrack->chrom, blastpTrack->strand, blastpTrack->chromStart, blastpTrack->chromEnd);
        srRefSeq = sqlGetResult(conn, query);
        if ((row = sqlNextRow(srRefSeq)) != NULL)
        {
            geneCount = atoi(row[0]);
            sqlFreeResult(&srRefSeq);

        if (geneCount == 1)
            {
                sprintf(query, "select name, cdsStart, cdsEnd from %s where chrom = '%s' and strand = '%s' and cdsStart <= %u and cdsEnd >= %u",
                        queryTable, blastpTrack->chrom, blastpTrack->strand, blastpTrack->chromStart, blastpTrack->chromEnd);
                srRefSeq = sqlGetResult(conn, query);
                if ((row = sqlNextRow(srRefSeq)) != NULL)
                {
                    strcpy(queryName, row[0]);
                    queryStart = strtoul(row[1], buffer, 10);
                    queryEnd = strtoul(row[2], buffer, 10);
                }
                sqlFreeResult(&srRefSeq);
            }
            else
            {
                /* Check blastpHits if more than 1 query gene is found within the region */
                sprintf(query, "select a.name, a.cdsStart, a.cdsEnd from %s a, %s b where a.chrom = '%s' and a.strand = '%s' and a.cdsStart <= %u and a.cdsEnd >= %u and a.name = b.query and b.target like '%%%s'",
                        queryTable, blastpHits,
                        blastpTrack->chrom, blastpTrack->strand, blastpTrack->chromStart, blastpTrack->chromEnd, targetGeneName[0]);
                srRefSeq = sqlGetResult(conn, query);
                if ((row = sqlNextRow(srRefSeq)) != NULL)
                {
                    strcpy(queryName, row[0]);
                    queryStart = strtoul(row[1], buffer, 10);
                    queryEnd = strtoul(row[2], buffer, 10);
                }
                sqlFreeResult(&srRefSeq);
        }

        if ((queryStart == 0) && (queryEnd == 0))
                printf("Query gene not found for %s at %s:%u-%u\n", blastpTrack->name, blastpTrack->chrom, blastpTrack->chromStart, blastpTrack->chromEnd);
        else
            {
                *querySeqLength = queryEnd - queryStart;

                /* Print query gene info */
                printf("<B>Gene: </B>%s<BR>\n", queryName);

                ginfo = getGbProtCodeInfo(conn, database, queryName);
                if (ginfo != NULL)
                {
                    if (ginfo->product != NULL && differentString(ginfo->product,"none"))
                        medlineLinkedLine("Product", ginfo->product, ginfo->product);
                }

                printf("<B>Position:</B> "
                       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                       hgTracksPathAndSettings(), database, blastpTrack->chrom, queryStart + 1, queryEnd);
                printf("%s:%d-%d</A><BR>\n", blastpTrack->chrom, queryStart + 1, queryEnd);
                printf("<B>Strand:</B> %s<BR>\n", blastpTrack->strand);
                printf("<B>Genomic size: </B> %d nt<BR>\n", (queryEnd - queryStart));
            }
        }
        else
            printf("<BR>Query gene not found for %s at %s:%u-%u<BR>\n", blastpTrack->name, blastpTrack->chrom, blastpTrack->chromStart, blastpTrack->chromEnd);
    }

    sqlFreeResult(&srRefSeq);
    free(targetGeneName[0]);
    free(targetGeneName[1]);
}

struct slName* getAllClades(struct sqlConnection *conn)
/* Get all available clades in database */
{
    char query[512];
    struct sqlResult *srDb;
    char **rowDb;
    struct slName *list = NULL;
    char clade[50];

    sprintf(query, "select label from %s.clade", cfgOption("central.db"));
    srDb = sqlGetResult(conn, query);
    while ((rowDb = sqlNextRow(srDb)) != NULL)
    {
        strcpy(clade, rowDb[0]);
        slNameAddTail(&list, clade);
    }
    sqlFreeResult(&srDb);

    return list;
}

struct blastTab* loadBlastpHits(struct sqlConnection *conn, char* queryName)
/* Load all blastp hits of the given query gene into a list */
{
    char query[512];
    struct sqlResult *srBlastpHits = NULL;
    struct blastTab *list = NULL;
    struct blastTab *blastpHits;
    char **row;
    char blastpHitsTable[] = "blastpHits";

    if (hTableExists(database, blastpHitsTable))
    {
        sprintf(query, "select * from %s where query = '%s'", blastpHitsTable, queryName);
        srBlastpHits = sqlGetResult(conn, query);
        while ((row = sqlNextRow(srBlastpHits)) != NULL)
        {
            blastpHits = blastTabLoad(row);
            slAddTail(&list, blastpHits);
        }
    }
    if (srBlastpHits != NULL)
        sqlFreeResult(&srBlastpHits);
    return list;
}

void printBlastpResult(struct sqlConnection *conn, struct blastTab *blastpHitsList, unsigned int querySeqLength)
/* Print Blastp result of given clade */
{
    char query[512];
    struct sqlResult *sr;
    char **row;
    char refSeq[50];
    char xraTable[50];
    char product[255];
    struct blastTab *blastpHits;
    struct minGeneInfo *ginfo;
    char *blastpTarget[2];
    char *clades[2];
    int cladePortionCount = 0;
    char genome[50] = "";
    char clade[50] = "";
    unsigned int hitStart = 0;
    unsigned int hitEnd = 0;
    char **buffer = NULL;
    boolean findTable = FALSE;

    int tStart = cartInt(cart, "o");
    int tEnd = cartInt(cart, "t");
    char *tChrom = cartString(cart, "c");

    printf("<br>\n");

    /* Print table */
    printf("<table style=\"width: 100%%;\" bgcolor=\"#%s\" border=\"0\" cellpadding=\"1\" cellspacing=\"0\">", HG_COL_BORDER);
    printf("<tbody><tr><td>\n");
    printf("<table style='width:100%%; text-align:left; background-color:#%s;' border=1 cellpadding=2 cellspacing=2>\n", HG_COL_INSIDE);
    printf("<tbody>\n");

    /* Print table column heading */
    printf("<tr style=\"vertical-align: top;\">\n");
    printf("<td width=\"18%%\"><b>Organism</b></td>\n");
    printf("<td width=\"7%%\"><b>Clade</b></td>\n");
    printf("<td width=\"7%%\"><b>Gene</b></td>\n");
    printf("<td><b>Product</b></td>\n");
    printf("<td width=\"5%%\"><b>Percent Length of Full Protein</b></td>\n");
    printf("<td width=\"8%%\"><b>Position in Source Protein</b></td>\n");
    printf("<td width=\"5%%\"><b>Protein Identity (%%)</b></td>\n");
    printf("<td width=\"5%%\"><b>E-value</b></td>\n");
    printf("<td width=\"5%%\"><b>Log of E-value</b></td>\n");
    printf("<td width=\"5%%\"><b>Bit Score</b></td>\n");
    printf("<td width=\"6%%\"><b>Protein Alignment Length</b></td>\n");
    printf("<td width=\"5%%\"><b>Protein Mismatches</b></td>\n");
    printf("<td width=\"5%%\"><b>Alignment Gaps</b></td>\n");
    printf("</tr>\n");

    blastpHits = blastpHitsList;
    while (blastpHits != NULL)
    {
        parseDelimitedString(blastpHits->target, ':', blastpTarget, 2);

        /* Get species info */
        memset(genome, 0, 50);
        memset(clade, 0, 50);
        getGenomeClade(conn, blastpTarget[0], genome, clade);

        if ((strcmp(genome , "") != 0) && (strcmp(clade, "") != 0))
        {
            cladePortionCount = parseDelimitedString(clade, '-', clades, 2);

            printf("<tr style=\"vertical-align: top;\">\n");
            printf("<td><a name=\"%s:%s:%u-%u\"><i>%s</i></td>\n", blastpTarget[1], tChrom, tStart, tEnd, genome);

            if (cladePortionCount == 1)
                printf("<td>%s</td>\n", clades[0]);
            else if (cladePortionCount == 2)
                printf("<td>%s<br>%s</td>\n", clades[0], clades[1]);

            /* Get target gene position from refSeq */
            strcpy(refSeq, blastpTarget[0]);
            strcat(refSeq, ".");
            if (hDbExists(blastpTarget[0]))
            {
                if (hTableExists(blastpTarget[0], "lookup"))
                {
                    sprintf(query, "select lookupValue from %s.lookup where lookupCode = 'annotRev'", blastpTarget[0]);
                    sr = sqlGetResult(conn, query);
                    if ((row = sqlNextRow(sr)) != NULL)
                    {
                        strcat(refSeq, row[0]);
                        findTable = TRUE;
                        sqlFreeResult(&sr);
                    }
                }
                else if (hTableExists(blastpTarget[0], "refSeq"))
                {
                    strcat(refSeq, "refSeq");
                    findTable = TRUE;
                }
                if (findTable)
                {
                    sprintf(query, "select chrom, cdsStart, cdsEnd from %s where name = '%s'",
                            refSeq, blastpTarget[1]);
                    sr = sqlGetResult(conn, query);
                    if ((row = sqlNextRow(sr)) != NULL)
                    {
                        hitStart = strtoul(row[1], buffer, 10) + blastpHits->tStart * 3 + 1;
                        hitEnd = strtoul(row[1], buffer, 10) + blastpHits->tEnd * 3;
                        printf("<td><a href=\"hgTracks\?position=%s:%u-%u&db=%s\" TARGET=_blank>%s</a></td>\n",
                               row[0], hitStart, hitEnd, blastpTarget[0], blastpTarget[1]);
                    }
                    else
                        printf("<td>%s</td>\n", blastpTarget[1]);
                    sqlFreeResult(&sr);
                }
                else
                    printf("<td>%s</td>\n", blastpTarget[1]);
            }
            else
                printf("<td>%s</td>\n", blastpTarget[1]);

            /* Get target gene product annotation */
            if (hDbExists(blastpTarget[0]))
            {
                if (hTableExists(blastpTarget[0], "lookup"))
                {
                    sprintf(query, "select lookupValue from %s.lookup where lookupCode = 'annotRevXra'", blastpTarget[0]);
                    sr = sqlGetResult(conn, query);
                    if ((row = sqlNextRow(sr)) != NULL)
                    {
                        strcpy(xraTable, row[0]);
                        sqlFreeResult(&sr);
                    }
                    else
                        strcpy(product, "N/A");

                    sprintf(query, "select product from %s.%s where name = '%s'", blastpTarget[0], xraTable, blastpTarget[1]);
                    sr = sqlGetResult(conn, query);
                    if ((row = sqlNextRow(sr)) != NULL)
                    {
                        strcpy(product, row[0]);
                        sqlFreeResult(&sr);
                    }
                    else
                        strcpy(product, "N/A");
                }
                else
                {
                    ginfo = getGbProtCodeInfo(conn, blastpTarget[0], blastpTarget[1]);
                    if (ginfo != NULL && ginfo->product != NULL && differentString(ginfo->product,"none"))
                        strcpy(product, ginfo->product);
                    else
                        strcpy(product, "N/A");
                }
                printf("<td>%s</td>\n", product);
            }
            else
                printf("<td>%s</td>\n", "N/A");

            printf("<td style=\"text-align: center;\">%0.f</td>\n", ((double) (blastpHits->qEnd - blastpHits->qStart) / ((double) (querySeqLength-3) / 3.0f)) * 100.0f);
            printf("<td style=\"text-align: center;\">%u - %u</td>\n", blastpHits->qStart + 1, blastpHits->qEnd);
            printf("<td style=\"text-align: right;\">%0.1f</td>\n", blastpHits->identity);
            printf("<td style=\"text-align: right;\">%0.0e</td>\n", blastpHits->eValue);
            printf("<td style=\"text-align: right;\">e%0.0f</td>\n", (blastpHits->eValue == 0)? 0 : log(blastpHits->eValue) / log(10));
            printf("<td style=\"text-align: right;\">%0.1f</td>\n", blastpHits->bitScore);
            printf("<td style=\"text-align: right;\">%u</td>\n", blastpHits->aliLength);
            printf("<td style=\"text-align: right;\">%u</td>\n", blastpHits->mismatch);
            printf("<td style=\"text-align: right;\">%u</td>\n", blastpHits->gapOpen);

            printf("</tr>\n");
        }
        free(blastpTarget[0]);
        free(blastpTarget[1]);
        blastpHits = blastpHits->next;
    }
    /* Close table */
    printf("</tbody>\n");
    printf("</table>\n");
    printf("</td></tr></tbody>\n");
    printf("</table>\n");
}

void doBlastP(struct trackDb *tdb, char *targetName)
/* Handle the BlastP Archaea and BlastP Bacteria tracks. */
{
    char queryName[50];
    char queryTable[50];
    unsigned int querySeqLength = 0;
    struct sqlConnection *conn = hAllocConn(database);
    struct bed *blastpTrack;
    struct blastTab *blastpHitsList;
    char query[512];
    struct sqlResult *sr;
    char **row;

    cartWebStart(cart, database, "%s", "BlastP Alignment Hits");

    blastpTrack = getBlastpTrackRecord(conn, tdb, targetName);

    if (hTableExists(database, "lookup"))
    {
        sprintf(query, "select lookupValue from lookup where lookupCode = 'annotRev'");
        sr = sqlGetResult(conn, query);
        if ((row = sqlNextRow(sr)) != NULL)
        {
            strcpy(queryTable, row[0]);
            sqlFreeResult(&sr);
        }
    }
    else
        strcpy(queryTable, "refSeq");
    printQueryGeneInfo(conn, blastpTrack, queryName, &querySeqLength, queryTable);

    blastpHitsList = loadBlastpHits(conn, queryName);

    printBlastpResult(conn, blastpHitsList, querySeqLength);

    printf("<BR>Note: All measurements are counted by the number of amino acids.<BR>\n");
    printf("<hr>\n");
    hFreeConn(&conn);
    printTrackHtml(tdb);
}

void doSelfHomologs(struct trackDb *tdb, char *targetName)
/* Handle the Self Homologs tracks. */
{
    char queryName[50];
    char queryTable[50];
    unsigned int querySeqLength = 0;
    struct sqlConnection *conn = hAllocConn(database);
    struct bed *blastpTrack;
    struct blastTab *blastpHitsList;
    char query[512];
    struct sqlResult *sr;
    char **row;

    cartWebStart(cart, database, "%s", "Homologs Within Genome by BlastP Search");

    blastpTrack = getBlastpTrackRecord(conn, tdb, targetName);

    if (hTableExists(database, "lookup"))
    {
        sprintf(query, "select lookupValue from lookup where lookupCode = 'annotRev'");
        sr = sqlGetResult(conn, query);
        if ((row = sqlNextRow(sr)) != NULL)
        {
            strcpy(queryTable, row[0]);
            sqlFreeResult(&sr);
        }
    }
    else
        strcpy(queryTable, "refSeq");
    printQueryGeneInfo(conn, blastpTrack, queryName, &querySeqLength, queryTable);

    blastpHitsList = loadSelfBlastpHits(conn, queryName, 1);

    printBlastpResult(conn, blastpHitsList, querySeqLength);

    printf("<BR>Note: All measurements are counted by the number of amino acids.<BR>\n");
    printf("<hr>\n");
    hFreeConn(&conn);
    printTrackHtml(tdb);
}

void printQuerySequenceInfo(struct sqlConnection *conn, struct bed *blastxTrack, char *queryName, unsigned int *querySeqLength, char *queryTable)
/* Get and print blastx query sequence info */
{
    char query[512];
    struct sqlResult *srQuerySeq;
    char **row;
    int seqCount;
    char **buffer = NULL;
    char *targetGeneName[2];

    char blastxHits[] = "blastxHits";

    unsigned int queryStart = 0;
    unsigned int queryEnd = 0;

    parseDelimitedString(blastxTrack->name, ':', targetGeneName, 2);

    if (hTableExists(database, queryTable) && hTableExists(database, blastxHits))
    {
        /* Get query sequence from query table */
        sprintf(query, "select count(*) from %s where chrom = '%s' and chromStart <= %u and chromEnd >= %u",
                queryTable, blastxTrack->chrom, blastxTrack->chromStart, blastxTrack->chromEnd);
        srQuerySeq = sqlGetResult(conn, query);
        if ((row = sqlNextRow(srQuerySeq)) != NULL)
        {
            seqCount = atoi(row[0]);
            sqlFreeResult(&srQuerySeq);

            if (seqCount == 1)
            {
                sprintf(query, "select name, chromStart, chromEnd from %s where chrom = '%s' and chromStart <= %u and chromEnd >= %u",
                        queryTable, blastxTrack->chrom, blastxTrack->chromStart, blastxTrack->chromEnd);
                srQuerySeq = sqlGetResult(conn, query);
                if ((row = sqlNextRow(srQuerySeq)) != NULL)
                {
                    strcpy(queryName, row[0]);
                    queryStart = strtoul(row[1], buffer, 10);
                    queryEnd = strtoul(row[2], buffer, 10);
                }
                sqlFreeResult(&srQuerySeq);
            }
            else
            {
                /* Check blastxHits if more than 1 query sequence is found within the region */
                sprintf(query, "select a.name, a.chromStart, a.chromEnd from %s a, %s b where a.chrom = '%s' and a.chromStart <= %u and a.chromEnd >= %u and a.name = b.query and b.target like '%%%s'",
                        queryTable, blastxHits,
                        blastxTrack->chrom, blastxTrack->chromStart, blastxTrack->chromEnd, targetGeneName[0]);
                srQuerySeq = sqlGetResult(conn, query);
                if ((row = sqlNextRow(srQuerySeq)) != NULL)
                {
                    strcpy(queryName, row[0]);
                    queryStart = strtoul(row[1], buffer, 10);
                    queryEnd = strtoul(row[2], buffer, 10);
                }
                sqlFreeResult(&srQuerySeq);
            }

            if ((queryStart == 0) && (queryEnd == 0))
                printf("Query sequence not found for %s at %s:%u-%u\n", blastxTrack->name, blastxTrack->chrom, blastxTrack->chromStart, blastxTrack->chromEnd);
            else
            {
                *querySeqLength = (blastxTrack->chromEnd - blastxTrack->chromStart);

                /* Print query sequence info */
                printf("<B>Query sequence: </B>%s<BR>\n", queryName);
                printf("<B>Position:</B> "
                       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                       hgTracksPathAndSettings(), database, blastxTrack->chrom, queryStart + 1, queryEnd);
                printf("%s:%d-%d</A><BR>\n", blastxTrack->chrom, queryStart + 1, queryEnd);
                printf("<B>Genomic size: </B> %d nt<BR><BR>\n", (queryEnd - queryStart));

                printf("<B>BlastX hit position:</B> "
                       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                       hgTracksPathAndSettings(), database, blastxTrack->chrom, blastxTrack->chromStart + 1, blastxTrack->chromEnd);
                printf("%s:%d-%d</A><BR>\n", blastxTrack->chrom, blastxTrack->chromStart + 1, blastxTrack->chromEnd);
                printf("<B>BlastX hit position relative to query sequence: </B> %d-%d<BR>\n", blastxTrack->chromStart - queryStart + 1, blastxTrack->chromEnd - queryStart);
                printf("<B>BlastX hit genomic size: </B> %d nt<BR>\n", (blastxTrack->chromEnd - blastxTrack->chromStart));
            }
        }
        else
            printf("<BR>Query sequence not found for %s at %s:%u-%u<BR>\n", blastxTrack->name, blastxTrack->chrom, blastxTrack->chromStart, blastxTrack->chromEnd);
    }

    sqlFreeResult(&srQuerySeq);
    free(targetGeneName[0]);
    free(targetGeneName[1]);
}

struct blastTab* loadBlastxHits(struct sqlConnection *conn, char* queryName, char* queryTable, struct bed *blastxTrack)
/* Load all blastx hits of the given query region into a list */
{
    char query[512];
    struct sqlResult *srBlastxHits = NULL;
    struct blastTab *list = NULL;
    struct blastTab *blastxHits;
    struct sqlResult *srQuery = NULL;
    struct bed *queryTrack = NULL;
    char **rowQuery;
    int rowOffset;
    char **row;
    char blastxHitsTable[] = "blastxHits";
    unsigned int queryStart = 0;
    unsigned int queryEnd = 0;
    unsigned int qStart = 0;
    unsigned int qEnd = 0;

    if (hTableExists(database, queryTable) && hTableExists(database, blastxHitsTable))
    {
        rowOffset = hOffsetPastBin(database, seqName, queryTable);
        sprintf(query, "select * from %s where name = '%s'", queryTable, queryName);
        srQuery = sqlGetResult(conn, query);
        if ((rowQuery = sqlNextRow(srQuery)) != NULL)
        {
            queryTrack = bedLoadN(rowQuery+rowOffset, 6);
            queryStart = blastxTrack->chromStart - queryTrack->chromStart + 1;
            queryEnd = blastxTrack->chromEnd - queryTrack->chromStart;
        }
        sqlFreeResult(&srQuery);
        srQuery = NULL;

        sprintf(query, "select * from %s where query = '%s'", blastxHitsTable, queryName);
        srBlastxHits = sqlGetResult(conn, query);
        while ((row = sqlNextRow(srBlastxHits)) != NULL)
        {
            blastxHits = blastTabLoad(row);
            if (blastxHits->qStart < blastxHits->qEnd)
            {
                qStart = blastxHits->qStart;
                qEnd = blastxHits->qEnd;
            }
            else
            {
                qStart = blastxHits->qEnd;
                qEnd = blastxHits->qStart;
            }
            if (((qStart <= queryStart) && (qEnd >= queryEnd)) ||
                ((qStart >= queryStart) && (qStart <= queryEnd)) ||
                ((qEnd >= queryStart) && (qEnd <= queryEnd)))
                slAddTail(&list, blastxHits);
        }
    }
    if (srQuery != NULL)
        sqlFreeResult(&srQuery);
    if (srBlastxHits != NULL)
        sqlFreeResult(&srBlastxHits);
    return list;
}

void printBlastxResult(struct sqlConnection *conn, struct blastTab *blastxHitsList, unsigned int querySeqLength)
/* Print Blastx result of given clade */
{
    char query[512];
    struct sqlResult *sr;
    char **row;
    char refSeq[50];
    struct blastTab *blastxHits;
    struct minGeneInfo *ginfo;
    char *blastxTarget[2];
    char *clades[2];
    int cladePortionCount = 0;
    char genome[50] = "";
    char clade[50] = "";
    unsigned int targetProteinStart = 0;
    unsigned int targetProteinEnd = 0;
    unsigned int hitStart = 0;
    unsigned int hitEnd = 0;
    char **buffer = NULL;

    int tStart = cartInt(cart, "o");
    int tEnd = cartInt(cart, "t");
    char *tChrom = cartString(cart, "c");

    printf("<br>\n");

    /* Print table */
    printf("<table style=\"width: 100%%;\" bgcolor=\"#%s\" border=\"0\" cellpadding=\"1\" cellspacing=\"0\">", HG_COL_BORDER);
    printf("<tbody><tr><td>\n");
    printf("<table style='width:100%%; text-align:left; background-color:#%s;' border=1 cellpadding=2 cellspacing=2>\n", HG_COL_INSIDE);
    printf("<tbody>\n");

    /* Print table column heading */
    printf("<tr style=\"vertical-align: top;\">\n");
    printf("<td width=\"10%%\"><b>Organism</b></td>\n");
    printf("<td width=\"7%%\"><b>Clade</b></td>\n");
    printf("<td width=\"7%%\"><b>Gene</b></td>\n");
    printf("<td><b>Product</b></td>\n");
    printf("<td width=\"7%%\"><b>Position in Query Genomic Sequence</b></td>\n");
    printf("<td width=\"7%%\"><b>Position in Target Protein</b></td>\n");
    printf("<td width=\"7%%\"><b>Percent Length of Target Protein</b></td>\n");
    printf("<td width=\"5%%\"><b>Protein Identity (%%)</b></td>\n");
    printf("<td width=\"5%%\"><b>E-value</b></td>\n");
    printf("<td width=\"5%%\"><b>Log of E-value</b></td>\n");
    printf("<td width=\"5%%\"><b>Bit Score</b></td>\n");
    printf("<td width=\"8%%\"><b>Protein Alignment Length</b></td>\n");
    printf("<td width=\"8%%\"><b>Protein Mismatches</b></td>\n");
    printf("<td width=\"8%%\"><b>Alignment Gaps</b></td>\n");
    printf("</tr>\n");

    blastxHits = blastxHitsList;
    while (blastxHits != NULL)
    {
        parseDelimitedString(blastxHits->target, ':', blastxTarget, 2);

        /* Get species info */
        memset(genome, 0, 50);
        memset(clade, 0, 50);
        getGenomeClade(conn, blastxTarget[0], genome, clade);

        if ((strcmp(genome , "") != 0) && (strcmp(clade, "") != 0))
        {
            cladePortionCount = parseDelimitedString(clade, '-', clades, 2);

            printf("<tr style=\"vertical-align: top;\">\n");

            printf("<td><a name=\"%s:%s:%u-%u\"><i>%s</i></td>\n", blastxTarget[1], tChrom, tStart, tEnd, genome);
            if (cladePortionCount == 1)
                printf("<td>%s</td>\n", clades[0]);
            else if (cladePortionCount == 2)
                printf("<td>%s<br>%s</td>\n", clades[0], clades[1]);

            /* Get target gene position from refSeq */
            strcpy(refSeq, blastxTarget[0]);
            strcat(refSeq, ".refSeq");
            if (hDbExists(blastxTarget[0]) && hTableExists(blastxTarget[0], "refSeq"))
            {
                sprintf(query, "select chrom, cdsStart, cdsEnd from %s where name = '%s'",
                        refSeq, blastxTarget[1]);
                sr = sqlGetResult(conn, query);
                if ((row = sqlNextRow(sr)) != NULL)
                {
                    targetProteinStart = strtoul(row[1], buffer, 10);
                    targetProteinEnd = strtoul(row[2], buffer, 10);
                    hitStart = targetProteinStart + blastxHits->tStart * 3 + 1;
                    hitEnd = targetProteinStart + blastxHits->tEnd * 3;
                    printf("<td><a href=\"hgTracks\?position=%s:%u-%u&db=%s\" TARGET=_blank>%s</a></td>\n",
                           row[0], hitStart, hitEnd, blastxTarget[0], blastxTarget[1]);
                }
                else
                    printf("<td>%s</td>\n", blastxTarget[1]);
                sqlFreeResult(&sr);
            }
            else
                printf("<td>%s</td>\n", blastxTarget[1]);

            /* Get target gene product annotation */
            if (hDbExists(blastxTarget[0]))
            {
                ginfo = getGbProtCodeInfo(conn, blastxTarget[0], blastxTarget[1]);
                if (ginfo != NULL && ginfo->product != NULL && differentString(ginfo->product,"none"))
                    printf("<td>%s</td>\n", ginfo->product);
                else
                    printf("<td>%s</td>\n", "N/A");
                }
            else
                printf("<td>%s</td>\n", "N/A");

            printf("<td style=\"text-align: center;\">%u - %u</td>\n", blastxHits->qStart + 1, blastxHits->qEnd);
            printf("<td style=\"text-align: center;\">%u - %u</td>\n", blastxHits->tStart + 1, blastxHits->tEnd);
            if ((targetProteinStart == 0) || (targetProteinEnd == 0))
                printf("<td style=\"text-align: center;\">N/A</td>\n");
            else
                printf("<td style=\"text-align: center;\">%0.f</td>\n", ((double) (blastxHits->tEnd - blastxHits->tStart) /
                                                                     ((double) (targetProteinEnd-targetProteinStart-3) / 3.0f)) * 100.0f);
            printf("<td style=\"text-align: right;\">%0.1f</td>\n", blastxHits->identity);
            printf("<td style=\"text-align: right;\">%0.0e</td>\n", blastxHits->eValue);
            printf("<td style=\"text-align: right;\">e%0.0f</td>\n", (blastxHits->eValue == 0)? 0 : log(blastxHits->eValue) / log(10));
            printf("<td style=\"text-align: right;\">%0.1f</td>\n", blastxHits->bitScore);
            printf("<td style=\"text-align: right;\">%u</td>\n", blastxHits->aliLength);
            printf("<td style=\"text-align: right;\">%u</td>\n", blastxHits->mismatch);
            printf("<td style=\"text-align: right;\">%u</td>\n", blastxHits->gapOpen);

            printf("</tr>\n");
        }
        free(blastxTarget[0]);
        free(blastxTarget[1]);
        blastxHits = blastxHits->next;
    }
    /* Close table */
    printf("</tbody>\n");
    printf("</table>\n");
    printf("</td></tr></tbody>\n");
    printf("</table>\n");
}

void doBlastX(struct trackDb *tdb, char *targetName)
/* Handle the BlastX Archaea and BlastX Bacteria tracks. */
{
    char queryName[50];
    char queryTable[50];
    unsigned int querySeqLength = 0;
    struct sqlConnection *conn = hAllocConn(database);
    struct bed *blastxTrack;
    struct blastTab *blastxHitsList;

    cartWebStart(cart, database, "%s", "BlastX Alignment Hits");

    blastxTrack = getBlastpTrackRecord(conn, tdb, targetName);
/*    if (!differentWord(blastxTrack->chrom, "assembly"))*/
    if (hTableExists(database, "scaffolds"))
        strcpy(queryTable, "scaffolds");
    else
        strcpy(queryTable, "igenics");
    printQuerySequenceInfo(conn, blastxTrack, queryName, &querySeqLength, queryTable);

    blastxHitsList = loadBlastxHits(conn, queryName, queryTable, blastxTrack);

    printBlastxResult(conn, blastxHitsList, querySeqLength);
    printf("<BR>\n");

    printf("<hr>\n");
    hFreeConn(&conn);
    printTrackHtml(tdb);
}

void doUltraConserved(struct trackDb *tdb, char *item)
/* Handle the ultraConserved track. */
{
    char tableName[65];
    int start = cartInt(cart, "o");
    int end = cartInt(cart, "t");
    struct hashEl* hashItem;
    struct hashCookie cookie;
    struct trackDb *multizTrack;
    struct sqlConnection *conn = hAllocConn(database);

    cookie = hashFirst(trackHash);
    hashItem = hashNext(&cookie);
    while (hashItem != NULL)
    {
        memset(tableName, 0, 65);
        strcpy(tableName, hashItem->name);
        if (strstr(tableName, "multiz") != NULL)
            break;
        hashItem = hashNext(&cookie);
    }
    multizTrack = hashFindVal(trackHash, tableName);
    winStart = start;
    winEnd = end;

    genericHeader(tdb, NULL);
    if (tdb->html != NULL && tdb->html[0] != 0)
    {
        puts(tdb->html);
        htmlHorizontalLine();
    }
    genericMafClick(conn, multizTrack, item, start);
    printTrackHtml(multizTrack);
    hFreeConn(&conn);
}

float computeMolecularWeight(char* dna, int length)
{
    float weight = 0.0f;
    int count[4] = {0,0,0,0};
    int i = 0;
    for (i = 0; i < length; i++)
    {
        if ((dna[i] == 'A') || (dna[i] == 'a'))
            count[0]++;
        else if ((dna[i] == 'C') || (dna[i] == 'c'))
            count[1]++;
        else if ((dna[i] == 'G') || (dna[i] == 'g'))
            count[2]++;
        else if ((dna[i] == 'T') || (dna[i] == 't'))
            count[3]++;
    }
    weight = count[0] * 313.209 + count[1] * 289.184 + count[2] * 329.208 + count[3] * 304.196 - 63.980 + 2.016;
    return weight;
}

float computeNMolePerOD(char* dna, int length)
{
    int extCoeff = 0;
    float nmolePerOD = 0.0f;

    enum
    {
        baseA = 0,
        baseC,
        baseG,
        baseT
    };

    int indExtCoeff[4] = {15400, 7400, 11500, 8700};
    int neighborExtCoeff[4][4] = {
        {27400, 21200, 25000, 22800},
        {21200, 14600, 18000, 15200},
        {25200, 17600, 21600, 20000},
        {23400, 16200, 19000, 16800}};

    int i = 0;
    int *bases = malloc(sizeof(int) * length);

    for (i = 0; i < length; i++)
    {
        if ((dna[i] == 'A') || (dna[i] == 'a'))
            bases[i] = baseA;
        else if ((dna[i] == 'C') || (dna[i] == 'c'))
            bases[i] = baseC;
        else if ((dna[i] == 'G') || (dna[i] == 'g'))
            bases[i] = baseG;
        else if ((dna[i] == 'T') || (dna[i] == 't'))
            bases[i] = baseT;
    }

    for (i = 0; i < (length - 1); i++)
        extCoeff += neighborExtCoeff[bases[i]][bases[i + 1]];

    for (i = 1; i < (length - 1); i++)
        extCoeff -= indExtCoeff[bases[i]];

    nmolePerOD = pow(10, 6) / (float) extCoeff;

    free(bases);

    return nmolePerOD;
}

void doPrimers(struct trackDb *tdb, char *primerName)
/* Handle the array primer and GOLD primer tracks. */
{
    struct bed *primer;
    struct dnaSeq *sequence;
    char query[512];
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char *dupe, *words[16];
    char **row;
    int wordCount;
    int rowOffset;
    int bedSize = 0;
    int pairCount = 0;
    boolean forwardPrimer = TRUE;

    genericHeader(tdb, primerName);

    if (startsWith("Asn", primerName))
        forwardPrimer = FALSE;

    dupe = cloneString(tdb->type);
    wordCount = chopLine(dupe, words);
    if (wordCount > 1)
        bedSize = atoi(words[1]);
    if (bedSize < 3) bedSize = 3;

    rowOffset = hOffsetPastBin(database, seqName, tdb->table);
    sprintf(query, "select * from %s where name = '%s'", tdb->table, primerName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
        primer = bedLoadN(row+rowOffset, bedSize);
        printf("<B>Primer name: </B> %s<BR>\n",primerName);

        printf("<BR><B>Position:</B> "
               "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
               hgTracksPathAndSettings(), database, primer->chrom, primer->chromStart + 1, primer->chromEnd);
        printf("%s:%d-%d</A><BR>\n", primer->chrom, primer->chromStart + 1, primer->chromEnd);
        printf("<B>Strand:</B> %s<BR>\n", primer->strand);
        printf("<B>Genomic size:</B> %d nt<BR><BR>\n", (primer->chromEnd - primer->chromStart));

        sequence = hDnaFromSeq(database, primer->chrom, primer->chromStart, primer->chromEnd, dnaUpper);
        if (sequence != NULL)
        {
            if (strcmp(primer->strand, "-") == 0)
                reverseComplement(sequence->dna, sequence->size);
            printf("<B>Sequence:</B> 5' %s 3'<BR>\n", sequence->dna);
            printf("<B>GC content:</B> %0.2f%%<BR>\n", computeGCContent(sequence->dna, sequence->size));
            printf("<B>Molecular weight:</B> %0.1f g/mol<BR>\n", computeMolecularWeight(sequence->dna, sequence->size));
            printf("<B>nmole/OD<sub>260</sub>:</B> %0.3f <BR>\n", computeNMolePerOD(sequence->dna, sequence->size));
            printf("<B>ug/OD<sub>260</sub>:</B> %0.3f <BR>\n", computeNMolePerOD(sequence->dna, sequence->size) *
                   computeMolecularWeight(sequence->dna, sequence->size) * pow(10,-3));
        }

        if (primer->next != NULL)
            printf("<hr>\n");
    }
    sqlFreeResult(&sr);

    printf("<br><B>Primer pairing</B><br>\n");

    /* Print table */
    printf("<table style=\"width: 90%%;\" bgcolor=\"#%s\" border=\"0\" cellpadding=\"1\" cellspacing=\"0\">", HG_COL_BORDER);
    printf("<tbody><tr><td>\n");
    printf("<table style='width:100%%; text-align:left; background-color:#%s;' border=1 cellpadding=2 cellspacing=2>\n", HG_COL_INSIDE);
    printf("<tbody>\n");

    /* Print table column heading */
    printf("<tr style=\"vertical-align: top;\">\n");
    printf("<td width=\"20%%\"><b>Primer Name</b></td>\n");
    printf("<td width=\"10%%\"><b>Primer Type</b></td>\n");
    printf("<td width=\"30%%\"><b>Primer Sequence</b></td>\n");
    printf("<td width=\"10%%\"><b>PCR Region</b></td>\n");
    printf("<td width=\"10%%\"><b>PCR Length (bp)</b></td>\n");
    printf("<td width=\"10%%\"><b>PCR Region<BR>GC Content (%%)</b></td>\n");
    printf("</tr>\n");

    memset(query, 0, 512);
    if (strcmp(primer->strand, "+") == 0)
    {
        if (hTableExists(database, "genomePcrPrimers"))
            sprintf(query, "select *, 'Array PCR' primerType from genomePcrPrimers where chrom = '%s' and chromStart > %d and strand = '-'", primer->chrom, primer->chromEnd);
        if (hTableExists(database, "goldRTprimers"))
        {
            if (strcmp(query, "") != 0)
                sprintf(query, "%s union ", query);
            sprintf(query, "%sselect *, 'GOLD RT' primerType from goldRTprimers where chrom = '%s' and chromStart > %d and strand = '-'",
                    query, primer->chrom, primer->chromEnd);
        }
        sprintf(query, "%s order by chromStart", query);
    }
    else
    {
        if (hTableExists(database, "genomePcrPrimers"))
            sprintf(query, "select *, 'Array PCR' primerType from genomePcrPrimers where chrom = '%s' and chromEnd < %d and strand = '+'", primer->chrom, primer->chromStart);
        if (hTableExists(database, "goldRTprimers"))
        {
            if (strcmp(query, "") != 0)
                sprintf(query, "%s union ", query);
            sprintf(query, "%sselect *, 'GOLD RT' primerType from goldRTprimers where chrom = '%s' and chromEnd < %d and strand = '+'",
                    query, primer->chrom, primer->chromStart);
        }
        sprintf(query, "%s order by chromStart desc", query);
    }

    sr = sqlGetResult(conn, query);
    while (((row = sqlNextRow(sr)) != NULL) && (pairCount < 6))
    {
        if ((forwardPrimer && startsWith("Asn", row[4])) || (!forwardPrimer && startsWith("Sn", row[4])))
        {
            printf("<tr style=\"vertical-align: top;\">\n");

            printf("<td>"
                   "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                   hgTracksPathAndSettings(), database, row[1], atoi(row[2]) + 1, atoi(row[3]));
            printf("%s</A></td>\n", row[4]);
            printf("<td>%s</td>\n", row[7]);
            sequence = hDnaFromSeq(database, row[1], atoi(row[2]), atoi(row[3]), dnaUpper);
            if (sequence != NULL)
            {
                if (strcmp(row[6], "-") == 0)
                    reverseComplement(sequence->dna, sequence->size);
                printf("<td>5' %s 3'</td>\n", sequence->dna);
            }
            else
                printf("<td>N/A</td>\n");

            if (strcmp(primer->strand, "+") == 0)
            {
                sequence = hDnaFromSeq(database, primer->chrom, primer->chromStart, atoi(row[3]), dnaUpper);
                printf("<td>"
                       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                   hgTracksPathAndSettings(), database, primer->chrom, primer->chromStart + 1, atoi(row[3]));
                printf("%s:%d-%d</A></td>\n", primer->chrom, primer->chromStart + 1, atoi(row[3]));
            }
            else
            {
                sequence = hDnaFromSeq(database, primer->chrom, atoi(row[2]), primer->chromEnd, dnaUpper);
                printf("<td>"
                       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                   hgTracksPathAndSettings(), database, primer->chrom, atoi(row[2]) + 1, primer->chromEnd);
                printf("%s:%d-%d</A></td>\n", primer->chrom, atoi(row[2])+ 1, primer->chromEnd);
            }
            if (sequence != NULL)
            {
                printf("<td style=\"text-align: right;\">%d</td>\n", sequence->size);
                printf("<td style=\"text-align: right;\">%0.2f</td>\n", computeGCContent(sequence->dna, sequence->size));
            }
            else
            {
                printf("<td>N/A</td>\n");
                printf("<td>N/A</td>\n");
            }

            printf("</tr>\n");

            pairCount++;
        }
    }
    sqlFreeResult(&sr);

    /* Close table */
    printf("</tbody>\n");
    printf("</table>\n");
    printf("</td></tr></tbody>\n");
    printf("</table>\n");

    bedFree(&primer);

    hFreeConn(&conn);
    printTrackHtml(tdb);
}

void doWiki(char *track, struct trackDb *tdb, char *itemName)
{
  char strand[2];
  char wikiea[] = "wikiea";
  char wikibme[] = "microbewiki";
  char *wiki;

  if(sameWord(track, "wiki"))
    wiki = wikiea;
  else
    wiki = wikibme;


  printf("<HEAD>");

  if(startsWith("Make", itemName))
  {
    strand[0] = itemName[strlen(itemName)-1];
    strand[1] = 0;
    printf("<META HTTP-EQUIV=\"REFRESH\" content=\"0; URL=http://lowelabwiki.cse.ucsc.edu/index.php/BED:%s:%s:%d-%d:%s\"</META>", database, seqName, winStart, winEnd, strand);
  }
  else
  {
    printf("<META HTTP-EQUIV=\"REFRESH\" content=\"0; URL=http://lowelabwiki.cse.ucsc.edu/index.php/BED:%s:%s:%s\"</META>", database, seqName, itemName);
  }

  printf("</HEAD>");
}

void doRNAHybridization(struct trackDb *tdb, char *itemName)
{
  struct sqlConnection *conn = hAllocConn(database);
  char query[512];
  struct sqlResult *sr;
  char **row;
  struct rnaHybridization *rnaHyb;
  char rnaHybridizationTable[] = "rnaHybridization";
  char tRNATable[] = "tRNAs";
  char jgiTable[] = "jgiGene";
  char *saveTableName;
  int i;


  cartWebStart(cart, database, "%s", "RNAHybridization Sites");

  if (hTableExists(database, rnaHybridizationTable))
    {
      /* Get query gene from refSeq */
      sprintf(query, "select * from %s where name='%s'", rnaHybridizationTable, itemName);
      sr = sqlGetResult(conn, query);
      if ((row = sqlNextRow(sr)) != NULL)
    {
      rnaHyb = rnaHybridizationLoad(row);

      printf("<b>Hybridization Site:</b><br/><br/>");

      /* print hybridization site */
      printf("<span style='font-family:Courier;'>");
      printf("Pattern 5%s3<br>", rnaHyb->patternSeq);

      printf("&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp");
      for(i=0;i<rnaHyb->matchLength;i++)
        printf("|");
      printf("<br/>");

      printf("Target&nbsp 3%s5", rnaHyb->targetSeq);
      printf("</span><br/><br/>");


      printf("<BR><B>Genomic size: </B> %d nt<BR>\n",rnaHyb->matchLength);
      printf("<b>Percentage G-C base-pairs:</b> %d<br/>", (int)(rnaHyb->gcContent * 100));

      printf("<B>Position:</B> "
         "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
         hgTracksPathAndSettings(), database, rnaHyb->chrom, rnaHyb->chromStart+1, rnaHyb->chromEnd);
      printf("%s:%d-%d</A><BR>\n", rnaHyb->chrom, rnaHyb->chromStart+1, rnaHyb->chromEnd);
      printf("<B>Strand:</B> %s<BR>\n", rnaHyb->strand);

      printf("<B>Target Position:</B> "
         "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
         hgTracksPathAndSettings(), database, rnaHyb->chromTarget, rnaHyb->chromStartTarget+1, rnaHyb->chromEndTarget+1);
      printf("%s:%d-%d</A><BR>\n", rnaHyb->chromTarget, rnaHyb->chromStartTarget+1, rnaHyb->chromEndTarget+1);
      printf("<B>Target Strand:</B> %s<BR>\n", rnaHyb->strandTarget);


      printf("<br/>");


      if(strlen(rnaHyb->refSeqTarget) > 0)
        {
          printf("<b>Additional information for target<b><br/>");
          printf("<hr/>");
          doRefSeq(tdb, rnaHyb->refSeqTarget,"gbProtCodePep","gbProtCodeXra");
        }

      if(strlen(rnaHyb->JGITarget) > 0)
        {
          saveTableName = tdb->table;
          tdb->table = jgiTable;
          printf("<b>Additional information for target<b><br/>");
          printf("<hr/>");
          doJgiGene(tdb, rnaHyb->JGITarget);
          tdb->table = saveTableName;
        }

      if(strlen(rnaHyb->trnaTarget) > 0)
        {
          saveTableName = tdb->table;
          tdb->table = tRNATable;
          printf("<b>Additional information for target<b><br/>");
          printf("<hr/>");
          doTrnaGenes(tdb, rnaHyb->trnaTarget);
          tdb->table = saveTableName;
        }


      freeMem(rnaHyb);
    }

      sqlFreeResult(&sr);
    }

  hFreeConn(&conn);
}

void doarCOGs(struct trackDb *tdb, char *itemName)
{
  char query[512];
  struct sqlConnection *conn = hAllocConn(database);
  struct sqlResult *sr;
  char *dupe, *words[16];
  char **row;
  int wordCount;
  int rowOffset;
  struct arCOGs *infoload;
  int start = cartInt(cart, "o");
  int end = cartInt(cart, "t");


    sprintf(query, "select * from mgCommonDb.arcogdesc where name = '%s'", itemName);
    //sprintf(query, "select * from %s where query = '%s'", blastpHitsTable, queryName);
    struct sqlResult *srarcogdesc = sqlGetResult(conn, query);


    struct arcogdesc *description = NULL;
    while ((row = sqlNextRow(srarcogdesc)) != NULL)
    {
        struct arcogdesc *element;
        element = arcogdescLoad(row);
        slAddTail(&description, element);
    }
    sqlFreeResult(&srarcogdesc);


  genericHeader(tdb,itemName);
  dupe = cloneString(tdb->type);
  wordCount = chopLine(dupe, words);

  rowOffset = hOffsetPastBin(database,seqName, tdb->table);

  sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d and chromEnd = '%d';", tdb->table, itemName,seqName,start, end);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
        infoload = arCOGsLoad(row+rowOffset);
    printf("<B>Name:</B> %s<BR>\n", infoload->name);
    printf("<B>Description:</B> %s<BR>\n", description->description);
    printf("<B>Code:</B> %s<BR>\n", description->code);
    printf("<B>Gene:</B> %s<BR>\n", infoload->gene);

          printf("<B>Position:</B> "
                 "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                 hgTracksPathAndSettings(), database, infoload->chrom, infoload->chromStart + 1, infoload->chromEnd);
          printf("%s:%d-%d</A><BR>\n", infoload->chrom, infoload->chromStart + 1, infoload->chromEnd);
          printf("<B>Strand:</B> %s<BR>\n", infoload->strand);
          printf("<B>Genomic size: </B> %d nt<BR>\n", (infoload->chromEnd - infoload->chromStart));
          if (infoload->next != NULL)
            printf("<hr>\n");
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  arCOGsFree(&infoload);
  printTrackHtml(tdb);
}

void doloweOrthologs(struct trackDb *tdb, char *itemName)
{
  char query[512];
  struct sqlConnection *conn = hAllocConn(database);
  struct sqlResult *sr;
  char *dupe, *words[16];
  char **row;
  int wordCount;
  int rowOffset;
  struct bed *infoload;
  int bedSize = 0;
  int start = cartInt(cart, "o");
  int end = cartInt(cart, "t");

    dupe = cloneString(tdb->type);
    wordCount = chopLine(dupe, words);
    if (wordCount > 1)
        bedSize = atoi(words[1]);
    if (bedSize < 3) bedSize = 3;

  genericHeader(tdb,itemName);
  dupe = cloneString(tdb->type);
  wordCount = chopLine(dupe, words);

  rowOffset = hOffsetPastBin(database,seqName, tdb->table);

  sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d and chromEnd = '%d';", tdb->table, itemName,seqName,start, end);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
        infoload = bedLoadN(row+rowOffset, bedSize);
    printf("<B>Name:</B> %s\n", infoload->name);
    printf(" <A HREF=\"http://archdev-holmes.cse.ucsc.edu/cgi-bin/hgFrame?track=loweOrthologs&refseq=1&db=%s&name=%s\">List of Orthologs</A><BR>",database,infoload->name);
    printf("<B>Position:</B> "
                 "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                 hgTracksPathAndSettings(), database, infoload->chrom, infoload->chromStart + 1, infoload->chromEnd);
          printf("%s:%d-%d</A><BR>\n", infoload->chrom, infoload->chromStart + 1, infoload->chromEnd);
      printf("<B>Strand:</B> %s<BR>\n", infoload->strand);
      printf("<B>Genomic size: </B> %d nt<BR>\n", (infoload->chromEnd - infoload->chromStart));

          if (infoload->next != NULL)
            printf("<hr>\n");
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  printTrackHtml(tdb);
}

void doCddInfo(struct trackDb *tdb, char *itemName)
{
  char query[512];
  struct sqlConnection *conn = hAllocConn(database);
  struct sqlResult *sr;
  char *dupe, *words[16];
  char **row;
  int wordCount;
  int rowOffset;
  struct cddInfo *infoload;
  int bedSize = 0;
  int start = cartInt(cart, "o");
  int end = cartInt(cart, "t");

    dupe = cloneString(tdb->type);
    wordCount = chopLine(dupe, words);
    if (wordCount > 1)
        bedSize = atoi(words[1]);
    if (bedSize < 3) bedSize = 3;

    sprintf(query, "select * from mgCommonDb.cddDesc where accession = '%s'", itemName);
    //sprintf(query, "select * from %s where query = '%s'", blastpHitsTable, queryName);
    struct sqlResult *srCddDesc = sqlGetResult(conn, query);


    struct cddDesc *description = NULL;
    while ((row = sqlNextRow(srCddDesc)) != NULL)
    {
        struct cddDesc *element;
        element = cddDescLoad(row);
        slAddTail(&description, element);
    }
    sqlFreeResult(&srCddDesc);

  genericHeader(tdb,itemName);
  dupe = cloneString(tdb->type);
  wordCount = chopLine(dupe, words);

  rowOffset = hOffsetPastBin(database,seqName, tdb->table);

  sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d and chromEnd = '%d';", tdb->table, itemName,seqName,start, end);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
        infoload = cddInfoLoad(row+rowOffset);
    printf("<B>Name:</B> %s<BR>\n", infoload->fullname);
    printf("<B>Accession:</B>  %s<A HREF=\"http://www.ncbi.nlm.nih.gov/Structure/cdd/cddsrv.cgi?uid=%s\" TARGET=_blank>",
           infoload->name, infoload->NCBInum);
    printf(" Link to NCBI Site</A> <BR>\n");
    printf("<B>E-value:</B> %0.0e<BR>\n", infoload->evalue);
    printf("<B>Description:</B> %s<BR>\n", description->name);
    printf("<B>Protein Identity:</B> %u%%<BR>\n", infoload->percentident);
    printf("<B>Percent Length:</B> %u%%<BR>\n", infoload->percentlength);
          printf("<B>Position:</B> "
                 "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                 hgTracksPathAndSettings(), database, infoload->chrom, infoload->chromStart + 1, infoload->chromEnd);
          printf("%s:%d-%d</A><BR>\n", infoload->chrom, infoload->chromStart + 1, infoload->chromEnd);
          printf("<B>Strand:</B> %s<BR>\n", infoload->strand);
          printf("<B>Genomic size: </B> %d nt<BR>\n", (infoload->chromEnd - infoload->chromStart));
          if (infoload->next != NULL)
            printf("<hr>\n");
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  cddInfoFree(&infoload);
  printTrackHtml(tdb);
}

void domegablastInfo(struct trackDb *tdb, char *itemName)
{
  char query[512];
  struct sqlConnection *conn = hAllocConn(database);
  struct sqlResult *sr;
  char *dupe, *words[16];
  char **row;
  int wordCount;
  int rowOffset;
  struct megablastInfo *infoload;
  int start = cartInt(cart, "o");
  int end = cartInt(cart, "t");


  genericHeader(tdb,itemName);
  dupe = cloneString(tdb->type);
  wordCount = chopLine(dupe, words);

  rowOffset = hOffsetPastBin(database,seqName, tdb->table);

  sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d and chromEnd = '%d';", tdb->table, itemName,seqName,start, end);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
        infoload = megablastInfoLoad(row+rowOffset);
    printf("<B>Name:</B> %s<BR>\n", infoload->name);
    printf("<B>Accession:</B>  %s<A HREF=\"http://www.ncbi.nlm.nih.gov/nuccore/%s\" TARGET=_blank>",
           infoload->name, infoload->name);
    printf(" Link to NCBI Site</A> <BR>\n");
    printf("<B>Description:</B> %s<BR>\n", infoload->fullname);
    printf("<B>E-value:</B> %0.0e", infoload->evalue);
    #ifdef LISTUI
    printf(" <A HREF=\"http://archdev-holmes.cse.ucsc.edu/cgi-bin/hgList?track=megablastInfo&order=evalue&db=%s\">Sort by E-value</A>",database);
    #endif
    printf("<BR>\n");
    printf("<B>Protein Identity:</B> %u%%\n", infoload->percentident);
    #ifdef LISTUI
    printf(" <A HREF=\"http://archdev-holmes.cse.ucsc.edu/cgi-bin/hgList?track=megablastInfo&order=percentident&db=%s\">Sort by Percent Identity</A>",database);
    #endif
    printf("<BR>\n");
          printf("<B>Position:</B> "
                 "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                 hgTracksPathAndSettings(), database, infoload->chrom, infoload->chromStart + 1, infoload->chromEnd);
          printf("%s:%d-%d</A><BR>\n", infoload->chrom, infoload->chromStart + 1, infoload->chromEnd);
          printf("<B>Strand:</B> %s<BR>\n", infoload->strand);
          printf("<B>Genomic size: </B> %d nt<BR>\n", (infoload->chromEnd - infoload->chromStart));
          if (infoload->next != NULL)
            printf("<hr>\n");
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  megablastInfoFree(&infoload);
  printTrackHtml(tdb);
}

void doAlignInfo(struct trackDb *tdb, char *itemName)
{
  char query[512];
  struct sqlConnection *conn = hAllocConn(database);
  struct sqlResult *sr;
  char *dupe, *words[16];
  char **row;
  int wordCount;
  int rowOffset;
  struct alignInfo *infoload;
  int bedSize = 0;
  int start = cartInt(cart, "o");

    dupe = cloneString(tdb->type);
    wordCount = chopLine(dupe, words);
    if (wordCount > 1)
        bedSize = atoi(words[1]);
    if (bedSize < 3) bedSize = 3;


  genericHeader(tdb,itemName);
  dupe = cloneString(tdb->type);
  wordCount = chopLine(dupe, words);

  rowOffset = hOffsetPastBin(database, seqName, tdb->table);

  sprintf(query, "select * from %s where name = '%s' and chrom = '%s' and chromStart = %d;", tdb->table, itemName,seqName,start);
  sr = sqlGetResult(conn, query);
  while ((row = sqlNextRow(sr)) != NULL)
    {
        infoload = alignInfoLoad(row+rowOffset);

    printf("<B>Name:</B> %s  ", infoload->name);
    linkToOtherBrowserTitle(infoload->orgn, infoload->alignChrom, infoload->alignChromStart + 1, infoload->alignChromEnd, "Aligned Feature");
                 //printf("<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                 //hgTracksPathAndSettings(), infoload->orgn, infoload->alignChrom, infoload->alignChromStart + 1, infoload->alignChromEnd);
   printf("Link to Feature</A><BR>\n" );

          printf("<B>Position:</B> "
                 "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
                 hgTracksPathAndSettings(), database, infoload->chrom, infoload->chromStart + 1, infoload->chromEnd);
          printf("%s:%d-%d</A><BR>\n", infoload->chrom, infoload->chromStart + 1, infoload->chromEnd);
          printf("<B>Strand:</B> %s<BR>\n", infoload->strand);
          printf("<B>Genomic size: </B> %d nt<BR>\n", (infoload->chromEnd - infoload->chromStart));
          if (infoload->next != NULL)
            printf("<hr>\n");
            break;
    }
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  alignInfoFree(&infoload);
  printTrackHtml(tdb);
}

void doCRISPRs(struct trackDb *tdb, char *crisprName)
/* Handle the CRISPR array track. */
{
    struct bed *crispr;
    struct dnaSeq *sequence;
	char tempSeq[512];
    char query[512];
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char *dupe, *words[16];
    char **row;
    int wordCount;
    int rowOffset;
    int bedSize = 0;
	int pairCount = 0;

    genericHeader(tdb, crisprName);

    dupe = cloneString(tdb->type);
    wordCount = chopLine(dupe, words);
    if (wordCount > 1)
        bedSize = atoi(words[1]);
    if (bedSize < 3) bedSize = 3;

    rowOffset = hOffsetPastBin(database, seqName, tdb->table);
    safef(query, ArraySize(query), "select * from %s where name = '%s'", tdb->table, crisprName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    {
        crispr = bedLoadN(row+rowOffset, bedSize);
        printf("<B>Name: </B> %s<BR>\n", crisprName);
		printf("<B>Position:</B> "
               "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">",
               hgTracksPathAndSettings(), database, crispr->chrom, crispr->chromStart + 1, crispr->chromEnd);
        printf("%s:%d-%d</A><BR>\n", crispr->chrom, crispr->chromStart + 1, crispr->chromEnd);
        printf("<B>Strand:</B> %s<BR>\n", crispr->strand);
        printf("<B>Genomic size:</B> %d nt<BR><BR>\n", (crispr->chromEnd - crispr->chromStart));
		printf("<B>Number of spacers:</B> %u<BR><BR>\n", crispr->blockCount - 1);

        sequence = hDnaFromSeq(database, crispr->chrom, crispr->chromStart, crispr->chromEnd, dnaUpper);
        if (sequence != NULL)
        {
			/* Print table */
			printf("<table style=\"width: 100%%;\" bgcolor=\"#%s\" border=\"0\" cellpadding=\"1\" cellspacing=\"0\">", HG_COL_BORDER);
			printf("<tbody><tr><td>\n");
			printf("<table style='width:100%%; text-align:left; background-color:#%s;' border=1 cellpadding=2 cellspacing=2>\n", HG_COL_INSIDE);
			printf("<tbody>\n");

			/* Print table column heading */
			printf("<tr style=\"vertical-align: top;\">\n");
			printf("<td colspan=\"3\"><b>Direct Repeat</b></td>\n");
			printf("<td colspan=\"3\"><b>Spacer</b></td>\n");
			printf("</tr>\n");
			printf("<tr style=\"vertical-align: top;\">\n");
			printf("<td width=\"8%%\"><b>Start Pos</b></td>\n");
			printf("<td width=\"36%%\"><b>Sequence</b></td>\n");
			printf("<td width=\"4%%\"><b>Length</b></td>\n");
			printf("<td width=\"8%%\"><b>Start Pos</b></td>\n");
			printf("<td><b>Sequence</b></td>\n");
			printf("<td width=\"4%%\"><b>Length</b></td>\n");
			printf("</tr>\n");

			if (strcmp(crispr->strand, "+") == 0)
			{
				for (pairCount = 0; pairCount < (int) crispr->blockCount; pairCount++)
				{
					printf("<tr style=\"vertical-align: top;\">\n");

					memset(tempSeq, '\0', sizeof(tempSeq));
					memcpy(tempSeq, sequence->dna + crispr->chromStarts[pairCount], crispr->blockSizes[pairCount]);
					printf("<td>%d</td><td>%s</td><td>%d</td>\n", crispr->chromStart + 1 + crispr->chromStarts[pairCount], tempSeq, crispr->blockSizes[pairCount]);
					if (pairCount + 1 < crispr->blockCount)
					{
						memset(tempSeq, '\0', sizeof(tempSeq));
						memcpy(tempSeq, sequence->dna + crispr->chromStarts[pairCount] + crispr->blockSizes[pairCount],
							crispr->chromStarts[pairCount+1] - crispr->blockSizes[pairCount] - crispr->chromStarts[pairCount]);
						printf("<td>%d</td><td>%s</td><td>%d</td>\n", crispr->chromStart + 1 + crispr->chromStarts[pairCount] + crispr->blockSizes[pairCount], tempSeq,
							   crispr->chromStarts[pairCount+1] - crispr->blockSizes[pairCount] - crispr->chromStarts[pairCount]);
					}
					else
					{
						printf("<td>&nbsp</td><td>&nbsp</td><td>&nbsp</td>\n");
					}

					printf("</tr>\n");
				}
			}
            else
			{
				for (pairCount = ((int)crispr->blockCount - 1); pairCount >= 0; pairCount--)
				{
					printf("<tr style=\"vertical-align: top;\">\n");

					memset(tempSeq, '\0', sizeof(tempSeq));
					memcpy(tempSeq, sequence->dna + crispr->chromStarts[pairCount], crispr->blockSizes[pairCount]);
					reverseComplement(tempSeq, strlen(tempSeq));
					printf("<td>%d</td><td>%s</td><td>%d</td>\n", crispr->chromStart + crispr->chromStarts[pairCount] + crispr->blockSizes[pairCount], tempSeq, crispr->blockSizes[pairCount]);
					if (pairCount - 1 >= 0)
					{
						memset(tempSeq, '\0', sizeof(tempSeq));
						memcpy(tempSeq, sequence->dna + crispr->chromStarts[pairCount-1] + crispr->blockSizes[pairCount-1],
							   crispr->chromStarts[pairCount] - crispr->blockSizes[pairCount-1] - crispr->chromStarts[pairCount-1]);
						reverseComplement(tempSeq, strlen(tempSeq));
						printf("<td>%d</td><td>%s</td><td>%d</td>\n", crispr->chromStart + crispr->chromStarts[pairCount], tempSeq,
							   crispr->chromStarts[pairCount] - crispr->blockSizes[pairCount-1] - crispr->chromStarts[pairCount-1]);
					}
					else
					{
						printf("<td>&nbsp</td><td>&nbsp</td><td>&nbsp</td>\n");
					}

					printf("</tr>\n");
				}
			}

			/* Close table */
			printf("</tbody>\n");
			printf("</table>\n");
			printf("</td></tr></tbody>\n");
			printf("</table>\n");
        }

        if (crispr->next != NULL)
            printf("<hr>\n");
    }
    sqlFreeResult(&sr);

    bedFree(&crispr);

    hFreeConn(&conn);
    printTrackHtml(tdb);
}

bool loweLabClick(char *track, char *item, struct trackDb *tdb)
/* check if we have one of the lowelab tracks */
{
if (sameWord(track, "gbProtCode"))
    {
    llDoCodingGenes(tdb, item,"gbProtCodePep","gbProtCodeXra");
    }
else if (sameWord(track, "refSeq"))
    {
    doRefSeq(tdb, item,"gbProtCodePep","gbProtCodeXra");
    }
else if (startsWith("annotRev", track))
    {
    doRefSeq(tdb, item,"Pep","Xra");
    }
else if (sameWord(track, "sargassoSea"))
    {
    doSargassoSea(tdb, item);
    }
else if (sameWord(track, "tigrCmrORFs"))
    {
    doTigrCmrGene(tdb,item);
    }
else if (sameWord(track, "jgiGene"))
    {
    doJgiGene(tdb,item);
    }
else if (sameWord(track, "lowelabPfamHits"))
    {
    doPfamHit(tdb,item);
    }
/*else if (sameWord(track, "tigrOperons"))*/
else if (sameWord(track, "lowelabTIGROperons"))
    {
    doTigrOperons(tdb,item);
    }
else if (sameWord(track, "lowelabArkinOperons"))
    {
    doArkinOperons(tdb,item);
    }
else if (sameWord(track,"codeBlast"))
    {
    doCodeBlast(tdb, item);
    }
else if (sameWord(track,"gbRNAs"))
    {
    doGbRnaGenes(tdb, item);
    }
else if (sameWord(track,"tRNAs"))
    {
    doTrnaGenes(tdb, item);
    }
else if (sameWord(track,"snoRNAs"))
    {
    doSnornaGenes(tdb, item);
    }
else if (sameWord(track,"easyGene"))
    {
    doEasyGenes(tdb, item);
    }
else if (startsWith("BlastP_", track)
        && differentWord(track, "BlastP_Crenarchaea") && differentWord(track, "BlastP_Nanoarch") && differentWord(track, "BlastP_Euryarch")
        && differentWord(track, "BlastP_Bacteria"))
    {
    doBlastP(tdb, item);
    }
else if (startsWith("BlastX_", track)
        && differentWord(track, "BlastX_Crenarchaea") && differentWord(track, "BlastX_Nanoarch") && differentWord(track, "BlastX_Euryarch")
        && differentWord(track, "BlastX_Bacteria"))
    {
    doBlastX(tdb, item);
    }
else if (sameWord(track,"ultraConserved"))
  {
    doUltraConserved(tdb, item);
  }
else if (sameWord(track,"genomePcrPrimers") || sameWord(track,"goldRTPrimers"))
  {
    doPrimers(tdb, item);
  }
else if (sameWord(track,"wiki") || sameWord(track,"wikibme"))
   {
   doWiki(track,tdb,item);
   }
else if (sameWord(track,"rnaHybridization"))
  {
    doRNAHybridization(tdb, item);
  }
else if (sameWord(track,"cddInfo"))
  {
    doCddInfo(tdb, item);
  }
else if (sameWord(track,"alignInfo"))
  {
    doAlignInfo(tdb, item);
  }
else if (sameWord(track,"arCOGs"))
  {
    doarCOGs(tdb, item);
  }
else if (sameWord(track,"megablastInfo") || sameWord(track,"vecScreenInfo"))
  {
    domegablastInfo(tdb, item);
  }
else if (sameWord(track,"loweOrthologs"))
  {
    doloweOrthologs(tdb, item);
  }
else if (sameWord(track,"selfHomologs"))
  {
    doSelfHomologs(tdb, item);
  }
else if (sameWord(track,"CRISPRs"))
	{
		doCRISPRs(tdb, item);
	}
else
    return FALSE;
return TRUE;
}

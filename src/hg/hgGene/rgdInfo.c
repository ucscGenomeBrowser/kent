/* rgdGeneInfo - functions related to RGD Genes */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "hdb.h"
#include "hgGene.h"

// define external URLs used
#define  RGD_GENE_URL   "http://rgd.mcw.edu/tools/genes/genes_view.cgi?id="
#define  PUBMED_URL	"http://www.ncbi.nlm.nih.gov/pubmed/"
#define  GBNK_PROT_URL	"http://www.ncbi.nlm.nih.gov/protein/"
#define  GBNK_DNA_URL	"http://www.ncbi.nlm.nih.gov/nuccore/"
#define  ENTREZ_URL	"http://www.ncbi.nlm.nih.gov/sites/entrez?db=gene&cmd=retrieve&list_uids="
#define  UNIPROT_URL	"http://www.uniprot.org/uniprot/"
#define  UNIGENE_URL	"http://www.ncbi.nlm.nih.gov/UniGene/clust.cgi?ORG=Rn&CID="
#define  ENSEMBL_URL	"http://www.ensembl.org/Rattus_norvegicus/Gene/Summary?g="
#define  TIGR_URL	"http://compbio.dfci.harvard.edu/tgi/cgi-bin/tgi/tc_report.pl?gudb=rat&tc="
#define  SSLP_URL	"http://rgd.mcw.edu/objectSearch/sslpReport.jsp?rgd_id="
#define  QTL_URL	"http://rgd.mcw.edu/objectSearch/qtlReport.jsp?rgd_id="
#define  IMAGE_URL	"http://image.hudsonalpha.org/IQ/bin/singleCloneQuery?clone_id="
#define  MGC_URL	"http://mgc.nci.nih.gov/Genes/CloneList?ORG=Rn&LIST="

char *getRgdGeneId(struct sqlConnection *conn, char *geneId)
/* Return rgdGene ID for now. */
{
return(geneId);
}

char *getRgdGeneUniProtAcc(char *geneId, struct sqlConnection *conn)
/* get UniProt Acc from an RGD Gene ID */
{
char query[256];
struct sqlResult *sr;
char **row;
char *protAcc;

sqlSafef(query, sizeof(query), "select value from rgdGene2ToUniProt where name = '%s'", geneId);
sr = sqlGetResult(conn, query);

row = sqlNextRow(sr);
if (row != NULL)
    {
    protAcc = strdup(row[0]);
    sqlFreeResult(&sr);
    return(protAcc);
    }
else
    {
    sqlFreeResult(&sr);
    return(NULL);
    }
}

boolean isRgdGene(struct sqlConnection *conn)
/* Return true if the gene set is RGD Genes. */
{
/* The existence of the table rgdGene2Raw storing
 * GENES_RAT data from RGD indicates that we are using RGD Genes as
 * our main gene set for this genome. */
return(hTableExists(sqlGetDatabase(conn), "rgdGene2Raw"));
}

static boolean rgdGeneInfoExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if rgdGene info tables exist. */
{
char *rgdGeneId = getRgdGeneId(conn, geneId);

if (rgdGeneId == NULL)
    return FALSE;
else
    return(TRUE);
}

void do1Colx(char **row, int iCol, char *colName, char *entryName, char *externalUrl)
    {
    char *chp, *chp1;
    if (strlen(row[iCol]) > 0) 
    	{
	printf("<b>%s:</b>\n", entryName);

	if (externalUrl == NULL)
	    {
	    printf("%s<br>", row[iCol]);
	    return;
	    }
	chp1 = row[iCol];
	chp  = chp1;
	chp++;
	while (*chp != '\0')
	    {
	    if (*chp == ',')
	    	{
		*chp = '\0';
		printf("<A HREF=\"%s", externalUrl);
		printf("%s", chp1);fflush(stdout);
		printf("\" TARGET=_blank>%s</A>, ", chp1);fflush(stdout);

		chp1 = chp;
		chp1++;
		}
	    chp ++;
    	    }
	
	/* print the last entry */
	printf("<A HREF=\"%s", externalUrl);
	printf("%s", chp1);
	printf("\" TARGET=_blank>%s</A>", chp1);
	printf("<br>\n");
	}	
    }

void do1Col(char **row, int iCol, char *colName)
    {
    if (strlen(row[iCol]) > 0) 
    	{
	printf("<b>%s:</b> %s<br>\n", colName, row[iCol]);fflush(stdout);
    	}
    }

static void rgdGeneInfoPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out RgdGene info. */
{
char *rgdGeneId = getRgdGeneId(conn, geneId);
char query[256], **row;
struct sqlResult *sr;
char *chp;
int iCol;

chp = strstr(rgdGeneId, ":"); chp++;
sqlSafef(query, sizeof(query),
      "select * from %s where gene_rgd_id='%s'", section->rgdGeneTable, chp);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    	{
    	/* Data of some columns are not displayed.
	   The statements for those are left there but commented out for documentation purpose.
	   In case we need additional variables displayed, just un-comment the lines.
	*/

	iCol = 0;
	do1Colx(row, iCol, "gene_rgd_id", "RGD Gene", RGD_GENE_URL); 
	
	iCol++;
	do1Colx(row, iCol, "symbol", "Gene Symbol", NULL);
	
	iCol++;
	do1Colx(row, iCol, "name", "Name", NULL); 
	
	iCol++;
	//do1Col(row, iCol, "gene_desc"); 
	
	iCol++;
	//do1Col(row, iCol, "chromosome_celera"); 
	
	iCol++;
	//do1Col(row, iCol, "chromosome_31"); 
	
	iCol++;
	//do1Col(row, iCol, "chromosome_34"); 
	
	iCol++;
	//do1Col(row, iCol, "fish_band"); 
	
	iCol++;
	//do1Col(row, iCol, "start_pos_celera"); 
	
	iCol++;
	//do1Col(row, iCol, "stop_pos_celera"); 
	
	iCol++;
	//do1Col(row, iCol, "strand_celera"); 
	
	iCol++;
	//do1Col(row, iCol, "start_pos_31"); 
	
	iCol++;
	//do1Col(row, iCol, "stop_pos_31"); 
	
	iCol++;
	//do1Col(row, iCol, "strand_31"); 
	
	iCol++;
	//do1Col(row, iCol, "start_pos_34"); 
	
	iCol++;
	//do1Col(row, iCol, "stop_pos_34"); 
	
	iCol++;
	//do1Col(row, iCol, "strand_34"); 
	
	iCol++;
	//do1Col(row, iCol, "curated_ref_rgd_id"); 
	
	iCol++;
	do1Colx(row, iCol, "curated_ref_pubmed_id", "Curated Pubmed Papers", PUBMED_URL); 
	
	iCol++;
	do1Colx(row, iCol, "uncurated_pubmed_id", "Uncurated Pubmed Papers", PUBMED_URL); 
	
	iCol++;
	//do1Col(row, iCol, "ratmap_id"); 
	
	iCol++;
	do1Colx(row, iCol, "entrez_gene", "Entrez Gene", ENTREZ_URL); 
	
	iCol++;
	do1Colx(row, iCol, "uniprot_id", "UniProt", UNIPROT_URL); 
	
	iCol++;
	//do1Col(row, iCol, "rhdb_id"); 
	
	iCol++;
	//do1Col(row, iCol, "uncurated_ref_medline_id"); 
	
	iCol++;
	do1Colx(row, iCol, "genbank_nucleotide", "GenBank Nucleotide", GBNK_DNA_URL); 
	
	iCol++;
	do1Colx(row, iCol, "tigr_id", "TIGR", TIGR_URL); 
	
	iCol++;
	do1Colx(row, iCol, "genbank_protein", "Genebank Protein", GBNK_PROT_URL); 
	
	iCol++;
	stripString(row[iCol],"Rn.");
	do1Colx(row, iCol, "unigene_id", "UniGene", UNIGENE_URL); 
	
	iCol++;
	//do1Col(row, iCol, "gdb_id"); 
	
	iCol++;
	do1Colx(row, iCol, "sslp_rgd_id", "RGD SSLP", SSLP_URL); 
	
	iCol++;
	//do1Col(row, iCol, "sslp_symbol"); 
	
	iCol++;
	//do1Col(row, iCol, "old_symbol"); 
	
	iCol++;
	//do1Col(row, iCol, "old_name"); 
	
	iCol++;
	do1Colx(row, iCol, "qtl_rgd_id", "RGD QTL", QTL_URL); 
	
	iCol++;
	//do1Col(row, iCol, "qtl_symbol"); 
	
	iCol++;
	//do1Col(row, iCol, "nomenclature_status"); 
	
	iCol++;
	//do1Col(row, iCol, "splice_rgd_id"); 
	
	iCol++;
	//do1Col(row, iCol, "splice_symbol"); 
	
	iCol++;
	//do1Col(row, iCol, "gene_type"); 
	
	iCol++;
	do1Colx(row, iCol, "ensembl_id", "Ensembl Gene", ENSEMBL_URL); 
    }
sqlFreeResult(&sr);

/* display IMAGE info */
sqlSafef(query, sizeof(query), "select info from rgdGene2Xref where rgdGeneId='%s' and infoType='IMAGE'", rgdGeneId);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    printf("<b>IMAGE CLONE: </b>");

    while (row != NULL)
    	{
    	printf("<A HREF=\"%s", IMAGE_URL);
    	printf("%s", row[0]);fflush(stdout);
    	printf("\" TARGET=_blank>%s</A>", row[0]);fflush(stdout);
	row = sqlNextRow(sr);
	if (row != NULL) printf(", ");
        }
    printf("<br>");
    }
sqlFreeResult(&sr);

/* display MGC info */
sqlSafef(query, sizeof(query), "select info from rgdGene2Xref where rgdGeneId='%s' and infoType='MGC'", rgdGeneId);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    printf("<b>MGC: </b>");

    while (row != NULL)
    	{
    	printf("<A HREF=\"%s", MGC_URL);
    	printf("%s", row[0]);fflush(stdout);
    	printf("\" TARGET=_blank>%s</A>", row[0]);fflush(stdout);
	row = sqlNextRow(sr);
	if (row != NULL) printf(", ");
        }
    printf("<br>");
    }
sqlFreeResult(&sr);
}

struct section *rgdGeneInfoSection(struct sqlConnection *conn,
	struct hash *sectionRa, char *sectionName, char *table)
/* Create RgdGene info section. */
{
struct section *section = sectionNew(sectionRa, sectionName);
if (section != NULL)
    {
    section->exists       = rgdGeneInfoExists;
    section->print 	  = rgdGeneInfoPrint;
    section->rgdGeneTable = table;
    }
return section;
}

/* display info from downloaded raw data file */
struct section *rgdGeneRawSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create RgdGene roles section. */
{
return rgdGeneInfoSection(conn, sectionRa, "rgdGeneRaw", "rgdGene2Raw");
}


/* hgc - Human Genome Click processor - gets called when user clicks
 * on something in human tracks display. */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgXref - create Known Gene cross reference table kgXref.tab file."
  "usage:\n"
  "   kgXref xxxx yyyy\n"
  "          xxxx is genome  database name\n"
  "          yyyy is protein database name \n"
  "example: kgXref hg15 proteins0405\n");
}

int main(int argc, char *argv[])
    {
    struct sqlConnection *conn, *conn2;
    char query[256], query2[256];
    struct sqlResult *sr, *sr2;
    char **row, **row2;
    char cond_str[256];  
  
    char *kgID;
    char *proteinDisplayID;
    
    char *seqType;	/* sequence type m=mRNA g=genomic u=undefined */

    FILE *o1;
    char *database;
    char *proteinDB;
    char *refSeqName;
    char *hugoID;
    char *answer;

    int leg;
    char *mRNA, *spID, *spDisplayID, *geneSymbol, *refseqID, *desc;

    if (argc != 3) usage();
    database  = cloneString(argv[1]);
    proteinDB = cloneString(argv[2]);

    conn = hAllocConn();
    conn2= hAllocConn();

    o1 = fopen("j.dat", "w");
	
    sprintf(query2,"select name, proteinID from %s.knownGene;", database);
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    while (row2 != NULL)
	{
	kgID 		= row2[0];
	spDisplayID	= row2[1];
	
	refseqID 	= strdup("");
	geneSymbol 	= strdup("");
	desc		= strdup("");

        sprintf(cond_str, "displayID='%s'", spDisplayID);
        spID = sqlGetField(conn, proteinDB, "spXref3", "accession", cond_str);
        
	// use description for the protein as default. Later replace it with HUGO desc if available.
	sprintf(cond_str, "displayID='%s'", spDisplayID);
        desc  = sqlGetField(conn, proteinDB, "spXref3", "description", cond_str);
        
	sprintf(cond_str, "name='%s' and seqType='g'", kgID);
        seqType = sqlGetField(conn, database, "knownGeneLink", "seqType", cond_str);

        if (seqType != NULL)
            {
	    leg = 1;
            // special processing for RefSeq DNA based genes
            sprintf(cond_str, "mrnaAcc = '%s'", kgID);
            refSeqName = sqlGetField(conn, database, "refLink", "name", cond_str);
            if (refSeqName != NULL)
                {
		//printf("leg 1\n");fflush(stdout);
                geneSymbol = cloneString(refSeqName);
		refseqID   = kgID;
            	sprintf(cond_str, "mrnaAcc = '%s'", kgID);
            	desc = sqlGetField(conn, database, "refLink", "product", cond_str);
                }
            }
        else
            {
            sprintf(cond_str, "displayID = '%s'", spDisplayID);
            hugoID = sqlGetField(conn, proteinDB, "spXref3", "hugoSymbol", cond_str);
            if (!((hugoID == NULL) || (*hugoID == '\0')) )
                {
		leg = 21;
		//printf("leg 3\n");fflush(stdout);
                geneSymbol = cloneString(hugoID);

            	sprintf(cond_str, "displayID = '%s'", spDisplayID);
            	desc = sqlGetField(conn, proteinDB, "spXref3", "hugoDesc", cond_str);
		}

            sprintf(cond_str, "mrna = '%s'", kgID);
            answer = sqlGetField(conn, database, "mrnaRefseq", "refseq", cond_str);
	    if (answer != NULL) 
		{
		leg = 22;
		refseqID = strdup(answer);
		//printf("refseq=%s\n", refseqID);fflush(stdout);
		}
            	
	    if (strlen(geneSymbol) == 0)
		{ 
		leg = 23;
		if (strlen(refseqID) != 0)
			{
			sprintf(cond_str, "mrnaAcc = '%s'", refseqID);
			answer = sqlGetField(conn, database, "refLink", "name", cond_str);
			if (answer != NULL) 
				{
				leg = 24;
				geneSymbol = strdup(answer);
				}
			}
                }
            }

	// fix missing fields 
	if (strlen(refseqID) == 0)
		{
		//printf("%3d %s reseqID is empty.\n", leg, kgID);fflush(stdout);
		}

	if (strlen(geneSymbol) == 0)
		{
		//printf("%3d %s geneSymbol is empty.\n", leg, kgID);fflush(stdout);
		geneSymbol = strdup(kgID);
		}

	if (strlen(desc) == 0)
		{
		//printf("%3d %s desc is empty.\n", leg, kgID);fflush(stdout);
		desc = strdup("N/A");
		}
	
	fprintf(o1, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
		kgID, kgID, spID, spDisplayID, geneSymbol, refseqID, desc);
	fflush(stdout);
	row2 = sqlNextRow(sr2);
	}

    fclose(o1);
    system("cat j.dat|sort|uniq  >kgXref.tab");
    system("rm j.dat");
    return(0);
    }
	

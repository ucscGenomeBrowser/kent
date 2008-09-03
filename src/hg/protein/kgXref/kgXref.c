/* kgXref - create Known Gene cross reference table kgXref.tab file */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgXref - create Known Gene cross reference table kgXref.tab file."
  "usage:\n"
  "   kgXref <db> <proteinsYYMMDD> <ro_db>\n"
  "          <db> is known Genes database under construction\n"
  "          <proteinsYYMMDD> is protein database name \n"
  "          <ro_db> is target organism database\n"
  "example: kgXref kgDB proteins040115 hg16\n");
}

int main(int argc, char *argv[])
    {
    struct sqlConnection *conn, *conn2, *conn3;
    char query2[256];
    struct sqlResult *sr2;
    char **row2;
    char cond_str[256];  
  
    char *kgID;
    
    char *seqType;	/* sequence type m=mRNA g=genomic u=undefined */

    FILE *o1;
    char *database;
    char *proteinDB;
    char *ro_DB;
    char *refSeqName;
    char *hugoID;
    char *protAcc;	/* protein Accession number from NCBI */
    char *answer;

    int leg;		/* marker for debugging */
    char *spID, *spDisplayID, *geneSymbol, *refseqID, *desc;

    if (argc != 4) usage();
    database  = cloneString(argv[1]);
    proteinDB = cloneString(argv[2]);
    ro_DB = cloneString(argv[3]);

    conn = hAllocConn(database);
    conn2= hAllocConn(database);
    conn3= hAllocConn(database);

    o1 = mustOpen("j.dat", "w");
	
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
	protAcc		= strdup("");

        sprintf(cond_str, "displayID='%s'", spDisplayID);
        spID = sqlGetField(proteinDB, "spXref3", "accession", cond_str);
        
	/* use description for the protein as default, replace it with HUGO desc if available. */
	sprintf(cond_str, "displayID='%s'", spDisplayID);
        desc  = sqlGetField(proteinDB, "spXref3", "description", cond_str);
        
	sprintf(cond_str, "name='%s' and seqType='g'", kgID);
        seqType = sqlGetField(database, "knownGeneLink", "seqType", cond_str);

        if (seqType != NULL)
            {
	    leg = 1;
            /* special processing for RefSeq DNA based genes */
            sprintf(cond_str, "mrnaAcc = '%s'", kgID);
            refSeqName = sqlGetField(ro_DB, "refLink", "name", cond_str);
            if (refSeqName != NULL)
                {
                geneSymbol = cloneString(refSeqName);
		refseqID   = kgID;
            	sprintf(cond_str, "mrnaAcc = '%s'", kgID);
            	desc = sqlGetField(ro_DB, "refLink", "product", cond_str);
		
		sprintf(cond_str, "mrnaAcc='%s'", refseqID);
        	answer = sqlGetField(ro_DB, "refLink", "protAcc", cond_str);
        	if (answer != NULL)
            	    {
	    	    protAcc = strdup(answer);
	    	    }
                }
            }
        else
            {
            sprintf(cond_str, "displayID = '%s'", spDisplayID);
            hugoID = sqlGetField(proteinDB, "spXref3", "hugoSymbol", cond_str);
            if (!((hugoID == NULL) || (*hugoID == '\0')) )
                {
		leg = 21;
                geneSymbol = cloneString(hugoID);

            	sprintf(cond_str, "displayID = '%s'", spDisplayID);
            	desc = sqlGetField(proteinDB, "spXref3", "hugoDesc", cond_str);
		}

            sprintf(cond_str, "mrna = '%s'", kgID);
            answer = sqlGetField(database, "mrnaRefseq", "refseq", cond_str);
	    if (answer != NULL) 
		{
		leg = 22;
		refseqID = strdup(answer);
		
		sprintf(cond_str, "mrnaAcc='%s'", refseqID);
        	answer = sqlGetField(ro_DB, "refLink", "protAcc", cond_str);
        	if (answer != NULL)
            	    {
	    	    protAcc = strdup(answer);
	    	    }
		}
            	
	    if (strlen(geneSymbol) == 0)
		{ 
		leg = 23;
		if (strlen(refseqID) != 0)
			{
			sprintf(cond_str, "mrnaAcc = '%s'", refseqID);
			answer = sqlGetField(ro_DB, "refLink", "name", cond_str);
			if (answer != NULL) 
				{
				leg = 24;
				geneSymbol = strdup(answer);
				}
			}
                }
            }

	/* fix missing fields */
	if (strlen(refseqID) == 0)
		{
		/* printf("%3d %s reseqID is empty.\n", leg, kgID);*/
		}

	if (strlen(geneSymbol) == 0)
		{
		/* printf("%3d %s geneSymbol is empty.\n", leg, kgID);fflush(stdout);*/
		geneSymbol = strdup(kgID);
		}

	if (strlen(desc) == 0)
		{
		/* printf("%3d %s desc is empty.\n", leg, kgID);fflush(stdout);*/
		desc = strdup("N/A");
		}
	
	fprintf(o1, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
		kgID, kgID, spID, spDisplayID, geneSymbol, refseqID, protAcc, desc);
	row2 = sqlNextRow(sr2);
	}

    fclose(o1);
    hFreeConn(&conn);
    hFreeConn(&conn2);
    hFreeConn(&conn3);
    system("cat j.dat|sort|uniq  >kgXref.tab");
    system("rm j.dat");
    return(0);
    }

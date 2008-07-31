/* kgXref2 - create new Known Gene cross reference table kgXref.tab file */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgXref2 - create new Known Gene cross reference table kgXref2.tab file."
  "usage:\n"
  "   kgXref2 <tmpDb> <YYMMDD> <roDb>\n"
  "          <tmpDb> is temp KG database under construction\n"
  "          <YYMMDD> is protein database release date \n"
  "          <roDb> is read only target organism database\n"
  "example: kgXref2 kgHg17FTempgDB 050415 hg17\n");
}

int main(int argc, char *argv[])
    {
    struct sqlConnection *conn, *conn2, *conn3;
    char query2[256];
    struct sqlResult *sr2;
    char **row2;
    char cond_str[256];  
  
    char *protDbDate;
    char *kgID;
    char *protDisplayId;
    
    FILE *o1;
    char *kgTempDb;
    char spDb[255],proteinsDb[255];
    char *ro_DB;
    char *refSeqName;
    char *hugoID;
    char *protAcc;	/* protein Accession number from NCBI */
    char *answer;
    char *emptyStr;
    char *parSpID;
    
    int leg;		/* marker for debugging */
    char *spID, *kgProteinID, *geneSymbol, *refseqID, *desc;

    if (argc != 4) usage();
    kgTempDb  = cloneString(argv[1]);
    protDbDate = cloneString(argv[2]);
    ro_DB = cloneString(argv[3]);
    
    safef(spDb, sizeof(spDb), "sp%s",  protDbDate);
    safef(proteinsDb, sizeof(proteinsDb), "proteins%s", protDbDate);

    conn = hAllocConn(ro_DB);
    conn2= hAllocConn(ro_DB);
    conn3= hAllocConn(ro_DB);

    o1 = mustOpen("j.dat", "w");

    emptyStr = strdup("");

    sprintf(query2,"select name, proteinID from %s.knownGene;", kgTempDb);
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    while (row2 != NULL)
	{
	kgID 		= row2[0];
	kgProteinID	= row2[1];
	
	refseqID 	= strdup("");
	geneSymbol 	= strdup("");
	desc		= strdup("");
	protAcc		= strdup("");

        sprintf(cond_str, "displayID='%s'", kgProteinID);
        spID = sqlGetField(proteinsDb, "spXref3", "accession", cond_str);
    
        /* process variant splice proteins */
	if (spID == NULL)
	    {
            sprintf(cond_str, "varAcc='%s'", kgProteinID);
	    spID = kgProteinID;
	    
            parSpID = sqlGetField(proteinsDb, "splicProt", "parAcc", cond_str);
	    if (parSpID != NULL)
	    	{
        	sprintf(cond_str, "accession='%s'", parSpID);
        	protDisplayId = sqlGetField(proteinsDb, "spXref3", "displayID", cond_str);
		}
	    else
	    	{
		fprintf(stderr, "%s not found in kgXref3 nor in varProtein.\n", kgProteinID);
		exit(1);
		}
	    }
	else
	    {
	    protDisplayId = kgProteinID;	
	    }
	/* use description for the protein as default, replace it with HUGO desc if available. */
	sprintf(cond_str, "displayID='%s'", protDisplayId);
        desc  = sqlGetField(proteinsDb, "spXref3", "description", cond_str);
        
        if (strstr(kgID, "NM_") != NULL)
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
            sprintf(cond_str, "displayID = '%s'", protDisplayId);
            hugoID = sqlGetField(proteinsDb, "spXref3", "hugoSymbol", cond_str);
            if (!((hugoID == NULL) || (*hugoID == '\0')) )
                {
		leg = 21;
                geneSymbol = cloneString(hugoID);

            	sprintf(cond_str, "displayID = '%s'", protDisplayId);
            	desc = sqlGetField(proteinsDb, "spXref3", "hugoDesc", cond_str);
		if (desc == NULL) 
		    {
		    printf("%s/%s don't have hugo desc ...\n", kgProteinID, protDisplayId);
		    fflush(stdout);
		    }
		}

	    refseqID = emptyStr;
	    protAcc  = emptyStr;
            sprintf(cond_str, "mrna = '%s'", kgID);
            answer = sqlGetField(ro_DB, "mrnaRefseq", "refseq", cond_str);
	    if (answer != NULL) 
	    	{
		refseqID = answer;
		}
	    else
	    	{
		/*printf("%s does not have a related RefSeq.\n", kgID);fflush(stdout); */
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
		/* printf("%3d %s reseqID is empty.\n", leg, kgID); */
		}

	if (strlen(geneSymbol) == 0)
		{
		/* printf("%3d %s geneSymbol is empty.\n", leg, kgID);fflush(stdout);*/
		geneSymbol = strdup(kgID);
		}

	if (strlen(desc) == 0)
		{
		/* printf("%3d %s desc is empty.\n", leg, kgID);fflush(stdout); */
		desc = strdup("N/A");
		}
	
	fprintf(o1, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
		kgID, kgID, spID, protDisplayId, geneSymbol, refseqID, protAcc, desc);
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

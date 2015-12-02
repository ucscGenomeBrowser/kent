/* hgSuperfam - make tables for Supfamily track */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"

#define MAX_LOG_EVALUE 730.0

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSuperfam - Generate supfamily table for the Superfamily track.\n"
  "usage:\n"
  "   hgSuperfam gDb pDb\n"
  "      gDb   is the genome database\n"
  "      sDb   is the superfamily database\n"
  "example: hgSuperfam mm5 superfam041128\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *connEnsGene, *connSf;
char query[256], query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
char cond_str[200];

char *seqID, *eValue, *sfID, *sfDesc;
char *name, *chrom, *cdsStart, *cdsEnd;
float E,score;

char *translation_name = NULL;

char *genomeDb, *superfamDb;
char gene_name[200];

char *chp;

FILE *o3, *o4;
   
if (argc != 3) usage();
genomeDb = argv[1];
superfamDb = argv[2];
 
o3 = mustOpen("j.dat", "w");
o4 = mustOpen("jj.dat", "w");

conn = hAllocConn(genomeDb);
connEnsGene= hAllocConn(genomeDb);
connSf= hAllocConn(genomeDb);

sqlSafef(query2,sizeof(query2),"select * from %s.ensGene;", genomeDb);

sr2 = sqlMustGetResult(connEnsGene, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    name 	= row2[0];
    chrom 	= row2[1];
    cdsStart	= row2[5]; 
    cdsEnd	= row2[6];
			
    strcpy(gene_name, name);

    
    /* get rid of version number */
    chp = strstr(name, ".");
    if (chp != NULL) *chp = '\0';

    if (hTableExists(genomeDb, "ensGeneXref"))
        {
        sqlSafefFrag(cond_str, sizeof(cond_str), "transcript_name='%s'", name);
        translation_name = sqlGetField(genomeDb, "ensGeneXref", "translation_name", cond_str);
        }
    if (hTableExists(genomeDb,"ensemblXref3") && translation_name == NULL)
        {
        sqlSafefFrag(cond_str, sizeof(cond_str), "transcript='%s'", name);
        translation_name = sqlGetField(genomeDb, "ensemblXref3", "protein", cond_str);
        }
    if (translation_name == NULL) 
	{
	printf("transcript not found, skipping %s\n", name);
	fflush(stdout);
	}
    else
	{
	/* get rid of version number */
    	chp = strstr(translation_name, ".");
    	if (chp != NULL) *chp = '\0';

    	sqlSafef(query, sizeof query, "select * from %s.sfAssign where seqID='%s'", superfamDb, translation_name);
    	sr = sqlMustGetResult(conn, query);
    	row = sqlNextRow(sr);

    	if (row == NULL) 
	    {
	    printf("%s not found in sfAssign!\n", translation_name);
	    fflush(stdout);
	    }

    	while (row != NULL)
            {      
 	    seqID 	= row[1];
 	    eValue	= row[4];
 	    sfID	= row[5];
 	    sfDesc	= row[6];	/* 0302 and other supfam releases has an error here */
		
	    sqlSafefFrag(cond_str, sizeof cond_str, "id=%s", sfID);
	    sfDesc  = sqlGetField(superfamDb, "des", "description", cond_str);

	    E = atof(eValue);
	    if (E > 0.02) continue;

	    score = (-log(E)/MAX_LOG_EVALUE)*10000.0;
	    if (score <= 0.0) score = 0.0;
	    if (score > 1000.0) score = 1000.0;
		
	    fprintf(o3, "%s\t%s\t%s\t%s\n", chrom, cdsStart, cdsEnd, name);
	    fprintf(o4, "%s\t%s\t%s\n", name, seqID, sfDesc);
            row = sqlNextRow(sr);
	    }
    	sqlFreeResult(&sr);
    	}
    row2 = sqlNextRow(sr2);
    }

sqlFreeResult(&sr2);

hFreeConn(&conn);
hFreeConn(&connEnsGene);
hFreeConn(&connSf);
    
fclose(o3);
fclose(o4);

mustSystem("cat j.dat |sort|uniq  > superfamily.tab");
mustSystem("cat jj.dat|sort|uniq  > sfDescription.tab");
mustSystem("rm j.dat");
mustSystem("rm jj.dat");
   
return(0);
}


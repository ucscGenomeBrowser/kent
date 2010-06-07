/* hgJaxQtl - generate the bed file for jaxQTL3 table using the mySQL table jaxQtlRaw as input. */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgJaxQtl - generate bed file for jaxQTL3 table \n"
  "           using the table jaxQtlRaw as input\n"
  "           output file is jaxQTL3.tab.\n"
  "usage:\n"
  "   hgJaxQtl xxxx\n"
  "      xxxx is the database\n"
  "example: hgJaxQtl mm5\n");
}

/* get valid identNo from stsMapMouseNew table, return NULL if not found */
char *getStsId(struct sqlConnection *conn3, char *database, char *stsName)
{
char cond_str[255];
char *identNo;
safef(cond_str, sizeof(cond_str), "name='%s'", stsName);
identNo = sqlGetField(database, "stsMapMouseNew", "identNo", cond_str);

if (identNo == NULL) 
    {
    /* check to see if stsAlias has it */
    safef(cond_str, sizeof(cond_str), "alias='%s'", stsName);
    identNo = sqlGetField(database, "stsAlias", "identNo", cond_str);
    
    /* now make sure that stsMapMouseNew has an entry for this identNo */
    if (identNo != NULL)
    	{
	safef(cond_str, sizeof(cond_str), "identNo=%s", identNo);
    	identNo = sqlGetField(database, "stsMapMouseNew", "identNo", cond_str);
    	}
    }
return(identNo);
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3;
  
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;

FILE   *outF;

char *database;

char *mgiID, *name, *chrom, *description, *flank1, *flank2, *peak;
char *flank1Pos, *flank2Pos, *peakPos;
char *peakChrom, *peakChromStart, *peakChromEnd;

char *identNo;
char *flanks[2];

char *qtlChrom, *qtlChromStart, *qtlChromEnd, *qtlName, *qtlScore, *qtlStrand;
char *qtlThickStart, *qtlThickEnd, *qtlMarker, *qtlMgiID, *qtlDescription, *qtlCMScore;

char *flankChrom,  *flankChromStart, *flankChromEnd;

char na[10] = {""};
char zero[10] = {"0"};
int qtlValid;
int pos;
int iFlank;

int regionStart, regionEnd;

if (argc != 2) usage();
database  = argv[1];
   
/* the following fields are not available yet */
qtlScore   = strdup("0");
qtlCMScore = strdup("0");
qtlStrand  = strdup(".");

outF = fopen("jaxQTL3.tab", "w");
conn2= hAllocConn(database);
conn3= hAllocConn(database);
	
sprintf(query2,"select * from %s.jaxQtlRaw", database);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    mgiID 	= row2[0];
    name        = row2[1];
    description = row2[2];
    chrom	= row2[3];
    flank1 	= row2[4];
    flank2  	= row2[5];
    peak        = row2[6]; 
    flank1Pos	= row2[7];
    flank2Pos   = row2[8]; 
    peakPos     = row2[9]; 
    
    flanks[0] = flank1;
    flanks[1] = flank2;
    
    qtlValid = 0;	/* this record is not valid until we found something to put out */
    
    qtlMgiID = mgiID;
    qtlName  = name;
    qtlDescription = description;
  
    /* set default values */ 
    qtlChrom 		= strdup(na);
    qtlChromStart 	= zero;
    qtlChromEnd 	= zero;
    qtlThickStart 	= zero;
    qtlThickEnd 	= zero;
    qtlMarker		= na;
    
    regionStart 	= 0;
    regionEnd 		= 0;
    
    peakChrom		= strdup(na);

    /* process peak marker */
    if (!sameWord(peak, ""))
        {
	identNo = getStsId(conn3, database, peak);

	/* if getStsId result is not NULL, there exist an entry in stsMapMouseNew */ 	     
	if (identNo != NULL)
	    {
	    qtlValid = 1;
	    sprintf(query3, "select * from %s.stsMapMouseNew where identNo=%s", database, identNo);

            sr3 = sqlMustGetResult(conn3, query3);
            row3 = sqlNextRow(sr3);
            
	    /* only a few markers have mulitple answers, so just grab the first one */
	    peakChrom 		= strdup(row3[0]);
       	    peakChromStart 	= strdup(row3[1]);	 
            peakChromEnd   	= strdup(row3[2]);

	    qtlChrom 		= peakChrom;
	    qtlChromStart 	= peakChromStart;
	    qtlChromEnd 	= peakChromEnd;
	    qtlThickStart 	= peakChromStart;
	    qtlThickEnd 	= peakChromEnd;
	    qtlMarker		= peak;
	
	    qtlValid = 1;
	    sqlFreeResult(&sr3);
    	    }
    	}
    else
    	{
	/* printf("%s has no peak marker.\n", name);fflush(stdout); */
    	}

    /* process flanking markers to get regionStart and regionEnd if both flanking markers exist */
    if (!sameWord(flank1, "") && !sameWord(flank2,""))
	{
	for (iFlank=0; iFlank<2; iFlank++)
	    {
	    if (flanks[iFlank] != NULL)
		{
		qtlValid = 1;
		identNo = getStsId(conn3, database, flanks[iFlank]);
		if (identNo != NULL)
		    {
		    sprintf(query3,"select * from %s.stsMapMouseNew where identNo=%s",database,identNo);

		    sr3 = sqlMustGetResult(conn3, query3);
		    row3 = sqlNextRow(sr3);
	
		    /* only a few has mulitple answers, so just grab the first one */
		    flankChrom 	= row3[0];
		    flankChromStart = row3[1];	 
		    flankChromEnd   = row3[2];

		    /* if this QTL does not have a peak, use flanking marker's chrom */
		    if (sameWord(qtlChrom,na)) qtlChrom = strdup(flankChrom);
		    
		    pos = sqlUnsigned(flankChromStart);
		    /* initialize regionStart if not initialized */
		    if (regionStart == 0)  regionStart = pos;
		    
		    if (pos < regionStart) regionStart = pos;
		    if (pos > regionEnd)   regionEnd   = pos;
		    
		    pos = sqlUnsigned(flankChromEnd);
		    if (pos < regionStart) regionStart = pos;
		    if (pos > regionEnd)   regionEnd   = pos;
		    
		    sqlFreeResult(&sr3);
		    }
		}
	    }
	}

    /* set regionStart/End for those entries that does not have flanking markers */
    if ((regionStart == 0) && !sameWord(qtlChromStart, zero))
    	{
	regionStart = atoi(qtlChromStart);
	}
    if ((regionEnd == 0) && !sameWord(qtlChromEnd, zero))
    	{
	regionEnd = atoi(qtlChromEnd);
	}

    /* fix problematic cases that thickStart/thickEnd are outside regionStart/regionEnd */
    pos = atoi(qtlThickStart);
    if (! ((pos >= regionStart) && (pos <= regionEnd)))
    	{
	qtlThickStart = strdup(zero);
	}
    pos = atoi(qtlThickEnd);
    if (! ((pos >= regionStart) && (pos <= regionEnd)))
    	{
	qtlThickEnd = strdup(zero);
	}

    /* the following logic was used to check raw data vs processed data */
    /*
    sprintf(chrCheck, "chr%s", chrom);
    if (!sameWord(chrCheck, qtlChrom))printf("=== ");
    
    if (!sameWord(peak, "") && !sameWord(peakPos, "0"))
    	printf("%s\t%s\txx%dxx\t%s\t%s\n", name, peak, -(atoi(peakChromStart)-atoi(peakPos)),
    				    peakPos, peakChromStart);fflush(stdout);
    */

    /* only put out valid entries */	
    if (qtlValid && !sameWord(qtlChrom, na))
       fprintf(outF, "%s\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
       	       qtlChrom, regionStart, regionEnd, qtlName, qtlScore, qtlStrand,
	       qtlThickStart, qtlThickEnd, qtlMarker, qtlMgiID, qtlDescription, 
	       qtlCMScore, flank1, flank2);
    else
    	{
	/* printf("%s\t%s\t%s\t%s\n", flank1, peak, flank2, qtlName);fflush(stdout); */
	}
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(outF);
    
return(0);
}


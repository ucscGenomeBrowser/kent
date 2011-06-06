/* doOmimLocation - This is a one shot program used in the OMIM related subtracks build pipeline */
#include "common.h"
#include "hCommon.h"
#include "dnautil.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "doOmimLocation - This program is part of the OMIM related subtracks build pipeline.\n"
  "usage:\n"
  "   doOmimLocation db outFileName\n"
  "      db is the database name\n"
  "      outFileName is the filename of the output file\n"
  "example: doOmimLocation hg19 omimLocation.tab\n");
}

char *avId, *omimId;
char *location;
FILE   *outf;

int main(int argc, char *argv[])
{
char *database;
char *outFn;
char pqc;
boolean has2Bands;

char gapTableName[255];

struct sqlConnection *conn,*conn2;
char query[1024];
char query2[1024];
struct sqlResult *sr;
struct sqlResult *sr2;
char **row;
char **row2;

int startPos;
int endPos;
int startPosBand1;
int endPosBand1;
int startPosBand2;
int endPosBand2;

char locSav[255];

char *chp;
char *chpTer;
char *chpCen;
char chrom[255];

char band1[255];
char band2[255];

if (argc != 3) usage();

database = argv[1];
conn= hAllocConn(database);
conn2= hAllocConn(database);

outFn   = argv[2];
outf    = mustOpen(outFn, "w");

/* process omimGeneMap records with *p* or *q* locations */
sprintf(query2,
        "select omimId, location from omimGeneMap where location like '\%cp%c' or location like '%cq%c'", 
        '%','%', '%','%');
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    boolean band1Success;
    
    boolean band1HasTer;
    boolean band2HasTer;
    boolean band1HasCen;
    boolean band2HasCen;
    
    band1Success      = FALSE;
    band1HasTer       = FALSE;
    band2HasTer       = FALSE;
    band1HasCen       = FALSE;
    band2HasCen       = FALSE;

    omimId   = row2[0];
    location = row2[1];

    /* save original location string */
    strcpy(locSav, location);
    
    /* break band1 and band2 if there is one */
    chp = strstr(location, "-");
    if (chp != NULL)
    	{
        has2Bands = TRUE;
	*chp = '\0';
	chp++;
	strncpy(band2, chp, (size_t)(sizeof(band2)));
	}
    else
    	{
	has2Bands = FALSE;
	}

    /* construct chrom first */	
    safef(chrom, sizeof(chrom), "chr%s", location);
    chp = chrom;
    
    /* skip "chr" */
    chp++; 
    chp++; 
    chp++; 
   
    /* get the end position of "chrX" */
    while (! ( (*chp == 'p') || (*chp == 'q') || \
               (*chp == 'c') || (*chp == '\0') ) ) chp++;
    
    if (*chp == '\0')
    	{
	/* something is wrong, skip this record */
	fprintf(stderr, "in chrom processing, did not get p, q, or c got %s, locSav=%s, omimId=%s\n", location, locSav, omimId);
	goto skip1;
	}

    /* now we have p or q or c (first char of "cen") */
    pqc = *chp;

    /* construct band1 */
    strncpy(band1, chp, sizeof(band1));
    
    /* terminate the chrom str*/
    *chp = '\0';
    
    // process band1 first 
    
    strcpy(gapTableName, "gap");

    if (sameWord(database, "hg18")) 
    	safef(gapTableName, sizeof(gapTableName), "%s_gap", chrom);

    band1Success  = FALSE;
    chpTer = strstr(band1, "ter");
    if (chpTer != NULL)
    	{
	band1HasTer = TRUE;
	*chpTer = '\0';
	}
    
    if (band1HasTer == TRUE)
    	{
	sprintf(query,
                "select chromStart, chromEnd from %s where chrom = '%s' and type ='telomere' and chromStart = 0", gapTableName, chrom);
    	}
    else
        {
    	/* process "cen" */
    	if (pqc == 'c') 
    	    {
	    band1HasCen  = TRUE;
	    sprintf(query,
                    "select chromStart, chromEnd from %s where chrom = '%s' and type ='centromere'", gapTableName, chrom);
	    }
	else
	    {
	    /* process p or q */
	    if ((pqc == 'p') || (pqc == 'q'))
	    	{
		sprintf(query,
                        "select chromStart, chromEnd from cytoBand where chrom = '%s' and name = '%s'", 
		        chrom, band1);
		}
	    else
	    	{
		fprintf(stderr, "can not deal with omimId=%s locaSav=%s band1=%s\n", omimId, locSav, band1);
		goto skip1;
		}
    	    }
	}

    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL) 
    	{
	startPosBand1 = atoi(row[0]);
	endPosBand1   = atoi(row[1]);
	band1Success  = TRUE;
	}
    else
	{
    	sqlFreeResult(&sr);
    	sprintf(query,
                "select min(chromStart), max(chromEnd) from cytoBand where chrom = '%s' and name like '%s%c'", chrom, band1, '%');
	sr = sqlMustGetResult(conn, query);
    	row = sqlNextRow(sr);
    	if ((row != NULL) && (row[0] != NULL) && (row[1] != NULL))
    	    {
	    startPosBand1 = atoi(row[0]);
	    endPosBand1   = atoi(row[1]);
	    
	    // special treatment for "...ter" band1 location
	    if ((band1HasTer) && (pqc == 'p') && (has2Bands))
	    	{
		endPosBand1   = 0;
		}
	    
	    band1Success  = TRUE;
	    }
	else
	    {
	    band1Success  = FALSE;
	    fprintf(stderr, "band1 processing failed and skipped: band1=%s, locSav=%s, omimId=%s\n", band1, locSav, omimId);
	    }
	}
    sqlFreeResult(&sr);
  
    /* now process band2 */
    if (band1Success && has2Bands)
    	{
	boolean band2Success;
	band2Success = FALSE;

	// do a special processing here for cases like "2q32-34" 
	if (isdigit(band2[0]) && pqc == 'q')
	    {
	    char temp[300];
	    safef(temp, sizeof(temp), "q%s", band2);
	    safef(band2, sizeof(band2), "%s", temp);
	    }

	chpTer = strstr(band2, "ter");
	if (chpTer != NULL) 
	    {
	    band2HasTer = TRUE;
	    *chpTer = '\0';
    	    sprintf(query,
            		"select max(chromEnd), max(chromEnd) from cytoBand where chrom = '%s' and name like '%s%c'", chrom, band2, '%');
	    }
	else
	    {
	    chpCen = strstr(band2, "cen");
	    if (chpCen != NULL) 
	    	{
	    	band2HasCen = TRUE;
    		sprintf(query,
            		"select chromStart, chromEnd from %s where chrom = '%s' and type = 'centromere'", gapTableName, chrom);
	    	}
	    else
	    	{
    		sprintf(query,
            		"select chromStart, chromEnd from cytoBand where chrom = '%s' and name = '%s'", chrom, band2);
    		}
	    }

	sr = sqlMustGetResult(conn, query);
    	row = sqlNextRow(sr);
    	if (row != NULL) 
    	    {
	    startPosBand2 = atoi(row[0]);
	    endPosBand2   = atoi(row[1]);
	    band2Success  = TRUE;
	    }  
	else
	    {
    	    sqlFreeResult(&sr);
            sprintf(query, 
	    	    "select min(chromStart), max(chromEnd) from cytoBand where chrom = '%s' and name like '%s%c'", chrom, band2, '%');

	    sr = sqlMustGetResult(conn, query);
    	    row = sqlNextRow(sr);
    	    if ((row != NULL) && (row[0] != NULL) && (row[1] != NULL))
    	    	{
	    	startPosBand2 = atoi(row[0]);
	    	endPosBand2   = atoi(row[1]);
	    	endPos   = atoi(row[1]);
	    	band2Success = TRUE;
	    	}
	    else
	    	{
	    	band2Success  = FALSE;
		fprintf(stderr, "band2 processing failed and skipped: band2=%s, locSav=%s, omimId=%s\n", band2, locSav, omimId);
	    	}
	    }
	
	if (band2Success) 
	    {
	    startPos = min(startPosBand1, startPosBand2);
	    endPos   = max(endPosBand1,   endPosBand2);

	    fprintf(outf, "%s\t%d\t%d\t%s\n", chrom, startPos, endPos, omimId);
	    }

	}
    else
    	{
	if (band1Success) 
	    {
	    fprintf(outf, "%s\t%d\t%d\t%s\n", chrom, startPosBand1, endPosBand1, omimId);
	    }
	}
    sqlFreeResult(&sr);
skip1:    
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

fclose(outf);
hFreeConn(&conn2);
return(0);
}

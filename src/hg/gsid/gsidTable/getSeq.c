/* getSeq - pages to get protein and nucleic acid sequence. */

#include "common.h"
#include "jksql.h"
#include "cart.h"
#include "dnautil.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "gsidTable.h"

static char const rcsid[] = "$Id: getSeq.c,v 1.7 2008/08/24 19:14:53 fanhsu Exp $";


static void getSeqFromBlob(struct sqlConnection *conn,
        struct subjInfo *siList, char *tableName, char *xrefField)
/* Get sequence from blob field in table and print it as fasta. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct subjInfo *si;
int seqCnt = 0;
hPrintf("<TT><PRE>");
for (si = siList; si != NULL; si = si->next)
    {
    char *subjId = si->fields[0];
    /* currently just 3 Thailand or 4 US */
    safef(query, sizeof(query),
        "select id, seq from %s s, gsIdXref g where g.subjId='%s' and g.%s=s.id", 
	tableName, subjId, xrefField);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char *id = row[0];
        char *seq = row[1];
        hPrintf(">%s", id);
        hPrintf(":%s\n", subjId);
        writeSeqWithBreaks(stdout, seq, strlen(seq), 60);
        hPrintf("\n");
	seqCnt++;
        }
    sqlFreeResult(&sr);
    }
if (seqCnt == 0) hPrintf("No sequence data available for subject(s) selected.");
hPrintf("</TT></PRE>");
}


static void getProtein( struct sqlConnection *conn, struct subjInfo *siList)
/* Print out proteins. */
{
getSeqFromBlob(conn, siList, "aaSeq", "aaSeqId");
}

static void getGenomic( struct sqlConnection *conn, struct subjInfo *siList)
/* Print out dna. */
{
getSeqFromBlob(conn, siList, "dnaSeq", "dnaSeqId");
}



void doGetSeq(struct sqlConnection *conn, 
        struct subjInfo *siList, char *how)
/* Put up the get sequence page. */
{
if (sameString(how, "protein"))
    getProtein(conn, siList);
else if (sameString(how, "genomic"))
    getGenomic(conn, siList);
else
    errAbort("Unrecognized %s value %s", getSeqHowVarName, how);
}



static void howRadioButton(char *how)
/* Put up a getSeqHow radio button. */
{
char *howName = getSeqHowVarName;
char *oldVal = cartUsualString(cart, howName, "protein");
cgiMakeRadioButton(howName, how, sameString(how, oldVal));
}



void doGetSeqPage(struct sqlConnection *conn, struct column *colList)
/* Put up the get sequence page asking how to get sequence. */
{
hPrintf("<H2>Get Sequence</H2>");
hPrintf("<FORM ACTION=\"../cgi-bin/gsidTable\" METHOD=GET>\n");
cartSaveSession(cart);
hPrintf("Select sequence type:<BR>\n");
howRadioButton("protein");
hPrintf("Protein<BR>\n");
howRadioButton("genomic");
hPrintf("Genomic<BR>\n");
cgiMakeButton(getSeqVarName, "get sequence");
hPrintf("</FORM>\n");
}


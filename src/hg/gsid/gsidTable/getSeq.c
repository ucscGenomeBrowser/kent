/* getSeq - pages to get protein and nucleic acid sequence. */

#include "common.h"
//#include "linefile.h"
//#include "hash.h"
//#include "obscure.h"
#include "jksql.h"
#include "cart.h"
#include "dnautil.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "hPrint.h"
//#include "hgSeq.h"
#include "gsidTable.h"
//#include "genePred.h"
//#include "bed.h"

static char const rcsid[] = "$Id: getSeq.c,v 1.1 2006/11/17 23:13:22 galt Exp $";


static void getSeqFromBlob(struct sqlConnection *conn,
        struct subjInfo *siList, char *tableName, char *template)
/* Get sequence from blob field in table and print it as fasta. */
{
struct sqlResult *sr;
char **row;
char query[256];
char pattern[256];
struct subjInfo *si;
struct sqlConnection *conn2 = hAllocConn();

hPrintf("<TT><PRE>");
for (si = siList; si != NULL; si = si->next)
    {
    char *s = skipToNumeric(si->fields[0]);
    ++s;  /* skip over leading '3' */
    
    safef(pattern,sizeof(pattern),template,s);
    safef(query, sizeof(query),
        "select id, seq from %s where id like '%s'", tableName, pattern);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char *id = row[0];
        char *seq = row[1];
        hPrintf(">%s\n", id);
        writeSeqWithBreaks(stdout, seq, strlen(seq), 60);
        hPrintf("\n");
        }
    sqlFreeResult(&sr);
    }
hPrintf("</TT></PRE>");
hFreeConn(&conn2);
}


static void getProtein( struct sqlConnection *conn, struct subjInfo *siList)
/* Print out proteins. */
{
getSeqFromBlob(conn, siList, "aaSeq", "p1._-%s%%");
}

static void getGenomic( struct sqlConnection *conn, struct subjInfo *siList)
/* Print out dna. */
{
getSeqFromBlob(conn, siList, "dnaSeq", "ss._-%s%%");
}



void doGetSeq(struct sqlConnection *conn, 
        struct subjInfo *siList, char *how)
/* Put up the get sequence page. */
{
if (sameString(how, "protein"))
    getProtein(conn, siList);
//else if (sameString(how, "mRNA"))
//    getMrna(conn, colList, siList);
//else if (sameString(how, "promoter"))
//    getPromoter(conn, colList, siList);
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
//howRadioButton("mRNA");
//hPrintf("mRNA<BR>\n");
//howRadioButton("promoter");
//hPrintf("Promoter including ");
//cgiMakeIntVar(proUpSizeVarName,
//        cartUsualInt(cart, proUpSizeVarName, 1000), 4);
//hPrintf(" bases upstream and ");
//cgiMakeIntVar(proDownSizeVarName,
//        cartUsualInt(cart, proDownSizeVarName, 50), 3);
//hPrintf(" downstream.<BR>\n");
//hPrintf("&nbsp;&nbsp;&nbsp;");
//cgiMakeCheckBox(proIncludeFiveOnly,
//    cartUsualBoolean(cart, proIncludeFiveOnly, TRUE));
//hPrintf("Include only those with annotated 5' UTRs<BR>");
howRadioButton("genomic");
hPrintf("Genomic<BR>\n");
cgiMakeButton(getSeqVarName, "get sequence");
hPrintf("</FORM>\n");
}


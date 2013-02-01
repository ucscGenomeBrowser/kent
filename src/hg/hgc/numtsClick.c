/* numtsClick - hgc code to display Numts track item detail page */
#include "common.h"
#include "hgc.h"
#include "numtsClick.h"

void doNumtS(struct trackDb *tdb, char *itemName)
/* Put up page for NumtS. */
{
char *table = tdb->table;
struct sqlConnection *conn = hAllocConn(database);
struct bed *bed;
char query[512];
struct sqlResult *sr;
char **row;
boolean firstTime = TRUE;
int start = cartInt(cart, "o");
int num = 6;
char itemNameDash[64]; /* itenName appended with a "_" */
char itemNameTrimmed[64]; /* itemName trimed at last "_" */
int sDiff = 30; /* acceptable difference of genomics size */
/* message strings */
char *clickMsg = NULL;
char *openMsg1 = "Click 'browser' link below to open Genome Browser at genomic position where";
char *openMsg2 = "maps\n";
char *openMsgM = "Click 'browser' link below to open Genome Browser at mitochondrial position where";


genericHeader(tdb, itemName);
genericBedClick(conn, tdb, itemName, start, num);

safecpy(itemNameDash, sizeof(itemNameDash),itemName);
safecat(itemNameDash,64,"_");
safecpy(itemNameTrimmed, sizeof(itemNameTrimmed),itemName);
chopSuffixAt(itemNameTrimmed, '_');

safef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, score, strand from %s where name='%s'",
      table, itemName);
sr = sqlGetResult(conn, query);
int sSize=0;
while ((row = sqlNextRow(sr)) != NULL)
    {
        bed = bedLoad6(row);
        sSize = bed->chromEnd - bed->chromStart;
        /* printf("sSize is: %5d <BR>", sSize); */
    }


if (sameString("hg18", database))
{
  if (sameString("numtS", table) || sameString("numtSAssembled", table))
      {
      safef(query, sizeof(query),
          "select  chrom, chromStart, chromEnd, name, score, strand "
          "from numtSMitochondrionChrPlacement where ( "
          "(name = '%s') OR (((name REGEXP '^%s') OR (name='%s')) AND "
          " (ABS((chromEnd - chromStart)-%d) <= %d ))) ",
      itemName, itemNameDash, itemNameTrimmed, sSize, sDiff);
      clickMsg = openMsgM;
      }
    else if (sameString("numtSMitochondrion", table))
      {
      safef(query, sizeof(query),
          "select  chrom, chromStart, chromEnd, name, score, strand "
          "from numtS where ( "
          "(name = '%s') OR (((name REGEXP '^%s') OR (name='%s')) AND "
          " (ABS((chromEnd - chromStart)-%d) <= %d ))) ",
      itemName, itemNameDash, itemNameTrimmed, sSize, sDiff);
      clickMsg = openMsg1;
        }
    else if (sameString("numtSMitochondrionChrPlacement", table))
      {
      safef(query, sizeof(query),
          "select  chrom, chromStart, chromEnd, name, score, strand "
          "from numtS where ( "
          "(name = '%s') OR (((name REGEXP '^%s') OR (name='%s')) AND "
          " (ABS((chromEnd - chromStart)-%d) <= %d ))) ",
      itemName, itemNameDash, itemNameTrimmed, sSize, sDiff);
      clickMsg = openMsg1;
      }
} else {
    if (sameString("numtS", table) || sameString("numtSAssembled", table))
    {
      if (sameString("hg19", database) || sameString("mm9", database))
      {  
          safef(query, sizeof(query),
              "select  chrom, chromStart, chromEnd, name, score, strand "
              "from numtSMitochondrion where name = '%s'  ", itemName);
      } else {
          safef(query, sizeof(query),
              "select  chrom, chromStart, chromEnd, name, score, strand "
            "from numtSMitochondrion where name = '%s'  ", itemNameTrimmed);
      }  
        clickMsg = openMsgM;
     }
  else if (sameString("numtSMitochondrion", table))
     {
      safef(query, sizeof(query),
          "select  chrom, chromStart, chromEnd, name, score, strand "
          "from numtS where name like '%s%%'", itemName);
      clickMsg = openMsg1;
     }
}

    sr = sqlGetResult(conn, query);
    firstTime = TRUE;

    while ((row = sqlNextRow(sr)) != NULL)
        {
        printf("<PRE><TT>");
        if (firstTime)
            {
            firstTime = FALSE;
        printf("<BR><H3>%s item '%s' %s</H3><BR>", clickMsg, itemName, openMsg2);

            printf("BROWSER | NAME                CHROMOSOME      START        END     SIZE    SCORE  STRAND \n");
            printf("--------|--------------------------------------------------------------------------------------------\n");

            }
        bed = bedLoad6(row);
        printf("<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">browser</A> | ",
               hgTracksPathAndSettings(), database,
               bed->chrom, bed->chromStart+1, bed->chromEnd);

        printf("%-20s %-10s %9d  %9d    %5d    %5d    %1s",
            bed->name, bed->chrom, bed->chromStart+1, bed->chromEnd,
            (bed->chromEnd - bed->chromStart),bed->score, bed->strand);

        printf("</TT></PRE>");
        }

 printf("<BR>");
 printTrackHtml(tdb);
 hFreeConn(&conn);
}


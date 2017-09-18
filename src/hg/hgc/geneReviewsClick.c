/* geneReviewsClick - hgc code to display geneReviews track item detail page */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hgc.h"
#include "geneReviewsClick.h"

void doGeneReviews(struct trackDb *tdb, char *itemName)
/* generate the detail page for geneReviews */
{
struct sqlConnection *conn = hAllocConn(database);
//char *table = tdb->table;
int start = cartInt(cart, "o");
int num = 4;

 genericHeader(tdb, itemName);
 genericBedClick(conn, tdb, itemName, start, num);
 prGeneReviews(conn, itemName);
 printf("<BR>");
 printTrackHtml(tdb);
 hFreeConn(&conn);
}

void prGeneReviews(struct sqlConnection *conn, char *itemName)
/* print GeneReviews associated to this item
   Note: this print function has been replaced by addGeneReviewToBed.pl
         which print the same information to the field 5 of bigBed file
*/
{
struct sqlResult *sr;
char **row;
char query[512];
int i;
char *clickMsg = "Click GR short name link to find the GeneReviews article on NCBI Bookshelf.";
char *spacer = "   ";
boolean firstTime = TRUE;

if (!sqlTableExists(conn, "geneReviewsDetail")) return;


sqlSafef(query, sizeof(query), "select  grShort, NBKid, grTitle from geneReviewsDetail where geneSymbol='%s'", itemName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
        char *grShort = *row++;
        char *NBKid  = *row++;
        char *grTitle = *row++;


        if (firstTime)
        {
          printf("<BR><B> GeneReviews available for %s:</B> (%s)<BR>",itemName,clickMsg);
          firstTime = FALSE;
          printf("<PRE><TT>");
              // #123456789-123456789-123456789-123456789-123456789-123456789-
          printf("GR short name          Disease name<BR>");

          printf("---------------------------------------------------------");
          printf("--------------------<BR>");
        }
        printf("<A HREF=\"https://www.ncbi.nlm.nih.gov/books/%s\" TARGET=_blank><B>%s</B></A>", NBKid, grShort);
        if (strlen(grShort) <= 20) {
          for (i = 0; i <  20-strlen(grShort); i ++ )
             {
                printf("%s", " " );
             }
           }
       printf("%s%s<BR>", spacer, grTitle);

//        printf("<A HREF=\"https://www.ncbi.nlm.nih.gov/books/%s\" TARGET=_blank><B>%s</B></A>%s%s<BR>", NBKid, NBKid, spacer, grTitle);
    }  /* end while */
 printf("</TT></PRE>");
 sqlFreeResult(&sr);
} /* end of prGeneReviews */

void prGRShortRefGene(char *itemName)
/* print GeneReviews short label associated to this refGene item */
{
struct sqlConnection *conn  = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[512];
boolean firstTime = TRUE;

if (!sqlTableExists(conn, "geneReviewsDetail")) return;

sqlSafef(query, sizeof(query), "select grShort, NBKid, grTitle from geneReviewsDetail where geneSymbol='%s'", itemName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
        char *grShort = *row++;
	char *NBKid = *row++;
        char *grTitle = *row++;
        if (firstTime)
        {
          printf("<B>Related GeneReviews disease(s): </B>");
          firstTime = FALSE;
       printf("<A HREF=\"https://www.ncbi.nlm.nih.gov/books/%s\" TARGET=_blank><B>%s</B></A>", NBKid, grShort);
        printf(" (");
       printf("%s", grTitle);
       printf(")");
        } else {
          printf(", ");
       printf("<A HREF=\"https://www.ncbi.nlm.nih.gov/books/n/gene/%s\" TARGET=_blank><B>%s</B></A>", grShort, grShort);
       printf(" (");
       printf("%s", grTitle);
       printf(")");
        }
     }
     printf("<BR>");
     sqlFreeResult(&sr);
} /* end of prGRShortRefGene */



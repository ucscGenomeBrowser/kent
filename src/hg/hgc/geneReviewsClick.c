/* geneReviewsClick - hgc code to display geneReviews track item detail page */
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
char *clickMsg = "Click Short name link to find the GeneReviews article, Click the Disease name link to find all GeneReviews articles which contain the disease name.";
boolean firstTime = TRUE;

if (!sqlTableExists(conn, "geneReviewsRefGene")) return;

sqlSafef(query, sizeof(query), "select  grShort, diseaseID, diseaseName from geneReviewsRefGene where geneSymbol='%s'", itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
        char *grShort = *row++;
        char *diseaseID = *row++;
        char *diseaseName = *row++;


        if (firstTime)
        {
          printf("<BR><B> GeneReview(s) available for %s:</B> (%s)<BR>",itemName,clickMsg);
          firstTime = FALSE;
          printf("<PRE><TT>");
              // #123456789-123456789-123456789-123456789-123456789-123456789-
          printf("Short name         Disease ID     Disease name<BR>");

          printf("------------------------------------------------------------");
          printf("--------------------<BR>");
        }
        printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/books/n/gene/%s\" TARGET=_blank><B>%s</B></A>", grShort, grShort);
        if (strlen(grShort) <= 20) {
          for (i = 0; i <  20-strlen(grShort); i ++ )
             {
                printf("%s", " " );
             }
           }
         printf("%-10s    ", diseaseID);
        printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/books/?term=%s+AND+gene{[book]\" TARGET=_blank><B>%s</B></A><BR>", diseaseName, diseaseName);
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

if (!sqlTableExists(conn, "geneReviewsRefGene")) return;

sqlSafef(query, sizeof(query), "select grShort, diseaseName from geneReviewsRefGene where geneSymbol='%s'", itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
        char *grShort = *row++;
        char *diseaseName = *row++;
        if (firstTime)
        {
          printf("<B>Related GeneReview(s) and GeneTests disease(s): </B>");
          firstTime = FALSE;
       printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/books/n/gene/%s\" TARGET=_blank><B>%s</B></A>", grShort, grShort);
        printf(" (");
       printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/sites/GeneTests/review/disease/%s?db=genetests&search_param=contains\" TARGET=_blank>%s</A>", diseaseName, diseaseName);
       printf(")");
        } else {
          printf(", ");
       printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/books/n/gene/%s\" TARGET=_blank><B>%s</B></A>", grShort, grShort);
       printf(" (");
       printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/sites/GeneTests/review/disease/%s?db=genetests&search_param=contains\" TARGET=_blank>%s</A>", diseaseName, diseaseName);
       printf(")");
        }
     }
     printf("<BR>");
     sqlFreeResult(&sr);
} /* end of prGRShortRefGene */



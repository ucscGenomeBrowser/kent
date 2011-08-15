/* GeneReviews - print out GeneReviews for this gene. */

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "linefile.h"
#include "dystring.h"
#include "hgGene.h"

static void geneReviewsPrint(struct section *section, 
	struct sqlConnection *conn, char *itemName)
/* print GeneReviews short label associated to this refGene item */
{
if (sqlTablesExist(conn, "geneReviews"))
    {
    hPrintf("<B>Printing geneReviews for %s </B><BR>", itemName );
    } else {
    hPrintf("<B>geneReviews table not found </B><BR>" );
    }
/*******************************************************************
struct sqlResult *sr;
char **row;
char query[512];
boolean firstTime = TRUE;

safef(query, sizeof(query), "select  grShort from geneReviewsRefGene where geneSymbol='%s'", itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
        char *grShort = *row++;
        if (firstTime)
        {
          printf("<B> GeneReview: </B>");
          firstTime = FALSE;
          printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/books/n/gene/%s\" TARGET=_blank>%s</A>", grShort, grShort);
        } else {
          printf(", ");
          printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/books/n/gene/%s\" TARGET=_blank>%s</A>", grShort, grShort);
        }
     }
     printf("<BR>");
**********************************************************/
}


struct section *geneReviewsSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create geneReviews (aka Other Names) section. */
{
struct section *section = sectionNew(sectionRa, "geneReviews");
section->print = geneReviewsPrint;
return section;
}

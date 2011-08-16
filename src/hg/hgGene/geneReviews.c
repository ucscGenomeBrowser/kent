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
char query[256];
char * geneSymbol;
if (sqlTablesExist(conn, "geneReviews"))
    {
    safef(query, sizeof(query), "select geneSymbol from kgXref where kgId = '%s'", itemName);
    geneSymbol = sqlQuickString(conn, query);
    if (geneSymbol != NULL)
        {
           prGRShortKg(conn,geneSymbol);
        } else {
           hPrintf("<B>No GeneReviews for this gene </B><BR>" );
    }
  }
}

void prGRShortKg(struct sqlConnection *conn, char *itemName)
/* print GeneReviews short label associated to this refGene item */
{

struct sqlResult *sr;
char **row;
char query[512];
boolean firstTime = TRUE;

safef(query, sizeof(query), "select  geneSymbol, grShort, diseaseID, diseaseName from geneReviewsRefGene where geneSymbol='%s'", itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
      if (firstTime)
        {
          printf("<B> Gene Symbol: </B>%s<BR>", row[0]);    
          firstTime = FALSE;
        }
       printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/books/n/gene/%s\" TARGET=_blank><B>%s:</B></A>",row[1], row[1]);
       printf(" %s (Id: %s)<BR>", row[3], row[2]);
     }
}

static boolean geneReviewsExists(struct section *section,
        struct sqlConnection *conn, char *geneId)
/* Return TRUE if geneReviews table exist and have GeneReviews articles
 * on this one. */
{
char query[256];
char * geneSymbol;
char * grSymbol;

if (sqlTablesExist(conn, "geneReviews"))
    {
       safef(query, sizeof(query), "select geneSymbol from kgXref where kgId = '%s'", geneId);
       geneSymbol = sqlQuickString(conn, query);
       if (geneSymbol != NULL)
          {
             safef(query, sizeof(query), "select  geneSymbol from geneReviewsRefGene where geneSymbol='%s'", geneSymbol);
             grSymbol = sqlQuickString(conn, query);
             if (grSymbol != NULL)
                {
                  return TRUE;
                }
           }
     }
return FALSE;
}

struct section *geneReviewsSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create geneReviews (aka Other Names) section. */
{
struct section *section = sectionNew(sectionRa, "geneReviews");
section->exists = geneReviewsExists;
section->print = geneReviewsPrint;
return section;
}

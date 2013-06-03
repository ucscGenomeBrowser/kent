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
if (sqlTableExists(conn, "geneReviewsRefGene"))
    {
    sqlSafef(query, sizeof(query), "select geneSymbol from kgXref where kgId = '%s'", itemName);
    geneSymbol = sqlQuickString(conn, query);
    if (geneSymbol != NULL)
        {
           prGRShortKg(conn,geneSymbol);
        } else {
           hPrintf("<B>No GeneReview for this gene </B><BR>" );
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

sqlSafef(query, sizeof(query), "select  geneSymbol, grShort, diseaseID, diseaseName from geneReviewsRefGene where geneSymbol='%s'", itemName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
      if (firstTime)
        {
          printf("<B>GeneReview(s) and GeneTest disease(s) related to gene  </B>%s:<BR>", row[0]);    
          firstTime = FALSE;
        }
       printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/books/n/gene/%s\" TARGET=_blank><B>%s</B></A>",row[1], row[1]);
       printf(" ("); 
       printf("<A HREF=\"http://www.ncbi.nlm.nih.gov/sites/GeneTests/review/disease/%s?db=genetests&search_param==begins_with\" TARGET=_blank>%s</A>", row[3], row[3]);
       printf(")<BR>");
     }
     sqlFreeResult(&sr);
}

static boolean geneReviewsExists(struct section *section,
        struct sqlConnection *conn, char *geneId)
/* Return TRUE if geneReviewsRefGene table exist and have GeneReviews articles
 * on this one. */
{
char query[256];
char * geneSymbol;
char * grSymbol;

if (sqlTableExists(conn, "geneReviewsRefGene"))
    {
       sqlSafef(query, sizeof(query), "select geneSymbol from kgXref where kgId = '%s'", geneId);
       geneSymbol = sqlQuickString(conn, query);
       if (geneSymbol != NULL)
          {
             sqlSafef(query, sizeof(query), "select  geneSymbol from geneReviewsRefGene where geneSymbol='%s'", geneSymbol);
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
/* Create geneReviews section. */
{
struct section *section = sectionNew(sectionRa, "geneReviews");
section->exists = geneReviewsExists;
section->print = geneReviewsPrint;
return section;
}

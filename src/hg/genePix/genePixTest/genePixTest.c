/* genePixTest - Test data in genePix database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "genePix.h"

/* Variables you can override from command line. */
boolean replace = FALSE;
boolean multicolor = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePixTest - Test data in genePix database\n"
  "usage:\n"
  "   genePixLoad database geneId\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void printNameList(char *label, struct slName *list)
/* Print a list of matchers. */
{
struct slName *el;
printf("%s ", label);
for (el = list; el != NULL; el = el->next)
    printf(" %s", el->name);
printf("\n");
}

void printNumberList(char *label, struct slInt *list)
/* Print a list of matchers. */
{
struct slInt *el;
printf("%s ", label);
for (el = list; el != NULL; el = el->next)
    printf(" %d", el->val);
printf("\n");
}

void genePixTest(char *database, int geneId)
/* Test out some genePix things. */
{
struct sqlConnection *conn = sqlConnect(database);
struct slName *conList = NULL, *con;
con = slNameNew("Kent");
slAddHead(&conList, con);
con = slNameNew("Gray");
slAddHead(&conList, con);


printf("geneId %d\n", geneId);
printf("genePixFullSizePath %s\n", genePixFullSizePath(conn, geneId));
printf("genePixScreenSizePath %s\n", genePixScreenSizePath(conn, geneId));
printf("genePixThumbSizePath %s\n", genePixThumbSizePath(conn, geneId));
printf("genePixOrganism %s\n", genePixOrganism(conn, geneId));
printf("genePixStage (short) %s\n", genePixStage(conn, geneId, FALSE));
printf("genePixStage (long) %s\n", genePixStage(conn, geneId, TRUE));
printNameList("genePixGeneName", genePixGeneName(conn, geneId));
printNameList("genePixRefSeq", genePixRefSeq(conn, geneId));
printNameList("genePixGenbank", genePixGenbank(conn, geneId));
printNameList("genePixUniProt", genePixUniProt(conn, geneId));
printf("genePixSubmitId %s\n", genePixSubmitId(conn, geneId));
printf("genePixBodyPart %s\n", genePixBodyPart(conn, geneId));
printf("genePixSliceType %s\n", genePixSliceType(conn, geneId));
printf("genePixTreatment %s\n", genePixTreatment(conn, geneId));
printf("genePixContributors %s\n", genePixContributors(conn, geneId));
printf("genePixPublication %s\n", genePixPublication(conn, geneId));
printf("genePixPubUrl %s\n", genePixPubUrl(conn, geneId));
printf("genePixSetUrl %s\n", genePixSetUrl(conn, geneId));
printf("genePixItemUrl %s\n", genePixItemUrl(conn, geneId));
printNumberList("genePixSelectNamed Hmx1 gpsExact:", 
	genePixSelectNamed(conn, "Hmx1", gpsExact));
printNumberList("genePixSelectNamed freen gpsPrefix:", 
	genePixSelectNamed(conn, "freen", gpsPrefix));
printNumberList("genePixSelectNamed px gpsText:", 
	genePixSelectNamed(conn, "px", gpsText));
printNumberList("genePixOnSameGene:",
	genePixOnSameGene(conn, geneId));
printNumberList("selectMulti taxon=9606: ",
	genePixSelectMulti(conn, NULL, 9606, NULL, gpsNone, TRUE, 0, FALSE, genePixMaxAge));
printNumberList("selectMulti init=sameAs px: ",
	genePixSelectMulti(conn, 
		genePixSelectNamed(conn, "px", gpsText),
		0, NULL, gpsNone, TRUE, 0, FALSE, genePixMaxAge));
printNumberList("selectMulti contributors Kent W.J. exact",
	genePixSelectMulti(conn, NULL, 0,
		slNameNew("Kent W.J."), gpsExact, 
		TRUE, 0, FALSE, genePixMaxAge));
printNumberList("selectMulti contributors Kent prefix",
	genePixSelectMulti(conn, NULL, 0,
		slNameNew("Kent"), gpsPrefix, 
		TRUE, 0, FALSE, genePixMaxAge));
printNumberList("selectMulti contributors W.J. text",
	genePixSelectMulti(conn, NULL, 0,
		slNameNew("W.J."), gpsText, 
		TRUE, 0, FALSE, genePixMaxAge));
printNumberList("selectMulti e5 to n0:",
	genePixSelectMulti(conn, NULL, 0,
		NULL, gpsText, 
		TRUE, 5, FALSE, 0));
printNumberList("selectMulti e13.5 to n1:",
	genePixSelectMulti(conn, NULL, 0,
		NULL, gpsText, 
		TRUE, 13.5, FALSE, 1));
printNumberList("selectMulti n0 up:",
	genePixSelectMulti(conn, NULL, 0,
		NULL, gpsText, 
		FALSE, 0, FALSE, genePixMaxAge));
printNumberList("selectMulti below n0:",
	genePixSelectMulti(conn, NULL, 0,
		NULL, gpsText, 
		TRUE, 0, FALSE, 0));
printNumberList("selectMultiz Kent embryo taxon 9986:",
	genePixSelectMulti(conn, NULL, 9986,
		slNameNew("Kent"), gpsPrefix, 
		TRUE, 0, FALSE, 0));
printNumberList("selectMultiz Kent or Gray adult with x in name:",
	genePixSelectMulti(conn, 
		genePixSelectNamed(conn, "x", gpsText),
		0,
		conList, gpsPrefix, 
		FALSE, 0, FALSE, genePixMaxAge));
	

}

struct slInt *genePixSelectMulti(
        /* Note most parameters accept NULL or 0 to mean ignore parameter. */
	struct sqlConnection *conn, 
	struct slInt *init,	/* Restrict results to those with id in list. */
	int taxon,		/* Taxon of species to restrict it to. */
	struct slName *contributors,  /* List of contributers to restrict it to. */
	enum genePixSearchType how,   /* How to match contributors */
	boolean startIsEmbryo, double startAge,
	boolean endIsEmbryo, double endAge);

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
genePixTest(argv[1], atoi(argv[2]));
return 0;
}

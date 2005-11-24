/* probePage - put up page of info on probe. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "cart.h"
#include "htmshell.h"
#include "dnautil.h"
#include "web.h"
#include "visiGene.h"
#include "hgVisiGene.h"
#include "probePage.h"

static char *orgName(struct sqlConnection *conn, int taxon)
/* Return organism name associated with taxon. */
{
char *binomial;
char query[256];
safef(query, sizeof(query), 
	"select binomial from uniProt.taxon where id=%d", taxon);
binomial = sqlQuickString(conn, query);
return shortOrgName(binomial);
}

static void labeledText(char *label, char *text)
/* Print out labeled text */
{
printf("<B>%s</B>: %s ", label, naForEmpty(text));
}

static void labeledResult(char *label, struct sqlConnection *conn, char *query)
/* Put up labeled query result. */
{
labeledText(label, sqlQuickString(conn, query));
}

static void rnaProbeInfo(struct sqlConnection *conn, int probeId)
/* Print out info on RNA/DNA based probe */
{
char query[256];
char *seq;
printf("<BR>\n");
safef(query, sizeof(query), 
    "select fPrimer from probe where id=%d", probeId);
labeledResult("forward primer", conn, query);
printf("<BR>\n");
safef(query, sizeof(query), 
    "select rPrimer from probe where id=%d", probeId);
labeledResult("reverse primer", conn, query);
printf("<BR>\n");
safef(query, sizeof(query),
    "select seq from probe where id=%d", probeId);
seq = naForEmpty(sqlQuickString(conn, query));
printf("<BR><B>sequence:</B>\n");
if (sameWord(seq, "n/a"))
   printf("%s<BR>", seq);
else
    {
    printf("<TT><PRE>");
    writeSeqWithBreaks(stdout, seq, strlen(seq), 60);
    printf("</PRE></TT>");
    }
}

static void antibodyProbeInfo(struct sqlConnection *conn, int probeId)
/* Print out info on antibody based probe */
{
char query[256];
int abId;
int taxon;
char *species = "n/a";

/* Lookup antibody id and taxon. */
safef(query, sizeof(query), "select antibody from probe where id=%d", 
	probeId);
abId = sqlQuickNum(conn, query);
safef(query, sizeof(query), "select taxon from antibody where id=%d", abId);
taxon = sqlQuickNum(conn, query);

safef(query, sizeof(query), "select name from antibody where id=%d", abId);
labeledResult("name", conn, query);
if (taxon != 0)
    species = orgName(conn, taxon);
labeledText("species", species);
printf("<BR>\n");
safef(query, sizeof(query), "select description from antibody where id=%d", 
	abId);
labeledResult("description", conn, query);
printf("<BR>\n");
}

void probePage(struct sqlConnection *conn, int probeId)
/* Put up a page of info on probe. */
{
char query[256];
int geneId, taxon;
char *probeType;

webStartWrapper(cart, "Probe Information", NULL, FALSE, FALSE);

/* Get gene ID. */
safef(query, sizeof(query), "select gene from probe where id=%d", probeId);
geneId = sqlQuickNum(conn, query);

/* Print gene symbol. */
safef(query, sizeof(query), "select name from gene where id=%d", geneId);
labeledResult("gene symbol", conn, query);

/* Print organism. */
safef(query, sizeof(query), "select taxon from gene where id=%d", geneId);
taxon = sqlQuickNum(conn, query);
labeledText("organism", orgName(conn, taxon));

/* Print synonyms if any. */
safef(query, sizeof(query), 
	"select name from geneSynonym where gene=%d", geneId);
printf("<B>synonyms: </B> %s<BR>\n", 
	makeCommaSpacedList(sqlQuickList(conn, query)));

/* Print refSeq, genbank, uniProt */
safef(query, sizeof(query), "select refSeq from gene where id=%d", geneId);
labeledResult("RefSeq", conn, query);
safef(query, sizeof(query), "select genbank from gene where id=%d", geneId);
labeledResult("GenBank", conn, query);
safef(query, sizeof(query), "select uniProt from gene where id=%d", geneId);
labeledResult("UniProt", conn, query);

/* Now print probe information. */
safef(query, sizeof(query),
	"select probeType.name from probe,probeType "
	"where probe.id = %d and probe.probeType = probeType.id"
	, probeId);
probeType = sqlQuickString(conn, query);
labeledText("probe type", probeType);
if (sameWord(probeType, "antibody"))
    antibodyProbeInfo(conn, probeId);
else
    rnaProbeInfo(conn, probeId);

webEnd();
}




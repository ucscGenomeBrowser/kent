/* pathways - do pathways section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "hdb.h"
#include "spDb.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: pathways.c,v 1.19 2008/09/03 19:18:50 markd Exp $";

struct pathwayLink
/* Info to link into a pathway. */
    {
    char *name;		/* Symbolic name */
    char *shortLabel;	/* Short label. */
    char *longLabel;	/* Long label. */
    char *tables;	/* Tables that must exist. */

    int (*count)(struct pathwayLink *pl, 
    	struct sqlConnection *conn, char *geneId);
    /* Count number of items referring to this gene. */

    void (*printLinks)(struct pathwayLink *pl, 
    	struct sqlConnection *conn, char *geneId);
    /* Print out links. */
    };

static void keggLink(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Print out kegg database link. */
{
char query[512], **row;
struct sqlResult *sr;

if (isRgdGene(conn))
{
safef(query, sizeof(query), 
	"select k.locusID, k.mapID, keggMapDesc.description"
	" from rgdGene2KeggPathway k, keggMapDesc, rgdGene2 x"
	" where k.rgdId=x.name "
	" and x.name='%s'"
	" and k.mapID = keggMapDesc.mapID"
	, geneId);
}
else
{
safef(query, sizeof(query), 
	"select k.locusID, k.mapID, keggMapDesc.description"
	" from keggPathway k, keggMapDesc, kgXref x"
	" where k.kgID=x.kgId "
	" and x.kgID='%s'"
	" and k.mapID = keggMapDesc.mapID"
	, geneId);
}

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<A HREF=\"http://www.genome.ad.jp/dbget-bin/show_pathway?%s+%s\" TARGET=_blank>",
    	row[1], row[0]);
    hPrintf("%s</A> - %s<BR>", row[1], row[2]);
    }
sqlFreeResult(&sr);
}

static int keggCount(struct pathwayLink *pl, struct sqlConnection *conn,
	char *geneId)
/* Count up number of hits. */
{
char query[256];
if (!isRgdGene(conn))
    {
    safef(query, sizeof(query), 
	"select count(*) from keggPathway k, kgXref x where k.kgID=x.kgId and x.kgId='%s'", geneId);
    }
else
    {
    safef(query, sizeof(query), 
	"select count(*) from rgdGene2KeggPathway k, rgdGene2 x where k.rgdId=x.name and x.name='%s'", geneId);
    }
return sqlQuickNum(conn, query);
}

static void bioCycLink(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Print out bioCyc database link. */
{
char query[512], **row;
struct sqlResult *sr;
char *oldMapId = cloneString("");

safef(query, sizeof(query),
	"select bioCycPathway.mapId,description"
	" from bioCycPathway,bioCycMapDesc"
	" where bioCycPathway.kgId='%s'"
	" and bioCycPathway.mapId = bioCycMapDesc.mapId order by bioCycPathway.mapId"
	, geneId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* only print new ones */
    if (!sameWord(oldMapId, row[0]))
    	{
	hPrintf("<A HREF=\"http://biocyc.org/%s/new-image?type=PATHWAY&object=%s&detail-level=2\" TARGET=_blank>",
    		genome, row[0]);
    	hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
	}
    oldMapId = cloneString(row[0]);	
    }
sqlFreeResult(&sr);
}

static int bioCycCount(struct pathwayLink *pl, struct sqlConnection *conn,
	char *geneId)
/* Count up number of hits. */
{
char query[256];
safef(query, sizeof(query), 
	"select count(*) from bioCycPathway where kgID='%s'", geneId);
return sqlQuickNum(conn, query);
}

static char *getCgapId(struct sqlConnection *conn)
/* Get cgap ID. */
{
char query[256];
safef(query, sizeof(query), 
	"select cgapId from cgapAlias where alias=\"%s\"", curGeneName);
return sqlQuickString(conn, query);
}

static void reactomeLink(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
{
char condStr[255];
char *spID, *chp;

struct sqlConnection *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;
char *eventDesc;
char *eventID;

/* check the existence of kgXref table first */
if (isRgdGene(conn))
    {
    if (!sqlTableExists(conn, "rgdGene2Xref")) return;
    }
else
    {
    if (!sqlTableExists(conn, "kgXref")) return;
    }
if (isRgdGene(conn))
    {
    safef(condStr, sizeof(condStr), "name='%s'", geneId);
    spID = sqlGetField(database, "rgdGene2ToUniProt", "value", condStr);
    }
else
    {
    safef(condStr, sizeof(condStr), "kgID='%s'", geneId);
    spID = sqlGetField(database, "kgXref", "spID", condStr);
    }

if (spID != NULL)
    {
    /* convert splice variant UniProt ID to its main root ID */
    chp = strstr(spID, "-");
    if (chp != NULL) *chp = '\0';
    
    hPrintf(
    "<BR>Protein %s (<A href=\"http://www.reactome.org/cgi-bin/link?SOURCE=UniProt&ID=%s\" TARGET=_blank>Reactome details)</A> participates in the following event(s):<BR><BR>" 
    , spID, spID);

    conn2= hAllocConn(database);
    safef(query2,sizeof(query2), 
    	  "select eventID, eventDesc from proteome.spReactomeEvent where spID='%s'", spID);
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    while (row2 != NULL)
    	{
    	eventID   = row2[0];
    	eventDesc = row2[1];
	hPrintf(
	"<A href=\"http://www.reactome.org/cgi-bin/eventbrowser?DB=gk_current&ID=%s\" TARGET=_blank>%s</A> %s<BR>\n",
	eventID, eventID, eventDesc);
    	row2 = sqlNextRow(sr2);
    	}
    sqlFreeResult(&sr2);
    hFreeConn(&conn2);
    }
}

static void rgdPathwayLink(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Print out bioCarta database link. */
{
char query[512], **row;
struct sqlResult *sr;
char *rgdId = geneId;
safef(query, sizeof(query),
    	"select x.pathwayId, description from rgdPathway p, rgdGenePathway x "
	" where p.pathwayId = x.pathwayId "
	" and x.geneId = '%s'"
	, rgdId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<A HREF=\"http://rgd.mcw.edu/tools/ontology/ont_annot.cgi?ontology=wo&ont_id=%s\" TARGET=_blank>", row[0]);
    hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
    }
sqlFreeResult(&sr);
}

static void bioCartaLink(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Print out bioCarta database link. */
{
char *cgapId = getCgapId(conn);
if (cgapId != NULL)
    {
    struct hash *uniqHash = newHash(8);
    char query[512], **row;
    struct sqlResult *sr;
    safef(query, sizeof(query),
    	"select cgapBiocDesc.mapID,cgapBiocDesc.description "
	" from cgapBiocPathway,cgapBiocDesc"
	" where cgapBiocPathway.cgapID='%s'"
	" and cgapBiocPathway.mapID = cgapBiocDesc.mapID"
	, cgapId);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *name = row[0];
	if (!hashLookup(uniqHash, name))
	    {
	    hashAdd(uniqHash, name, NULL);
	    hPrintf("<A HREF=\"http://cgap.nci.nih.gov/Pathways/BioCarta/%s\" TARGET=_blank>", row[0]);
	    hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
	    }
	}
    freez(&cgapId);
    hashFree(&uniqHash);
    }
}

static int bioCartaCount(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Count up number of hits. */
{
int ret = 0;
char *cgapId = getCgapId(conn);
if (cgapId != NULL)
    {
    char query[256];
    safef(query, sizeof(query), 
	    "select count(*) from cgapBiocPathway where cgapID='%s'", cgapId);
    ret = sqlQuickNum(conn, query);
    freez(&cgapId);
    }
return ret;
}

static int rgdPathwayCount(struct pathwayLink *pl, struct sqlConnection *conn,
char *geneId)
/* Count up number of hits. */
{
char query[256];
safef(query, sizeof(query),
      "select count(*) from rgdGenePathway where geneId ='%s'", geneId);
return sqlQuickNum(conn, query);
}

static int reactomeCount(struct pathwayLink *pl, struct sqlConnection *conn, 
	char *geneId)
/* Count up number of hits. */
{
int ret = 0;
char query[256];
char *spID, *chp;
char condStr[256];
char *origSpID;
/* check the existence of kgXref table first */
if (!isRgdGene(conn))
    {
    if (!sqlTableExists(conn, "kgXref")) return(0);
    }
else
    {
    if (!sqlTableExists(conn, "rgdGene2Xref")) return(0);
    }

if (isRgdGene(conn))
    {
    safef(condStr, sizeof(condStr), "name='%s'", geneId);
    spID = sqlGetField(database, "rgdGene2ToUniProt", "value", condStr);
    }
else
    {
    safef(condStr, sizeof(condStr), "kgID='%s'", geneId);
    spID = sqlGetField(database, "kgXref", "spID", condStr);
    }

if (spID != NULL)
    {
    origSpID = cloneString(spID);
    /* convert splice variant UniProt ID to its main root ID */
    chp = strstr(spID, "-");
    if (chp != NULL) *chp = '\0';

    if (!isRgdGene(conn))
        {
        safef(query, sizeof(query), 
	  "select count(*) from %s.spReactomeEvent, %s.spVariant, %s.kgXref where kgID='%s' and kgXref.spID=variant and variant = '%s' and spReactomeEvent.spID=parent", 
	  PROTEOME_DB_NAME, PROTEOME_DB_NAME, database, geneId, origSpID);
	}
    else
    	{
        safef(query, sizeof(query), 
	  "select count(*) from %s.spReactomeEvent, %s.spVariant, %s.rgdGene2ToUniProt where name='%s' and value=variant and variant = '%s' and spReactomeEvent.spID=parent", 
	  PROTEOME_DB_NAME, PROTEOME_DB_NAME, database, geneId, origSpID);
	}

    ret = sqlQuickNum(conn, query);
    }
return ret;
}

struct pathwayLink pathwayLinks[] =
{
   { "kegg", "KEGG", "KEGG - Kyoto Encyclopedia of Genes and Genomes", 
   	"keggPathway keggMapDesc", 
	keggCount, keggLink},
   { "bioCyc", "BioCyc", "BioCyc Knowledge Library",
        "bioCycPathway bioCycMapDesc", 
	bioCycCount, bioCycLink},
   { "bioCarta", "BioCarta", "BioCarta from NCI Cancer Genome Anatomy Project",
   	"cgapBiocPathway cgapBiocDesc cgapAlias",
	bioCartaCount, bioCartaLink},
   { "reactome", "Reactome", "Reactome (by CSHL, EBI, and GO)",
   	"proteome.spReactomeEvent",
	reactomeCount, reactomeLink},
   { "rgdPathway", "RGDPathway", "RGD Pathway",
   	"rgdPathway rgdGenePathway",
	rgdPathwayCount, rgdPathwayLink},
};

static boolean pathwayExists(struct pathwayLink *pl,
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if pathway exists and has data. */
{
if (!sqlTablesExist(conn, pl->tables))
    return FALSE;
return pl->count(pl, conn, geneId) > 0;
}

static boolean pathwaysExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if there's some pathway info on this one. */
{
int i;
for (i=0; i<ArraySize(pathwayLinks); ++i)
    {
    if (pathwayExists(&pathwayLinks[i], conn, geneId))
         return TRUE;
    }
return FALSE;
}

static void pathwaysPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out pathway links. */
{
int plIx;
struct pathwayLink *pl;
boolean isFirst = TRUE;

for (plIx=0; plIx < ArraySize(pathwayLinks); ++plIx)
    {
    pl = &pathwayLinks[plIx];
    if (pathwayExists(pl, conn, geneId))
        {
	if (isFirst)
	    isFirst = !isFirst;
	else
	    hPrintf("<BR>\n");
	hPrintf("<B>%s</B><BR>", pl->longLabel);
	pl->printLinks(pl, conn, geneId);
	}
    }
}

struct section *pathwaysSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create pathways section. */
{
struct section *section = sectionNew(sectionRa, "pathways");
section->exists = pathwaysExists;
section->print = pathwaysPrint;
return section;
}


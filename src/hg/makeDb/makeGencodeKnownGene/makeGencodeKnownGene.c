/* makeGencodeKnownGene - make knownGene from Gencode tables. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "genePred.h"
#include "encode/wgEncodeGencodeAttrs.h"
#include "encode/wgEncodeGencodeRefSeq.h"
#include "encode/wgEncodeGencodeTag.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeGencodeKnownGene - make knownGene from Gencode tables\n"
  "usage:\n"
  "   makeGencodeKnownGene database tempDatabase version# txToAcc.tab \n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct hashes
{
struct hash *genToUC;
struct hash *genToUniProt;
struct hash *genToAnnRemark;;
struct hash *genToAttrs;
struct hash *genToRefSeq;
struct hash *genToMrna;
struct hash *genToTags;
struct hash *refSeqToDescription;
//struct hash *mrnaToDescription;
struct hash *refSeqToPepName;
struct hash *hgncDescriptionFromGeneName;
//struct hash *displayIdFromUniProtId;
//struct hash *descriptionFromUniProtId;
};

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *getMap(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(6);
char *words[2];
int wordsRead;

while( (wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *)) ))
    hashAdd(hash, words[0], cloneString(words[1]));

lineFileClose(&lf);
return hash;
}

struct hash *getRefSeqTable(struct sqlConnection *conn,  char *version)
{
char versionQuery[4096];

safef(versionQuery, sizeof versionQuery, "select * from wgEncodeGencodeRefSeq%s",  version);

char **row;
struct sqlResult *sr;
struct hash *hash = newHash(6);

sr = sqlGetResult(conn, versionQuery);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct wgEncodeGencodeRefSeq *wga = wgEncodeGencodeRefSeqLoad(row);
    hashAdd(hash, wga->transcriptId, wga);
    }
sqlFreeResult(&sr);

return hash;
}

struct hash *getAttrsTable(struct sqlConnection *conn,  char *version)
{
char versionQuery[4096];

safef(versionQuery, sizeof versionQuery, "select * from wgEncodeGencodeAttrs%s",  version);

char **row;
struct sqlResult *sr;
struct hash *hash = newHash(6);

sr = sqlGetResult(conn, versionQuery);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct wgEncodeGencodeAttrs *wga = wgEncodeGencodeAttrsLoad(row);
    hashAdd(hash, wga->transcriptId, wga);
    }
sqlFreeResult(&sr);

return hash;
}

struct hash *getMapTable(struct sqlConnection *conn, char *query, char *version)
{
char versionQuery[4096];

if (version != NULL)
    safef(versionQuery, sizeof versionQuery, "%s%s", query, version);
else
    safecpy(versionQuery, sizeof versionQuery,  query);

char **row;
struct sqlResult *sr;
struct hash *hash = newHash(6);

sr = sqlGetResult(conn, versionQuery);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(hash, row[0], cloneString(row[1]));
    }
sqlFreeResult(&sr);

return hash;
}

struct genePred *loadGenePreds(struct sqlConnection *conn, char *query, char *version)
{
struct genePred *gpList = NULL;
char versionQuery[4096];
char **row;
struct sqlResult *sr;

safef(versionQuery, sizeof versionQuery, "%s%s", query, version);
sr = sqlGetResult(conn, versionQuery);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = genePredLoad(&row[1]);

    slAddHead(&gpList, gp);
    }

sqlFreeResult(&sr);

slReverse(&gpList);
return gpList;
}

void writeOutOneKnownGene(FILE *f, struct genePred *gp, struct hashes *hashes)
{

char buffer[1024];
safef(buffer, sizeof buffer, "%s.%s", gp->name, gp->chrom);
fprintf(f, "%s\t", (char *)hashMustFindVal(hashes->genToUC, buffer));
fprintf(f, "%s\t", gp->chrom);
fprintf(f, "%s\t", gp->strand);
fprintf(f, "%d\t", gp->txStart);
fprintf(f, "%d\t", gp->txEnd);
fprintf(f, "%d\t", gp->cdsStart);
fprintf(f, "%d\t", gp->cdsEnd);
fprintf(f, "%d\t", gp->exonCount);
int i;
for (i=0; i<gp->exonCount; ++i)
    fprintf(f, "%d,", gp->exonStarts[i]);
fprintf(f, "\t");
for (i=0; i<gp->exonCount; ++i)
    fprintf(f, "%d,", gp->exonEnds[i]);
fprintf(f, "\t");
char *uniProt = (char *)hashFindVal(hashes->genToUniProt, gp->name);
if (uniProt != NULL)
    fprintf(f, "%s\t", uniProt);
else
    fputs("\t", f);
fprintf(f, "%s\n", gp->name);
}

void outputKnownGene( struct genePred *compGenePreds, struct hashes *hashes)
{
struct genePred *gp;
FILE *f = mustOpen("knownGene.gp", "w");

for (gp = compGenePreds; gp; gp = gp->next)
    {
    writeOutOneKnownGene(f, gp, hashes);
    }
fclose(f);
}

char *descriptionFromAcc(struct sqlConnection *conn, char *acc)
{
char query[4096];

safef(query, sizeof query, "select d.name from all_mrna a, gbCdnaInfo c, description d where a.qName=\"%s\" and  a.qName=c.acc and c.description = d.id", acc );
return sqlQuickString(conn, query);
}

char *uniProtDescriptionFromAcc(struct sqlConnection *conn, char *acc)
{
char query[4096];

safef(query, sizeof query, "select v.val from commentVal v, comment c where c.acc = '%s' and v.id=c.commentVal", acc);
return sqlQuickString(conn, query);
}

char *displayIdFromAcc(struct sqlConnection *conn, char *acc)
{
char query[4096];

safef(query, sizeof query, "select val from displayId where acc = '%s'", acc);
return sqlQuickString(conn, query);
}

/*
char *hgncDescriptionFromAcc(struct sqlConnection *conn, char *acc)
{
char query[4096];

safef(query, sizeof query, "select geneFamilyDesc from hgnc where symbol = '%s'", acc);
return sqlQuickString(conn, query);
}
*/



void outputDescription(FILE *f, struct sqlConnection *conn,  struct sqlConnection *uconn,struct sqlConnection *pconn, struct genePred *gp, struct hashes *hashes)
{
char *description;
// try refSeq
struct wgEncodeGencodeRefSeq *wgr = (struct wgEncodeGencodeRefSeq *)hashFindVal(hashes->genToRefSeq, gp->name);

if (wgr != NULL)
    {
    char buffer[256];

    safecpy(buffer, sizeof buffer, wgr->rnaAcc);
    char *ptr = strrchr(buffer, '.');
    if (ptr != NULL)
	*ptr = 0;

    description = (char *)hashFindVal(hashes->refSeqToDescription, buffer);

    if (!isEmpty(description))
	{
	fprintf(f, "%s (from RefSeq %s)\t", description, buffer);
	return;
	}
    }
//
//  try Uniprot
char *uniProtAcc = (char *)hashFindVal(hashes->genToUniProt, gp->name);

if (uniProtAcc != NULL)
    {
    //char *description = (char *)hashFindVal(hashes->descriptionFromUniProtId, gp->name);
    char *description = uniProtDescriptionFromAcc(uconn, uniProtAcc);
    if (!isEmpty(description))
	{
	fprintf(f, "%s (from UniProt %s)\t", description, uniProtAcc);
	return;
	}
    }


// try HGNC
struct wgEncodeGencodeAttrs *wga = (struct wgEncodeGencodeAttrs *)hashFindVal(hashes->genToAttrs, gp->name);

if (wga != NULL)
    {
    char *description = (char *)hashFindVal(hashes->hgncDescriptionFromGeneName, wga->geneName);

    if (!isEmpty(description))
	{
	fprintf(f, "%s (from HGNC %s)\t", description, wga->geneName);
	return;
	}
    }

char buffer[1024];
safef(buffer, sizeof buffer, "%s.%s", gp->name, gp->chrom);
char *ucscName = (char *)hashMustFindVal(hashes->genToUC, buffer);
char *mrnaAcc = (char *)hashFindVal(hashes->genToMrna, ucscName);
if (mrnaAcc)
    {
    // TODO:  needs to get mrna descriptions
    //char buffer[256];

    //safecpy(buffer, sizeof buffer, wgr->rnaAcc);
    //char *ptr = strrchr(buffer, '.');
    //if (ptr != NULL)
	//*ptr = 0;
    //description = (char *)hashFindVal(hashes->mrnaToDescription, buffer);
    description = descriptionFromAcc(conn, mrnaAcc);
    if (!isEmpty(description))
	{
	fprintf(f, "%s (from mRNA %s)\t", description,mrnaAcc);
	return;
	}
    }
#ifdef NOTNOW
// try Gencode description
description = (char *)hashFindVal(hashes->genToAnnRemark, gp->name);

if (!isEmpty(description))
    {
    fprintf(f, "%s\t", description);
	printf("got %s\n", description);
    return;
    }
#endif

if ((wga != NULL) && !isEmpty(wga->geneName))
    {
    fprintf(f, "%s (from geneSymbol)\t", wga->geneName);
    return;
    }
fprintf(f, "\t");
}

static char *noDot(char *string)
{
static char buffer[1024];

safecpy(buffer, sizeof buffer, string);
char *ptr = strrchr(buffer, '.');

if (ptr)
    *ptr = 0;

return buffer;
}

void writeOutOneKgXref(FILE *f, struct sqlConnection *conn,struct sqlConnection *uconn,struct sqlConnection *pconn, struct genePred *gp, struct hashes *hashes)
{
char buffer[1024];
safef(buffer, sizeof buffer, "%s.%s", gp->name, gp->chrom);
fprintf(f, "%s\t", (char *)hashMustFindVal(hashes->genToUC, buffer));
//struct wgEncodeGencodeRefSeq *wgr = (struct wgEncodeGencodeRefSeq *)hashFindVal(hashes->genToRefSeq, gp->name);
/*
if (wgr != NULL)
    fprintf(f, "%s\t", wgr->rnaAcc);
elsed
    fprintf(f, "%s\t", ""); // mRNA
    */
char *mrnaAcc = (char *)hashFindVal(hashes->genToMrna, gp->name);
if (mrnaAcc)
    fprintf(f, "%s\t", mrnaAcc);
else
    fputs("\t",f);

char *uniProtAcc = (char *)hashFindVal(hashes->genToUniProt, gp->name);
if (uniProtAcc)
    {
    fprintf(f, "%s\t", uniProtAcc);

    //char *displayId = (char *) hashFindVal(hashes->displayIdFromUniProtId, uniProtAcc);
    char *displayId = displayIdFromAcc(uconn, uniProtAcc);
    if (displayId)
	fprintf(f, "%s\t", displayId);

    else
	fputs("\t",f);
    }
else
    fputs("\t\t",f);

struct wgEncodeGencodeAttrs *wga = (struct wgEncodeGencodeAttrs *)hashMustFindVal(hashes->genToAttrs, gp->name);
fprintf(f, "%s\t", wga->geneName);


//char *refSeqName = (char *)hashFindVal(hashes->genToRefSeq, gp->name);
//if (refSeqName != NULL)
struct wgEncodeGencodeRefSeq *wgr = (struct wgEncodeGencodeRefSeq *)hashFindVal(hashes->genToRefSeq, gp->name);
if (wgr != NULL)
    {
    //char *refPepName = (char *)hashFindVal(hashes->refSeqToPepName, gp->name);
    //fprintf(f, "%s\t%s\t", refSeqName, refPepName);
    fprintf(f, "%s\t%s\t", noDot(wgr->rnaAcc), noDot(wgr->pepAcc));
    }
else
    fputs("\t\t",f);

outputDescription(f, conn, uconn, pconn, gp, hashes);

fprintf(f, "%s\t", ""); // rfamAcc

#ifdef NOTNOW
struct hashEl *hel = hashLookup(hashes->genToTags, gp->name);

char *set = "composite";
for(; hel; hel = hel->next)
    {
    char *tag = (char *)hel->val;
    if (sameString(tag, "basic"))
	{
	set = "basic";
	break;
	}
    }
fprintf(f, "%s\n", set); // trnaName
#endif
fputc('\n', f);
}

void outputKgXref( struct sqlConnection *conn, struct genePred *compGenePreds, struct hashes *hashes)
{
struct sqlConnection *uconn = sqlConnect("uniProt");
struct sqlConnection *pconn = sqlConnect("proteome");
struct genePred *gp;
FILE *f = mustOpen("kgXref.tab", "w");

for (gp = compGenePreds; gp; gp = gp->next)
    {
    writeOutOneKgXref(f, conn, uconn, pconn, gp, hashes);
    }
fclose(f);
}

void outputKnownCanonical( struct genePred *compGenePreds, struct hashes *hashes)
{
// walk throught the different genes in the Attrs table and choose a transcriptfor each one
// TODO: right now we're choosing an arbitrary one, we should at least
// make sure it's in the basic set
/*
struct hashCookie cookie = hashFirst(hashes->genToAttrs);
//while ((el = hashNext(&cookie)) != NULL)
*/
struct hash *geneHash = newHash(10);
struct genePred *gp;
FILE *canonF = mustOpen("knownCanonical.tab", "w");
FILE *isoF = mustOpen("knownIsoforms.tab", "w");
unsigned clusterId = 1;
char buffer[1024];

for (gp = compGenePreds; gp; gp = gp->next)
    {
    struct wgEncodeGencodeAttrs *wga = (struct wgEncodeGencodeAttrs *)hashMustFindVal(hashes->genToAttrs, gp->name);
    safef(buffer, sizeof buffer, "%s.%s", gp->name, gp->chrom);
    char *ucscId = (char *)hashMustFindVal(hashes->genToUC, buffer);
    struct hashEl *el;
    unsigned isoClustId;

    if ((el = hashLookup(geneHash, wga->geneId)) == NULL)
	{
	fprintf(canonF, "%s\t%d\t%d\t%d\t%s\t%s\n",gp->chrom, gp->txStart, gp->txEnd,
	    clusterId, ucscId, wga->geneId);
	hashAddInt(geneHash, wga->geneId, clusterId );
	isoClustId = clusterId;
	clusterId++;
	}
    else
	{
	char *pt = NULL;
	isoClustId = (char *)el->val - pt;
	}
    fprintf(isoF, "%d\t%s\n", isoClustId, ucscId);
    }
fclose(canonF);
fclose(isoF);
}

void outputKnownToRefSeq( struct genePred *compGenePreds, struct hashes *hashes)
{
FILE *f = mustOpen("knownToRefSeq.tab", "w");
struct genePred *gp;

for (gp = compGenePreds; gp; gp = gp->next)
    {
    struct wgEncodeGencodeRefSeq *wgr = (struct wgEncodeGencodeRefSeq *)hashFindVal(hashes->genToRefSeq, gp->name);

    if (wgr != NULL)
	{
	char buffer[1024];
	safef(buffer, sizeof buffer, "%s.%s", gp->name, gp->chrom);
	fprintf(f, "%s\t%s\n", 
	    (char *)hashMustFindVal(hashes->genToUC, buffer), wgr->rnaAcc);
	}
    }
}

void makeGencodeKnownGene(char *database, char *tempDatabase, char *version, char *txToAccTab)
/* makeGencodeKnownGene - make knownGene from Gencode tables. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *tconn = sqlConnect(tempDatabase);
struct sqlConnection *pconn = sqlConnect("proteome");
//struct sqlConnection *uconn = sqlConnect("uniProt");
struct hashes hashes;
hashes.genToUC = getMap(txToAccTab);
hashes.genToAttrs = getAttrsTable(conn, version);
//hashes.genToAnnRemark = getMapTable(conn, "select transcriptId, remark from wgEncodeGencodeAnnotationRemark", version);
hashes.genToUniProt = getMapTable(conn, "select transcriptId, acc from wgEncodeGencodeUniProt", version);
hashes.genToRefSeq = getRefSeqTable(conn, version);
//hashes.genToRefSeq = getMapTable(tconn, "select name, value from knownToRefSeq", NULL);
struct genePred *compGenePreds = loadGenePreds(conn, "select * from wgEncodeGencodeComp", version);
hashes.refSeqToPepName = getMapTable(conn, "select mrnaAcc,protAcc from refLink", NULL);
//hashes.mrnaToDescription = getMapTable(conn, "select a.name,d.name from all_mrna a, gbCdnaInfo c, description d where a.qName=c.acc and c.description = d.id", NULL);
hashes.refSeqToDescription = getMapTable(conn, "select g.name,d.name from refGene g, gbCdnaInfo c, description d where g.name=c.acc and c.description = d.id", NULL);
hashes.hgncDescriptionFromGeneName = getMapTable(pconn, "select symbol, name from hgnc", NULL);
//hashes.displayIdFromUniProtId = getMapTable(uconn, "select acc, val from displayId", NULL);
//printf("displayIdFromUniProtId %ld\n", time(NULL) - start);
//hashes.descriptionFromUniProtId = getMapTable(uconn, "select c.acc, v.val from commentVal v, comment c where v.id=c.commentVal", NULL);
//printf("descriptionFromUniProtId %ld\n", time(NULL) - start);
outputKnownGene(compGenePreds, &hashes);
hashes.genToMrna = getMapTable(tconn, "select name, value from knownToMrna", NULL);
hashes.genToTags = getMapTable(conn, "select transcriptId, tag from wgEncodeGencodeTag", version);
outputKgXref(conn, compGenePreds, &hashes);
outputKnownCanonical(compGenePreds, &hashes);
//outputKnownToRefSeq(compGenePreds, &hashes);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
makeGencodeKnownGene(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

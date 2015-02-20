/* makeKnownGene - make knownGene from Gencode tables. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "genePred.h"
#include "encode/wgEncodeGencodeAttrs.h"
#include "encode/wgEncodeGencodeRefSeq.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeKnownGene - make knownGene from Gencode tables\n"
  "usage:\n"
  "   makeKnownGene database version# txToAcc.tab \n"
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
struct hash *refSeqToDescription;
struct hash *hgncDescriptionFromGeneName;
struct hash *displayIdFromUniProtId;
struct hash *descriptionFromUniProtId;
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

fprintf(f, "%s\t", (char *)hashMustFindVal(hashes->genToUC, gp->name));
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

/*
char *descriptionFromAcc(struct sqlConnection *conn, char *acc)
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

char *hgncDescriptionFromAcc(struct sqlConnection *conn, char *acc)
{
char query[4096];

safef(query, sizeof query, "select geneFamilyDesc from hgnc where symbol = '%s'", acc);
return sqlQuickString(conn, query);
}
*/



void outputDescription(FILE *f, struct sqlConnection *uconn,struct sqlConnection *pconn, struct genePred *gp, struct hashes *hashes)
{
char *description;
// first try Uniprot
char *uniProtAcc = (char *)hashFindVal(hashes->genToUniProt, gp->name);

if (uniProtAcc != NULL)
    {
    char *description = (char *)hashFindVal(hashes->descriptionFromUniProtId, gp->name);
    if (!isEmpty(description))
	{
	fprintf(f, "%s\t", description);
	return;
	}
    }

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
	fprintf(f, "%s\t", description);
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
	fprintf(f, "%s\t", description);
	return;
	}
    }

// try Gencode description
description = (char *)hashFindVal(hashes->genToAnnRemark, gp->name);

if (!isEmpty(description))
    {
    fprintf(f, "%s\t", description);
	printf("got %s\n", description);
    return;
    }

fprintf(f, "\t");
}

void writeOutOneKgXref(FILE *f, struct sqlConnection *uconn,struct sqlConnection *pconn, struct genePred *gp, struct hashes *hashes)
{
fprintf(f, "%s\t", (char *)hashMustFindVal(hashes->genToUC, gp->name));
struct wgEncodeGencodeRefSeq *wgr = (struct wgEncodeGencodeRefSeq *)hashFindVal(hashes->genToRefSeq, gp->name);
/*
if (wgr != NULL)
    fprintf(f, "%s\t", wgr->rnaAcc);
else
    fprintf(f, "%s\t", ""); // mRNA
    */
fprintf(f, "%s\t", gp->name); // mRNA

char *uniProtAcc = (char *)hashFindVal(hashes->genToUniProt, gp->name);
if (uniProtAcc)
    {
    fprintf(f, "%s\t", uniProtAcc);

    char *displayId = (char *) hashFindVal(hashes->displayIdFromUniProtId, uniProtAcc);
    if (displayId)
	fprintf(f, "%s\t", displayId);

    else
	fputs("\t",f);
    }
else
    fputs("\t\t",f);

struct wgEncodeGencodeAttrs *wga = (struct wgEncodeGencodeAttrs *)hashMustFindVal(hashes->genToAttrs, gp->name);
fprintf(f, "%s\t", wga->geneName);

if (wgr != NULL)
    {
    fprintf(f, "%s\t%s\t", wgr->rnaAcc, wgr->pepAcc);
    }
else
    fputs("\t\t",f);

outputDescription(f, uconn, pconn, gp, hashes);

fprintf(f, "%s\t", ""); // rfamAcc
fprintf(f, "%s\n", ""); // trnaName
}

void outputKgXref( struct genePred *compGenePreds, struct hashes *hashes)
{
struct sqlConnection *uconn = sqlConnect("uniProt");
struct sqlConnection *pconn = sqlConnect("proteome");
struct genePred *gp;
FILE *f = mustOpen("kgXref.tab", "w");

for (gp = compGenePreds; gp; gp = gp->next)
    {
    writeOutOneKgXref(f, uconn, pconn, gp, hashes);
    }
fclose(f);
}

void outputKnownCanonical( struct genePred *compGenePreds, struct hashes *hashes)
{
// walk throught the different genes in the Attrs table and choose a transcriptfor each one
/*
struct hashCookie cookie = hashFirst(hashes->genToAttrs);
//while ((el = hashNext(&cookie)) != NULL)
*/
struct hash *geneHash = newHash(10);
struct genePred *gp;
FILE *f = mustOpen("kgCanonical.tab", "w");
unsigned clusterId = 1;

for (gp = compGenePreds; gp; gp = gp->next)
    {
    struct wgEncodeGencodeAttrs *wga = (struct wgEncodeGencodeAttrs *)hashMustFindVal(hashes->genToAttrs, gp->name);
    struct hashEl *el;

    if ((el = hashLookup(geneHash, wga->geneId)) == NULL)
	{
	fprintf(f, "%s\t%d\t%d\t%d\t%s\t%s\n",gp->chrom, gp->txStart, gp->txEnd,
	    clusterId++, (char *)hashMustFindVal(hashes->genToUC, gp->name), wga->geneId);
	hashStore(geneHash, wga->geneId);
	}
    }
fclose(f);
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
	fprintf(f, "%s\t%s\n", 
	    (char *)hashMustFindVal(hashes->genToUC, gp->name), wgr->rnaAcc);
	}
    }
}

void makeKnownGene(char *database, char *version, char *txToAccTab)
/* makeKnownGene - make knownGene from Gencode tables. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *pconn = sqlConnect("proteome");
struct sqlConnection *uconn = sqlConnect("uniProt");
struct hashes hashes;
hashes.genToUC = getMap(txToAccTab);
hashes.genToAttrs = getAttrsTable(conn, version);
hashes.genToAnnRemark = getMapTable(conn, "select transcriptId, remark from wgEncodeGencodeAnnotationRemark", version);
hashes.genToUniProt = getMapTable(conn, "select transcriptId, acc from wgEncodeGencodeUniProt", version);
hashes.genToRefSeq = getRefSeqTable(conn, version);
struct genePred *compGenePreds = loadGenePreds(conn, "select * from wgEncodeGencodeComp", version);
hashes.refSeqToDescription = getMapTable(conn, "select g.name,d.name from refGene g, gbCdnaInfo c, description d where g.name=c.acc and c.description = d.id", NULL);
hashes.hgncDescriptionFromGeneName = getMapTable(pconn, "select symbol, geneFamilyDesc from hgnc", NULL);
hashes.displayIdFromUniProtId = getMapTable(uconn, "select acc, val from displayId", NULL);
hashes.descriptionFromUniProtId = getMapTable(uconn, "select c.acc, v.val from commentVal v, comment c where v.id=c.commentVal", NULL);
outputKnownGene(compGenePreds, &hashes);
outputKgXref(compGenePreds, &hashes);
outputKnownCanonical(compGenePreds, &hashes);
outputKnownToRefSeq(compGenePreds, &hashes);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
makeKnownGene(argv[1], argv[2], argv[3]);
return 0;
}

/* makeGencodeKnownGene - make knownGene from Gencode tables. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "genePred.h"
#include "memgfx.h"
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
  "   -justKnown\n"
  );
}

boolean justKnown;

struct hashes
{
struct hash *genToUC;
struct hash *genToUniProt;
struct hash *genToAnnRemark;;
struct hash *genToAttrs;
struct hash *genToRefSeq;
struct hash *genToMrna;
struct hash *genToTags;
struct hash *genToPdb;
struct hash *refSeqToDescription;
//struct hash *mrnaToDescription;
struct hash *refSeqToPepName;
struct hash *refSeqToStatus;
struct hash *hgncDescriptionFromGeneName;
//struct hash *displayIdFromUniProtId;
//struct hash *descriptionFromUniProtId;
};

/* Command line validation table. */
static struct optionSpec options[] = {
   {"justKnown", OPTION_BOOLEAN},
   {NULL, 0},
};

static struct hash *getMap(char *fileName)
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

static struct hash *getRefSeqTable(struct sqlConnection *conn,  char *version)
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

static struct hash *getAttrsTable(struct sqlConnection *conn,  char *version)
{
char versionQuery[4096];

safef(versionQuery, sizeof versionQuery, "select * from wgEncodeGencodeAttrs%s",  version);

char **row;
struct sqlResult *sr;
struct hash *hash = newHash(6);

sr = sqlGetResult(conn, versionQuery);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct wgEncodeGencodeAttrs *wga = wgEncodeGencodeAttrsLoad(row, sqlCountColumns(sr));
    hashAdd(hash, wga->transcriptId, wga);
    }
sqlFreeResult(&sr);

return hash;
}

static struct hash *getMapTable(struct sqlConnection *conn, char *query, char *version)
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

static struct genePred *loadGenePreds(struct sqlConnection *conn, char *query, char *version)
{
struct genePred *gpList = NULL;
char versionQuery[4096];
char **row;
struct sqlResult *sr;

safef(versionQuery, sizeof versionQuery, "%s%s", query, version);
sr = sqlGetResult(conn, versionQuery);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = genePredExtLoad(&row[1], 15);

    slAddHead(&gpList, gp);
    }

sqlFreeResult(&sr);

slReverse(&gpList);
return gpList;
}

static void writeOutOneKnownGeneNoNl(FILE *f, struct genePred *gp, struct hashes *hashes)
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
}

static void writeOutOneKnownGene(FILE *f, struct genePred *gp, struct hashes *hashes)
{
writeOutOneKnownGeneNoNl(f, gp, hashes);
char *uniProt = (char *)hashFindVal(hashes->genToUniProt, gp->name);
if (uniProt != NULL)
    fprintf(f, "%s\t", uniProt);
else
    fputs("\t", f);
fprintf(f, "%s\n", gp->name);
}

static void writeOutOneKnownGeneExt(FILE *f, struct genePred *gp, struct hashes *hashes)
{
writeOutOneKnownGeneNoNl(f, gp, hashes);
fprintf(f, "0\t%s\t", gp->name);
fprintf(f, "%s\t", genePredCdsStatStr(gp->cdsStartStat));
fprintf(f, "%s\t", genePredCdsStatStr(gp->cdsEndStat));
int i;
for (i=0; i< gp->exonCount; ++i)
    {
    fprintf(f, "%d",  gp->exonFrames[i]);
    fputc(',', f);
    }
fputs("\n", f);
}


static struct rgbColor black = {0,0,0};
static struct rgbColor trueBlue = {12,12,120};
static struct rgbColor mediumBlue = {80, 80, 160};
static struct rgbColor lightBlue = {130, 130, 210};

static void outputKnownGeneColor( struct genePred *compGenePreds, struct hashes *hashes)
/* Output the kgColor table. */
{
struct genePred *gp;
FILE *f = mustOpen("kgColor.tab", "w");
struct rgbColor *color;

for (gp = compGenePreds; gp; gp = gp->next)
    {
    char buffer[256];

    color = &lightBlue;
    safef(buffer, sizeof buffer, "%s.%s", gp->name, gp->chrom);
    char *ucName = (char *)hashMustFindVal(hashes->genToUC, buffer);

    char *pdb = (char *)hashFindVal(hashes->genToPdb, gp->name);
    char *uniProtAcc = (char *)hashFindVal(hashes->genToUniProt, gp->name);

    if (pdb != NULL)
	color = &black;
    else if (uniProtAcc != NULL)
	color = &trueBlue;
    else
	{
	struct wgEncodeGencodeRefSeq *wgr = (struct wgEncodeGencodeRefSeq *)hashFindVal(hashes->genToRefSeq, gp->name);
	if (wgr != NULL)
	    {
	    char *acc = cloneString(wgr->rnaAcc);
	    char *ptr = strchr(acc, '.');
	    if (ptr)
		*ptr = 0;
	    char *status = (char *)hashFindVal(hashes->refSeqToStatus, acc);

	    if ((status != NULL) && 
		(sameString(status, "Validated") ||
		sameString(status, "Reviewed")))
		color = &trueBlue;
	    else
		color = &mediumBlue;
	    }
	}
    fprintf(f, "%s\t%d\t%d\t%d\n",ucName, color->r,color->g,color->b);
    }
}

static void outputKnownGene( struct genePred *compGenePreds, struct hashes *hashes)
/* Output the knownGene table */
{
struct genePred *gp;
FILE *f = mustOpen("knownGene.gp", "w");
FILE *fx = mustOpen("knownGeneExt.gp", "w");

for (gp = compGenePreds; gp; gp = gp->next)
    {
    writeOutOneKnownGene(f, gp, hashes);
    writeOutOneKnownGeneExt(fx, gp, hashes);
    }
fclose(f);
fclose(fx);
}

static char *descriptionFromAcc(struct sqlConnection *conn, char *acc)
{
char query[4096];

safef(query, sizeof query, "select d.name from all_mrna a, gbCdnaInfo c, description d where a.qName=\"%s\" and  a.qName=c.acc and c.description = d.id", acc );
return sqlQuickString(conn, query);
}

static char *uniProtDescriptionFromAcc(struct sqlConnection *conn, char *acc)
{
char query[4096];

safef(query, sizeof query, "select v.val from commentVal v, comment c where c.acc = '%s' and v.id=c.commentVal", acc);
return sqlQuickString(conn, query);
}

static char *displayIdFromAcc(struct sqlConnection *conn, char *acc)
{
char query[4096];

safef(query, sizeof query, "select val from displayId where acc = '%s'", acc);
return sqlQuickString(conn, query);
}


static void outputDescription(FILE *f, struct sqlConnection *conn,  struct sqlConnection *uconn,struct sqlConnection *pconn, struct genePred *gp, struct hashes *hashes)
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

#ifdef NOTNOW   // the annotation remark is not a very useful string
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

static void writeOutOneKgXref(FILE *f, struct sqlConnection *conn,struct sqlConnection *uconn,struct sqlConnection *pconn, struct genePred *gp, struct hashes *hashes)
{
char buffer[1024];
safef(buffer, sizeof buffer, "%s.%s", gp->name, gp->chrom);
char *ucName = (char *)hashMustFindVal(hashes->genToUC, buffer);
fprintf(f, "%s\t", ucName);
struct wgEncodeGencodeRefSeq *wgr = (struct wgEncodeGencodeRefSeq *)hashFindVal(hashes->genToRefSeq, gp->name);
if (wgr != NULL)
    fprintf(f, "%s\t", noDot(wgr->rnaAcc));
else
    {
    char *mrnaAcc = (char *)hashFindVal(hashes->genToMrna, ucName);
    if (mrnaAcc)
	fprintf(f, "%s\t", mrnaAcc);
    else
	fputs("\t",f);
    }

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
//struct wgEncodeGencodeRefSeq *wgr = (struct wgEncodeGencodeRefSeq *)hashFindVal(hashes->genToRefSeq, gp->name);
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

static void outputKgXref( struct sqlConnection *conn, struct genePred *compGenePreds, struct hashes *hashes)
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

struct transInfo
{
struct transInfo *next;
char *transcriptId;
char *geneId;
char *tag;
unsigned length;
unsigned tagVal;
struct genePred *gp;
unsigned clusterId;
char *ucscId;
};


static int tiCmp(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct transInfo *a = *((struct transInfo **)va);
const struct transInfo *b = *((struct transInfo **)vb);
int cmp = strcmp(a->geneId, b->geneId);
if (cmp != 0)
    return cmp;
//cmp = strcmp(a->transcriptId, b->transcriptId);
//if (cmp != 0)
 //   return cmp;
cmp = b->tagVal - a->tagVal;
if (cmp != 0)
    return cmp;

return b->length - a->length;

}

static void outputKnownCanonical( struct genePred *compGenePreds, struct hashes *hashes)
{
// walk throught the different genes in the Attrs table and choose a transcriptfor each one
/*
struct hashCookie cookie = hashFirst(hashes->genToAttrs);
//while ((el = hashNext(&cookie)) != NULL)
*/
//struct hash *geneHash = newHash(10);
struct genePred *gp;
FILE *canonF = mustOpen("knownCanonical.tab", "w");
FILE *isoF = mustOpen("knownIsoforms.tab", "w");
unsigned clusterId = 1;
char buffer[1024];

struct transInfo *tiList = NULL, *ti;

for (gp = compGenePreds; gp; gp = gp->next)
    {
    safef(buffer, sizeof buffer, "%s.%s", gp->name, gp->chrom);
    char *ucscId = (char *)hashMustFindVal(hashes->genToUC, buffer);
    struct wgEncodeGencodeAttrs *wga = (struct wgEncodeGencodeAttrs *)hashMustFindVal(hashes->genToAttrs, gp->name);

    struct hashEl *hel = hashLookup(hashes->genToTags, gp->name);
    if (hel == NULL)
	{
	AllocVar(ti);
	slAddHead(&tiList, ti);

	ti->transcriptId = gp->name;
	ti->geneId = wga->geneId;
	ti->length = gp->txEnd - gp->txStart;
	ti->tag = "none";
	ti->tagVal = 0;
	ti->gp = gp;
	ti->ucscId = ucscId;
	}
    for(; hel; hel = hel->next)
	{
	char *tag = (char *)hel->val;

	AllocVar(ti);
	slAddHead(&tiList, ti);

	ti->transcriptId = gp->name;
	ti->geneId = wga->geneId;
	ti->length = gp->txEnd - gp->txStart;

	ti->tag = cloneString(tag);
	ti->gp = gp;
	ti->ucscId = ucscId;

	if (startsWith("appris_principal", tag))
	    ti->tagVal = 100;
	else if (startsWith("appris_alternative", tag))
	    ti->tagVal = 90;
	else if (startsWith("basic", tag))
	    ti->tagVal = 80;
	else 
	    ti->tagVal = 0;
	}
    }

slSort(&tiList, tiCmp);

struct hash *geneHash = newHash(10);
for(ti=tiList; ti; ti = ti->next)
    {
    if (hashLookup(geneHash, ti->geneId) == NULL)
	{
	ti->clusterId = clusterId;
	hashAdd(geneHash, ti->geneId, ti);
	struct genePred *gp = ti->gp;
	fprintf(canonF, "%s\t%d\t%d\t%d\t%s\t%s\n",gp->chrom, gp->txStart, gp->txEnd,
	    clusterId, ti->ucscId, ti->geneId);

	clusterId++;
//	printf("%s %s %s %d\n", ti->geneId, ti->transcriptId, ti->tag, ti->length);
	}
    }
for (gp = compGenePreds; gp; gp = gp->next)
    {
    struct wgEncodeGencodeAttrs *wga = (struct wgEncodeGencodeAttrs *)hashMustFindVal(hashes->genToAttrs, gp->name);
    safef(buffer, sizeof buffer, "%s.%s", gp->name, gp->chrom);
    char *ucscId = (char *)hashMustFindVal(hashes->genToUC, buffer);
    struct hashEl *hel;
    if ((hel = hashLookup(geneHash, wga->geneId)) == NULL)
	errAbort("gene not in geneHash");

    struct transInfo *ti = (struct transInfo *)hel->val;
    fprintf(isoF, "%d\t%s\n", ti->clusterId, ucscId);
    }
fclose(canonF);
fclose(isoF);
}

#ifdef NOTNOW // not currently used, use hgMapGene instead (at the moment ;-)
static void outputKnownToRefSeq( struct genePred *compGenePreds, struct hashes *hashes)
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
#endif

static void makeGencodeKnownGene(char *database, char *tempDatabase, char *version, char *txToAccTab)
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
hashes.refSeqToStatus = getMapTable(conn, "select mrnaAcc, status from refSeqStatus", NULL);
hashes.genToPdb = getMapTable(conn, "select transcriptId, pdbId from wgEncodeGencodePdb", version);
//hashes.displayIdFromUniProtId = getMapTable(uconn, "select acc, val from displayId", NULL);
//printf("displayIdFromUniProtId %ld\n", time(NULL) - start);
//hashes.descriptionFromUniProtId = getMapTable(uconn, "select c.acc, v.val from commentVal v, comment c where v.id=c.commentVal", NULL);
//printf("descriptionFromUniProtId %ld\n", time(NULL) - start);
hashes.genToTags = getMapTable(conn, "select transcriptId, tag from wgEncodeGencodeTag", version);
if (!justKnown)
    outputKnownCanonical(compGenePreds, &hashes);
outputKnownGene(compGenePreds, &hashes);
if (justKnown)
    exit(0);
outputKnownGeneColor(compGenePreds, &hashes);
hashes.genToMrna = getMapTable(tconn, "select name, value from knownToMrnaSingle", NULL);
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
justKnown = optionExists("justKnown");
makeGencodeKnownGene(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

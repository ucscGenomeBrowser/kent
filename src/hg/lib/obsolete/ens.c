/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/

#include "common.h"
#include "hash.h"
#include "dlist.h"
#include "dystring.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "jksql.h"
#include "ens.h"

static struct sqlConnCache *ecc = NULL;

static struct sqlConnection *ensAllocConn()
/* Get free connection if possible. If not allocate a new one. */
{
if (ecc == NULL)
    ecc = sqlNewConnCache("ensdev");
return sqlAllocConnection(ecc);
}

static struct sqlConnection *ensFreeConn(struct sqlConnection **pConn)
/* Put back connection for reuse. */
{
sqlFreeConnection(ecc, pConn);
}

void makeShortName(struct ensAnalysis *ea)
/* Fill in short name field of analysis. */
{
char shortName[256];
if (ea->program != NULL && ea->db != NULL)
    sprintf(shortName, "%s:%s", ea->db, ea->program);
else if (ea->gffSource != NULL && ea->gffFeature != NULL)
    sprintf(shortName, "%s:%s", ea->gffFeature, ea->gffSource);
else if (ea->gffFeature != NULL && ea->program != NULL)
    sprintf(shortName, "%s:%s", ea->gffFeature, ea->program);
else if (ea->gffSource != NULL)
    strcpy(shortName, ea->gffSource);
else if (ea->gffFeature != NULL)
    strcpy(shortName, ea->gffFeature);
else if (ea->db != NULL)
    strcpy(shortName, ea->db);
else if (ea->program != NULL)
    strcpy(shortName, ea->program);
else
    sprintf(shortName, "type %d", ea->id);
shortName[16] = 0;
ea->shortName = cloneString(shortName);
}

void ensGetAnalysisTable(struct ensAnalysis ***retTable, int *retCount)
/* Returns analysis table (array of different things a feature can be). */
{
static int tableCount = 0;
static struct ensAnalysis **table = NULL;

if (table == NULL)
    {
    char *query = "select db,db_version,program,program_version,gff_source,gff_feature,id "
                  "from analysis";
    struct sqlConnection *conn = ensAllocConn();
    struct ensAnalysis *list = NULL, *el;
    int maxId = 0;
    char **row;
    struct sqlResult *sr;

    /* Issue query and load up a list of analysis structs from rows. */
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	AllocVar(el);
	el->db = cloneString(row[0]);
	el->dbVersion = cloneString(row[1]);
	el->program = cloneString(row[2]);
	el->programVersion = cloneString(row[3]);
	el->gffSource = cloneString(row[4]);
	el->gffFeature = cloneString(row[5]);
	el->id = sqlUnsigned(row[6]);
	if (el->id > maxId)
	    maxId = el->id;
	makeShortName(el);
	slAddHead(&list, el);
	}
    sqlFreeResult(&sr);
    ensFreeConn(&conn);

    /* Make an array where index in array corresponds to id. */
    tableCount = maxId + 1;
    table = needMem(sizeof(*table) * tableCount);
    for (el = list; el != NULL; el = el->next)
	{
	table[el->id] = el;
	}
    }
*retTable = table;
*retCount = tableCount;
}


void ensParseContig(char *combined, char retBac[32], int *retContig)
/* Parse combined bac.contig into two separate values.
 * This does cannibalize the combined input.... */
{
char *e = strchr(combined, '.');
if (e == NULL)
    {
    *retContig = 0;
    strcpy(retBac, combined);
    }
else
    {
    int bacNameSize = e - combined;
    if (bacNameSize > 31)
	errAbort("Contig %s - name too long", combined);
    *retContig = sqlUnsigned(e+1);
    memcpy(retBac, combined, bacNameSize);
    retBac[bacNameSize] = 0;
    }
}

static struct contigTree *ensContigForest;	/* Pointer to top level contig trees. */
static struct hash *ensContigHash;

static struct contigTree *ensGetPreloadedContig(char *id)
/* Get a contig that has been preloaded.  (Squawk and die if it
 * hasn't.) */
{
struct hashEl *hel;
if ((hel = hashLookup(ensContigHash, id)) == NULL)
    errAbort("Can't find preloaded contig %s", id);
return hel->val;
}

static int cmpCorder(const void *va, const void *vb)
/* Compare corder's from two contigs. */
{
const struct contigTree *a = *((struct contigTree **)va);
const struct contigTree *b = *((struct contigTree **)vb);
return a->corder - b->corder;
}

static struct contigTree *getContigs(struct contigTree *bacParent)
/* Get contigs of bac. */
{
char *clone = bacParent->id;
struct sqlConnection *conn = ensAllocConn();
struct sqlResult *sr;
char query[256];
char **row;
struct contigTree *leafList = NULL, *leaf;
struct hashEl *hel;
int browserOffset = 0;

sprintf(query,
 "SELECT id,length,offset,orientation,corder FROM contig "
 "WHERE clone = '%s'",
 clone);
sr = sqlGetResult(conn, query);
if (sr == NULL)
    errAbort("No contigs for clone %s", clone);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(leaf);
    hel = hashAdd(ensContigHash, row[0], leaf);
    hel->val = leaf;
    leaf->parent = bacParent;
    leaf->id = hel->name;
    leaf->submitLength = sqlUnsigned(row[1]);
    leaf->submitOffset = sqlSigned(row[2])-1;
    leaf->orientation = sqlSigned(row[3]);
    leaf->corder = sqlUnsigned(row[4]);
    slAddHead(&leafList, leaf);
    }
slSort(&leafList, cmpCorder);
for (leaf = leafList; leaf != NULL; leaf = leaf->next)
    {
    leaf->browserOffset = browserOffset;
    leaf->browserLength = leaf->submitLength;
    browserOffset += contigPad + leaf->submitLength;
    }
sqlFreeResult(&sr);
ensFreeConn(&conn);
return leafList;
}

static void lengthFromKids(struct contigTree *children, int *retSubmitLength, int *retBrowserLength)
/* Figure out length of parent from children. */
{
struct contigTree *child;
int maxSubmitLen = 0;
int end;
int maxBrowseLen = 0;

for (child = children; child != NULL; child = child->next)
    {
    end = child->submitLength + child->submitOffset;
    if (end > maxSubmitLen)
	maxSubmitLen = end;
    end = child->browserLength + child->browserOffset;
    if (end > maxBrowseLen)
	maxBrowseLen = end;
    }
*retSubmitLength = maxSubmitLen;
*retBrowserLength = maxBrowseLen;
}

struct contigTree *ensBacContigs(char *bacId)
/* Return contigTree rooted at Bac.  Do not free this or modify it, 
 * the system takes care of it. */
{
struct hashEl *hel;
struct contigTree *bacTree;
struct contigTree *contigLeaf;

if (ensContigHash == NULL)
    ensContigHash = newHash(16);
if ((hel = hashLookup(ensContigHash, bacId)) == NULL)
    {
    AllocVar(bacTree);
    hel = hashAdd(ensContigHash, bacId, bacTree);
    bacTree->id = hel->name;
    bacTree->children = getContigs(bacTree);
    lengthFromKids(bacTree->children, &bacTree->submitLength, &bacTree->browserLength);
    slAddHead(&ensContigForest, bacTree);
    }
else
    bacTree = hel->val;
return bacTree;
}

struct contigTree *ensGetContig(char *contigId)
/* Return contig associated with contigId. Do not free this, system
 * takes care of it. */
{
char cloneName[32];
int contigIx;
struct contigTree *parent;
ensParseContig(contigId, cloneName, &contigIx);
parent = ensBacContigs(cloneName);
return ensGetPreloadedContig(contigId);
}

int ensBrowserCoordinates(struct contigTree *contig, int x)
/* Return x in coordinates browser uses. */
{
return x + contig->browserOffset;
}

int ensSubmitCoordinates(struct contigTree *contig, int x)
/* Return x in GenBank/EMBL submission coordinates. */
{
return x + contig->submitOffset;
}

int ensBacBrowserLength(char *clone)
/* Return size of clone in browser coordinate space. */
{
struct contigTree *bacTree = ensBacContigs(clone);
return bacTree->browserLength;
}

int ensBacSubmitLength(char *clone)
/* Return size of clone in GenBank/EMBL submission  coordinate space. */
{
struct contigTree *bacTree = ensBacContigs(clone);
return bacTree->submitLength;
}

static struct ensFeature *featFromRow(char **row)
/* Turn feature from row representation to structure. */
{
struct ensFeature *ef;
static struct hash *typeNames = NULL;
struct hashEl *hel;
char bacName[32];

AllocVar(ef);
ef->tContig = ensGetPreloadedContig(row[0]);
ef->tStart = sqlUnsigned(row[1])-1;
ef->tEnd = sqlUnsigned(row[2]);
ef->score = sqlUnsigned(row[3]);
ef->orientation = sqlSigned(row[4]);
ef->type = sqlUnsigned(row[5]);
if (typeNames == NULL)
    typeNames = newHash(0);
ef->typeName = hashStoreName(typeNames, row[6]);
ef->qStart = sqlUnsigned(row[7])-1;
ef->qEnd = sqlUnsigned(row[8]);
ef->qName = cloneString(row[9]);
return ef;
}

static struct ensFeature *ensFeaturesQuery(char *query)
/* Return a feature list from a properly constructed query. 
 * Helper routine for ensFeatForBac, etc. */
{
struct sqlConnection *conn = ensAllocConn();
struct sqlResult *sr;
struct ensFeature *efList = NULL, *ef;
char **row;

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ef = featFromRow(row);
    slAddHead(&efList, ef);
    }
sqlFreeResult(&sr);
ensFreeConn(&conn);
return efList;
}


struct ensFeature *ensFeaturesInBac(char *clone)
/* Get list of features associated with BAC clone. */
{
char query[256];
ensBacContigs(clone);
sprintf(query, "SELECT contig,seq_start,seq_end,score,strand,analysis,name,hstart,hend,hid "
               "FROM feature WHERE contig LIKE '%s%%'", clone);
return ensFeaturesQuery(query);
}

struct ensFeature *ensFeaturesInBacRange(char *clone, int start, int end)
/* Get list of features associated a section of BAC clone. */
{
struct dyString *ds = newDyString(512);
struct contigTree *bacTree, *contig;
struct ensFeature *efFullList, *efRangeList = NULL, *ef, *next;
int cStart, cEnd;
int fStart, fEnd;
boolean firstTime = TRUE;

/* Construct a query that returns features that hit any contigs that intersect our range. */
dyStringAppend(ds, "SELECT contig,seq_start,seq_end,score,strand,analysis,name,hstart,hend,hid "
               "FROM feature WHERE ");
bacTree = ensBacContigs(clone);
for (contig = bacTree->children; contig != NULL; contig = contig->next)
    {
    cStart = contig->browserOffset;
    cEnd = cStart + contig->browserLength;
    if (rangeIntersection(start, end, cStart, cEnd) > 0)
	{
	if (firstTime)
	    firstTime = FALSE;
	else
	    dyStringAppend(ds, " OR ");
	dyStringPrintf(ds, "contig = '%s'", contig->id);
	}
    }
efFullList = ensFeaturesQuery(ds->string);
freeDyString(&ds);

/* Filter out the features that aren't actually in range. */
for (ef = efFullList; ef != NULL; ef = next)
    {
    next = ef->next;
    contig = ef->tContig;
    fStart = ensBrowserCoordinates(contig, ef->tStart);
    fEnd = ensBrowserCoordinates(contig, ef->tEnd);
    if (rangeIntersection(start, end, fStart, fEnd) > 0)
	{
	slAddHead(&efRangeList, ef);
	}
    else
	{
	ensFreeFeature(&ef);
	}
    }
slReverse(&efRangeList);
return efRangeList;
}

struct ensFeature *ensGetFeature(char *featureId)
/* Get a single feature of the given ID.  Returns NULL if no such feature. */
{
char query[256];
sprintf(query, "SELECT contig,seq_start,seq_end,score,strand,analysis,name,hstart,hend,hid "
               "FROM feature WHERE id = '%s%'", featureId);
return ensFeaturesQuery(query);
}

void ensFreeFeature(struct ensFeature **pFeature)
/* Free up a single feature. */
{
struct ensFeature *feat;
if ((feat = *pFeature) != NULL)
    {
    freeMem(feat->qName);
    freez(pFeature);
    }
}

void ensFreeFeatureList(struct ensFeature **pFeatureList)
/* Free up a list of features. */
{
struct ensFeature *feat, *next;

for (feat = *pFeatureList; feat != NULL; feat = next)
    {
    next = feat->next;
    ensFreeFeature(&feat);
    }
*pFeatureList = NULL;
}


struct slName *ensGeneNamesQuery(char *query)
/* Returns a list of gene names from a properly constructed query.
 * Helper function for ensGeneNamesXXX family. */
{
struct sqlConnection *conn = ensAllocConn();
struct sqlResult *sr;
struct slName *geneList = 0, *gene;

sr = sqlGetResult(conn, query);
if (sr != NULL)
    {
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	gene = newSlName(row[0]);
	slAddHead(&geneList, gene);
	}
    }
sqlFreeResult(&sr);
ensFreeConn(&conn);
return geneList;
}

struct slName *ensGeneNamesInBac(char *bacName)
/* Get list of all genes in bac. */
{
char query[512];
sprintf(query,
  "SELECT gene "
  "FROM geneclone_neighbourhood "
  "WHERE clone = '%s'",
  bacName);
return ensGeneNamesQuery(query);
}

boolean ensGeneExists(char *geneName)
/* Returns true if gene exists, false otherwise. */
{
char query[256];
char buf[64];
boolean ok;
struct sqlConnection *conn = ensAllocConn();
sprintf(query, "SELECT id FROM gene WHERE id = '%s'", geneName);
ok =  (sqlQuickQuery(conn, query,buf,sizeof(buf)) != NULL);
ensFreeConn(&conn);
return ok;
}

struct ensGene *ensGetGene(char *geneName)
/* Get named gene. */
{
/* This requires two nested queries - one for the
 * transcripts and one for the exons. */
char gnbuf[64];
char query[512];
struct sqlConnection *tranConn;
struct sqlResult *tranRes;
char **tranRow;
struct sqlConnection *exonConn;
struct sqlResult *exonRes;
char **exonRow;
struct ensGene *eg;
struct hashEl *hel;

/* Bail out quickly if gene doesn't exist. */
if (!ensGeneExists(geneName))
    errAbort("%s - no such gene.", geneName);

/* Open database connections and make first query. */
tranConn = ensAllocConn();
exonConn = ensAllocConn();
sprintf(query, 
   "SELECT transcript.id, translation.start_exon, translation.end_exon,"
          "translation.seq_start, translation.seq_end "
   "FROM transcript,translation "
   "WHERE transcript.gene = '%s' "
     "AND transcript.translation = translation.id",
   geneName);
if ((tranRes = sqlGetResult(tranConn, query)) == NULL)
    warn("No transcripts for gene %s!", geneName);
else
    {
    /* Allocate gene structure and fill it in. */
    AllocVar(eg);
    eg->id = cloneString(geneName);
    eg->exonIdHash = newHash(6);
    while ((tranRow = sqlNextRow(tranRes)) != NULL)
	{
	struct ensTranscript *et;
	AllocVar(et);
	et->id = cloneString(tranRow[0]);
	et->exonList = newDlList();
	et->startSeq = sqlUnsigned(tranRow[3])-1;
	et->endSeq = sqlUnsigned(tranRow[4]);
	sprintf(query,
	   "SELECT exon_transcript.rank, exon.id,exon.contig, exon.seq_start, exon.seq_end, "
		  "exon.strand, exon.phase, exon.end_phase "
	   "FROM exon_transcript,exon "
	   "WHERE exon_transcript.transcript = '%s'"
	    "AND exon.id = exon_transcript.exon",
	   et->id);
	if ((exonRes = sqlGetResult(exonConn, query)) == NULL)
	    warn("No exons in transcript %s!", et->id);
	else
	    {
	    struct ensExon *ee;
	    int exonIx = 0;
	    int maxRank = 0;
	    struct ensExon *orderedExons[512];	/* Keep track in case they come out of order. */
	    memset(orderedExons, 0, sizeof(orderedExons));
	    while ((exonRow = sqlNextRow(exonRes)) != NULL)
		{
		char *exonId;
		int rank = sqlUnsigned(exonRow[0])-1;

		if (rank < 0 || rank >= ArraySize(orderedExons))
		    errAbort("%d exons in transcript %s???\n", rank+1,  et->id);
		if (rank > maxRank)
		    maxRank = rank;
		if (exonIx != rank)
		    warn("Hmm - exonIx %d, rank %d in %s", exonIx, rank, geneName);
		++exonIx;
		exonId = exonRow[1];
		if ((hel = hashLookup(eg->exonIdHash, exonId)) == NULL)
		    {
		    char bacName[32];
		    int contig;
		    char *contigName = exonRow[2];
		    struct hashEl *contigHel;
		    AllocVar(ee);
		    hel = hashAdd(eg->exonIdHash, exonId, ee);
		    ee->id = hel->name;

		    /* Deal with contig - get all contigs in clone actually. */
		    ensParseContig(contigName, bacName, &contig);
		    ensBacContigs(bacName);
		    contigHel = hashLookup(ensContigHash, contigName);
		    if (contigHel == NULL)
			errAbort("Couldn't find contig %s", contigName);
		    ee->contig  = contigHel->val;

		    ee->seqStart = sqlUnsigned(exonRow[3])-1;
		    ee->seqEnd = sqlUnsigned(exonRow[4]);
		    ee->orientation = sqlSigned(exonRow[5]);
		    ee->phase = sqlUnsigned(exonRow[6]);
		    ee->endPhase = sqlUnsigned(exonRow[7]);
		    slAddHead(&eg->exonList, ee);
		    }
		else
		    ee = hel->val;
		orderedExons[rank] = ee;
		}
	    sqlFreeResult(&exonRes);
	    for (exonIx = 0; exonIx <= maxRank; ++exonIx)
		{
		if ((ee = orderedExons[exonIx]) == NULL)
		    warn("Gaps in ranks of exons in transcript %s", et->id);
		else
		    {
		    dlAddValTail(et->exonList, ee);
		    }
		}
	    }
	/* Lookup up start and end exons. */
	if ((hel = hashLookup(eg->exonIdHash, tranRow[1])) == NULL)
	    warn("Start exon %s isn't in transcript %s", tranRow[1], et->id);
	else
	    et->startExon = hel->val;
	if ((hel = hashLookup(eg->exonIdHash, tranRow[2])) == NULL)
	    warn("End exon %s isn't in transcript %s", tranRow[2], et->id);
	else
	    et->endExon = hel->val;
	slAddHead(&eg->transcriptList, et);
	}
    slReverse(&eg->transcriptList);
    slReverse(&eg->exonList);
    sqlFreeResult(&tranRes);
    }
ensFreeConn(&exonConn);
ensFreeConn(&tranConn);
return eg;
}

void *ensGetGeneAndTranscript(char *transcriptName, struct ensGene **retGene, 
	struct ensTranscript **retTranscript)
/* Get gene and transcript given transcript name.  When done free the gene.
 * (The transcript is allocated as part of the gene). */
{
char query[256];
char geneId[64];
struct sqlConnection *conn = ensAllocConn();
struct ensGene *gene;
struct ensTranscript *trans;

sprintf(query,
	"SELECT transcript.gene FROM transcript WHERE id = '%s'", transcriptName);

if (!sqlQuickQuery(conn, query, geneId, sizeof(geneId)))
    errAbort("Couldn't find transcript %s", transcriptName);
ensFreeConn(&conn);

gene = ensGetGene(geneId);
for (trans = gene->transcriptList; trans != NULL; trans = trans->next)
    {
    if (sameWord(trans->id, transcriptName))
	break;
    }
if (trans == NULL)
    errAbort("Database inconsistency - no transcript %s in gene %s", geneId, transcriptName);
*retGene = gene;
*retTranscript = trans;
}

static struct ensGene *ensGenesFromNames(struct slName *nameList)
/* Get list of all named genes. */
{
struct ensGene *geneList = NULL, *gene;
struct slName *name;

for (name = nameList; name != NULL; name = name->next)
    {
    gene = ensGetGene(name->name);
    slAddHead(&geneList, gene);
    }
slReverse(&geneList);
return geneList;
}

struct ensGene *ensGenesInBac(char *bacName)
/* Get list of all genes in bac. */
{
struct slName *nameList = ensGeneNamesInBac(bacName);
struct ensGene *geneList = NULL;
geneList = ensGenesFromNames(nameList);
slFreeList(&nameList);
return geneList;
}

void ensTranscriptBounds(struct ensTranscript *trans, int *retStart, int *retEnd)
/* Find beginning and end of transcript in browser coordinates. */
{
struct dlNode *node;
int start = 0x1ffffff;	/* Really big number but still 32 bit positive. */
int end = -start;

/* Figure out bounds of transcript and draw a horizontal line across it. */
for (node = trans->exonList->head; node->next != NULL; node = node->next)
    {
    struct ensExon *exon = node->val;
    int eStart, eEnd;
    struct contigTree *contig = exon->contig;
    eStart = ensBrowserCoordinates(contig, exon->seqStart);
    eEnd = ensBrowserCoordinates(contig, exon->seqEnd);
    if (start > eStart)
	start = eStart;
    if (end < eEnd)
	end = eEnd;
    }
*retStart = start;
*retEnd = end;
}

void ensGeneBounds(struct ensGene *gene, int *retStart, int *retEnd)
/* Find beginning and end of gene in browser coordinates. */
{
struct ensTranscript *trans;
int start = 0x1ffffff;	/* Really big number but still 32 bit positive. */
int end = -start;

for (trans = gene->transcriptList; trans != NULL; trans = trans->next)
    {
    int tStart, tEnd;
    ensTranscriptBounds(trans, &tStart, &tEnd);
    if (tStart < start)
	start = tStart;
    if (tEnd > end)
	end = tEnd;
    }
*retStart = start;
*retEnd = end;
}

struct ensGene *ensGenesInBacRange(char *bacName, int start, int end)
/* Get list of genes in a section of a BAC clone.  The start/end are
 * in browser coordinates. */
{
/* Get all genes in bac.  Weed out ones not in range. */
struct ensGene *bacGeneList = ensGenesInBac(bacName);
struct ensGene *rangeGeneList = NULL;
struct ensGene *gene, *next;
for (gene = bacGeneList;  gene != NULL; gene = next)
    {
    int gStart,gEnd;
    next = gene->next;
    ensGeneBounds(gene, &gStart, &gEnd);
    if (rangeIntersection(start, end, gStart, gEnd) > 0)
	{
	slAddHead(&rangeGeneList, gene);
	}
    else
	ensFreeGene(&gene);
    }
slReverse(&rangeGeneList);
return rangeGeneList;
}

struct dnaSeq *allocDnaSeq(char *name, int size)
/* Return an allocated DNA sequence. */
{
struct dnaSeq *seq;
AllocVar(seq);
seq->name = cloneString(name);
seq->size = size;
seq->dna = needLargeMem(size);
return seq;
}

void ensUpcExons(char *clone, int start, int end, DNA *dna)
/* Upper-Case coding DNA corresponding to clone start-end */
{
struct ensGene *geneList = ensGenesInBacRange(clone, start, end);
struct ensGene *gene;
struct ensExon *exon;

for (gene = geneList; gene != NULL; gene = gene->next)
    {
    for (exon = gene->exonList; exon != NULL; exon = exon->next)
	{
	struct contigTree *contig = exon->contig;
	int eStart = ensBrowserCoordinates(contig, exon->seqStart);
	int eEnd = ensBrowserCoordinates(contig, exon->seqEnd);
	int e, s;
	int size;
	s = max(eStart,start);
	e = min(eEnd,end);
	size = e - s;
	toUpperN(dna + s - start, size);
	}
    }
ensFreeGeneList(&geneList);
}

struct dnaSeq *ensDnaInBacRange(char *clone, int start, int end, enum dnaCase dnaCase)
/* Get DNA for range of clone in browser coordinates, including NNNs between contigs. */
{
struct contigTree *bacTree = ensBacContigs(clone);
struct contigTree *contig;
int cStart, cEnd;
struct contigTree *firstContig = NULL, *lastContig = NULL;
boolean firstTime = TRUE;
int dnaOffset = 0;
DNA *dna;
struct dnaSeq *seq;
int size = end - start;
char nameBuf[64];
char nChar = ((dnaCase == dnaLower) ? 'n' : 'N');
struct sqlConnection *conn = ensAllocConn();

sprintf(nameBuf, "%s:%d-%d", clone, start, end);
seq = allocDnaSeq(nameBuf, size);
dna = seq->dna;
memset(dna, nChar, size);

for (contig = bacTree->children; contig != NULL; contig = contig->next)
    {
    int s, e;
    int overlap;

    cStart = contig->browserOffset;
    cEnd = cStart + contig->browserLength;
    s = max(start, cStart);
    e = min(end, cEnd);
    overlap = e - s;
    if (overlap > 0)
	{
	char query[128];
	struct sqlResult *sr;
	char **row;
	DNA *contigDna;
	int contigSize = contig->submitLength;

	sprintf(query, "SELECT sequence FROM dna WHERE contig = '%s'", contig->id);
	sr = sqlGetResult(conn, query);
	row = sqlNextRow(sr);
	if (row == NULL)
	    internalErr();
	contigDna = row[0];
	dnaFilterToN(contigDna, contigDna);	/* There are some 'm's in here... */
	if (contig->orientation < 0)
	    reverseComplement(contigDna, contigSize);
	if (dnaOffset+overlap > size)
	    internalErr();
	if (dnaCase == dnaUpper)
	    touppers(contigDna);
	else
	    tolowers(contigDna);
	memcpy(dna+dnaOffset, contigDna + s-cStart, overlap);
	dnaOffset += overlap + contigPad;
	sqlFreeResult(&sr);
	}
    }
ensFreeConn(&conn);
if (dnaCase == dnaMixed)
    ensUpcExons(clone, start, end, dna);
return seq;
}

struct dnaSeq *ensDnaInBac(char *clone, enum dnaCase dnaCase)
/* Get DNA for clone in browser coordinates, including NNNs between contigs. */
{
int length = ensBacBrowserLength(clone);
return ensDnaInBacRange(clone, 0, length, dnaCase);
}

void ensFreeTranscript(struct ensTranscript **pTrans)
/* Free up a single transcript. */
{
struct ensTranscript *trans;
if ((trans = *pTrans) != NULL)
    {
    freeMem(trans->id);
    freeDlList(&trans->exonList);
    freez(pTrans);
    }
}

void ensFreeTranscriptList(struct ensTranscript **pTransList)
/* Free up a list of transcripts. */
{
struct ensTranscript *trans, *next;
for (trans = *pTransList; trans != NULL; trans = next)
    {
    next = trans->next;
    ensFreeTranscript(&trans);
    }
*pTransList = NULL;
}

void ensFreeGene(struct ensGene **pGene)
/* Free up a single gene. */
{
struct ensGene *gene;
if ((gene = *pGene) != NULL)
    {
    freeMem(gene->id);
    ensFreeTranscriptList(&gene->transcriptList);
    freeHash(&gene->exonIdHash);
    slFreeList(&gene->exonList);
    freez(pGene);
    }
}

void ensFreeGeneList(struct ensGene **pGeneList)
/* Free up a list of genes. */
{
struct ensGene *gene, *next;
for (gene = *pGeneList; gene != NULL; gene = next)
    {
    next = gene->next;
    ensFreeGene(&gene);
    }
*pGeneList = NULL;
}


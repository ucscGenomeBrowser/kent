/* spTest - Test out sp library.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"

static char const rcsid[] = "$Id: spTest.c,v 1.2 2003/10/01 04:44:54 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spTest - Test out sp library.\n"
  "usage:\n"
  "   spTest database acc\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

/* spDb - access database generate from SwissProt by spToDb. 
 * Most of the routines take the primary SwissProt accession
 * as input, and will abort if this is not found in the database.
 * Use spIsPrimaryAcc to check for existence first if you want
 * to avoid this. The routines that return strings need them
 * free'd when you are done. */

typedef char SpAcc[9];	/* Enough space for accession and 0 terminator. */

boolean spIsPrimaryAcc(struct sqlConnection *conn, char *acc)
/* Return TRUE if this is a primary accession in database. */
{
char query[256]; 
SpAcc temp;
safef(query, sizeof(query), "select acc from displayId where acc = '%s'",
	acc);
return sqlQuickQuery(conn, query, temp, sizeof(temp)) != NULL;
}

char *spAccToId(struct sqlConnection *conn, char *acc)
/* Convert primary accession to SwissProt ID (which will
 * often look something like HXA1_HUMAN. */
{
char query[256];
safef(query, sizeof(query), "select val from displayId where acc = '%s'",
	acc);
return sqlNeedQuickString(conn, query);
}

char *spIdToAcc(struct sqlConnection *conn, char *id)
/* Convert SwissProt ID (things like HXA1_HUMAN) to
 * accession. Returns NULL if the conversion fails. 
 * (doen't abort). */
{
char query[256];
safef(query, sizeof(query), "select acc from displayId where val = '%s'",
	id);
return sqlQuickString(conn, query);
}

char *spLookupPrimaryAcc(struct sqlConnection *conn, 
	char *anyAcc) 	/* Primary or secondary accession. */
/* This will return the primary accession.  It's ok to pass in
 * either a primary or secondary accession. */
{
char query[256], *pri;
if (spIsPrimaryAcc(conn, anyAcc))
     return cloneString(anyAcc);
else
     {
     safef(query, sizeof(query), 
    	"select acc from otherAcc where val = '%s'", anyAcc);
     return sqlNeedQuickString(conn, query);
     }
}

char *spOrganelle(struct sqlConnection *conn, 
	char *acc)
/* Return text description of organelle.  FreeMem this when done. 
 * This may return NULL if it's not an organelle. */
{
char query[256];
safef(query, sizeof(query), 
	"select organelle.val from info,organelle "
	"where info.acc = '%s' and info.organelle = organelle.id"
	, acc);
return sqlQuickString(conn, query);
}

boolean spIsCurated(struct sqlConnection *conn, char *acc)
/* Return TRUE if it is a curated entry. */
{
char query[256];
safef(query, sizeof(query), 
	"select isCurated from info where acc = '%s'", acc);
return sqlNeedQuickNum(conn, query);
}

int spAaSize(struct sqlConnection *conn, char *acc)
/* Return number of amino acids. */
{
char query[256];
safef(query, sizeof(query), 
	"select aaSize from info where acc = '%s'", acc);
return sqlNeedQuickNum(conn, query);
}

int spMolWeight(struct sqlConnection *conn, char *acc)
/* Return molecular weight in daltons. */
{
char query[256];
safef(query, sizeof(query), 
	"select molWeight from info where acc = '%s'", acc);
return sqlNeedQuickNum(conn, query);
}

char *spCreateDate(struct sqlConnection *conn, char *acc)
/* Return creation date in YYYY-MM-DD format.  FreeMem 
 * this when done. */
{
char query[256];
safef(query, sizeof(query), 
	"select createDate from info where acc = '%s'", acc);
return sqlNeedQuickString(conn, query);
}

char *spSeqDate(struct sqlConnection *conn, char *acc)
/* Return sequence last update date in YYYY-MM-DD format.  FreeMem 
 * this when done. */
{
char query[256];
safef(query, sizeof(query), 
	"select seqDate from info where acc = '%s'", acc);
return sqlNeedQuickString(conn, query);
}

char *spAnnDate(struct sqlConnection *conn, char *acc)
/* Return annotation last update date in YYYY-MM-DD format.  FreeMem 
 * this when done. */
{
char query[256];
safef(query, sizeof(query), 
	"select annDate from info where acc = '%s'", acc);
return sqlNeedQuickString(conn, query);
}

char *spDescription(struct sqlConnection *conn, char *acc)
/* Return protein description.  FreeMem this when done. */
{
char query[256];
safef(query, sizeof(query), 
	"select val from description where acc = '%s'", acc);
return sqlNeedQuickString(conn, query);
}

struct slName *spGeneToAccs(struct sqlConnection *conn, 
	char *gene, int taxon)
/* Get list of accessions associated with this gene.  If
 * taxon is zero then this will return all accessions,  if
 * taxon is non-zero then it will restrict it to a single
 * organism with that taxon ID. */
{
char query[256];
if (taxon == 0)
    {
    safef(query, sizeof(query),
	"select acc from gene where val = '%s'", gene);
    }
else
    {
    safef(query, sizeof(query),
	"select gene.acc from gene,accToTaxon "
	"where gene.val = '%s' "
	"and gene.acc = accToTaxon.acc "
	"and accToTaxon.taxon = %d"
	, gene, taxon);
    }
return sqlQuickList(conn, query);
}

struct slName *spGenes(struct sqlConnection *conn, char *acc)
/* Return list of genes associated with accession */
{
char query[256];
safef(query, sizeof(query),
    "select val from gene where acc = '%s'", acc);
return sqlQuickList(conn, query);
}

struct slName *spBinomialNames(struct sqlConnection *conn, char *acc)
/* Return list of scientific names of organisms
 * associated with accessoin */
{
char query[256];
safef(query, sizeof(query),
     "select binomial from accToTaxon,taxon "
     "where accToTaxon.acc = '%s' and accToTaxon.taxon = taxon.id"
     , acc);
return sqlQuickList(conn, query);
}

int spTaxon(struct sqlConnection *conn, char *acc)
/* Return taxon of first organism associated with accession. */
{
char query[256];
safef(query, sizeof(query), 
	"select taxon from accToTaxon where acc = '%s'", acc);
return sqlNeedQuickNum(conn, query);
}

int spBinomialToTaxon(struct sqlConnection *conn, char *name)
/* Return taxon associated with binomial (Mus musculus) name. */
{
char query[256];
safef(query, sizeof(query),
    "select id from taxon where binomial = '%s'", name);
return sqlNeedQuickNum(conn, query);
}

int spCommonToTaxon(struct sqlConnection *conn, char *commonName)
/* Return taxon ID associated with common (mouse) name or
 * binomial name. */
{
char query[256];
int taxon;
safef(query, sizeof(query),
   "select taxon from commonName where val = '%s'", commonName);
taxon = sqlQuickNum(conn, query);
if (taxon == 0)
    taxon = spBinomialToTaxon(conn, commonName);
return taxon;
}

char *spTaxonToBinomial(struct sqlConnection *conn, int taxon)
/* Return Latin binomial name associated with taxon. */
{
char query[256];
if (taxon <= 0)
    errAbort("Bad taxon id %d\n", taxon);
safef(query, sizeof(query),
    "select binomial from taxon where id = %d", taxon);
return sqlNeedQuickString(conn, query);
}

char *spTaxonToCommon(struct sqlConnection *conn, int taxon)
/* Given NCBI common ID return first common name associated
 * with it if possible, otherwise return scientific name. */
{
char query[256];
char *ret;
if (taxon <= 0)
    errAbort("Bad taxon id %d\n", taxon);
safef(query, sizeof(query),
   "select val from commonName where taxon = %d", taxon);
ret = sqlQuickString(conn, query);
if (ret == NULL)
    ret = spTaxonToBinomial(conn, taxon);
return ret;
}

struct slName *spKeywords(struct sqlConnection *conn, char *acc)
/* Return list of keywords for accession. */
{
char query[256];
safef(query, sizeof(query),
    "select keyword.val from accToKeyword,keyword "
    "where accToKeyword.acc = '%s' "
    "and accToKeyword.keyword = keyword.id"
    , acc);
return sqlQuickList(conn, query);
}

struct slName *spKeywordSearch(struct sqlConnection *conn, char *keyword,
	int taxon)
/* Return list of accessions that use keyword.  If taxon is non-zero
 * then restrict accessions to given organism. */
{
char query[256];
int kwId;
safef(query, sizeof(query), 
	"select id from keyword where val = '%s'", keyword);
kwId = sqlQuickNum(conn, query);
if (kwId == 0)
    return NULL;

if (taxon == 0)
    {
    safef(query, sizeof(query),
	"select acc from accToKeyword where keyword = %d", kwId);
    }
else
    {
    safef(query, sizeof(query),
	"select accToKeyword.acc from accToKeyword,accToTaxon "
	"where accToKeyword.keyword = %d "
	"and accToKeyword.acc = accToTaxon.acc "
	"and accToTaxon.taxon = %d"
	, kwId, taxon);
    }
return sqlQuickList(conn, query);
}

struct slName *slComments(struct sqlConnection *conn,
	char *acc,	/* Primary accession. */
	char *type)	/* Comment type name, NULL for all comments. */
/* Get list of comments associated with accession. 
 * If optional type parameter is included it should be
 * something in the commentType table.  Some good types
 * are: DISEASE, FUNCTION, "SUBCELLULAR LOCATION" etc. */
{
char query[256];
if (type == NULL)
    {
    safef(query, sizeof(query),
	"select commentVal.val from comment,commentVal "
	"where comment.acc = '%s' "
	"and comment.commentVal = commentVal.id"
	, acc);
    }
else
    {
    int typeId;
    safef(query, sizeof(query),
        "select id from commentType where val = '%s'", type);
    typeId = sqlNeedQuickNum(conn, query);
    safef(query, sizeof(query),
	"select commentVal.val from comment,commentVal "
	"where comment.acc = '%s' "
	"and comment.commentType = %d "
	"and comment.commentVal = commentVal.id "
	, acc, typeId);
    }
return sqlQuickList(conn, query);
}

struct slName *slCommentTypes(struct sqlConnection *conn)
/* Get list of comment types in database. */
{
char query[256];
safef(query, sizeof(query), "select val from commentType");
return sqlQuickList(conn, query);
}

struct slName *spExtDbAcc1List(struct sqlConnection *conn, char *acc,
	char *db)
/* Get list of accessions from database associated with this. */
{
char query[256];
safef(query, sizeof(query), 
    "select extDbRef.extAcc1 from extDbRef,extDb "
    "where extDbRef.acc = '%s' "
    "and extDbRef.extDb = extDb.id "
    "and extDb.val = '%s'"
    , acc, db);
return sqlQuickList(conn, query);
}

struct slName *spEmblAccs(struct sqlConnection *conn, char *acc)
/* Get list of EMBL/Genbank mRNAaccessions associated with this. */
{
return spExtDbAcc1List(conn, acc, "EMBL");
}

struct slName *spPdbAccs(struct sqlConnection *conn, char *acc)
/* Get list of PDB associations associated with this. */
{
return spExtDbAcc1List(conn, acc, "PDB");
}

char *spAccFromEmbl(struct sqlConnection *conn, char *acc)
/* Get SwissProt accession associated with EMBL mRNA. */
{
char query[256];
int emblId;
safef(query, sizeof(query), "select id from extDb where val = 'EMBL'");
emblId = sqlNeedQuickNum(conn, query);
safef(query, sizeof(query),
    "select acc from extDbRef where extAcc1 = '%s' and extDb = %d"
    , acc, emblId);
return sqlQuickString(conn, query);
}

struct spFeature
/* A swissProt feature. */
    {
    struct spFeature *next;
    int start;	/* Start coordinate (zero-based) */
    int end;	/* End coordinate (non-inclusive) */
    int featureClass; /* ID of featureClass */
    int featureType;  /* ID of featureType */
    };

struct spFeature *spFeatures(struct sqlConnection *conn, char *acc,
	int classId, 	/* Feature class ID, 0 for all */
	int typeId)	/* Feature type ID, 0 for all */
/* Get feature list.  slFreeList this when done. */
{
struct dyString *dy = dyStringNew(0);
struct spFeature *list = NULL, *el;
char query[256], **row;
struct sqlResult *sr;

dyStringAppend(dy, 
	"select start,end,featureClass,featureType from feature ");
dyStringPrintf(dy, 
        "where acc = '%s'", acc);
if (classId != 0)
    dyStringPrintf(dy, " and featureClass=%d", classId);
if (typeId != 0)
    dyStringPrintf(dy, " and featureType=%d", typeId);
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(el);
    el->start = sqlUnsigned(row[0]);
    el->end = sqlUnsigned(row[1]);
    el->featureClass = sqlUnsigned(row[2]);
    el->featureType = sqlUnsigned(row[3]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
dyStringFree(&dy);
slReverse(&list);
return list;
}

char *spFeatureTypeName(struct sqlConnection *conn, int featureType)
/* Return name associated with featureType */
{
if (featureType == 0)
   return cloneString("n/a");
else
    {
    char query[256];
    safef(query, sizeof(query),
	    "select val from featureType where id=%d", featureType);
    return sqlNeedQuickString(conn, query);
    }
}

int spFeatureTypeId(struct sqlConnection *conn, char *name)
/* Return feature type id associated with given name. */
{
if (sameString(name, "n/a"))
    return 0;
else
    {
    char query[256];
    safef(query, sizeof(query),
	    "select id from featureType where val='%s'", name);
    return sqlNeedQuickNum(conn, query);
    }
}

char *spFeatureClassName(struct sqlConnection *conn, int featureClass)
/* Return name associated with featureClass. */
{
char query[256];
safef(query, sizeof(query),
	"select val from featureClass where id=%d", featureClass);
return sqlNeedQuickString(conn, query);
}

int spFeatureClassId(struct sqlConnection *conn, char *name)
/* Return feature class id associated with given name. */
{
char query[256];
safef(query, sizeof(query),
	"select id from featureClass where val='%s'", name);
return sqlNeedQuickNum(conn, query);
}



void printPartialList(char *title1, char *title2, 
	struct slName *list, int maxCount)
/* Print out up to maxCount of list. */
{
struct slName *n;
int i;
printf("%s %s (%d):", title1, title2, slCount(list));
for (n=list, i=0; n != NULL && i<maxCount; n = n->next, ++i)
    printf(" %s", n->name);
if (n != NULL)
    printf("...");
printf("\n");
}

void printFeat(struct sqlConnection *conn, struct spFeature *feat)
/* Print out one feature looking up IDs. */
{
char *className = spFeatureClassName(conn, feat->featureClass);
char *typeName = spFeatureTypeName(conn, feat->featureType);
printf("%s\t%d\t%d\t%s\n", className, feat->start, feat->end, 
    typeName);
freez(&className);
freez(&typeName);
}

void spTest(char *database, char *someAcc)
/* spTest - Test out sp library.. */
{
struct sqlConnection *conn = sqlConnect(database);
char *acc, *id, *binomial, *common;
struct slName *geneList, *gene, *accList, *n, *list;
struct slName *nameList, *name, *keyList, *key, *typeList, *type;
struct spFeature *featList, *feat;
boolean ok;
int taxon;
int classId = 0, typeId = 0;

printf("input: %s\n", someAcc);
acc = spLookupPrimaryAcc(conn, someAcc);
printf("primary accession: %s\n", acc);
id = spAccToId(conn, acc);
printf("SwissProt id: %s\n", id);
printf("acc from id: %s\n", spIdToAcc(conn, id));
printf("organelle: %s\n", spOrganelle(conn, acc));
printf("isCurated: %d\n", spIsCurated(conn, acc));
printf("aaSize: %d\n", spAaSize(conn,acc));
printf("molWeight: %d\n", spMolWeight(conn,acc));
printf("createDate: %s\n", spCreateDate(conn,acc));
printf("seqDate: %s\n", spSeqDate(conn,acc));
printf("annDate: %s\n", spAnnDate(conn,acc));
printf("description: %s\n", spDescription(conn, acc));
taxon = spTaxon(conn, acc);
printf("taxon: %d\n", taxon);
binomial = spTaxonToBinomial(conn, taxon);
printf("first scientific name: %s\n", binomial);
common = spTaxonToCommon(conn, taxon);
printf("first common name: %s\n", common);
printf("taxon from sci: %d\n", spBinomialToTaxon(conn, binomial));
printf("taxon from common: %d\n", spCommonToTaxon(conn, common));
printf("all scientific names:");
nameList = spBinomialNames(conn, acc);
for (name = nameList; name != NULL; name = name->next)
    printf(" %s,", name->name);
printf("\n");
printf("gene(s):");
geneList = spGenes(conn,acc);
for (gene=geneList; gene != NULL; gene = gene->next)
    printf(" %s,", gene->name);
printf("\n");
for (gene=geneList; gene != NULL; gene = gene->next)
    {
    accList = spGeneToAccs(conn, gene->name, 0);
    printf(" any %s:", gene->name);
    for (n = accList; n != NULL; n = n->next)
        printf(" %s,", n->name);
    printf("\n");
    slFreeList(&accList);
    printf(" %s %s:", common, gene->name);
    accList = spGeneToAccs(conn, gene->name, taxon);
    for (n = accList; n != NULL; n = n->next)
        printf(" %s,", n->name);
    printf("\n");
    slFreeList(&accList);
    }
slFreeList(&geneList);
printf("keyword(s):");
keyList = spKeywords(conn, acc);
for (key = keyList; key != NULL; key = key->next)
    printf(" %s,", key->name);
printf("\n");
for (key = keyList; key != NULL; key = key->next)
    {
    accList = spKeywordSearch(conn, key->name, taxon);
    printPartialList(common, key->name, accList, 4);
    slFreeList(&accList);
    break;	/* This is a little slow, once is enough. */
    }
for (key = keyList; key != NULL; key = key->next)
    {
    accList = spKeywordSearch(conn, key->name, 0);
    printPartialList("all", key->name, accList, 4);
    slFreeList(&accList);
    }
slFreeList(&keyList);

printf("All comments:\n");
list = slComments(conn, acc, NULL);
for (n = list; n != NULL; n = n->next)
    printf(" %s\n", n->name);
slFreeList(&list);

typeList = slCommentTypes(conn);
for (type = typeList; type != NULL; type = type->next)
    {
    list = slComments(conn, acc, type->name);
    if (list != NULL)
	{
	printf("%s comments:\n", type->name);
	for (n = list; n != NULL; n = n->next)
	    printf(" %s\n", n->name);
	slFreeList(&list);
	}
    }
slFreeList(&typeList);

list = spEmblAccs(conn, acc);
printf("GenBank/EMBL:");
for (n = list; n != NULL; n = n->next)
    printf(" %s,", n->name);
printf("\n");
if (list != NULL)
    printf("acc from %s: %s\n", 
    	list->name, spAccFromEmbl(conn, list->name));
slFreeList(&list);

list = spPdbAccs(conn, acc);
printf("PDB:");
for (n = list; n != NULL; n = n->next)
    printf(" %s,", n->name);
printf("\n");

featList = spFeatures(conn, acc, 0, 0);
printf("All features:\n");
for (feat = featList; feat != NULL; feat = feat->next)
    {
    printFeat(conn, feat);
    classId = feat->featureClass;
    typeId = feat->featureType;
    }
slFreeList(&featList);
if (classId != 0 && typeId != 0)
    {
    printf("%s class features:\n", spFeatureClassName(conn, classId));
    featList = spFeatures(conn, acc, classId, 0);
    for (feat = featList; feat != NULL; feat = feat->next)
	printFeat(conn, feat);
    slFreeList(&featList);
    printf("%s type features:\n", spFeatureTypeName(conn, typeId));
    featList = spFeatures(conn, acc, 0, typeId);
    for (feat = featList; feat != NULL; feat = feat->next)
	printFeat(conn, feat);
    slFreeList(&featList);
    printf("same class & type features:\n");
    featList = spFeatures(conn, acc, classId, typeId);
    for (feat = featList; feat != NULL; feat = feat->next)
	printFeat(conn, feat);
    slFreeList(&featList);
    printf("class loop: %d->%s->%d\n", classId, 
    	spFeatureClassName(conn, classId),
	spFeatureClassId(conn, spFeatureClassName(conn, classId)));
    printf("type loop: %d->%s->%d\n", typeId, 
    	spFeatureTypeName(conn, typeId),
	spFeatureTypeId(conn, spFeatureTypeName(conn, typeId)));
    }

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
spTest(argv[1], argv[2]);
return 0;
}

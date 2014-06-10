/* spTest - Test out sp library.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "spDb.h"


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
struct spCitation *citeList, *cite;
char *ret = NULL;
int taxon;
int classId = 0, typeId = 0, refId = 0;

printf("input: %s\n", someAcc);
acc = spLookupPrimaryAcc(conn, someAcc);
printf("primary accession: %s\n", acc);
id = spAccToId(conn, acc);
printf("SwissProt id: %s\n", id);
printf("acc from id: %s\n", spIdToAcc(conn, id));
ret = spOrganelle(conn, acc);
printf("organelle: %s\n", (ret == NULL) ? "(null)" : ret);
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
    break;	/* This is a little slow, once is enough. */
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

citeList = spCitations(conn, acc);
for (cite = citeList; cite != NULL; cite = cite->next)
    {
    refId = cite->reference;
    printf("title: %s\n", spRefTitle(conn, refId));
    printf("authors:");
    list = spRefAuthors(conn, refId);
    for (n = list; n != NULL; n = n->next)
        printf(" %s, ", n->name);
    printf("\n");
    slFreeList(&list);
    printf("location: %s\n", spRefCite(conn, refId));
    printf("pubMed: %s\n", spRefPubMed(conn, refId));
    }
if (refId != 0)
    {
    printf("other accs associated with last reference:\n\t");
    list = spRefToAccs(conn, refId);
    printPartialList("", "", list, 6);
    slFreeList(&list);
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

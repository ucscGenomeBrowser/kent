/* spTest - Test out sp library.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "hdb.h"
#include "spDb.h"

static char const rcsid[] = "$Id: spDb.c,v 1.19 2008/09/03 19:19:27 markd Exp $";

boolean spIsPrimaryAcc(struct sqlConnection *conn, char *acc)
/* Return TRUE if this is a primary accession in database. */
{
char query[256]; 
SpAcc temp;
safef(query, sizeof(query), "select acc from displayId where acc = '%s'",
	acc);
return sqlQuickQuery(conn, query, temp, sizeof(temp)) != NULL;
}

char *spFindAcc(struct sqlConnection *conn, char *id)
/* Return primary accession given either display ID,
 * primary accession, or secondary accession.  Return
 * NULL if not found. */
{
char *acc;
if (spIsPrimaryAcc(conn, id))
    return cloneString(id);
acc = spIdToAcc(conn, id);
if (acc != NULL)
    return acc;
else
    {
    char query[256];
    safef(query, sizeof(query), 
    	"select acc from otherAcc where val = '%s'", id);
    return sqlQuickString(conn, query);
    }
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

char *spAnyAccToId(struct sqlConnection *conn, char *acc)
/* Convert primary accession to SwissProt ID (which will
 * often look something like HXA1_HUMAN. 
   Please note that compared to spAccToID(), this function
   calls sqlQuickString() instead of sqlNeedQuickString(),
   so if nothing is found, it will return NULL instead of abort.
 */
{
char query[256];
safef(query, sizeof(query), "select val from displayId where acc = '%s'",
	spFindAcc(conn, acc));
return sqlQuickString(conn, query);
}

char *spIdToAcc(struct sqlConnection *conn, char *id)
/* Convert SwissProt ID (things like HXA1_HUMAN) to
 * accession. Returns NULL if the conversion fails. 
 * (doesn't abort). */
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
char query[256];
if (spIsPrimaryAcc(conn, anyAcc))
     return cloneString(anyAcc);
else
     {
     safef(query, sizeof(query), 
    	"select acc from otherAcc where val = '%s'", anyAcc);
     return sqlNeedQuickString(conn, query);
     }
}

char *spDescription(struct sqlConnection *conn, char *acc)
/* Return protein description.  FreeMem this when done. */
{
char query[256];
safef(query, sizeof(query), 
	"select val from description where acc = '%s'", acc);
return sqlNeedQuickString(conn, query);
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

char *spOrganelle(struct sqlConnection *conn, char *acc)
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

struct slName *spProteinEvidence(struct sqlConnection *conn, char *acc)
/* Get list of evidence that protein exists for accession.  There will be at least one. */
{
char query[256];
safef(query, sizeof(query), 
	"select proteinEvidenceType.val from proteinEvidence,proteinEvidenceType "
	"where proteinEvidence.acc = '%s' "
	"and proteinEvidence.proteinEvidenceType = proteinEvidenceType.id"
	, acc);
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

struct slName *spTaxons(struct sqlConnection *conn, char *acc)
/* Return list of taxons associated with accession */
{
char query[256];
safef(query, sizeof(query),
    "select taxon from accToTaxon where acc = '%s'", acc);
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

char *spCommentType(struct sqlConnection *conn, int typeId)
/* Look up text associated with typeId. freeMem result when done. */
{
char query[256];
safef(query, sizeof(query), "select val from commentType where id=%d", typeId);
return sqlQuickString(conn, query);
}

char *spCommentVal(struct sqlConnection *conn, int valId)
/* Look up text associated with valId. freeMem result when done. */
{
char query[256];
safef(query, sizeof(query), "select val from commentVal where id=%d", valId);
return sqlQuickString(conn, query);
}


struct slName *spExtDbAcc1List(struct sqlConnection *conn, char *acc,
	char *db)
/* Get list of accessions from external database associated with this
 * swissProt entity.  The db parameter can be anything in the
 * extDb table.  Some common external databases are 'EMBL' 'PDB' 'Pfam'
 * 'Interpro'. */
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

struct spFeature *spFeatures(struct sqlConnection *conn, char *acc,
	int classId, 	/* Feature class ID, 0 for all */
	int typeId)	/* Feature type ID, 0 for all */
/* Get feature list.  slFreeList this when done. */
{
struct dyString *dy = dyStringNew(0);
struct spFeature *list = NULL, *el;
char **row;
struct sqlResult *sr;

dyStringAppend(dy, 
	"select start,end,featureClass,featureType,softEndBits from feature ");
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
    el->softEndBits = sqlUnsigned(row[4]);
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

struct spCitation *spCitations(struct sqlConnection *conn, char *acc)
/* Get list of citations to references associated with this
 * accession.  You can slFreeList this when done. */
{
struct sqlResult *sr;
char query[256], **row;
struct spCitation *list = NULL, *el;
safef(query, sizeof(query),
    "select id,acc,reference,rp from citation where acc='%s'", acc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(el);
    el->id = sqlUnsigned(row[0]);
    memcpy(el->acc, row[1], sizeof(el->acc));
    el->reference = sqlUnsigned(row[2]);
    el->rp = sqlUnsigned(row[3]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

char *spRefTitle(struct sqlConnection *conn, int refId)
/* Get title of reference. This can be NULL legitimately. */
{
char query[256];
safef(query, sizeof(query), 
	"select title from reference where id=%d", refId);
return sqlQuickString(conn, query);
}

struct slName *spRefAuthors(struct sqlConnection *conn, int refId)
/* Get list of authors associated with reference. */
{
char query[256];
safef(query, sizeof(query),
    "select author.val from referenceAuthors,author "
    "where referenceAuthors.reference = %d "
    "and referenceAuthors.author = author.id"
    , refId);
return sqlQuickList(conn, query);
}

char *spRefCite(struct sqlConnection *conn, int refId)
/* Get journal/page/etc of reference. */
{
char query[256];
safef(query, sizeof(query), 
	"select cite from reference where id=%d", refId);
return sqlNeedQuickString(conn, query);
}

char *spRefPubMed(struct sqlConnection *conn, int refId)
/* Get PubMed id.  May be NULL legitimately. */
{
char query[256];
safef(query, sizeof(query), 
	"select pubMed from reference where id=%d", refId);
return sqlQuickString(conn, query);
}

struct slName *spRefToAccs(struct sqlConnection *conn, int refId)
/* Get list of accessions associated with reference. */
{
char query[256];
safef(query, sizeof(query),
    "select acc from citation where reference = %d", refId);
return sqlQuickList(conn, query);
}

char *newSpDisplayId(char *oldSpDisplayId)
/* Convert from old Swiss-Prot display ID to new display ID */
{
static struct sqlConnection *conn=NULL;
char condStr[255];
char *newSpDisplayId;

if (conn==NULL)
    {
    conn = sqlConnect(PROTEOME_DB_NAME);
    if (conn == NULL) return NULL;
    }
    
safef(condStr, sizeof(condStr), "oldDisplayId='%s'", oldSpDisplayId);
newSpDisplayId = sqlGetField(PROTEOME_DB_NAME, "spOldNew", "newDisplayId", condStr);
    
return(newSpDisplayId);
}		   

char *oldSpDisplayId(char *newSpDisplayId)
/* Convert from new Swiss-Prot display ID to old display ID */
{
static struct sqlConnection *conn=NULL;
char condStr[255];
char *oldSpDisplayId;

if (conn==NULL)
    {
    conn = sqlConnect(PROTEOME_DB_NAME);
    if (conn == NULL) return NULL;
    }

safef(condStr, sizeof(condStr), "newDisplayId='%s'", newSpDisplayId);
oldSpDisplayId = sqlGetField(PROTEOME_DB_NAME, "spOldNew", "oldDisplayId", condStr);
    
return(oldSpDisplayId);
}	

char *uniProtFindPrimAcc(char *id)
/* Return primary accession given an alias. */
/* The alias could be an accession number, display ID, old display ID, etc. 
 * NULL if not found. */
{
static struct sqlConnection *conn=NULL;
char *acc;
char query[256];

if (conn==NULL)
    {
    conn = sqlMayConnect(PROTEOME_DB_NAME);
    if (conn == NULL) return NULL;
    }

safef(query, sizeof(query), "select acc from uniProtAlias where alias = '%s'", id);
    	
acc = sqlQuickString(conn, query);
return(acc);
}

char *uniProtFindPrimAccFromGene(char *gene, char *db)
/* Return primary accession given gene name.
 * NULL if not found. */
{
static struct sqlConnection *conn=NULL;
char *acc = NULL;
char query[256];
char *pdb = cloneString(UNIPROT_DB_NAME);
char *geneTable = "gene";

if (conn==NULL)
    {
    conn = sqlMayConnect(pdb);
    if (conn == NULL) return NULL;
    }
if (geneTable != NULL && sqlTableExists(conn,geneTable)) 
    {
//    safef(query, sizeof(query), "select acc from %s where val = '%s'", geneTable, gene);
    safef(query, sizeof(query), "select g.acc from %s g , accToTaxon a, taxon t where val = '%s' and g.acc = a.acc and id = a.taxon and binomial = '%s' ",geneTable, gene, hGenome(db));
    acc = sqlQuickString(conn, query);
    }
return(acc);
}

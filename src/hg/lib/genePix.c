/* genePix.c  - Interface to genePix database. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "genePix.h"

static char const rcsid[] = "$Id: genePix.c,v 1.4 2008/09/17 18:10:13 kent Exp $";

static char *cloneOrNull(char *s)
/* Return copy of string, or NULL if it is empty */
{
if (s == NULL || s[0] == 0)
    return NULL;
return cloneString(s);
}

static char *somePath(struct sqlConnection *conn, int id, char *locationField)
/* Fill in path based on given location field */
{
char query[256], path[PATH_LEN];
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), 
	"select fileLocation.name,imageFile.fileName from image,imageFile,fileLocation"
	" where image.id = %d and image.imageFile = imageFile.id "
	" and fileLocation.id=imageFile.%s", id, locationField);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("No image of id %d", id);
safef(path, PATH_LEN, "%s/%s", row[0], row[1]);
sqlFreeResult(&sr);
return cloneString(path);
}

char *genePixFullSizePath(struct sqlConnection *conn, int id)
/* Fill in path for full image genePix of given id.  FreeMem
 * this when done */
{
return somePath(conn, id, "fullLocation");
}

char *genePixScreenSizePath(struct sqlConnection *conn, int id)
/* Fill in path for screen-sized (~700x700) image genePix of given id. 
 * FreeMem when done. */
{
return somePath(conn, id, "screenLocation");
}

char *genePixThumbSizePath(struct sqlConnection *conn, int id)
/* Fill in path for thumbnail image (~200x200) genePix of given id. 
 * FreeMem when done. */
{
return somePath(conn, id, "thumbLocation");
}

char *genePixOrganism(struct sqlConnection *conn, int id)
/* Return binomial scientific name of organism associated with given image. 
 * FreeMem this when done. */
{
char query[256], buf[256];
safef(query, sizeof(query),
	"select uniProt.taxon.binomial from image,uniProt.taxon"
         " where image.id = %d and image.taxon = uniProt.taxon.id",
	 id);
return cloneOrNull(sqlQuickQuery(conn, query, buf, sizeof(buf)));
}

char *genePixStage(struct sqlConnection *conn, int id, boolean doLong)
/* Return string describing growth stage of organism.  The description
 * will be 5 or 6 characters wide if doLong is false, and about
 * 20 characters wide if it is true. FreeMem this when done. */
{
char query[256], buf[256];
boolean isEmbryo;
double daysPassed;
char *startMarker;
struct sqlResult *sr;
char **row;
safef(query, sizeof(query),
    "select isEmbryo,age from image where id = %d", id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("No image of id %d", id);
isEmbryo = differentString(row[0], "0");
daysPassed = atof(row[1]);
if (doLong)
    {
    if (isEmbryo)
	startMarker = "fertilization";
    else
	startMarker = "birth";
    safef(buf, sizeof(buf), "%3.1f days after %s", daysPassed, startMarker);
    }
else
    {
    char c;
    if (isEmbryo)
        c = 'e';
    else
        c = 'p';
    safef(buf, sizeof(buf), "%c%3.1f", c, daysPassed);
    }
sqlFreeResult(&sr);
return cloneString(buf);
}

struct slName *genePixGeneName(struct sqlConnection *conn, int id)
/* Return list of gene names  - HUGO if possible, RefSeq/GenBank, etc if not. 
 * FreeMem this when done. */
{
struct slName *list = NULL, *el = NULL;
struct sqlResult *sr;
char **row;
char query[256], buf[256];
safef(query, sizeof(query),
      "select gene.name,gene.locusLink,gene.refSeq,gene.genbank,gene.uniProt"
      " from imageProbe,probe,gene"
      " where imageProbe.image = %d"
      " and imageProbe.probe = probe.id"
      " and probe.gene = gene.id", id);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    char *locusLink = row[1];
    char *refSeq = row[2];
    char *genbank = row[3];
    char *uniProt = row[4];
    if (name[0] != 0)
        el = slNameNew(name);
    else if (refSeq[0] != 0)
        el = slNameNew(refSeq);
    else if (genbank[0] != 0)
        el = slNameNew(genbank);
    else if (uniProt[0] != 0)
        el = slNameNew(uniProt);
    else if (locusLink[0] != 0)
	{
	safef(buf, sizeof(buf), "Entrez Gene %s", locusLink);
        el = slNameNew(buf);
	}
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

static struct slName *extNamesForId(struct sqlConnection *conn, int id, 
	char *field)
/* Return list of external identifiers of type described in field
 * associated with image. */
{
struct slName *list = NULL, *el;
struct sqlResult *sr;
char **row;
char query[256];
safef(query, sizeof(query),
      "select gene.%s from imageProbe,probe,gene"
      " where imageProbe.image = %d"
      " and imageProbe.probe = probe.id"
      " and probe.gene = gene.id", field, id);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slNameNew(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

struct slName *genePixRefSeq(struct sqlConnection *conn, int id)
/* Return RefSeq accession or n/a if none available. 
 * FreeMem this when done. */
{
return extNamesForId(conn, id, "refSeq");
}

struct slName *genePixGenbank(struct sqlConnection *conn, int id)
/* Return Genbank accession or n/a if none available. 
 * FreeMem this when done. */
{
return extNamesForId(conn, id, "genbank");
}

struct slName *genePixUniProt(struct sqlConnection *conn, int id)
/* Return Genbank accession or n/a if none available. 
 * FreeMem this when done. */
{
return extNamesForId(conn, id, "uniProt");
}

char *genePixSubmitId(struct sqlConnection *conn, int id)
/* Return submitId  for image.  (Internal id for data set)
 * FreeMem this when done. */
{
char query[256];
safef(query, sizeof(query),
    "select imageFile.submitId from image,imageFile "
    "where image.id = %d and image.imageFile = imageFile.id", id);
return sqlQuickString(conn, query);
}

static char *indirectString(struct sqlConnection *conn, int id, char *table, char *valField)
/* Return value on table that is connected via id to image. */
{
char query[256], buf[512];
safef(query, sizeof(query),
	"select %s.%s from image,%s "
	"where image.id=%d and image.%s = %s.id",
	table, valField, table, id, table, table);
return cloneOrNull(sqlQuickQuery(conn, query, buf, sizeof(buf)));
}

char *genePixBodyPart(struct sqlConnection *conn, int id)
/* Return body part if any.  FreeMem this when done. */
{
return indirectString(conn, id, "bodyPart", "name");
}

char *genePixSliceType(struct sqlConnection *conn, int id)
/* Return slice type if any.  FreeMem this when done. */
{
return indirectString(conn, id, "sliceType", "name");
}

char *genePixTreatment(struct sqlConnection *conn, int id)
/* Return treatment if any.  FreeMem this when done.*/
{
return indirectString(conn, id, "treatment", "conditions");
}

static char *stringFieldInSubmissionSet(struct sqlConnection *conn, int id, 
	char *field)
/* Return some string field in submissionSet table when you have image id. */
{
char query[256];
safef(query, sizeof(query),
     "select submissionSet.%s from image,imageFile,submissionSet"
     " where image.id = %d"
     " and image.imageFile = imageFile.id "
     " and imageFile.submissionSet = submissionSet.id", field, id);
return sqlQuickString(conn, query);
}


char *genePixContributors(struct sqlConnection *conn, int id)
/* Return comma-separated list of contributors in format Kent W.H, Wu F.Y. 
 * FreeMem this when done. */
{
return stringFieldInSubmissionSet(conn, id, "contributors");
}

char *genePixPublication(struct sqlConnection *conn, int id)
/* Return name of publication associated with image if any.  
 * FreeMem this when done. */
{
return stringFieldInSubmissionSet(conn, id, "publication");
}

char *genePixPubUrl(struct sqlConnection *conn, int id)
/* Return url of publication associated with image if any.
 * FreeMem this when done. */
{
return stringFieldInSubmissionSet(conn, id, "pubUrl");
}

char *genePixSetUrl(struct sqlConnection *conn, int id)
/* Return contributor url associated with image set if any. 
 * FreeMem this when done. */
{
return stringFieldInSubmissionSet(conn, id, "setUrl");
}

char *genePixItemUrl(struct sqlConnection *conn, int id)
/* Return contributor url associated with this image. 
 * Substitute in submitId for %s before using.  May be null.
 * FreeMem when done. */
{
return stringFieldInSubmissionSet(conn, id, "itemUrl");
}

static void appendMatchHow(struct dyString *dy, char *pattern,
	enum genePixSearchType how)
/* Append clause to do search on pattern according to how on dy */
{
switch (how)
    {
    case gpsExact:
        dyStringPrintf(dy, " = \"%s\"", pattern);
	break;
    case gpsPrefix:
        dyStringPrintf(dy, " like \"%s%%\"", pattern);
	break;
    case gpsLike:
        dyStringPrintf(dy, " like \"%s\"", pattern);
	break;
    default:
        internalErr();
	break;
    }
}

struct slInt *genePixSelectNamed(struct sqlConnection *conn,
	char *name, enum genePixSearchType how)
/* Return ids of images that have probes involving gene that match name. */
{
struct hash *uniqHash = newHash(0);
struct slName *geneList = NULL, *geneEl;
struct slInt *imageList = NULL, *imageEl;
struct dyString *dy = dyStringNew(0);
char **row;
struct sqlResult *sr;

dyStringPrintf(dy, "select id from gene where name ");
appendMatchHow(dy, name, how);
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    geneEl = slNameNew(row[0]);
    slAddHead(&geneList, geneEl);
    }
sqlFreeResult(&sr);

dyStringClear(dy);
dyStringPrintf(dy, "select gene from geneSynonym where name ");
appendMatchHow(dy, name, how);
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    geneEl = slNameNew(row[0]);
    slAddHead(&geneList, geneEl);
    }
slReverse(&geneList);
sqlFreeResult(&sr);

for (geneEl = geneList; geneEl != NULL; geneEl = geneEl->next)
    {
    dyStringClear(dy);
    dyStringAppend(dy, "select imageProbe.image from probe,imageProbe");
    dyStringPrintf(dy, " where probe.gene = %s ", geneEl->name);
    dyStringAppend(dy, " and probe.id = imageProbe.probe");
    sr = sqlGetResult(conn, dy->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *ids = row[0];
	if (!hashLookup(uniqHash, ids))
	    {
	    hashAdd(uniqHash, ids, NULL);
	    imageEl = slIntNew(sqlUnsigned(ids));
	    slAddHead(&imageList, imageEl);
	    }
	}
    sqlFreeResult(&sr);
    }
dyStringFree(&dy);
hashFree(&uniqHash);
slReverse(&imageList);
return imageList;
}

struct slInt *matchingExtId(struct sqlConnection *conn, char *acc, char *field)
/* Return ids of images that have probes involving external id. */
{
struct slInt *imageList = NULL, *imageEl;
struct sqlResult *sr;
char query[256], **row;

if (acc[0] == 0)
    return NULL;
safef(query, sizeof(query),
    "select imageProbe.image from gene,probe,imageProbe"
    " where gene.%s = '%s'"
    " and gene.id = probe.gene"
    " and probe.id = imageProbe.probe",
    field, acc);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    imageEl = slIntNew(sqlUnsigned(row[0]));
    slAddHead(&imageList, imageEl);
    }
sqlFreeResult(&sr);
slReverse(&imageList);
return imageList;
}

struct slInt *genePixSelectRefSeq(struct sqlConnection *conn, char *acc)
/* Return ids of images that have probes involving refSeq mRNA seq. */
{
return matchingExtId(conn, acc, "refSeq");
}

struct slInt *genePixSelectLocusLink(struct sqlConnection *conn, char *id)
/* Return ids of images that have probes involving locusLink (entrez gene)
 * id. */
{
return matchingExtId(conn, id, "locusLink");
}

struct slInt *genePixSelectGenbank(struct sqlConnection *conn, char *acc)
/* Return ids of images that have probes involving genbank accessioned 
 * sequence */
{
return matchingExtId(conn, acc, "genbank");
}

struct slInt *genePixSelectUniProt(struct sqlConnection *conn, char *acc)
/* Return ids of images that have probes involving UniProt accessioned
 * sequence. */
{
return matchingExtId(conn, acc, "uniProt");
}

struct slInt *genePixSelectMulti(
        /* Note most parameters accept NULL or 0 to mean ignore parameter. */
	struct sqlConnection *conn, 
	struct slInt *init,	/* Restrict results to those with id in list. */
	int taxon,		/* Taxon of species to restrict it to. */
	struct slName *contributors,  /* List of contributors to restrict it to. */
	enum genePixSearchType how,   /* How to match contributors */
	boolean startIsEmbryo, double startAge,
	boolean endIsEmbryo, double endAge)
/* Return selected genes according to a number of criteria. */
{
struct slInt *imageList = NULL, *el;
struct sqlResult *sr;
struct dyString *dy = dyStringNew(0);
char **row;
boolean needAnd = FALSE;

if (init == NULL && taxon == 0 && contributors == NULL && 
	startIsEmbryo == TRUE && startAge == 0.0 && 
	endIsEmbryo == FALSE && endAge >= genePixMaxAge)
    errAbort("must select something on genePixSelectMulti");
if (contributors)
    {
    dyStringAppend(dy, "select image.id from ");
    dyStringAppend(dy, "image,imageFile,submissionSet,submissionContributor,contributor");
    }
else
    {
    dyStringAppend(dy, "select id from image");
    }
dyStringAppend(dy, " where");
if (taxon != 0)
    {
    dyStringPrintf(dy, " taxon=%d", taxon);
    needAnd = TRUE;
    }
if (startAge > 0.0 || !startIsEmbryo)
    {
    if (needAnd) dyStringAppend(dy, " and");
    if (startIsEmbryo)
        dyStringPrintf(dy, " (image.age >= %f or !image.isEmbryo)", startAge);
    else
        dyStringPrintf(dy, " (image.age >= %f and !image.isEmbryo)", startAge);
    needAnd = TRUE;
    }
if (endAge < genePixMaxAge || endIsEmbryo)
    {
    if (needAnd) dyStringAppend(dy, " and");
    if (endIsEmbryo)
        dyStringPrintf(dy, " (image.age < %f and image.isEmbryo)", endAge);
    else
        dyStringPrintf(dy, " (image.age < %f or image.isEmbryo)", endAge);
    needAnd = TRUE;
    }
if (init != NULL)
    {
    if (needAnd) dyStringAppend(dy, " and ");
    dyStringAppend(dy, "(");
    for (el = init; el != NULL; el = el->next)
        {
	dyStringPrintf(dy, "image.id = %d", el->val);
	if (el->next != NULL)
	    dyStringAppend(dy, " or ");
	}
    dyStringAppend(dy, ")");
    needAnd = TRUE;
    }
if (contributors != NULL)
    {
    struct slName *con;
    if (needAnd) dyStringAppend(dy, " and");
    dyStringAppend(dy, " (");
    for (con = contributors; con != NULL; con = con->next)
        {
	dyStringPrintf(dy, "contributor.name");
	appendMatchHow(dy, con->name, how);
	if (con->next != NULL)
	    dyStringAppend(dy, " or ");
	}
    dyStringAppend(dy, ")");

    dyStringAppend(dy, " and contributor.id = submissionContributor.contributor");
    dyStringAppend(dy, " and submissionContributor.submissionSet = submissionSet.id");
    dyStringAppend(dy, " and submissionSet.id = imageFile.submissionSet");
    dyStringAppend(dy, " and imageFile.id = image.imageFile");
    }
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slIntNew(sqlUnsigned(row[0]));
    slAddHead(&imageList, el);
    }
sqlFreeResult(&sr);
slReverse(&imageList);
return imageList;
}

struct slInt *genePixOnSameGene(struct sqlConnection *conn, int id)
/* Get bio images that are on same gene.  Do slFreeList when done. */
{
struct slInt *list = NULL, *el, *next;
struct slName *gene, *geneList = genePixGeneName(conn, id);
struct hash *uniqHash = newHash(0);
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    struct slInt *oneList = genePixSelectNamed(conn, gene->name, gpsExact);
    for (el = oneList; el != NULL; el = next)
        {
	char buf[9];
	next = el->next;
	safef(buf, sizeof(buf), "%X", el->val);
	if (hashLookup(uniqHash, buf))
	    freez(&el);
	else
	    {
	    hashAdd(uniqHash, buf, NULL);
	    slAddHead(&list, el);
	    }
	}
    }
hashFree(&uniqHash);
slReverse(&list);
return list;
}


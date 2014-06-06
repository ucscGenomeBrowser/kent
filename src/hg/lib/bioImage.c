/* bioImage.h - Interface to bioImage database. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "portable.h"
#include "bioImage.h"


static char *somePath(struct sqlConnection *conn, int id, char *locationField)
/* Fill in path based on given location field */
{
char query[256], path[PATH_LEN];
struct sqlResult *sr;
char **row;
sqlSafef(query, sizeof(query), 
	"select location.name,image.fileName from image,location"
	" where image.id = %d and location.id=image.%s", id, locationField);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("No image of id %d", id);
safef(path, PATH_LEN, "%s/%s", row[0], row[1]);
sqlFreeResult(&sr);
return cloneString(path);
}

char *bioImageFullSizePath(struct sqlConnection *conn, int id)
/* Fill in path for full image bioImage of given id. */
{
return somePath(conn, id, "fullLocation");
}

char *bioImageScreenSizePath(struct sqlConnection *conn, int id)
/* Fill in path for screen-sized (~700x700) image bioImage of given id. */
{
return somePath(conn, id, "screenLocation");
}

char *bioImageThumbSizePath(struct sqlConnection *conn, int id)
/* Fill in path for thumbnail image (~200x200) bioImage of given id. */
{
return somePath(conn, id, "thumbLocation");
}

static char *cloneOrNull(char *s)
/* Return copy of string, or NULL if it is empty */
{
if (s == NULL || s[0] == 0)
    return NULL;
return cloneString(s);
}

char *bioImageOrganism(struct sqlConnection *conn, int id)
/* Return binomial scientific name of organism associated with given image. 
 * FreeMem this when done. */
{
char query[256], buf[256];
sqlSafef(query, sizeof(query),
	"select uniProt.taxon.binomial from image,uniProt.taxon"
         " where image.id = %d and image.taxon = uniProt.taxon.id",
	 id);
return cloneOrNull(sqlQuickQuery(conn, query, buf, sizeof(buf)));
}

char *bioImageStage(struct sqlConnection *conn, int id, boolean doLong)
/* Return string describing growth stage of organism 
 * FreeMem this when done. */
{
char query[256], buf[256];
boolean isEmbryo;
double daysPassed;
char *startMarker;
struct sqlResult *sr;
char **row;
sqlSafef(query, sizeof(query),
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
        c = 'n';
    safef(buf, sizeof(buf), "%c%3.1f", c, daysPassed);
    }
sqlFreeResult(&sr);
return cloneString(buf);
}

char *bioImageGeneName(struct sqlConnection *conn, int id)
/* Return gene name  - HUGO if possible, RefSeq/GenBank, etc if not. */
{
char query[256], buf[256];
sqlSafef(query, sizeof(query),
	"select gene from image where id=%d", id);
sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
if (buf[0] == 0)
    {
    sqlSafef(query, sizeof(query),
        "select refSeq from image where id=%d", id);
    sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
    if (buf[0] == 0)
        {
	sqlSafef(query, sizeof(query),
	    "select genbank from image where id=%d", id);
	sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
	if (buf[0] == 0)
	    {
	    char locusLink[32];
	    sqlSafef(query, sizeof(query), 
		"select locusLink from image where id=%d", id);
	    sqlNeedQuickQuery(conn, query, locusLink, sizeof(locusLink));
	    if (locusLink[0] == 0)
	        {
		sqlSafef(query, sizeof(query),
		    "select submitId from image where id=%d", id);
		sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
		}
	    else
	        {
		safef(buf, sizeof(buf), "NCBI Gene ID #%s", locusLink);
		}
	    }
	}
    }
return cloneString(buf);
}

char *bioImageAccession(struct sqlConnection *conn, int id)
/* Return RefSeq/Genbank accession or n/a if none available. */
{
char query[256], buf[256];
sqlSafef(query, sizeof(query),
	"select refSeq from image where id=%d", id);
sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
if (buf[0] == 0)
    {
    sqlSafef(query, sizeof(query),
        "select genbank from image where id=%d", id);
    sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
    }
return cloneOrNull(buf);
}

char *bioImageSubmitId(struct sqlConnection *conn, int id)
/* Return submitId  for image. */
{
char query[256], buf[256];
sqlSafef(query, sizeof(query), 
	"select submitId from image"
	" where image.id=%d", id);
return cloneString(sqlQuickQuery(conn, query, buf, sizeof(buf)));
}

char *bioImageType(struct sqlConnection *conn, int id)
/* Return RefSeq/Genbank accession or NULL if none available. 
 * FreeMem this when done. */
{
char query[256], buf[256];
sqlSafef(query, sizeof(query),
	"select imageType.name from image,imageType "
	"where image.id=%d and imageType.id = image.imageType", id);
return cloneOrNull(sqlQuickQuery(conn, query, buf, sizeof(buf)));
}

static char *indirectString(struct sqlConnection *conn, int id, char *table, char *valField)
/* Return value on table that is connected via id to image. */
{
char query[256], buf[512];
sqlSafef(query, sizeof(query),
	"select %s.%s from image,%s "
	"where image.id=%d and image.%s = %s.id",
	table, valField, table, id, table, table);
return cloneOrNull(sqlQuickQuery(conn, query, buf, sizeof(buf)));
}

char *bioImageBodyPart(struct sqlConnection *conn, int id)
/* Return body part if any. */
{
return indirectString(conn, id, "bodyPart", "name");
}

char *bioImageSliceType(struct sqlConnection *conn, int id)
/* Return slice type if any. */
{
return indirectString(conn, id, "sliceType", "name");
}

char *bioImageTreatment(struct sqlConnection *conn, int id)
/* Return treatment if any. */
{
return indirectString(conn, id, "treatment", "conditions");
}

static char *submissionSetPart(struct sqlConnection *conn, int id, char *field)
/* Return field from submissionSet that is linked to image. */
{
return indirectString(conn, id, "submissionSet", field);
}

char *bioImageContributors(struct sqlConnection *conn, int id)
/* Return comma-separated list of contributors in format Kent W.H, Wu F.Y. */
{
return submissionSetPart(conn, id, "contributors");
}

char *bioImagePublication(struct sqlConnection *conn, int id)
/* Return name of publication associated with image if any.  
 * FreeMem this when done. */
{
return submissionSetPart(conn, id, "publication");
}

char *bioImagePubUrl(struct sqlConnection *conn, int id)
/* Return url of publication associated with image if any.
 * FreeMem this when done. */
{
return submissionSetPart(conn, id, "pubUrl");
}

char *bioImageSetUrl(struct sqlConnection *conn, int id)
/* Return contributor url associated with image set if any. 
 * FreeMem this when done. */
{
return submissionSetPart(conn, id, "setUrl");
}

char *bioImageItemUrl(struct sqlConnection *conn, int id)
/* Return contributor url associated with this image. 
 * Substitute in submitId for %s before using.  May be null.
 * FreeMem when done. */
{
return submissionSetPart(conn, id, "itemUrl");
}

struct slInt *bioImageOnSameGene(struct sqlConnection *conn, int id)
/* Get bio images that are on same gene.  Do slFreeList when done. */
{
struct dyString *dy = dyStringNew(0);
struct sqlResult *sr;
char **row;
char *gene, *refSeq, *locusLink, *genbank, *submitId;
int submissionSet;
struct slInt *list = NULL, *el;


/* Fetch all the gene ID data. */
sqlDyStringPrintf(dy,
    "select gene,refSeq,locusLink,genBank,submitId,submissionSet "
    "from image where id=%d", id);
sr = sqlGetResult(conn, dy->string);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Can't find image #%d in bioImage database", id);
gene = cloneOrNull(row[0]);
refSeq = cloneOrNull(row[1]);
locusLink = cloneOrNull(row[2]);
genbank = cloneOrNull(row[3]);
submitId = cloneOrNull(row[4]);
submissionSet = atoi(row[5]);
sqlFreeResult(&sr);

/* Fetch everything that refers to same gene. */
dyStringClear(dy);
sqlDyStringPrintf(dy, "select id from image ");
dyStringPrintf(dy, "where (image.submissionSet=%d and image.submitId='%s') ",
	submissionSet, submitId);
if (gene != NULL)
    sqlDyStringPrintf(dy, "or image.gene = '%s' ", gene);
if (refSeq != NULL)
    sqlDyStringPrintf(dy, "or image.refSeq = '%s' ", refSeq);
if (locusLink != NULL)
    sqlDyStringPrintf(dy, "or image.locusLink = '%s' ", locusLink);
if (genbank != NULL)
    sqlDyStringPrintf(dy, "or image.genbank = '%s' ", genbank);
dyStringPrintf(dy, "order by priority");
sr = sqlGetResult(conn, dy->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slIntNew(atoi(row[0]));
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);

/* Clean up and return */
freeMem(gene);
freeMem(refSeq);
freeMem(locusLink);
freeMem(genbank);
freeMem(submitId);
dyStringFree(&dy);
return list;
}



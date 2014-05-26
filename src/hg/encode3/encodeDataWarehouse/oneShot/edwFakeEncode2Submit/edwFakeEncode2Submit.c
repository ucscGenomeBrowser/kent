/* edwFakeEncode2Submit - Fake up biosample table from meld of encode3 and encode2 sources.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

struct hash *hg19DateHash, *hg19LabHash,  *hg19GrpLabHash;
struct hash *mm9DateHash, *mm9LabHash, *mm9GrpLabHash; 
struct hash *labUserHash;
//struct hash *hg19SubLabHash, *mm9SubLabHash;
boolean really;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFakeEncode2Submit - Fake up edwSubmit records for encode2 files imported to encode3 data warehouse.\n"
  "usage:\n"
  "   edwFakeEncode2Submit encode2labs.tsv hg19_encode2_meta.tab mm9_encode2_meta.tab output.sql\n"
  "Where encode2labs.tsv is provided by encodedcc's redmine #1079.\n"
  "and hg19_encode2_meta.tab and mm9_encode2_meta.tab are the result of running the command on hgw1\n"
  "    mdbQuery \"select metaObject,lab,dateSubmitted,dateResubmitted,dateUnrestricted from hg19\" and \n"
  "    mdbQuery \"select metaObject,lab,dateSubmitted,dateResubmitted,dateUnrestricted from mm9\"\n"
   "hg19_group_lab.tab and mm9_group_lab.tab are the results from using the commands below on the two raw meta.tab files:\n"
   "   cat hg19_encode2_meta.tab | grep -v '#' | awk -v OFS='\\t' '{print $6, $2}' | sort -u > hg19_group_lab.tab\n"
   "and \n"
   "   cat mm9_encode2_meta.tab | grep -v '#' | awk -v OFS='\\t' '{print $6, $2}' | sort -u > mm9_group_lab.tab\n\n"
  "and all 5 data files also live in the source tree.\n"
  "The output.sql is intended for sql statements to create the faked edwSubmit records.\n"
  "options:\n"
  "   -really - Needs to be set for anything to happen,  otherwise will just print sql statements.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"really", OPTION_BOOLEAN},
    {NULL, 0},
};

void createGrpLabHash(char *fName, struct hash *grpLabHash)
/* Create the "composite : lab" hash from *_group_lab.tab */
{
struct lineFile *lf = lineFileOpen(fName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    {
    char *composite = row[0];
    char *labName = row[1];
    if (sameWord(labName, "n/a"))
        continue;
    /**************** Special Cases: ******************/
    /* Skip a composite group when no file from       */
    /* the group has been submitted, such as          */
    /*   wgEncodeNhgriBip                             */ 
    /*   wgEncodeRegTxn                               */
    /*   wgEncodeRegTfbsClustered                     */
    /*   wgEncodeRegDnaseClustered                    */
    /*   wgEncodeAwgTfbsUniform                       */
    if (sameWord(composite, "wgEncodeNhgriBip"))
	continue;
    if (sameWord(composite, "wgEncodeRegTxn"))
        continue;
    if (sameWord(composite, "wgEncodeRegTfbsClustered"))
        continue;
    if (sameWord(composite, "wgEncodeRegDnaseClustered"))
        continue;
    if (sameWord(composite, "wgEncodeAwgTfbsUniform"))
        continue;
    /* wgEncodeAwgDnaseUniform from UW or Duke only:  */
    if (sameWord(composite, "wgEncodeAwgDnaseUniform")
        && (!(sameWord(labName, "Duke") 
	|| sameWord(labName, "UW"))))
        continue;
    /* No wgEncodeHaibTfbs file submitted from Caltech */
    if (sameWord(composite, "wgEncodeHaibTfbs")
        && sameWord(labName, "Caltech"))
        continue;
    /*                                                */ 
    /**************************************************/
    hashAdd(grpLabHash, composite, cloneString(labName));
    verbose(1, "Adding %s -- %s\n", composite, labName);
    }
}

void createEncode2Hash(char *fName, struct hash *labHash, struct hash *dateHash, char *assembly)
/* Read the mdbQuery encode2 objects output hg19_encode2_meta.tab and
 * mm9_encode2_meta.tab, create two hash table to keep track the
 * provider 
 * and imported date of the encode2 object */
{
struct lineFile *lf = lineFileOpen(fName, TRUE);

char *row[6];
while (lineFileRow(lf, row))
    {
    char *object = row[0];
    char *lab = row[1];
    char *dateSubmit = row[2];
    char *dateReSubmit = row[3];
    char *dateUnrestricted = row[4];
    char *dateImport = "";
    if (sameWord(lab, "n/a"))
	continue;
    if (!sameWord(dateReSubmit, "n/a"))
        dateImport = dateReSubmit;
    else if (!sameWord(dateSubmit, "n/a"))
        dateImport = dateSubmit;
    else if (!sameWord(dateUnrestricted, "n/a"))
        dateImport = dateUnrestricted;
    /* fixed up groups with no date at all */ 
    else if (startsWith("wgEncodeAwg", object)) 
	dateImport = cloneString("2012-09-01");
    else if (startsWith("wgEncodeCrgMapabilityAlign", object)) 
        dateImport = cloneString("2010-02-09");
    else
	errAbort("Can't decide import date of %s submit date: %s, resubmitDate: %s dateUnrestricted: %s", object, dateSubmit, dateReSubmit, dateUnrestricted);
    hashAdd(labHash, object, cloneString(lab));
    hashAdd(dateHash, object, cloneString(dateImport));
    }
lineFileClose(&lf);
}

void createLabUserHash(char *fName, struct hash *labUserHash)
/* Create the "Lab : email" hash from encode2labs.tsv */
{
struct lineFile *lf = lineFileOpen(fName, TRUE);
char *row[5];
while (lineFileRow(lf, row))
    {
    char *lab = row[0];
    char *userName = row[3];
    hashAdd(labUserHash, lab, cloneString(userName));
    }
}

void maybeDoUpdate(struct sqlConnection *conn, char *query, boolean really, FILE *f)
/* If really is true than do sqlUpdate with query, otherwise just print
 * it out. */
{
if (really)
    sqlUpdate(conn, query);
else
    fprintf(f,"Update: %s\n", query);
}

void updateEdwFile(struct sqlConnection *conn, struct slName *fList, int sId, long long ulTime, FILE *f)
/* once the encode2 object's edwSubmit row is created, update the
 * corresponding edwFile row(s) with the submit id and imported date. */ 
{
struct slName *sln;
char query[512];
for (sln = fList;  sln != NULL;  sln = sln->next)
    {
    sqlSafef(query, sizeof(query),
	"update edwFile set submitId = %d, startUploadTime = %lld, endUploadTime = %lld  where submitFilename = '%s'", 
        sId, ulTime, ulTime+5, sln->name);
    maybeDoUpdate(conn, query, really, f);
    }
}

void updateEdwSubmit(struct sqlConnection *conn, int sId, int nFile, long long tSize, long long ulTime, FILE *f)
/* once the encode2 object's edwSubmit row is created, update the
 * corresponding edwFile row(s) with the submit id and imported date. */
{
char query[512];
sqlSafef(query, sizeof(query),
    "update edwSubmit set startUploadTime = %lld, endUploadTime = %lld, fileCount = %d, newFiles = %d, byteCount = %lld, newBytes = %lld  where id = %d",
    ulTime, ulTime+5, nFile, nFile, tSize, tSize, sId);
maybeDoUpdate(conn, query, really, f);
}


void addEdwSubmitRows(struct sqlConnection *conn, struct hash *grpLabHash, struct hash *labUserHash, char *assembly, FILE *f)
{

char query[512];
struct hashEl *hel, *helList = hashElListHash(grpLabHash);
char *name, *value;
for (hel = helList; hel != NULL; hel = hel->next)
    {
    name = hel->name;
    value = hel->val;

    char *submitUserMail = hashMustFindVal(labUserHash, value);
    char submitUserName[256];
    strcpy(submitUserName, submitUserMail);
    chopSuffixAt(submitUserName, '@');
    struct edwUser *user = edwMustGetUserFromEmail(conn,submitUserMail);

    char grpName[512];
    char url[512];
    safecpy(grpName, sizeof(grpName),assembly);
    safecat(grpName, sizeof(grpName), "/");
    safecat(grpName, sizeof(grpName),name);
    safecat(grpName, sizeof(grpName),"/");
    safecat(grpName, sizeof(grpName),value);

    safef(url, sizeof(url), "http://encodedcc.sdsc.edu/encode2/%s/users/%s", grpName, submitUserName);

    sqlSafef(query, sizeof(query),
	"insert into edwSubmit (url,  userId) values (\"%s\",  %d)", url, user->id);
    maybeDoUpdate(conn, query, really, f);

    }
}

void linkSubmitAndFile(struct sqlConnection *conn, struct hash* grpLabHash, struct hash *labHash, struct hash *dateHash, struct hash *labUserHash, char *assembly, boolean really, FILE *f)
{
struct sqlResult *sr = NULL;
char **row = NULL;
char query[512];
struct hashEl *hel, *helList = hashElListHash(grpLabHash);
char *name, *labName;
for (hel = helList; hel != NULL; hel = hel->next)
    {
    name = hel->name;
    labName = hel->val;
    /* get the submitId for this group */
    char subName[256];
    char tName[256];
    char nameLike[512];
    char *percent = "%";
    safef(nameLike, sizeof(nameLike), "http://encodedcc.sdsc.edu/encode2/%s/%s/%s/users/%s", assembly, name, labName, percent);
    sqlSafef(query, sizeof(query), "select id from edwSubmit where url like '%s'", nameLike);
    int sId = sqlQuickNum(conn, query);
    verbose(1, " sId of %s  -- %d from %s\n", name, sId, labName);
    safef(subName, sizeof(subName), "%s", name);

    /**************** Special Cases: ******************/
    /*                                                */
    /* Change wgEncodeAwgDnaseUniform to              */ 
    /* wgEncodeAwgDnaseDuke% or wgEncodeAwgDnaseUw    */
    if (sameWord(name,"wgEncodeAwgDnaseUniform") && sameWord(labName,"Duke"))
	safef(subName, sizeof(subName), "wgEncodeAwgDnaseDuke");
    if (sameWord(name,"wgEncodeAwgDnaseUniform") && sameWord(labName,"UW"))
        safef(subName, sizeof(subName), "wgEncodeAwgDnaseUw");
    /* Change wgEncodeMapability to                   */
    /* wgEncodeCrgMapability, wgEncodeDacMapability,  */
    /* or wgEncodeDukeMapability                      */
    if (sameWord(name,"wgEncodeMapability") && 
	(sameWord(labName,"CRG-Guigo") || sameWord(labName,"CRG-Guigo-m")))
        safef(subName, sizeof(subName), "wgEncodeCrgMapability");
    if (sameWord(name,"wgEncodeMapability") && sameWord(labName,"DAC-Stanford"))
        safef(subName, sizeof(subName), "wgEncodeDacMapability");
    if (sameWord(name,"wgEncodeMapability") && sameWord(labName,"Duke"))
        safef(subName, sizeof(subName), "wgEncodeDukeMapability");
    /* Change wgEncodeGencode to wgEncodeGencode%V% like */
    /* wgEncodeGencode%V% for V7, V10, V11, and V12      */                     
    if (startsWith("wgEncodeGencode", name))
	{
	safecpy(tName, sizeof(tName), name);
        if (endsWith(tName,"V7")) 
	    {
	    chopPrefixAt(tName,'V');
	    safef(subName, sizeof(subName), "%s%s",tName, "%V7");
	    }
        if (endsWith(tName,"V10"))
            {
            chopPrefixAt(tName,'V');
            safef(subName, sizeof(subName), "%s%s",tName, "%V10");
            }
        if (endsWith(tName,"V11"))
            {
            chopPrefixAt(tName,'V');
            safef(subName, sizeof(subName), "%s%s",tName, "%V11");
            }
        if (endsWith(tName,"V12"))
            {
            chopPrefixAt(tName,'V');
            safef(subName, sizeof(subName), "%s%s",tName, "%V12");
            }
	}
    /*                                                */
    /**************************************************/

    safef(nameLike, sizeof(nameLike), "%s/%s/%s%s", assembly, name, subName, percent);
    sqlSafef(query, sizeof(query), "select * from edwFile where SubmitFilename like '%s'", nameLike);
    sr = sqlGetResult(conn, query);
    int nFile = 0;
    long long tSize = 0;
    long long ulTime = 0;
    char pathName[1024];
    char baseName[512];
    char *fromLab;
    struct slName *list = NULL, *n;

    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct edwFile *edwfile = edwFileLoad(row);
        strcpy(pathName, edwfile->submitFileName);
        chopPrefixAt(pathName, '.');
	strcpy(baseName,(basename(pathName)));
	/* skip the basename (subtrack) if it is not processed by this lab */
	fromLab = hashMustFindVal(labHash, baseName);
	if (!sameString(fromLab, labName)) 
	    continue; 

        nFile += 1;
        tSize += edwfile->size;
	char *dateImport = hashMustFindVal(dateHash, baseName);
	ulTime = dateToSeconds(dateImport, "%Y-%m-%d");
       /* remnber the submitFileName */
        n = slNameNew(edwfile->submitFileName);
        slAddHead(&list, n);
        } /* end while result row */
//verbose(1, "Total number of files: %d  total byte count %lld list count: %d\n", nFile, tSize, slCount(list));
    sqlFreeResult(&sr);
    /* loop tru filename list and update submitId and load time */
    if (nFile > 0)
        {
        updateEdwFile(conn, list, sId, ulTime, f);
        updateEdwSubmit(conn, sId, nFile, tSize, ulTime, f);
	slFreeList(&list);
	}
    } /* for each compsite group */
}

void edwFakeEncode2Submit(char *tsvLab, char *grpHg19, char *tsvHg19, char *grpMm9, char *tsvMm9,  char *outSql)
/* edwFakeEncode2Submit - Fake up edwSubmit records for encode2 files imported to encode3 data warehouse. */
{
hg19GrpLabHash = hashNew(0);
createGrpLabHash(grpHg19, hg19GrpLabHash);
verbose(1, "hg19GrpLabHash has  %d elements.\n", hg19GrpLabHash->elCount);

hg19LabHash = hashNew(0);
hg19DateHash = hashNew(0);
createEncode2Hash(tsvHg19, hg19LabHash, hg19DateHash, "hg19");
verbose(1, "hg19LabHash has  %d elements.\n", hg19LabHash->elCount);
verbose(1, "hg19DateHash has  %d elements.\n", hg19DateHash->elCount);

mm9GrpLabHash = hashNew(0);
createGrpLabHash(grpMm9, mm9GrpLabHash);
verbose(1, "mm9GrpLabHash has  %d elements.\n", mm9GrpLabHash->elCount);

mm9LabHash = hashNew(0);
mm9DateHash = hashNew(0);
createEncode2Hash(tsvMm9, mm9LabHash, mm9DateHash, "mm9");
verbose(1, "mm9LabHash has  %d elements.\n", mm9LabHash->elCount);
verbose(1, "mm9DteHash has  %d elements.\n", mm9DateHash->elCount);

labUserHash = hashNew(0);
createLabUserHash(tsvLab, labUserHash);
verbose(1, "labUserHash has  %d elements.\n", labUserHash->elCount);

FILE *f = mustOpen(outSql, "w");
//char *database = "chinhliTest";
//struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *conn =  edwConnectReadWrite(edwDatabase);
addEdwSubmitRows(conn, mm9GrpLabHash, labUserHash, "mm9", f);
addEdwSubmitRows(conn, hg19GrpLabHash, labUserHash, "hg19", f);
linkSubmitAndFile(conn, mm9GrpLabHash, mm9LabHash, mm9DateHash, labUserHash, "mm9", really, f);
linkSubmitAndFile(conn, hg19GrpLabHash, hg19LabHash, hg19DateHash, labUserHash, "hg19", really, f);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
really = optionExists("really");
if (really) verbose(1, "Really going to do it! \n");
if (argc != 7)
    usage();
edwFakeEncode2Submit(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}

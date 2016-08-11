/* cdwGetFile - given a file ID and a security token, send the file using Apache */

/* Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "sqlSanity.h"
#include "htmshell.h"
#include "portable.h"
#include "cart.h"
#include "cdw.h"
#include "hdb.h"
#include "hui.h"
#include "cdw.h"
#include "cdwValid.h"
#include "web.h"
#include "cdwLib.h"
#include "wikiLink.h"
#include "filePath.h"

/* Global vars */
struct cart *cart;	// User variables saved from click to click
struct hash *oldVars;	// Previous cart, before current round of CGI vars folded in
struct cdwUser *user;	// Our logged in user if any

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwWebBrowse is a cgi script not meant to be run from command line.\n"
  );
}

void errExit(char *msg, char *field) 
/* print http header + message and exit. msg can contain %s */
{
printf("Content-Type: text/html\n\n");
if (!field)
    puts(msg);
else
    printf(msg, field);
exit(0);
}

void mustHaveAccess(struct sqlConnection *conn, struct cdwFile *ef)
/* exit with error message if user does not have access to file ef */
{
if (cdwCheckAccess(conn, ef, user, cdwAccessRead))
    return;
else
    if (user==NULL)
        errExit("Sorry, this file has access restrictions. Please <a href='/cgi-bin/hgLogin'>log in</a> first.", NULL);
    else
        errExit("Sorry, user %s does not have access to this file.", user->email);
}

void apacheSendX(char *format, char *filePath, char *suggestFileName)
/* send pseudo-HTTP header to tell Apache to transfer filePath with suggestFileName on user's disk 
 * format is one of "fastq", "fasta", defined in cdw.as. if format is NULL, use file extension. */
{
if (format==NULL)
{
    char ext[FILEEXT_LEN];
    splitPath(filePath, NULL, NULL, ext);
    if (strlen(ext)>1)
        {
        format = cloneString(ext); 
        format++; // skip over . character
        }
}

// html files are shown directly in the internet browser, not downloaded
if (sameWord(format, "html"))
    printf("Content-Type: text/html\n");
else if (sameWord(format, "json"))
    printf("Content-Type: application/json\n");
else if (sameWord(format, "text"))
    printf("Content-Type: application/json\n");
else
    {
    printf("Content-Disposition: attachment; filename=%s\n", suggestFileName);
    printf("Content-Type: application/octet-stream\n");
    }
printf("X-Sendfile: %s\n\n", filePath);
}

void sendFileByAcc(struct sqlConnection *conn, char* acc, boolean useSubmitFname)
/* send file identified by acc (=cdwValidFile.licensePlate), suggests a canonical filename of the format
 * <licensePlate>.<originalExtension> 
 * Example URL: http://hgwdev.soe.ucsc.edu/cgi-bin/cdwGetFile?acc=SCH000FSW */
{
struct cdwValidFile *vf = cdwValidFileFromLicensePlate(conn, acc);
if (vf==NULL)
    errExit("%s is not a valid accession in the CDW.", acc);

struct cdwFile *ef = cdwFileFromId(conn, vf->fileId);
char* filePath = cdwPathForFileId(conn, vf->fileId);

mustHaveAccess(conn, ef);

char suggestName[8000];
if (useSubmitFname)
    safef(suggestName, sizeof(suggestName), "%s", basename(ef->submitFileName));
else
    {
    // use the license plate as the basename of the downloaded file.
    // Take the extension from the submitted filename, as cdwFile.format is not the same as the extension
    // e.g. when database says format=fasta -> file extension should be .fa.gz
    char *submitExt = skipBeyondDelimit(basename(ef->submitFileName), '.');
    safef(suggestName, sizeof(suggestName), "%s.%s", vf->licensePlate, submitExt);
    }

apacheSendX(vf->format, filePath, suggestName);
}

void sendFileByPath(struct sqlConnection *conn, char *path) 
/* send file identified by a submission pathname (cdwFile.submitFileName),
 * suggests the original filename. */
/* path can be a suffix that matches a filename, so the initial '/hive' or
 * '/data' can be omitted. *
 * Example URL for testing:
 * http://hgwdev.soe.ucsc.edu/cgi-bin/cdwGetFile/hive/groups/cirm/pilot/labs/quake/130625_M00361_0080_000000000-A43D1/Sample_1_L13_C31_IL3541-701-506/1_L13_C31_IL3541-701-506_TAAGGCGA-ACTGCATA_L001_R1_001.fastq.gz */
{
int fileId = cdwFileIdFromPathSuffix(conn, path);


if (fileId == 0)
    errExit("A file with suffix %s does not exist in the database", path);
    
char *localPath = cdwPathForFileId(conn, fileId);

if (localPath == NULL)
    errExit("A local file with suffix %s was not found in the database. This is an internal error.", path);

struct cdwFile *ef = cdwFileFromId(conn, fileId);

if (ef == NULL)
    errExit("Could not find cdwFile for path %s", path);

mustHaveAccess(conn, ef);
apacheSendX(NULL, localPath, basename(ef->submitFileName));
}

struct cdwUser *authUserViaToken(struct sqlConnection *conn) 
/* get the cgi variable "token" and return the cdw user for it */
{
char *token = cgiOptionalString("token");
if (token==NULL)
    return NULL;

// get the userId for this token
struct sqlConnection *central = hConnectCentral();
char query[4096];
sqlSafef(query, sizeof(query), "SELECT userId FROM cdwDownloadToken WHERE token='%s'", token);
int userId = sqlQuickNum(central, query);
if (userId==0)
    return NULL;
hDisconnectCentral(&central);

return cdwUserFromId(conn, userId);
}

void dispatch(struct sqlConnection *conn)
/* Dispatch page after to routine depending on cdwCommand variable */
{
char *userName = wikiLinkUserName();
if (userName==NULL)
    // alternative to cookie is a token on the URL, for queries from wget
    user = authUserViaToken(conn);
else
    user = cdwUserFromUserName(conn, userName);

char *acc = cgiOptionalString("acc");
char *path = getenv("PATH_INFO"); // CGI gets trailing /x/y/z like path via this env. var.
boolean useSubmitFname = cgiOptionalInt("useSubmitFname", 0);

if (acc != NULL)
    sendFileByAcc(conn, acc, useSubmitFname);
else if (path != NULL)
    sendFileByPath(conn, path);
else
    errExit("Need at least the HTTP GET parameter 'acc' with an accession ID " 
    "or a file path directly after the CGI name, "
    "separated by '/', e.g. cdwGetFile/valData/ce10/ce10.2bit';", NULL);

}

void localWebWrap(struct cart *theCart)
/* We got the http stuff handled, and a cart. */
{
cart = theCart;
struct sqlConnection *conn = sqlConnect(cdwDatabase);
dispatch(conn);
sqlDisconnect(&conn);
}

char *excludeVars[] = {"submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
oldVars = hashNew(0);
cartEmptyShellNoContent(localWebWrap, hUserCookie(), excludeVars, oldVars);
return 0;
}


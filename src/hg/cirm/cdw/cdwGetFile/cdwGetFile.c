/* cdwSendFile - given a file ID and a security token, send the file using Apache */

/* Copyright (C) 2014 The Regents of the University of California 
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

void errExit(char* msg) 
{
printf("Content-Type: text/html\n\n");
puts(msg);
exit(0);
}

void sendFile(struct sqlConnection *conn, char* acc)
/* send file identified by acc */
{
char *userName = wikiLinkUserName();
if (userName==NULL)
    errExit("Please <a href='../cgi-bin/hgLogin'>log in</a> before downloading files from this system.");

user = cdwUserFromUserName(conn, userName);
if (userName==NULL)
    errExit("There is no CDW account for the Genome Browser account that is currently logged in.");

struct cdwValidFile *vf = cdwValidFileFromLicensePlate(conn, acc);
struct cdwFile *ef = cdwFileFromId(conn, vf->fileId);
char* filePath = cdwPathForFileId(conn, vf->fileId);

// we need the fileName from the tags table to get the file extension right (e.g. "fastq.gz")
char query[1024];
sqlSafef(query, sizeof(query), "SELECT file_name FROM cdwFileTags WHERE file_id='%d'", vf->fileId);
char* fileName = sqlNeedQuickString(conn, query);
if (fileName==NULL)
    errExit("fileId not found in cdwTags. This is an internal error, please email us.");

if (cdwCheckAccess(conn, ef, user, cdwAccessRead))
    {
    // html files are shown directly in the internet browser, not downloaded
    if (sameWord(vf->format, "html"))
        printf("Content-Type: text/html\n");
    else
        {
        printf("Content-Disposition: attachment;filename=%s\n", fileName);
        printf("Content-Type: plain/text\n");
        }
    printf("X-Sendfile: %s\n\n", filePath);
    }
else
    printf("Content-Type: text/html\n\n");
    printf("User '%s' does not have access to file with accession %s", user->email, vf->licensePlate);
}

void dispatch(struct sqlConnection *conn)
/* Dispatch page after to routine depending on cdwCommand variable */
{

char *acc = cartOptionalString(cart, "acc");
if (acc == NULL)
    {
    printf("Content-Type: text/html\n\n");
    printf("Need at least the parameter acc");
    }
else 
    {
    sendFile(conn, acc);
    }
}

void doMiddle()
/* Not really outputting html on this page, still keeping the basic structure of a cdw CGI. */
{

struct sqlConnection *conn = sqlConnect(cdwDatabase);
dispatch(conn);
sqlDisconnect(&conn);
}


void localWebWrap(struct cart *theCart)
/* We got the http stuff handled, and a cart. */
{
cart = theCart;
doMiddle();
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


/* edwWebXSendFile - A cgi script for user to download files using xsendfile. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hmac.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "hgConfig.h"
#include "edwWebXSendFile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwWebXSendFile - A cgi script for user to download files using xsendfile.\n"
  "usage:\n"
  " Run by sending query string like \n"
  "  \"?accession=ENXXXXXXXXX&date=yyyy-mm-dd&user=someone@xyz.com&token=a41b..7a0\" \n"
  " to edwWebXSendFile cgi. \n"
  );
}

char *edwDataRoot()
/* Return the encodeDataWarehouse data root directory  */
{
return cloneString(cfgOption(CFG_ENCODEDATAWAREHOUSE_DATAROOT));
}

char *edwKey()
/* Return the encodeDataWarehouse key  */
{
return cloneString(cfgOption(CFG_ENCODEDATAWAREHOUSE_KEY));
}

char *getLicensePlate()
/* Return the value of accession in the query string */
{
return cloneString(cgiUsualString("accession", ""));
}

char *getDate()
/* Return the value of date in the query string */
{
return cloneString(cgiUsualString("date", ""));
}

char *getToken()
/* Return the value of token in the query string */
{
return cloneString(cgiUsualString("token", ""));
}

char *getUserMail()
/* Return the value of user (email) in the query string */
{
return cloneString(cgiUsualString("user", ""));
}

char *getFullFileName(char * licensePlate)
/* Return a complete path given dir and basename of file associated with this licenses plate */
{
struct sqlConnection *conn = edwConnect();
char query[512];
char *fileName;
char *fullFileName;
sqlSafef(query, sizeof(query),
    "select edwFileName from vf where licensePlate = '%s'", licensePlate);
fileName = sqlQuickString(conn, query);
fullFileName = strcat(edwDataRoot(), fileName);
return fullFileName;
}

char *getBaseName(char *fullFileName)
/* Return basename part of the full file name */
{
return basename(fullFileName);
}

void xSendFile(char *baseName, char *fullFileName)
/* send the file out using xsendfile */
{
printf("Content-Description: File Transfer\n");
printf("Content-Disposition: attachment; filename=\"%s\"\n", baseName);
printf("Content-Transfer-Encoding: binary\n");
printf("Expires: 0\n");
printf("Cache-Control: no-store, no-cache, must-revalidate\n");
printf("Pragma: no-cache\n");
printf("Content-Length: %lld\n", (long long)fileSize(fullFileName));
printf("X-Sendfile: %s\n\n", fullFileName);
}

boolean tokenValid(char *accession, char *date, char *token, char *user)
/* Check the token to ensure that the query string was created by
 * encodedcc without any modification */ 
{
char *hStr=strcat(strcat(date,accession),user);
char *digest;
digest=hmacSha1(edwKey(), hStr);
if (sameString(digest, token))
    return TRUE;
return FALSE;
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();
char *lp = getLicensePlate();
char *tk = getToken();
char *dt = getDate();
char *user = getUserMail();
/* send file out only when the query string was created by encodedcc */
if ( tokenValid(lp, dt, tk, user) )
    {
    char *xFile = getFullFileName(lp);
    char *bName = getBaseName(xFile);
    xSendFile(bName, xFile);
    }
return 0;
}

/* edwWebXSendFile - A small self-contained CGI for user to download submitted files  using xsendfile */
#include "common.h"
#include "hmac.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "hgConfig.h"
#include "edwWebXSendFile.h"

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
{
return cloneString(cgiUsualString("licenseplate", ""));
}

char *getDate()
{
return cloneString(cgiUsualString("date", ""));
}


char *getToken()
{
return cloneString(cgiUsualString("token", ""));
}

char *getFullFileName(char * licensePlate)
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
printf("Content-Length: filesize(xfile)\n");
printf("X-Sendfile: %s\n\n", fullFileName);
}

boolean tokenValid(char *licenseplate, char *date, char *token)
{
char *hStr=strcat(date,licenseplate);
char *digest;
digest=hmacSha1(edwKey(), hStr);
if (sameString(digest, token))
    return TRUE;
return FALSE;
}

int main(void) 
{
char *lp = getLicensePlate();
char *tk = getToken();
char *dt = getDate();
if ( tokenValid(lp, dt, tk) )
    {
    char *xFile = getFullFileName(lp);
    char *bName = getBaseName(xFile);
    xSendFile(bName, xFile);
    }
return 0;
}

/* edwWebRegisterScript - Create a user ID for a script, since scripts have a hard time with Persona.. */
#include <uuid/uuid.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "errabort.h"
#include "jksql.h"
#include "hex.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwWebRegisterScript - Register a script, giving it a random access key. "
  "This program is meant to be called as a CGI from a web server using https.");
}

void edwRandomHexed64(char hexed[65])
/* Create a string of random hexadecimal digits 64 digits long.  Add zero tag at end*/
{
/* Generate 32 bytes of random sequence with uuid generator */
unsigned char access[32];
uuid_generate(access);
uuid_generate(access+16);

/* Convert to hex. */
hexBinaryString(access, 32, hexed, 65);
}

char *userEmail = NULL;

static void localWarn(char *format, va_list args)
/* A little warning handler to override the one with the button that goes nowhere. */
{
printf("<B>Error:</B> ");
vfprintf(stdout, format, args);
}

void edwRegisterScript(struct sqlConnection *conn,
    struct edwUser *user, char *access, char *scriptName, char *description)
/* Register a new script with the database. */
{
scriptName = sqlEscapeString(scriptName);
description = sqlEscapeString(description);
struct edwScriptRegistry reg = {.userId=user->id, .name=scriptName, .description=description};
char secretHash[EDW_SID_SIZE];
edwMakeSid(access, secretHash);
reg.secretHash = secretHash;
edwScriptRegistrySaveToDb(conn, &reg, "edwScriptRegistry", 256);
}

void doMiddle()
/* Write what goes between BODY and /BODY */
{
pushWarnHandler(localWarn);
if (!cgiServerHttpsIsOn())
     usage();
struct sqlConnection *conn = edwConnectReadWrite();
printf("<FORM ACTION=\"edwWebRegisterScript\" METHOD=POST>\n");
printf("<B>Register Script with ENCODE Data Warehouse</B><BR>\n");
if (userEmail == NULL)
    {
    printf("Please sign in:");
    printf("<INPUT TYPE=BUTTON NAME=\"signIn\" VALUE=\"sign in\" id=\"signin\">");
    }
else if (cgiVarExists("script"))
    {
    struct edwUser *user = edwMustGetUserFromEmail(conn, userEmail);
    char *newScript = trimSpaces(cgiString("script"));
    if (!isEmpty(newScript))
	{
	char query[512];
	safef(query, sizeof(query), 
	    "select id from edwScriptRegistry where userId=%u and name='%s'",
	    user->id,  sqlEscapeString(newScript));
	if (sqlQuickNum(conn, query) != 0)
	    errAbort("Script %s already exists, please go back and use a new name.", newScript);
	char access[65];
	edwRandomHexed64(access);
	edwRegisterScript(conn, user, access, newScript, cgiString("description"));
	printf("Script %s is now registered.<BR>\n", newScript);
	printf("The script access key is %s.<BR>\n", access);
	printf("Please save this and the script name someplace. ");
	puts("Please pass your email address, the script name, and the access key,  and the URL");
	puts(" of your validated manifest file (validated.txt) to our server to submit data.");
	puts("Construct a URL of the form:<BR>");
	printf("<PRE>https://encodedcc.sdsc.edu/cgi-bin/edwScriptSubmit"
	       "?email=%s&script=%s&access=%s&url=%s\n</PRE>", 
	       sqlEscapeString(userEmail), sqlEscapeString(newScript), sqlEscapeString(access), 
	       sqlEscapeString("http://your.host.edu/your_dir/validated.txt"));
	puts("That is pass the CGI encoded variables email, script, access, and url to the ");
	puts("web services CGI at");
	puts("https://encodedcc.sdsc.edu/cgi-bin/edwScriptSubmit. ");
	puts("You can use the http://encodedcc.sdsc.edu/cgi-bin/edwWebBrowse site to ");
	puts("monitor your submission interactively. Please contact your wrangler if you ");
	puts("have any questions.<BR>");
	}
    cgiMakeButton("submit", "Register another script");
    printf(" ");
    edwPrintLogOutButton();
    }
else
    {
    edwMustGetUserFromEmail(conn, userEmail);
    edwPrintLogOutButton();
    printf("%s is authorized to register a new script<BR>\n", userEmail);
    printf("Script name:\n");
    cgiMakeTextVar("script", NULL, 40);
    printf("<BR>Description:\n");
    cgiMakeTextVar("description", NULL, 80);
    cgiMakeSubmitButton();
    }
printf("</FORM>\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (!cgiIsOnWeb())
    usage();
userEmail = edwGetEmailAndVerify();
edwWebHeaderWithPersona("ENCODE Data Warehouse Register Script");
htmEmptyShell(doMiddle, NULL);
edwWebFooterWithPersona();
return 0;
}

/* edwWebRegisterScript - Create a user ID for a script, since scripts have a hard time with Persona.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include <uuid/uuid.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "errAbort.h"
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
  "This program is meant to be called as a CGI from a web server using HTTPS.");
}

#define HEXED_32_SIZE	33    /* Size of 32 hexadecimal digits plus zero tag */

void edwRandomHexed32(char hexed[HEXED_32_SIZE])
/* Create a string of random hexadecimal digits 32 digits long.  Add zero tag at end*/
{
/* Generate 16 bytes of random sequence with uuid generator */
unsigned char uuid[16];
uuid_generate(uuid);

/* Convert to hex. */
hexBinaryString(uuid, 16, hexed, HEXED_32_SIZE);
}

void edwRandomBabble(char *babble, int babbleSize)
/* Create a string of random syllables . */
{
/* Generate 16 bytes of random sequence with uuid generator */
unsigned char uuid[16];
uuid_generate(uuid);
unsigned long ul;
memcpy(&ul, uuid, sizeof(ul));
edwMakeBabyName(ul, babble, babbleSize);
}

char *userEmail = NULL;

static void localWarn(char *format, va_list args)
/* A little warning handler to override the one with the button that goes nowhere. */
{
printf("<B>Error:</B> ");
vfprintf(stdout, format, args);
}

void edwRegisterScript(struct sqlConnection *conn,
    struct edwUser *user, char *name, char *password, char *description)
/* Register a new script with the database. */
{
struct edwScriptRegistry reg = {.userId=user->id, .name=name, .description=description};
char secretHash[EDW_SID_SIZE];
edwMakeSid(password, secretHash);
reg.secretHash = secretHash;
edwScriptRegistrySaveToDb(conn, &reg, "edwScriptRegistry", 256);
}

char **mainEnv;

void dumpEnv(char **env)
{
char *s;
while ((s = *env++) != NULL)
    printf("%s<BR>\n", s);
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
#ifdef SOON
uglyf("HTTP_AUTHENTICATION: '%s'<BR>\n", getenv("HTTP_AUTHENTICATION"));
uglyf("HTTP_AUTHORIZATION: '%s'<BR>\n", getenv("HTTP_AUTHORIZATION"));
dumpEnv(mainEnv);
#endif
if (userEmail == NULL)
    {
    printf("Please sign in:");
    printf("<INPUT TYPE=BUTTON NAME=\"signIn\" VALUE=\"sign in\" id=\"signin\">");
    }
else if (cgiVarExists("description"))
    {
    struct edwUser *user = edwUserFromEmail(conn, userEmail);
    if (user == NULL)
	edwWarnUnregisteredUser(userEmail);
    else
	{
	char password[HEXED_32_SIZE];
	edwRandomHexed32(password);
	char babyName[HEXED_32_SIZE];
	edwRandomBabble(babyName, sizeof(babyName));

	edwRegisterScript(conn, user, babyName, password, cgiString("description"));
	printf("Script now registered.<BR>\n");
	printf("The script user name is %s.<BR>\n", babyName);
	printf("The script password is %s.<BR>\n", password);
	printf("Please save the script user name and password somewhere. ");
	puts("Please pass these two and the URL");
	puts(" of your validated manifest file (validated.txt) to our server to submit data.");
	puts("Construct a URL of the form:<BR>");
	printf("<PRE>https://encodedcc.sdsc.edu/cgi-bin/edwScriptSubmit"
	       "?user=%s&password=%s&url=%s\n</PRE>", 
	       babyName, password,
	       cgiEncode("http://your.host.edu/your_dir/validated.txt"));
	puts("That is pass the CGI encoded variables user, password, and url to the ");
	puts("web services CGI at:<BR>");
	puts("<PRE>  https://encodedcc.sdsc.edu/cgi-bin/edwScriptSubmit\n</PRE> ");
	puts("You can monitor the status of the submission programmatically by passing<BR>");
	puts("the same user, password, and url variables to:<BR>");
	puts("<PRE>  https://encodedcc.sdsc.edu/cgi-bin/edwScriptSubmitStatus\n</PRE> ");
	puts("You can also use the http://encodedcc.sdsc.edu/cgi-bin/edwWebBrowse site to ");
	puts("monitor your submission interactively. Please contact your wrangler if you ");
	puts("have any questions.<BR>");
	cgiMakeButton("submit", "Register another script");
	}
    printf(" ");
    edwPrintLogOutButton();
    }
else
    {
    struct edwUser *user = edwUserFromEmail(conn, userEmail);
    edwPrintLogOutButton();
    if (user == NULL)
	edwWarnUnregisteredUser(userEmail);
    else
	{
	printf("%s is authorized to register a new script<BR>\n", userEmail);
	printf("<BR>Script description:\n");
	cgiMakeTextVar("description", NULL, 80);
	cgiMakeSubmitButton();
	}
    }
printf("</FORM>\n");
}

int main(int argc, char *argv[], char **env)
/* Process command line. */
{
if (!cgiIsOnWeb())
    usage();
mainEnv = env;
userEmail = edwGetEmailAndVerify();
edwWebHeaderWithPersona("ENCODE Data Warehouse Register Script");
htmEmptyShell(doMiddle, NULL);
edwWebFooterWithPersona();
return 0;
}

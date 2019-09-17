/* edwWebAuthLogin - A tiny little program to help manage Persona logins.  Specifically
 * a little CGI that gets passed in an assertion and then checks that assertion
 * against the persona verifier web service.  If it checks out it sets a cookie called
 * 'email' with the email address the user logged with Persona. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "net.h"
#include "jsHelper.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwWebAuthLogin - A tiny little program to help manage Persona logins.\n"
  "This is meant to be run as a CGI not from the command line\n"
  );
}

char *personaUrl = "https://verifier.login.persona.org/verify";

char *mustGetJsonObjString(struct jsonElement *obj, char *name, char *url)
/* Make sure json is an obj, and return a child of given name that is a string.
 * The URL is just used for error reporting. */
{
if (obj->type != jsonObject)
    errAbort("json response from %s doesn't begin with '{'", url);
struct jsonElement *string = hashFindVal(obj->val.jeHash, name);
if (string == NULL)
    errAbort("missing expected %s in json response from %s", name, url);
if (string->type != jsonString)
    errAbort("Expecting string value for %s in json from %s", name, url);
return string->val.jeString;
}

char *mustGetEnv(char *name)
/* Get environment variable or squawk and die */
{
char *val = getenv(name);
if (val == NULL)
    errAbort("Missing required environment variable %s", name);
return val;
}

char *checkAuth()
/* Check authorization assertion and abort if it does not look good. 
 * Returns with email address if all good. */
{
/* Get assertion out of CGI variables we were passed.  Put it and pointer to self
 * into cgi variables to pass to assertion checker. */
struct dyString *dyCgi = dyStringNew(0);
char *assertion=cgiString("assertion");
cgiEncodeIntoDy("assertion", assertion, dyCgi);
char *server = mustGetEnv("SERVER_NAME");
char *port = mustGetEnv("SERVER_PORT");
char serverAndPort[2+strlen(server)+strlen(port)];
safef(serverAndPort, sizeof(serverAndPort), "%s:%s", server, port);
cgiEncodeIntoDy("audience", serverAndPort, dyCgi);

/* Pass a little CGI post request to Persona including our CGI vars. */
struct dyString *dyHeader = dyStringNew(0);
dyStringPrintf(dyHeader, "Content-type: application/x-www-form-urlencoded\r\n");
dyStringPrintf(dyHeader, "Content-length: %ld\r\n", dyCgi->stringSize);
int sd = netOpenHttpExt(personaUrl, "POST", dyHeader->string);
mustWriteFd(sd, dyCgi->string, dyCgi->stringSize);

/* Get Persona's response up through end of HTTP header */
/* There's probably a simpler way to do this.  We don't need the forwarding. */
char *newUrl = NULL;
int newSd = 0;
if (!netSkipHttpHeaderLinesHandlingRedirect(sd, personaUrl, &newSd, &newUrl))
    errAbort("%s didn't give a good http header", personaUrl);
if (newUrl != NULL)
    {
    sd = newSd;
    freeMem(newUrl); 
    }

/* Parse out response as JavaScript and check status field and fetch email field. */
struct dyString *response = netSlurpFile(sd);
struct jsonElement *json = jsonParse(response->string);
char *status = mustGetJsonObjString(json, "status", personaUrl);
if (!sameString(status, "okay"))
    errAbort("Status not okay, is %s from %s",  status, personaUrl);

/* Return email */
return mustGetJsonObjString(json, "email", personaUrl);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (!cgiIsOnWeb())
    usage();
char *email = checkAuth();
char sid[EDW_SID_SIZE];
edwMakeSid(email, sid);
printf("Content-Type:text/plain\r\n");
printf("Set-Cookie: email=%s\r\n", cgiEncode(email));
printf("Set-Cookie: sid=%s\r\n", cgiEncode(sid));
printf("\r\n");
printf("ok\n");
return 0;
}

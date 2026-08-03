/* oauthLogin - social login (Google, ORCID) for hgLogin via OAuth 2.0 / OpenID Connect.
 * See oauthLogin.h for the hg.conf configuration. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cheapcgi.h"
#include "hgConfig.h"
#include "dystring.h"
#include "errCatch.h"
#include "net.h"
#include "htmlPage.h"
#include "jsonParse.h"
#include "oauthLogin.h"

static char *oauthCfg(char *provider, char *field)
/* Return the hg.conf value for login.<provider>.<field>, or NULL.  Do not free. */
{
char name[128];
safef(name, sizeof(name), "login.%s.%s", provider, field);
return cfgOption(name);
}

static char *orcidBase()
/* Return the ORCID base URL, honoring login.orcid.sandbox for testing. */
{
if (cfgOptionBooleanDefault("login.orcid.sandbox", FALSE))
    return "https://sandbox.orcid.org";
return "https://orcid.org";
}

boolean oauthProviderEnabled(char *provider)
/* Return TRUE if both clientId and clientSecret for provider are set in hg.conf. */
{
if (isEmpty(provider))
    return FALSE;
return isNotEmpty(oauthCfg(provider, "clientId")) && isNotEmpty(oauthCfg(provider, "clientSecret"));
}

boolean oauthAnyProviderEnabled()
/* Return TRUE if at least one social login provider is configured. */
{
return oauthProviderEnabled(OAUTH_PROVIDER_GOOGLE) || oauthProviderEnabled(OAUTH_PROVIDER_ORCID);
}

char *oauthLoginUrl(char *provider, char *redirectUri, char *state)
/* Return the provider's authorization-endpoint URL to redirect the browser to, or NULL. */
{
if (!oauthProviderEnabled(provider))
    return NULL;
char *clientId = oauthCfg(provider, "clientId");
struct dyString *dy = dyStringNew(512);
if (sameString(provider, OAUTH_PROVIDER_GOOGLE))
    {
    dyStringPrintf(dy, "https://accounts.google.com/o/oauth2/v2/auth?response_type=code");
    dyStringPrintf(dy, "&scope=%s", cgiEncode("openid email profile"));
    dyStringPrintf(dy, "&prompt=select_account");
    }
else if (sameString(provider, OAUTH_PROVIDER_ORCID))
    {
    dyStringPrintf(dy, "%s/oauth/authorize?response_type=code", orcidBase());
    dyStringPrintf(dy, "&scope=%s", cgiEncode("openid"));
    }
else
    {
    dyStringFree(&dy);
    return NULL;
    }
dyStringPrintf(dy, "&client_id=%s", cgiEncode(clientId));
dyStringPrintf(dy, "&redirect_uri=%s", cgiEncode(redirectUri));
dyStringPrintf(dy, "&state=%s", cgiEncode(state));
return dyStringCannibalize(&dy);
}

static char *httpRequest(char *url, char *method, char *header, char *body)
/* Make an HTTP(S) request and return the response body (allocd), or NULL on failure.
 * header holds extra request headers (each terminated with \r\n); body is the request
 * payload for POST (may be NULL).  Network errors are caught and turned into NULL. */
{
char *result = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    int sd = netOpenHttpExt(url, method, header);
    if (sd >= 0)
        {
        if (isNotEmpty(body))
            mustWriteFd(sd, body, strlen(body));
        struct dyString *dy = netSlurpFile(sd);
        close(sd);
        struct htmlPage *page = htmlPageParse(url, dyStringCannibalize(&dy));
        if (page != NULL && isNotEmpty(page->htmlText))
            result = cloneString(page->htmlText);
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    fprintf(stderr, "hgLogin oauth: request to %s failed: %s\n", url, errCatch->message->string);
    result = NULL;
    }
errCatchFree(&errCatch);
return result;
}

static struct jsonElement *jsonParseSafe(char *text)
/* Parse JSON, returning NULL instead of aborting on malformed input. */
{
if (isEmpty(text))
    return NULL;
struct jsonElement *json = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    json = jsonParse(text);
errCatchEnd(errCatch);
if (errCatch->gotError)
    json = NULL;
errCatchFree(&errCatch);
return json;
}

static struct jsonElement *postForm(char *url, char *body)
/* POST an x-www-form-urlencoded body and return the parsed JSON response, or NULL. */
{
struct dyString *header = dyStringNew(256);
dyStringPrintf(header, "Content-Type: application/x-www-form-urlencoded\r\n");
dyStringPrintf(header, "Accept: application/json\r\n");
dyStringPrintf(header, "Content-Length: %d\r\n", (int)strlen(body));
char *respBody = httpRequest(url, "POST", header->string, body);
dyStringFree(&header);
struct jsonElement *json = jsonParseSafe(respBody);
freeMem(respBody);
return json;
}

static struct dyString *tokenExchangeBody(char *provider, char *code, char *redirectUri)
/* Build the shared authorization_code token-exchange POST body for provider. */
{
struct dyString *body = dyStringNew(512);
dyStringPrintf(body, "grant_type=authorization_code");
dyStringPrintf(body, "&code=%s", cgiEncode(code));
dyStringPrintf(body, "&client_id=%s", cgiEncode(oauthCfg(provider, "clientId")));
dyStringPrintf(body, "&client_secret=%s", cgiEncode(oauthCfg(provider, "clientSecret")));
dyStringPrintf(body, "&redirect_uri=%s", cgiEncode(redirectUri));
return body;
}

static struct oauthIdentity *googleFetch(char *code, char *redirectUri)
/* Complete the Google code exchange and fetch the user's identity, or NULL on failure. */
{
struct dyString *body = tokenExchangeBody(OAUTH_PROVIDER_GOOGLE, code, redirectUri);
struct jsonElement *tok = postForm("https://oauth2.googleapis.com/token", body->string);
dyStringFree(&body);
if (tok == NULL)
    return NULL;
char *accessToken = jsonOptionalStringField(tok, "access_token", NULL);
if (isEmpty(accessToken))
    return NULL;

/* Fetch user info directly from Google over TLS.  Because the response comes straight
 * from Google, we don't need to verify the id_token's JWT signature ourselves. */
struct dyString *header = dyStringNew(256);
dyStringPrintf(header, "Authorization: Bearer %s\r\n", accessToken);
dyStringPrintf(header, "Accept: application/json\r\n");
char *infoText = httpRequest("https://openidconnect.googleapis.com/v1/userinfo", "GET",
                             header->string, NULL);
dyStringFree(&header);
struct jsonElement *info = jsonParseSafe(infoText);
freeMem(infoText);
if (info == NULL)
    return NULL;
char *sub = jsonOptionalStringField(info, "sub", NULL);
if (isEmpty(sub))
    return NULL;

struct oauthIdentity *id;
AllocVar(id);
id->provider = cloneString(OAUTH_PROVIDER_GOOGLE);
id->subject = cloneString(sub);
id->email = cloneString(jsonOptionalStringField(info, "email", NULL));
id->emailVerified = jsonOptionalBooleanField(info, "email_verified", FALSE);
id->displayName = cloneString(jsonOptionalStringField(info, "name", NULL));
return id;
}

static struct oauthIdentity *orcidFetch(char *code, char *redirectUri)
/* Complete the ORCID code exchange and read the identity from the token response, or NULL.
 * ORCID's token response carries the ORCID iD ('orcid') and the user's name directly;
 * it does not release an email address, so identity->email stays NULL. */
{
char tokenUrl[256];
safef(tokenUrl, sizeof(tokenUrl), "%s/oauth/token", orcidBase());
struct dyString *body = tokenExchangeBody(OAUTH_PROVIDER_ORCID, code, redirectUri);
struct jsonElement *tok = postForm(tokenUrl, body->string);
dyStringFree(&body);
if (tok == NULL)
    return NULL;
char *orcid = jsonOptionalStringField(tok, "orcid", NULL);
if (isEmpty(orcid))
    return NULL;

struct oauthIdentity *id;
AllocVar(id);
id->provider = cloneString(OAUTH_PROVIDER_ORCID);
id->subject = cloneString(orcid);
id->email = NULL;
id->emailVerified = FALSE;
id->displayName = cloneString(jsonOptionalStringField(tok, "name", NULL));
return id;
}

struct oauthIdentity *oauthFetchIdentity(char *provider, char *code, char *redirectUri)
/* Exchange code for tokens and fetch the authenticated identity, or NULL on any failure. */
{
if (isEmpty(code) || !oauthProviderEnabled(provider))
    return NULL;
if (sameString(provider, OAUTH_PROVIDER_GOOGLE))
    return googleFetch(code, redirectUri);
if (sameString(provider, OAUTH_PROVIDER_ORCID))
    return orcidFetch(code, redirectUri);
return NULL;
}

void oauthIdentityFree(struct oauthIdentity **pId)
/* Free an oauthIdentity. */
{
struct oauthIdentity *id = *pId;
if (id != NULL)
    {
    freeMem(id->provider);
    freeMem(id->subject);
    freeMem(id->email);
    freeMem(id->displayName);
    freez(pId);
    }
}

/* oauthLogin - social login for hgLogin via OAuth 2.0 / OpenID Connect.
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

struct oauthProvider
/* One configured social login provider, built from hg.conf. */
    {
    struct oauthProvider *next;
    char *name;             /* short key, used in URLs and gbMemberIdentity.provider */
    char *label;            /* button label */
    char *type;             /* "oidc" or "github" */
    char *clientId;
    char *clientSecret;
    char *authUrl;          /* authorization endpoint */
    char *tokenUrl;         /* token endpoint */
    char *userinfoUrl;      /* userinfo endpoint */
    char *scopes;           /* space-separated scopes */
    char *issuer;           /* OIDC issuer, for endpoint discovery */
    boolean discovered;     /* TRUE once discovery has run (avoid repeating) */
    };

/* Provider list is built once per process and cached.  Nothing here is freed: like the rest
 * of hgLogin these live for the life of the (short) CGI request. */
static struct oauthProvider *providerCache = NULL;
static boolean providerCacheDone = FALSE;

static char *provCfg(char *name, char *field)
/* Return hg.conf login.oauth.<name>.<field>, falling back to the older login.<name>.<field>. */
{
char key[256];
safef(key, sizeof(key), "login.oauth.%s.%s", name, field);
char *val = cfgOption(key);
if (isEmpty(val))
    {
    safef(key, sizeof(key), "login.%s.%s", name, field);
    val = cfgOption(key);
    }
return val;
}

static char *cfgTrim(char *name, char *field)
/* Like provCfg, but with surrounding whitespace removed and NULL for empty.  A stray trailing
 * space in an hg.conf value (e.g. on an issuer or endpoint) would otherwise corrupt the URLs
 * built from it. */
{
char *val = provCfg(name, field);
if (isEmpty(val))
    return NULL;
return trimSpaces(cloneString(val));
}

static void fillBuiltinDefaults(struct oauthProvider *p)
/* For well-known provider names, fill in type/label/endpoints/scopes that were not set
 * explicitly in hg.conf. */
{
if (sameWord(p->name, "google"))
    {
    if (isEmpty(p->type))        p->type = "oidc";
    if (isEmpty(p->label))       p->label = "Google";
    if (isEmpty(p->authUrl))     p->authUrl = "https://accounts.google.com/o/oauth2/v2/auth";
    if (isEmpty(p->tokenUrl))    p->tokenUrl = "https://oauth2.googleapis.com/token";
    if (isEmpty(p->userinfoUrl)) p->userinfoUrl = "https://openidconnect.googleapis.com/v1/userinfo";
    if (isEmpty(p->scopes))      p->scopes = "openid email profile";
    }
else if (sameWord(p->name, "orcid"))
    {
    if (isEmpty(p->type))        p->type = "oidc";
    if (isEmpty(p->label))       p->label = "ORCID";
    if (isEmpty(p->authUrl))     p->authUrl = "https://orcid.org/oauth/authorize";
    if (isEmpty(p->tokenUrl))    p->tokenUrl = "https://orcid.org/oauth/token";
    if (isEmpty(p->userinfoUrl)) p->userinfoUrl = "https://orcid.org/oauth/userinfo";
    if (isEmpty(p->scopes))      p->scopes = "openid";
    }
else if (sameWord(p->name, "github"))
    {
    if (isEmpty(p->type))        p->type = "github";
    if (isEmpty(p->label))       p->label = "GitHub";
    if (isEmpty(p->authUrl))     p->authUrl = "https://github.com/login/oauth/authorize";
    if (isEmpty(p->tokenUrl))    p->tokenUrl = "https://github.com/login/oauth/access_token";
    if (isEmpty(p->userinfoUrl)) p->userinfoUrl = "https://api.github.com/user";
    if (isEmpty(p->scopes))      p->scopes = "read:user user:email";
    }
}

static struct oauthProvider *newProvider(char *name)
/* Build a provider from its hg.conf block, or NULL if clientId/clientSecret are missing. */
{
struct oauthProvider *p;
AllocVar(p);
p->name = cloneString(name);
p->label = cfgTrim(name, "label");
p->type = cfgTrim(name, "type");
p->clientId = cfgTrim(name, "clientId");
p->clientSecret = cfgTrim(name, "clientSecret");
p->authUrl = cfgTrim(name, "authUrl");
p->tokenUrl = cfgTrim(name, "tokenUrl");
p->userinfoUrl = cfgTrim(name, "userinfoUrl");
p->scopes = cfgTrim(name, "scopes");
p->issuer = cfgTrim(name, "issuer");
fillBuiltinDefaults(p);
if (isEmpty(p->type))
    p->type = "oidc";
if (isEmpty(p->label))
    p->label = p->name;
if (isEmpty(p->scopes))
    p->scopes = "openid email profile";
if (isEmpty(p->clientId) || isEmpty(p->clientSecret))
    return NULL;
return p;
}

static void addProviderName(struct slName **pList, char *name)
/* Append name to the list if not already present and not blank. */
{
if (isEmpty(name))
    return;
if (!slNameInList(*pList, name))
    slNameAddTail(pList, name);
}

static struct oauthProvider *loadProviders()
/* Build the provider list from hg.conf: the login.oauth.providers list plus any of the
 * well-known names (google/orcid/github) that carry credentials via the legacy keys. */
{
struct slName *names = NULL;
struct slName *listed = slNameListFromComma(cfgOption("login.oauth.providers")), *n;
for (n = listed;  n != NULL;  n = n->next)
    addProviderName(&names, trimSpaces(n->name));
char *known[] = {"google", "orcid", "github"};
int i;
for (i = 0;  i < ArraySize(known);  i++)
    if (isNotEmpty(provCfg(known[i], "clientId")))
        addProviderName(&names, known[i]);

struct oauthProvider *list = NULL;
for (n = names;  n != NULL;  n = n->next)
    {
    struct oauthProvider *p = newProvider(n->name);
    if (p != NULL)
        slAddHead(&list, p);
    }
slReverse(&list);
return list;
}

static struct oauthProvider *getProviders()
/* Return the cached provider list, building it on first use. */
{
if (!providerCacheDone)
    {
    providerCache = loadProviders();
    providerCacheDone = TRUE;
    }
return providerCache;
}

static struct oauthProvider *providerByName(char *name)
/* Return the configured provider with this name, or NULL. */
{
struct oauthProvider *p;
for (p = getProviders();  p != NULL;  p = p->next)
    if (sameString(p->name, name))
        return p;
return NULL;
}

boolean oauthAnyProviderEnabled()
/* Return TRUE if at least one social login provider is configured. */
{
return (getProviders() != NULL);
}

boolean oauthProviderEnabled(char *name)
/* Return TRUE if the named provider is configured. */
{
return (isNotEmpty(name) && providerByName(name) != NULL);
}

struct slName *oauthProviderNames()
/* Return the short names of all configured providers, in hg.conf order. */
{
struct slName *names = NULL;
struct oauthProvider *p;
for (p = getProviders();  p != NULL;  p = p->next)
    slNameAddTail(&names, p->name);
return names;
}

char *oauthProviderLabel(char *name)
/* Return the display label for a provider (falls back to the name). */
{
struct oauthProvider *p = providerByName(name);
return (p != NULL) ? p->label : name;
}

/* ---- HTTP helpers ---- */

static char *httpRequest(char *url, char *method, char *header, char *body)
/* Make an HTTP(S) request and return the response body (allocd), or NULL on failure. */
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

static struct jsonElement *httpGetJson(char *url, char *bearer)
/* GET url with an Authorization: Bearer header (and a User-Agent, which GitHub requires) and
 * return the parsed JSON response, or NULL. */
{
struct dyString *header = dyStringNew(256);
dyStringPrintf(header, "Authorization: Bearer %s\r\n", bearer);
dyStringPrintf(header, "Accept: application/json\r\n");
dyStringPrintf(header, "User-Agent: UCSC-Genome-Browser\r\n");
char *body = httpRequest(url, "GET", header->string, NULL);
dyStringFree(&header);
struct jsonElement *json = jsonParseSafe(body);
freeMem(body);
return json;
}

static char *formValue(char *body, char *name)
/* Return the URL-decoded value of name in an x-www-form-urlencoded body, or NULL.  Allocd. */
{
char *dupe = cloneString(body);
char *result = NULL;
int n = countChars(dupe, '&') + 1;
char **pairs;
AllocArray(pairs, n);
n = chopByChar(dupe, '&', pairs, n);
int i;
for (i = 0;  i < n;  i++)
    {
    char *eq = strchr(pairs[i], '=');
    if (eq == NULL)
        continue;
    *eq = '\0';
    if (sameString(pairs[i], name))
        {
        char *val = eq + 1;
        cgiDecode(val, val, strlen(val));
        result = cloneString(val);
        break;
        }
    }
freeMem(pairs);
freeMem(dupe);
return result;
}

static void ensureEndpoints(struct oauthProvider *p)
/* For an OIDC provider configured with only an issuer, fetch the discovery document once and
 * fill in any endpoints that were not set explicitly. */
{
if (p->discovered || !sameWord(p->type, "oidc") || isEmpty(p->issuer))
    return;
p->discovered = TRUE;
if (isNotEmpty(p->authUrl) && isNotEmpty(p->tokenUrl) && isNotEmpty(p->userinfoUrl))
    return;
char url[1024];
safef(url, sizeof(url), "%s/.well-known/openid-configuration", p->issuer);
struct jsonElement *j = jsonParseSafe(httpRequest(url, "GET", "Accept: application/json\r\n", NULL));
if (j == NULL)
    {
    fprintf(stderr, "hgLogin oauth: OIDC discovery failed for provider '%s' at %s "
        "(check login.oauth.%s.issuer)\n", p->name, url, p->name);
    return;
    }
if (isEmpty(p->authUrl))
    p->authUrl = cloneString(jsonOptionalStringField(j, "authorization_endpoint", NULL));
if (isEmpty(p->tokenUrl))
    p->tokenUrl = cloneString(jsonOptionalStringField(j, "token_endpoint", NULL));
if (isEmpty(p->userinfoUrl))
    p->userinfoUrl = cloneString(jsonOptionalStringField(j, "userinfo_endpoint", NULL));
}

char *oauthLoginUrl(char *name, char *redirectUri, char *state)
/* Return the provider's authorization URL to redirect the browser to, or NULL. */
{
struct oauthProvider *p = providerByName(name);
if (p == NULL)
    return NULL;
ensureEndpoints(p);
if (isEmpty(p->authUrl))
    return NULL;
struct dyString *dy = dyStringNew(512);
dyStringPrintf(dy, "%s?response_type=code", p->authUrl);
dyStringPrintf(dy, "&scope=%s", cgiEncode(p->scopes));
dyStringPrintf(dy, "&client_id=%s", cgiEncode(p->clientId));
dyStringPrintf(dy, "&redirect_uri=%s", cgiEncode(redirectUri));
dyStringPrintf(dy, "&state=%s", cgiEncode(state));
if (sameWord(p->name, "google"))
    dyStringPrintf(dy, "&prompt=select_account");
return dyStringCannibalize(&dy);
}

static struct dyString *tokenExchangeBody(struct oauthProvider *p, char *code, char *redirectUri)
/* Build the shared authorization_code token-exchange POST body. */
{
struct dyString *body = dyStringNew(512);
dyStringPrintf(body, "grant_type=authorization_code");
dyStringPrintf(body, "&code=%s", cgiEncode(code));
dyStringPrintf(body, "&client_id=%s", cgiEncode(p->clientId));
dyStringPrintf(body, "&client_secret=%s", cgiEncode(p->clientSecret));
dyStringPrintf(body, "&redirect_uri=%s", cgiEncode(redirectUri));
return body;
}

static char *tokenExchange(struct oauthProvider *p, char *code, char *redirectUri)
/* Run the code->token exchange and return the access_token, or NULL.  The response may be
 * JSON (Google, ORCID) or x-www-form-urlencoded (GitHub's default), so try both. */
{
struct dyString *reqBody = tokenExchangeBody(p, code, redirectUri);
struct dyString *header = dyStringNew(256);
dyStringPrintf(header, "Content-Type: application/x-www-form-urlencoded\r\n");
dyStringPrintf(header, "Accept: application/json\r\n");
dyStringPrintf(header, "User-Agent: UCSC-Genome-Browser\r\n");
dyStringPrintf(header, "Content-Length: %d\r\n", (int)reqBody->stringSize);
char *resp = httpRequest(p->tokenUrl, "POST", header->string, reqBody->string);
dyStringFree(&header);
dyStringFree(&reqBody);
if (isEmpty(resp))
    return NULL;
char *access = NULL;
struct jsonElement *tok = jsonParseSafe(resp);
if (tok != NULL)
    access = cloneString(jsonOptionalStringField(tok, "access_token", NULL));
if (isEmpty(access))
    access = formValue(resp, "access_token");
freeMem(resp);
return access;
}

static boolean jsonFieldIsTrue(struct jsonElement *obj, char *field)
/* Read a boolean-ish field tolerantly: a JSON boolean true, or the string "true"/"1".
 * The OIDC spec says email_verified is a boolean, but some providers send it as a string;
 * jsonOptionalBooleanField would abort on that, so read the type ourselves. */
{
struct jsonElement *el = jsonFindNamedField(obj, "", field);
if (el == NULL)
    return FALSE;
if (el->type == jsonBoolean)
    return el->val.jeBoolean;
if (el->type == jsonString)
    return sameWord(el->val.jeString, "true") || sameString(el->val.jeString, "1");
return FALSE;
}

static struct oauthIdentity *oidcFetch(struct oauthProvider *p, char *code, char *redirectUri)
/* OpenID Connect: exchange code, then read the standard claims from the userinfo endpoint.
 * Works directly over TLS with the provider, so we don't verify the id_token signature. */
{
char *accessToken = tokenExchange(p, code, redirectUri);
if (isEmpty(accessToken))
    return NULL;
struct jsonElement *info = httpGetJson(p->userinfoUrl, accessToken);
if (info == NULL)
    return NULL;
char *sub = jsonOptionalStringField(info, "sub", NULL);
if (isEmpty(sub))
    return NULL;
struct oauthIdentity *id;
AllocVar(id);
id->provider = cloneString(p->name);
id->subject = cloneString(sub);
id->email = cloneString(jsonOptionalStringField(info, "email", NULL));
id->emailVerified = jsonFieldIsTrue(info, "email_verified");
id->displayName = cloneString(jsonOptionalStringField(info, "name", NULL));
return id;
}

static void githubBestEmail(char *accessToken, char **retEmail, boolean *retVerified)
/* Query GitHub's /user/emails and return the primary verified email, if any. */
{
*retEmail = NULL;
*retVerified = FALSE;
struct jsonElement *emails = httpGetJson("https://api.github.com/user/emails", accessToken);
if (emails == NULL || emails->type != jsonList)
    // GitHub returns an object (not an array) on error, e.g. a token without the user:email
    // scope.  Treat that as "no email available" and let login proceed without one.
    return;
struct slRef *list = jsonListVal(emails, "emails"), *ref;
for (ref = list;  ref != NULL;  ref = ref->next)
    {
    struct jsonElement *el = ref->val;
    if (jsonFieldIsTrue(el, "primary"))
        {
        *retEmail = cloneString(jsonOptionalStringField(el, "email", NULL));
        *retVerified = jsonFieldIsTrue(el, "verified");
        return;
        }
    }
}

static struct oauthIdentity *githubFetch(struct oauthProvider *p, char *code, char *redirectUri)
/* GitHub (plain OAuth2, not OIDC): exchange code, then read the profile from /user and the
 * primary verified email from /user/emails. */
{
char *accessToken = tokenExchange(p, code, redirectUri);
if (isEmpty(accessToken))
    return NULL;
struct jsonElement *info = httpGetJson(p->userinfoUrl, accessToken);
if (info == NULL)
    return NULL;
struct jsonElement *idEl = jsonFindNamedField(info, "", "id");
if (idEl == NULL)
    return NULL;
char subject[64];
safef(subject, sizeof(subject), "%lld", (long long)jsonNumberVal(idEl, "id"));

struct oauthIdentity *id;
AllocVar(id);
id->provider = cloneString(p->name);
id->subject = cloneString(subject);
id->displayName = cloneString(jsonOptionalStringField(info, "name", NULL));
if (isEmpty(id->displayName))
    id->displayName = cloneString(jsonOptionalStringField(info, "login", NULL));
githubBestEmail(accessToken, &id->email, &id->emailVerified);
return id;
}

struct oauthIdentity *oauthFetchIdentity(char *name, char *code, char *redirectUri)
/* Exchange code for tokens and fetch the authenticated identity, or NULL on any failure. */
{
struct oauthProvider *p = providerByName(name);
if (p == NULL || isEmpty(code))
    return NULL;
ensureEndpoints(p);
if (isEmpty(p->tokenUrl) || isEmpty(p->userinfoUrl))
    return NULL;
/* Catch any errAbort raised while reading unexpected JSON shapes from the provider (the
 * jsonXxxVal accessors abort on a type mismatch), so a misbehaving provider yields a clean
 * "login failed" rather than an error page. */
struct oauthIdentity *id = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (sameWord(p->type, "github"))
        id = githubFetch(p, code, redirectUri);
    else
        id = oidcFetch(p, code, redirectUri);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    fprintf(stderr, "hgLogin oauth: identity fetch for %s failed: %s\n", name, errCatch->message->string);
    id = NULL;
    }
errCatchFree(&errCatch);
return id;
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

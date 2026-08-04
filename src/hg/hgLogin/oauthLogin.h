/* oauthLogin - social login for hgLogin via OAuth 2.0 / OpenID Connect.
 *
 * Providers are configured entirely in hg.conf.  List the ones to offer with:
 *   login.oauth.providers = google,orcid,github,myuni
 * and give each a block of settings:
 *   login.oauth.<name>.label        Button text (defaults to <name>)
 *   login.oauth.<name>.clientId      OAuth client id      (required)
 *   login.oauth.<name>.clientSecret  OAuth client secret  (required)
 *   login.oauth.<name>.type          "oidc" (default) or "github"
 *   login.oauth.<name>.issuer        OIDC issuer; endpoints are auto-discovered from
 *                                    <issuer>/.well-known/openid-configuration
 *   login.oauth.<name>.authUrl       Explicit endpoints (used when there is no issuer,
 *   login.oauth.<name>.tokenUrl        or to override discovery)
 *   login.oauth.<name>.userinfoUrl
 *   login.oauth.<name>.scopes        Space-separated (default "openid email profile")
 *
 * "google", "orcid" and "github" are known names with built-in endpoints, so those only
 * need clientId/clientSecret.  The older login.<name>.clientId/clientSecret keys are still
 * honored.  A provider is offered only when both its clientId and clientSecret are set. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef OAUTHLOGIN_H
#define OAUTHLOGIN_H

struct oauthIdentity
/* An authenticated identity returned by an external OAuth/OpenID provider. */
    {
    struct oauthIdentity *next;
    char *provider;         /* provider short name, e.g. "google" */
    char *subject;          /* stable, unique id from the provider */
    char *email;            /* email reported by provider, or NULL */
    boolean emailVerified;  /* TRUE if the provider asserts the email is verified */
    char *displayName;      /* full name from provider, or NULL */
    };

boolean oauthAnyProviderEnabled();
/* Return TRUE if at least one social login provider is configured. */

boolean oauthProviderEnabled(char *name);
/* Return TRUE if the named provider is configured (clientId and clientSecret set). */

struct slName *oauthProviderNames();
/* Return the short names of all configured providers, in the order listed in hg.conf.
 * Do not free (owned by an internal cache). */

char *oauthProviderLabel(char *name);
/* Return the display label for a provider (falls back to the name).  Do not free. */

char *oauthLoginUrl(char *name, char *redirectUri, char *state);
/* Return the provider's authorization URL to redirect the browser to, or NULL.  Allocd. */

struct oauthIdentity *oauthFetchIdentity(char *name, char *code, char *redirectUri);
/* Exchange the authorization code for tokens and fetch the authenticated identity, or NULL
 * on any failure.  Dispose of the result with oauthIdentityFree(). */

void oauthIdentityFree(struct oauthIdentity **pId);
/* Free an oauthIdentity. */

#endif /* OAUTHLOGIN_H */

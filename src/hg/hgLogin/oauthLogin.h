/* oauthLogin - social login (Google, ORCID) for hgLogin via OAuth 2.0 / OpenID Connect.
 *
 * All provider configuration comes from hg.conf:
 *   login.google.clientId, login.google.clientSecret
 *   login.orcid.clientId,  login.orcid.clientSecret
 *   login.orcid.sandbox    (optional, "on" to use sandbox.orcid.org)
 * A provider is "enabled" only when both its clientId and clientSecret are set,
 * so mirrors without credentials simply don't see the buttons. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef OAUTHLOGIN_H
#define OAUTHLOGIN_H

/* hg.conf option prefixes. Full names are built as login.<provider>.<field>. */
#define CFG_LOGIN_OAUTH_PREFIX "login."
#define OAUTH_PROVIDER_GOOGLE "google"
#define OAUTH_PROVIDER_ORCID  "orcid"

struct oauthIdentity
/* An authenticated identity returned by an external OAuth/OpenID provider. */
    {
    struct oauthIdentity *next;
    char *provider;         /* "google" or "orcid" */
    char *subject;          /* stable, unique id from the provider (Google 'sub', ORCID iD) */
    char *email;            /* email reported by provider, or NULL */
    boolean emailVerified;  /* TRUE if the provider asserts the email is verified */
    char *displayName;      /* full name from provider, or NULL */
    };

boolean oauthProviderEnabled(char *provider);
/* Return TRUE if both clientId and clientSecret for provider are set in hg.conf. */

boolean oauthAnyProviderEnabled();
/* Return TRUE if at least one social login provider is configured. */

char *oauthLoginUrl(char *provider, char *redirectUri, char *state);
/* Return the provider's authorization-endpoint URL to redirect the browser to.
 * redirectUri must exactly match the URI registered with the provider (the hgLogin URL).
 * state is an opaque anti-CSRF nonce that the provider echoes back.
 * Returns NULL if provider is unknown or not enabled.  Result is allocd here. */

struct oauthIdentity *oauthFetchIdentity(char *provider, char *code, char *redirectUri);
/* Exchange the authorization code for tokens at the provider's token endpoint, then
 * fetch the user's identity.  Return the identity, or NULL on any failure.
 * Dispose of the result with oauthIdentityFree(). */

void oauthIdentityFree(struct oauthIdentity **pId);
/* Free an oauthIdentity. */

#endif /* OAUTHLOGIN_H */

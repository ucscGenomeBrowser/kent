/* Connect via https. */

#ifndef NET_HTTPS_H
#define NET_HTTPS_H

int netConnectHttps(char *hostName, int port, boolean noProxy, char *httpProtocol);
/* Return socket for https connection with server or -1 if error. */

void httpsSetCertCheck(char *mode);
/* Pin the TLS certificate-check mode ("abort", "warn", or "log") for every HTTPS connection this
 * process makes from here on, overriding hg.conf's httpsCertCheck and the https_cert_check env
 * var.  Use where a caller must never accept an unverified certificate however the site is
 * configured.  Order-independent: safe to call before or after the first HTTPS connection. */

#endif//ndef NET_HTTPS_H

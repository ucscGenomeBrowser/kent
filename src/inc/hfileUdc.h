/* hfileUdc -- hfile backend that routes HTTP/HTTPS/FTP through UDC.
 * Replaces legacy knetUdc.c / knetfile hook approach. */

#ifndef HFILEUDC_H
#define HFILEUDC_H

void hfileUdcInstall();
/* Install UDC as the hfile backend for HTTP/HTTPS/FTP URLs.
 * Call once at startup; replaces legacy knetUdcInstall(). */

#endif /* HFILEUDC_H */
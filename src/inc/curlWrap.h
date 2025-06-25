/* thin wrapper around curl, to make it easier to request strings over HTTP */

#ifndef CURLWRAP_H
#define CURLWRAP_H

char* curlPostUrl(char *url, char *data);
/* post data to URL and return as string. Must be freed. */

#endif /* UDC_H */

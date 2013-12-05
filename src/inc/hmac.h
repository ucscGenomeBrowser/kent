/* Calculate an openssl keyed-hash message authentication code (HMAC) */

#ifndef HMAC_H
#define HMAC_H

char *hmacSha1(char *key, char *data);
/* Calculate a openssl SHA1 keyed-hash message authentication code (HMAC) */

char *hmacMd5(char *key, char *data);
/* Calculate a openssl MD5 keyed-hash message authentication code (HMAC) */

#endif /* HMAC_H */

/* uuid - stuff to make working with uuid's a little bit easier.  These are the things
 * that look like 08965457-c5aa-43de-894c-cc01483a5691 in ascii, or as 16 unsigned chars
 * in binary.  Uuid stands for universally unique ID, and though they are a bit ugly
 * and hard to type or even cut an paste, they do make very effective nonredundant IDs.
 *
 * If you use this file you need to add -luuid to your link line. */

#include <uuid/uuid.h>

#define UUID_STRING_SIZE 37

char *makeUuidString(char buf[UUID_STRING_SIZE]);
/* Generate a random uuid and put it in the usual hex-plus-dashes form */

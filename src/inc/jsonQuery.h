/* jsonQuery - simple path syntax for retrieving specific descendants of a jsonElement. */

/* Path examples:
 *
 * "" --> the queried object itself (no-op)
 *
 * "name" --> the value of the "name" field/child of queried object
 *
 * "[0]" --> first element of the queried list
 *
 * "[*]" --> all elements of the queried list
 *
 * "name1.name2[4]" --> 5th element of list that is the value for "name2" child of object
 * that is the value for "name1" child of queried object
 *
 * "primary_snapshot_data.support[id.type=subsnp].submitter_handle" --> dbSNP submitter handles
 * for subsnp (not frequency) submissions
 *
 * See lib/tests/input/json* and lib/tests/expected/json* for more examples with both JSON & paths.
 */

#ifndef JSONQUERY_H
#define JSONQUERY_H

#include "jsonParse.h"

struct slRef *jsonQueryElement(struct jsonElement *el, char *name, char *path, struct lm *lm);
/* Return a ref list of jsonElement descendants of el that match path.
 * name is for error reporting. */

struct slRef *jsonQueryElementList(struct slRef *inList, char *name, char *path, struct lm *lm);
/* Return a ref list of jsonElement descendants matching path of all jsonElements in inList.
 * name is for error reporting. */

char *jsonQueryString(struct jsonElement *el, char *name, char *path, struct lm *lm);
/* Alloc & return the string value at the end of path in el. May be NULL. */

long jsonQueryInt(struct jsonElement *el, char *name, char *path, long defaultVal, struct lm *lm);
/* Return the int value at path in el, or defaultVal if not found. */

boolean jsonQueryBoolean(struct jsonElement *el, char *name, char *path, boolean defaultVal,
                         struct lm *lm);
/* Return the boolean value at path in el, or defaultVal if not found. */

struct slName *jsonQueryStrings(struct jsonElement *el, char *name, char *path, struct lm *lm);
/* Alloc & return a list of string values matching path in el. May be NULL. */

struct slInt *jsonQueryInts(struct jsonElement *el, char *name, char *path, struct lm *lm);
/* Alloc & return a list of int values matching path in el. May be NULL. */

struct slName *jsonQueryStringList(struct slRef *inList, char *name, char *path, struct lm *lm);
/* Alloc & return a list of string values matching path in all elements of inList. May be NULL. */

struct slInt *jsonQueryIntList(struct slRef *inList, char *name, char *path, struct lm *lm);
/* Alloc & return a list of int values matching path in all elements of inList. May be NULL. */

#endif /* JSONQUERY_H */

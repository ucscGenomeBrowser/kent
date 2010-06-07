/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* keys.h - Stuff to manage a little key/value table and
 * evaluate expressions on it. */
#ifndef KEYS_H
#define KEYS_H

struct keyVal
/* A key/value pair of strings. */
    {
    char *key;
    char *val;
    };

struct kvt *newKvt(int size);
/* Get a new key value table. */

void freeKvt(struct kvt **pKvt);
/* Free up key value table. */

void kvtClear(struct kvt *kvt);
/* Clear the keys table. */

struct keyVal *kvtAdd(struct kvt *kvt, char *key, char *val);
/* Add in new key. */

struct keyVal* kvtGet(struct kvt *kvt, char *key);
/* get the keyVal for the specified key, of NULL if not found. */

char *kvtLookup(struct kvt *kvt, char *key);
/* Search table for key.  Return key value, or NULL if
 * key not found. */

void kvtWriteAll(struct kvt *kvt, FILE *f, struct slName *hideList);
/* Write all keys to file except the ones in hideList */

void kvtParseAdd(struct kvt *kvt, char *text);
/* Add in keys from text.  Text is in format:
 *     key val
 * for each line of text. Text gets many of it's
 * space characters and newlines replaced by 0's
 * and should persist until call to keysClear(). */

struct keyExp
/* A handle on a parsed expression which can be
 * quickly evaluated.  */
    {
    void *rootExp;       /* Internally struct exp. */
    void *tokenList;     /* Internally struct tok. */
    };

boolean keyExpEval(struct keyExp *exp, struct kvt *kvt);
/* Recursively evaluate expression. */

struct keyExp *keyExpParse(char *text);
/* Parse text into key expression.  Squawk and die if it
 * fails. */

boolean keyTextScan(char *text, char *key, char *valBuf, int valBufSize);
/* Get value of key in text. Return FALSE if key doesn't exist. */

#endif /* KEYS_H */


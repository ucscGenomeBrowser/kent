/* hmacTest - check hmacMd5 and hmacSha1 against known answers, and check the one
 * property hgLogin depends on.
 *
 * hgLogin signs a pending social identity with hmacMd5(login.cookieSalt, "provider|id|email")
 * and hands the signature to the browser in the account chooser.  If the signature can be
 * forged, an attacker picks whose account the chooser links to, so this is a security
 * boundary with no visible symptom: a forged signature and a real one produce the same page.
 *
 * Before #37984 that signature was a plain MD5 of the salt concatenated in front of the same
 * fields.  A plain hash of a secret followed by attacker-chosen text is the wrong shape for
 * the job, and the test that says so is boundaryMoves() below: with concatenation, moving a
 * character from the end of the key to the front of the data leaves the hashed bytes
 * identical, so two different (key, data) pairs sign the same.  HMAC keeps them apart, and a
 * change that quietly went back to concatenation would fail there rather than in a page
 * nobody can tell apart.
 *
 * The known answers are RFC 2202 test case 2 for both algorithms, cross-checked on this
 * machine with `openssl dgst -md5 -hmac Jefe`.  Only the string-keyed cases from the RFC are
 * usable here, because this interface takes the key and the data as C strings and measures
 * both with strlen, so a vector with an embedded zero or a 0x0b key cannot be expressed.
 *
 * refs #37984 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hmac.h"

static int errCount = 0;

static void expect(char *what, char *got, char *want)
/* Print the answer, and say so when it is not the one expected.  Printing every line rather
 * than only the failures keeps the expected/ file a readable record of what this pins. */
{
printf("%-34s %s\n", what, got);
if (!sameString(got, want))
    {
    printf("    FAIL: expected %s\n", want);
    ++errCount;
    }
}

static void knownAnswers()
/* RFC 2202 test case 2, the one whose key and data are both plain text. */
{
char *key = "Jefe";
char *data = "what do ya want for nothing?";
expect("md5  rfc2202 case 2", hmacMd5(key, data),
       "750c783e6ab0b503eaa86e310a5db738");
expect("sha1 rfc2202 case 2", hmacSha1(key, data),
       "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
}

static void boundaryMoves()
/* The property hgLogin's signature rests on: the split between key and data is part of what
 * is signed.  Concatenating the two, which is what the code did before #37984, makes these
 * two calls hash the same bytes. */
{
char *ab = hmacMd5("a", "bc");
char *aB = hmacMd5("ab", "c");
printf("%-34s %s\n", "md5  key a  data bc", ab);
printf("%-34s %s\n", "md5  key ab data c", aB);
if (sameString(ab, aB))
    {
    printf("    FAIL: the key/data boundary is not being signed\n");
    ++errCount;
    }
else
    printf("%-34s differ, as they must\n", "    the two");
}

static void keyMatters()
/* A signature that does not depend on the whole key is not a signature.  One bit of the salt
 * has to change the answer. */
{
char *withSalt = hmacMd5("s3cret", "google|12345|a@example.org");
char *withOther = hmacMd5("s3crat", "google|12345|a@example.org");
printf("%-34s %s\n", "md5  salt s3cret", withSalt);
printf("%-34s %s\n", "md5  salt s3crat", withOther);
if (sameString(withSalt, withOther))
    {
    printf("    FAIL: the key does not reach the answer\n");
    ++errCount;
    }
}

static void shapeOfTheAnswer()
/* hgLogin puts the signature in a form field and compares it with sameString, so the text has
 * to be exactly this: lower case hex, no prefix, and the full width of the digest.  The
 * buffers in hmac.c are sized for exactly these lengths. */
{
char *md5 = hmacMd5("k", "d");
char *sha1 = hmacSha1("k", "d");
printf("%-34s %d\n", "md5  length", (int)strlen(md5));
printf("%-34s %d\n", "sha1 length", (int)strlen(sha1));
if (strlen(md5) != 32 || strlen(sha1) != 40)
    {
    printf("    FAIL: expected 32 and 40\n");
    ++errCount;
    }
int i;
for (i = 0;  md5[i] != '\0';  ++i)
    if (strchr("0123456789abcdef", md5[i]) == NULL)
        {
        printf("    FAIL: '%c' is not lower case hex\n", md5[i]);
        ++errCount;
        break;
        }
}

static void emptyData()
/* An empty string is a legitimate thing to sign, and it must not come back empty or equal to
 * the signature of anything else. */
{
char *empty = hmacMd5("k", "");
printf("%-34s %s\n", "md5  empty data", empty);
if (strlen(empty) != 32 || sameString(empty, hmacMd5("k", "x")))
    {
    printf("    FAIL: empty data signs wrong\n");
    ++errCount;
    }
}

int main(int argc, char *argv[])
{
knownAnswers();
boundaryMoves();
keyMatters();
shapeOfTheAnswer();
emptyData();
printf("\n%d failures\n", errCount);
return errCount == 0 ? 0 : 1;
}

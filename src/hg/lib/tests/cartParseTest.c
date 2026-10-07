/* cartParseTest - check how the cart parser reads a stored cart contents string, and that a
 * cart survives a trip out through cartEncodeState and back.  refs #38343 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "cart.h"
#include "errCatch.h"

static char *cases[] = {
/* the ordinary shape */
"db=hg38&position=chr1:1-1000",
/* an empty pair, at the front, in the middle and at the end.  The first two leave the
 * separator stuck to the front of the name that follows, which is how sessions came to hold
 * names like "&position".  The trailing one used to abort the whole request. */
"&db=hg38&position=chr1:1-1000",
"db=hg38&&position=chr1:1-1000",
"db=hg38&position=chr1:1-1000&&",
/* a pair with a name and no =value, in the same three places.  #38335 fixed this in the CGI
 * parsers; here the first two still run the two names together, because thousands of saved
 * sessions hold a name that was merged that way and reading them any other way would change
 * what they mean.  The trailing one used to abort. */
"g-catV2&db=hg38",
"db=hg38&i&position=chr1:1-1000",
"db=hg38&g-catV2",
/* A setting that names a file the server made for this user is checked before it is stored,
 * and the check matches the name by prefix.  A name with a separator stuck to the front of it
 * matched no prefix and so missed the check; it is matched past the separator now.
 * refs #37623 */
"db=hg38&ctfile_hg38=/somewhere/else",
"db=hg38&&ctfile_hg38=/somewhere/else",
"db=hg38&i&ctfile_hg38=/somewhere/else",
/* A track hub may name a track group anything, so a cart variable name can hold an ampersand
 * of its own.  It arrives correctly, because a form field name is encoded in the request and
 * the CGI parser decodes it, and it has to keep working here. */
"db=hg38&hgtgroup_Tanaka_Cut&Tag_IgG_close=1&position=chr1:1-1000",
/* the same name written the new way, escaped.  An old session and a new one read back to
 * the same setting, which is what lets the two spellings sit side by side with no migration. */
"db=hg38&hgtgroup_Tanaka_Cut%26Tag_IgG_close=1&position=chr1:1-1000",
/* a percent escape that is not one of ours is left alone, so a hub track name survives */
"db=hg38&hub_2089_hIPS%2c%20biol_rep3.CNhs14216_sel=1",
/* an empty value is a value, and is kept */
"db=hg38&position=",
/* nothing at all */
"",
};

/* Names that a real session holds.  The last three are the awkward ones: an ampersand from a
 * hub track group, a plus sign from a hub track name, and a name left over from a URL that
 * arrived with its ampersands written as &amp;. */
static char *roundTrip[][2] = {
    {"db", "hg38"},
    {"position", "chr1:1-1000"},
    {"hgtgroup_Tanaka_Cut&Tag_IgG_close", "1"},
    {"hub_1623_CompRoadmapCD4+CD25-N", "pack"},
    {"amp;g", "refGene"},
    /* a real hub track name whose own text holds a percent escape.  It must come back
     * unchanged, which is why the name encoder leaves a '%' alone unless it starts one of
     * the four sequences the reader knows. */
    {"hub_2089_hIPS%2c%20biol_rep3.CNhs14216_sel", "1"},
    /* the two shapes that do get escaped */
    {"a%26b", "1"},
    {"weird=name", "1"},
};

static struct cart *emptyCart()
/* A cart with nothing in it and no database behind it. */
{
struct cart *cart;
AllocVar(cart);
cart->hash = newHash(8);
cart->exclude = newHash(8);
return cart;
}

static void printCart(struct cart *cart)
/* Print the cart's settings in name order. */
{
struct hashEl *el, *list = hashElListHash(cart->hash);
slSort(&list, hashElCmp);
for (el = list;  el != NULL;  el = el->next)
    printf("[%s=%s]", el->name, (char *)el->val);
hashElFreeList(&list);
}

int main(int argc, char *argv[])
{
int i;
printf("reading a stored contents string\n\n");
for (i = 0;  i < ArraySize(cases);  ++i)
    {
    printf("in  : %s\n", cases[i]);
    struct cart *cart = emptyCart();
    printf("cart: ");
    /* The file-name check writes to stderr.  Flush so that the two streams stay in the same
     * order every run when the test's output is captured with 2>&1. */
    fflush(stdout);
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
	cartParseOverHash(cart, cloneString(cases[i]));
	printCart(cart);
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	printf("aborted: %s", trimSpaces(errCatch->message->string));
    errCatchFree(&errCatch);
    printf("\n\n");
    }

printf("writing a cart out and reading it back\n\n");
struct cart *cart = emptyCart();
for (i = 0;  i < ArraySize(roundTrip);  ++i)
    cartSetString(cart, roundTrip[i][0], roundTrip[i][1]);
struct dyString *dy = dyStringNew(256);
cartEncodeState(cart, dy);
printf("out : %s\n", dy->string);
struct cart *back = emptyCart();
fflush(stdout);
cartParseOverHash(back, cloneString(dy->string));
printf("back: ");
printCart(back);
printf("\n");
for (i = 0;  i < ArraySize(roundTrip);  ++i)
    {
    char *val = hashFindVal(back->hash, roundTrip[i][0]);
    if (val == NULL)
	printf("LOST [%s]\n", roundTrip[i][0]);
    else if (differentString(val, roundTrip[i][1]))
	printf("CHANGED [%s] %s became %s\n", roundTrip[i][0], roundTrip[i][1], val);
    }
printf("\n");
return 0;
}

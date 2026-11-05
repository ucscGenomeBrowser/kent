/* cartHubDatabaseTester - when the database a hub load chose counts as a change of assembly.
 *
 * After the hubs are loaded, cartNew puts the database they chose into the cart, and when that
 * is a change of assembly it sets db in oldVars to "none".  getDbAndGenome reads that as a
 * switch: the position goes to the assembly default and virtMode, virtModeType and
 * nonVirtPosition are dropped.  resolveGenarkDb takes db out of the cart, so before #38184 a
 * session loaded with a GenArk db= always looked like a change, and came up at the default
 * position instead of the one it saved.  Now only an old db that names a different database
 * counts.
 *
 * cartSetHubDatabase holds that decision.  This runs it on a cart with no database behind it,
 * and prints the cart's db and what is left in oldVars.
 *
 * refs #38184 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "cart.h"

#define GENARK "GCF_000001405.40"

static void showCase(char *what, char *cartDb, boolean haveOldVars, char *oldDb,
                     char *newDatabase)
/* Set up a cart and oldVars, apply newDatabase, and print the result. */
{
struct cart *cart = cartOfNothing();
if (cartDb != NULL)
    cartSetString(cart, "db", cartDb);
struct hash *oldVars = NULL;
if (haveOldVars)
    {
    oldVars = hashNew(0);
    if (oldDb != NULL)
        hashAdd(oldVars, "db", oldDb);
    }
cartSetHubDatabase(cart, oldVars, newDatabase);
char *oldAfter = (oldVars == NULL) ? "(no oldVars)" : hashFindVal(oldVars, "db");
printf("%s\n", what);
printf("  cart db %s, oldVars db %s%s\n", cartOptionalString(cart, "db"),
       oldAfter == NULL ? "(none set)" : oldAfter,
       sameOk(oldAfter, "none") ? ", so the position is reset" : "");
}

int main(int argc, char *argv[])
{
showCase("a session on a GenArk assembly, loaded with the same db=", NULL, TRUE, GENARK, GENARK);
showCase("a cart on hg38, loaded with a GenArk db=", NULL, TRUE, "hg38", GENARK);
showCase("no db before at all", NULL, TRUE, NULL, GENARK);
showCase("an empty db before", NULL, TRUE, "", GENARK);
showCase("the cart already holds that db", GENARK, TRUE, "hg38", GENARK);
showCase("the cart holds another db", "hg19", TRUE, GENARK, GENARK);
showCase("no oldVars", NULL, FALSE, NULL, GENARK);
return 0;
}

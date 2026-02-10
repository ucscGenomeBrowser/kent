/* cartDump - Dump contents of cart. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hdb.h"
#include "jsHelper.h"
#include "hui.h"
#include "botDelay.h"

/* for earlyBotCheck() function at the beginning of main() */
#define delayFraction   1.0     /* standard penalty for most CGIs */
static boolean issueBotWarning = FALSE;

#define CART_DUMP_REMOVE_VAR "n/a"
struct hash *oldVars = NULL;

void doMiddle(struct cart *cart)
/* cartDump - Dump contents of cart. */
{
#define MATCH_VAR  "match"

char *vName = "cartDump.varName";
char *vVal = "cartDump.newValue";
char *mName = "cartDump.metaDataId";
char *wildcard;
boolean asTable = cartVarExists(cart,CART_DUMP_AS_TABLE);

if (cgiVarExists("submit"))
    {
    char *varName = cgiOptionalString(vName);
    char *newValue = cgiOptionalString(vVal);
    if (isNotEmpty(varName) && isNotEmpty(newValue))
        {
	varName = skipLeadingSpaces(varName);
	eraseTrailingSpaces(varName);
        if (sameString(newValue, CART_DUMP_REMOVE_VAR) || sameString(newValue, CART_VAR_EMPTY))
	    cartRemove(cart, varName);
	else
	    cartSetString(cart, varName, newValue);
	}
    cartRemove(cart, vVal);
    cartRemove(cart, "submit");
    }
if (cgiVarExists("noDisplay"))
    {
    // update cart vars for a track, called by hgTracks.js and ajax.js
    //  not useful to hackers, so there is no need to call bottleneck.
    char *trackName = cgiOptionalString("g");
    if (trackName != NULL && hashNumEntries(oldVars) > 0)
        {
        char *db = cartString(cart, "db");
        struct trackDb *tdb = hTrackDbForTrack(db, trackName);
        if (tdb != NULL && tdbIsComposite(tdb))
            {
            struct lm *lm = lmInit(0);
            cartTdbTreeCleanupOverrides(tdb,cart,oldVars,lm);
	    lmCleanup(&lm);
            }
        }

    if (cgiVarExists(mName))
        {
        char *mdid = cgiOptionalString(mName);
        char mdid_de_was[1024], mdid_de_now[1024], mdid_dt_was[1024], mdid_dt_now[1024];
        safef(mdid_de_was, sizeof(mdid_de_was), "%s.de_was", mdid);
        safef(mdid_de_now, sizeof(mdid_de_now), "%s.de_now", mdid);
        safef(mdid_dt_was, sizeof(mdid_dt_was), "%s.dt_was", mdid);
        safef(mdid_dt_now, sizeof(mdid_dt_now), "%s.dt_now", mdid);
        // Grab the lists of which de/dt elements were on before and after the user
        // changed settings around
        struct slName *de_was_list = cgiStringList(mdid_de_was);
        struct slName *de_now_list = cgiStringList(mdid_de_now);
        struct slName *dt_was_list = cgiStringList(mdid_dt_was);
        struct slName *dt_now_list = cgiStringList(mdid_dt_now);

        // For faster lookup
        struct hash *de_was_hash = hashFromSlNameList(de_was_list);
        struct hash *de_now_hash = hashFromSlNameList(de_now_list);
        struct hash *dt_was_hash = hashFromSlNameList(dt_was_list);
        struct hash *dt_now_hash = hashFromSlNameList(dt_now_list);

        // Check if we sent dt_was/now variables, indicating that this composite has data types
        boolean hasDataTypes = (dt_was_list != NULL || dt_now_list != NULL);

        char subtrackSetting[1024];

        if (hasDataTypes)
            {
            // Cross-product mode: tracks are identified by mdid_de_dt

            // Turn ON: (de_on x dt_now), (de_now x dt_on)
            // where de_on = de_now - de_was, dt_on = dt_now - dt_was
            for (struct slName *de = de_now_list; de != NULL; de = de->next)
                {
                boolean de_is_new = (hashLookup(de_was_hash, de->name) == NULL);
                for (struct slName *dt = dt_now_list; dt != NULL; dt = dt->next)
                    {
                    boolean dt_is_new = (hashLookup(dt_was_hash, dt->name) == NULL);
                    if (de_is_new || dt_is_new)
                        {
                        safef(subtrackSetting, sizeof(subtrackSetting),
                              "%s_%s_%s_sel", mdid, de->name, dt->name);
                        cartSetString(cart, subtrackSetting, "1");
                        }
                    }
                }

            // Turn OFF: (de_off x dt_was), (de_was x dt_off)
            // where de_off = de_was - de_now, dt_off = dt_was - dt_now
            for (struct slName *de = de_was_list; de != NULL; de = de->next)
                {
                boolean de_is_off = (hashLookup(de_now_hash, de->name) == NULL);
                for (struct slName *dt = dt_was_list; dt != NULL; dt = dt->next)
                    {
                    boolean dt_is_off = (hashLookup(dt_now_hash, dt->name) == NULL);
                    if (de_is_off || dt_is_off)
                        {
                        safef(subtrackSetting, sizeof(subtrackSetting),
                              "%s_%s_%s_sel", mdid, de->name, dt->name);
                        cartSetString(cart, subtrackSetting, "0");
                        }
                    }
                }

            }
        else
            {
            // Data elements only mode: tracks are identified by mdid_de

            // Turn ON: de_now - de_was
            for (struct slName *de = de_now_list; de != NULL; de = de->next)
                {
                if (hashLookup(de_was_hash, de->name) == NULL)
                    {
                    safef(subtrackSetting, sizeof(subtrackSetting),
                          "%s_%s_sel", mdid, de->name);
                    cartSetString(cart, subtrackSetting, "1");
                    }
                }

            // Turn OFF: de_was - de_now
            for (struct slName *de = de_was_list; de != NULL; de = de->next)
                {
                if (hashLookup(de_now_hash, de->name) == NULL)
                    {
                    safef(subtrackSetting, sizeof(subtrackSetting),
                          "%s_%s_sel", mdid, de->name);
                    cartSetString(cart, subtrackSetting, "0");
                    }
                }

            }

        hashFree(&de_was_hash);
        hashFree(&de_now_hash);
        hashFree(&dt_was_hash);
        hashFree(&dt_now_hash);

        slFreeList(&de_was_list);
        slFreeList(&de_now_list);
        slFreeList(&dt_was_list);
        slFreeList(&dt_now_list);

        cartRemove(cart, mName);
        cartRemove(cart, mdid_de_was);
        cartRemove(cart, mdid_de_now);
        cartRemove(cart, mdid_dt_was);
        cartRemove(cart, mdid_dt_now);
        }

    return;
    }

// To discourage hacking, call bottleneck
if (issueBotWarning)
    {
    char *ip = getenv("REMOTE_ADDR");
    botDelayMessage(ip, botDelayMillis);
    }

if (asTable)
    {
    jsIncludeFile("jquery.js",NULL); // required by utils.js
    jsIncludeFile("utils.js",NULL);
    jsIncludeFile("ajax.js",NULL);
    printf("<A HREF='../cgi-bin/cartDump?%s=[]'>Show as plain text.</a><BR>",CART_DUMP_AS_TABLE);
    printf("<FORM ACTION=\"../cgi-bin/cartDump\" METHOD=GET>\n");
    cartSaveSession(cart);
    printf("<em>Variables can be altered by changing the values and then leaving the field (onchange event will use ajax).\n");
    printf("Enter </em><B><code style='color:%s'>%s</code></B><em> or </em><B><code style='color:%s'>%s</code></B><em> to remove a variable.</em>",
           COLOR_DARKBLUE,CART_DUMP_REMOVE_VAR,COLOR_DARKBLUE,CART_VAR_EMPTY);
    printf("<BR><em>Add a variable named:</em> ");
    cgiMakeTextVar(vName, "", 12);
    printf(" <em>value:</em> ");
    cgiMakeTextVar(vVal, "", 24);
    printf("&nbsp;");
    cgiMakeButton("submit", "refresh"); // Says refresh but works as a submit.
    printf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
           "<a HREF='../cgi-bin/cartReset?destination=cartDump'><INPUT TYPE='button' VALUE='Reset the cart' style='color:%s;'></a>\n",
           COLOR_RED);
    printf("</FORM>\n");
    }
else
    {
    printf("<A HREF='../cgi-bin/cartDump?%s=1'>Show as updatable table.</a><BR>",CART_DUMP_AS_TABLE);
    }
printf("<TT><PRE>");
wildcard = cgiOptionalString(MATCH_VAR);
if (wildcard)
    cartDumpLike(cart, wildcard);
else
    cartDump(cart);
printf("</TT></PRE>");
if (!asTable)
    {
    printf("<FORM ACTION=\"../cgi-bin/cartDump\" METHOD=GET>\n");
    cartSaveSession(cart);
    printf("<em>Add/alter a variable named:</em> ");
    cgiMakeTextVar(vName, cartUsualString(cart, vName, ""), 12);
    printf(" <em>new value</em> ");
    cgiMakeTextVar(vVal, "", 24);
    printf(" ");
    cgiMakeButton("submit", "submit");
    printf("<BR>Put </em><B><code style='color:%s'>%s</code></B><em> in for the new value to clear a variable.</em>",
           COLOR_DARKBLUE,CART_DUMP_REMOVE_VAR);
    printf("</FORM>\n");
    }
printf("<P><em>Cookies passed to</em> %s:<BR>\n%s\n</P>\n",
       cgiServerNamePort(), getenv("HTTP_COOKIE"));
}

char *excludeVars[] = { "submit", "Submit", "noDisplay", MATCH_VAR, NULL };

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
/* 0, 0, == use default 10 second for warning, 20 second for immediate exit */
issueBotWarning = earlyBotCheck(enteredMainTime, "cartDump", delayFraction, 0, 0, "html");
cgiSpoof(&argc, argv);
oldVars = hashNew(10);
cartHtmlShell("Cart Dump", doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("cartDump", enteredMainTime);
return 0;
}

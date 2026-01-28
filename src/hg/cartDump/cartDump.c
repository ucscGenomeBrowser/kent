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
        char mdid_de[1024], mdid_dt[1024];
        safef(mdid_de, sizeof(mdid_de), "%s.de", mdid);
        safef(mdid_dt, sizeof(mdid_dt), "%s.dt", mdid);
        struct slName *de_list = cgiStringList(mdid_de);
        struct slName *dt_list = cgiStringList(mdid_dt);

        // This component should change.  Erasing settings doesn't preserve any user selections
        // (like bigWig display changes) and doesn't work at all for tracks that are on by default.
        char wc_mdid[1024];
        safef(wc_mdid, sizeof(wc_mdid), "*_%s", mdid);
        cartRemoveLike(cart, wc_mdid);
        cartRemovePrefix(cart, mdid);

        // Check for empty-string sentinel: means no data type suffix
        boolean noDataTypes = (dt_list != NULL && 
                               dt_list->name[0] == '\0' && 
                               dt_list->next == NULL);

        char subtrackSetting[1024];
        for (struct slName *de_itr = de_list; de_itr; de_itr = de_itr->next)
            {
            if (noDataTypes)
                {
                // No data type suffix - use simpler track names
                safef(subtrackSetting, sizeof(subtrackSetting), "%s_%s_sel", mdid, de_itr->name);
                cartSetString(cart, subtrackSetting, "1");
                safef(subtrackSetting, sizeof(subtrackSetting), "%s_%s", mdid, de_itr->name);
                cartSetString(cart, subtrackSetting, "1");
                }
            else
                {
                struct slName *dt_itr = NULL;
                for (dt_itr = dt_list; dt_itr; dt_itr = dt_itr->next)
                    {
                    safef(subtrackSetting, sizeof(subtrackSetting), "%s_%s_%s_sel", mdid, de_itr->name, dt_itr->name);
                    cartSetString(cart, subtrackSetting, "1");
                    safef(subtrackSetting, sizeof(subtrackSetting), "%s_%s_%s", mdid, de_itr->name, dt_itr->name);
                    cartSetString(cart, subtrackSetting, "1");
                    }
                }
            }

        cartRemove(cart, mName);
        cartRemove(cart, mdid_de);
        cartRemove(cart, mdid_dt);
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

#include "common.h"
#include "cheapcgi.h"
#include "hgTrackUi.h"
#include "jksql.h"
#include "hdb.h"
#include "hui.h"
#include "trackDb.h"
#include "customTrack.h"
#include "encode/encodePeak.h"

void encodePeakUi(struct trackDb *tdb, struct customTrack *ct)
/* ENCODE peak UI options. */
{
struct sqlConnection *conn;
char *table;
char *trackName = tdb->track;
char pValVarName[256];
char qValVarName[256];
char scoreVarName[256];
boolean useScore = FALSE;
if (ct)
    {
    conn = hAllocConn(CUSTOM_TRASH);
    table = ct->dbTableName;
    }
else
    {
    conn = hAllocConn(database);
    table = tdb->table;
    if (!trackDbSettingOn(tdb, "filterPvalQval"))
	useScore = TRUE;
    }
    cgiDown(0.7);
if (useScore)
    {
    safef(scoreVarName, sizeof(scoreVarName), "%s.%s", trackName, ENCODE_PEAK_SCORE_FILTER_SUFFIX);
    puts("<BR><B>Minimum score (0-1000):</B>");
    cgiMakeTextVar(scoreVarName, cartCgiUsualString(cart, scoreVarName, "0"), 4);
    int scoreMax = atoi(trackDbSettingOrDefault(tdb, "scoreMax", "1000"));
    scoreGrayLevelCfgUi(cart, tdb, trackName, scoreMax);
    }
else
    {
    safef(pValVarName, sizeof(pValVarName), "%s.%s", trackName, ENCODE_PEAK_PVAL_FILTER_SUFFIX);
    safef(qValVarName, sizeof(qValVarName), "%s.%s", trackName, ENCODE_PEAK_QVAL_FILTER_SUFFIX);
    puts("<BR><B>Minimum p-value (-log10):</B>");
    cgiMakeTextVar(pValVarName, cartCgiUsualString(cart, pValVarName, "0.00"), 6);
    puts("<BR><B>Minimum q-value:</B>");
    cgiMakeTextVar(qValVarName, cartCgiUsualString(cart, qValVarName, "0.00"), 6);
    }
hFreeConn(&conn);
}

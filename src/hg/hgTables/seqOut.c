/* seqOut - Output sequence. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "trackDb.h"
#include "customTrack.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: seqOut.c,v 1.1 2004/07/19 21:09:33 kent Exp $";

static char *genePredMenu[] = 
    {
    "genomic",
    "protein",
    "mrna",
    };

static void genePredTypeButton(char *val, char *selVal)
/* Make region radio button including a little Javascript
 * to save selection state. */
{
cgiMakeRadioButton(hgtaGeneSeqType, val, sameString(val, selVal));
}

static void genePredOptions(struct trackDb *track, char *type, 
	struct sqlConnection *conn)
/* Put up quick options for gene prediction tracks. */
{
char *predType = cartUsualString(cart, hgtaGeneSeqType, genePredMenu[0]);
char *dupType = cloneString(type);
char *typeWords[3];
int typeWordCount, typeIx;

/* Type field has 1-3 words which are in order:
 *     genePred pepTable mrnaTable */
typeWordCount = chopLine(dupType, typeWords);
/* TypeIx will be 0 (genomic) 1 (protein) 2(mrna). */
typeIx = stringArrayIx(predType, genePredMenu, typeWordCount);
if (typeIx < 0)
    predType = genePredMenu[0];
htmlOpen("Select sequence type for %s", track->shortLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=GET>\n");
cartSaveSession(cart);

uglyf("%s<BR>\n", type);

for (typeIx = 0; typeIx < typeWordCount; ++typeIx)
    {
    genePredTypeButton(genePredMenu[typeIx], predType);
    hPrintf(" %s<BR>\n", genePredMenu[typeIx]);
    }
cgiMakeButton(hgtaDoGenePredSequence, "Submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>\n");
htmlClose();
freez(&dupType);
}

void doGenePredSequence(struct sqlConnection *conn)
/* Output genePred sequence. */
{
char *type = cartString(cart, hgtaGeneSeqType);
textOpen();
uglyf("Got type %s\n", type);
uglyf("All for now\n");
}

void doOutSequence(struct trackDb *track, struct sqlConnection *conn)
/* Output sequence page. */
{
char *table = track->tableName;
char *type = track->type;
if (startsWith("genePred", type))
    genePredOptions(track, type, conn);
else
    {
    uglyAbort("Don't know how to get that type of sequence yet");
    textOpen();
    }
}

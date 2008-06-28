#include "common.h"
#include "hash.h"
#include "portable.h"
#include "bed.h"
#include "htmshell.h"
#include "cart.h"
#include "hui.h"
#include "web.h"
#include "chromGraph.h"
#include "hPrint.h"
#include "hgGenome.h"
#include "trashDir.h"

static void quotedBrowserUrl(FILE *f, struct bed3 *bed, struct genoGraph *gg)
/* Print URL that will open browser at bed position with genoGraph track on */
{
fprintf(f, "\"../../cgi-bin/hgTracks?db=%s&position=%s:%d-%d&hgGenomeClick=browseRegions&%s=full\"",
        database, bed->chrom, bed->chromStart+1, bed->chromEnd,
	gg->name);
}

void browseRegions(struct sqlConnection *conn)
/* Put up a frame with a list of links on the left and the
 * first link selected on the right. */
{
struct genoGraph *gg = ggFirstVisible();
double threshold = getThreshold();
struct bed3 *bed, *bedList = regionsOverThreshold(gg);

/* Define names of our two frames. */
char *browserFrame = "browser";
char *indexFrame = "index";

/* Write index html file. */
struct tempName indexTn;
trashDirFile(&indexTn, "hgg", "index", ".html");
FILE *f = mustOpen(indexTn.forCgi, "w");
htmlSetBackground(addSuffix("../",hBackgroundImage()));
htmStart(f, "Region Index");
fprintf(f, "<BODY>");
fprintf(f, "<B>%s</B><BR>\n", gg->shortLabel);
fprintf(f, "%3.1f&nbsp;Mb&nbsp;in<BR>\n",
	0.000001*bedTotalSize((struct bed*)bedList));
fprintf(f, "%d&nbsp;regions&nbsp;>&nbsp;%g<BR>\n", 
	slCount(bedList), threshold);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    fprintf(f, "<A HREF=");
    quotedBrowserUrl(f, bed, gg);
    fprintf(f, " TARGET=\"%s\">", browserFrame);
    fprintf(f, "%s&nbsp;%3.1fM&nbsp;to&nbsp;%3.1fM</A><BR>", 
    	bed->chrom, 0.000001*bed->chromStart, 0.000001*bed->chromEnd);
    }
fprintf(f, "</BODY>\n");
carefulClose(&f);

/* Write frames */
hPrintf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Frameset//EN\">\n");
hPrintf("<HTML><HEAD><TITLE>%s Regions &gt;= %g</TITLE></HEAD>\n",
	gg->shortLabel, threshold);
hPrintf("<FRAMESET COLS=\"19%%,81%%\">\n");
hPrintf("<FRAME SRC=\"%s\" NAME=\"%s\">\n", indexTn.forCgi, indexFrame);
hPrintf("<FRAME SRC=");
quotedBrowserUrl(stdout, bedList, gg);
hPrintf(" NAME=\"%s\">\n", browserFrame);
hPrintf("<NOFRAMES><BODY></BODY></NOFRAMES>\n");
hPrintf("</FRAMESET>\n");
hPrintf("</HTML>\n");
}

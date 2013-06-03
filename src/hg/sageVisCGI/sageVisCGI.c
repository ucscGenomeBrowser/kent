/**

\page sageVisCGI.doxp sageVisCGI cgi program for viewing graphs of sage data
used standalone or called by the hgc.c program in ~kent/src/hg/hgc/.

<p>sageVisCGI creates graphs on the fly for the sage data of different
uniGene clusters. The clusters to be graphed are passed via cgi (or command
line) in the form u=<uniGene number>. There is also an optional 'md' parameter
which specifies a maximum tag count to go up to, default is 50.

<p>Example
<br><pre><code>
sageVisCGI md=20 u=202 u=122566 > out.html
</code></pre>
or http://genome-test.cse.ucsc.edu/cgi-bin/sageVisCGI?md=20&u=202&u=122566 .

\sa sageVisCGI.c

*/
#include "common.h"
#include "hCommon.h"
#include "dystring.h"
#include "sage.h"
#include "sageExp.h"
#include "gnuPlot2D.h"
#include "portable.h"
#include "jksql.h"
#include "dnautil.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "knownMore.h"
#include "hdb.h"
#include "genePred.h"
#include "hgConfig.h"

int maxDataVal =0;
int dataValCeiling;

/**
Just prints out the <img> tag and filename
*/
void doPlotPrintOut(char *fileName)
{
printf( "<img src=\"%s\">\n", fileName);
}


/**
 * Appends one string to another without having to
 * worry about going over. Assumes that the total length
 * of dest is the strlen(dest) + 1. Wasteful if you want to
 * use it lots of times in a row as it reallocates memory every time
 */
void dynamicStrncat(char **dest, const char *src)
{
ExpandArray(*dest, (strlen(*dest)+1), (strlen(*dest) +strlen(src) + 1));
strncat(*dest, src, strlen(src));
}

/**
Construct a graphPoint from data in sage and current index.
Assumes that the experiments are stored in order.
*/
struct graphPoint2D*createSageGraphPoint(struct sage *sg, int i)
{

struct graphPoint2D *gp = NULL;
char name[128];

sprintf(name, "Hs.%d", sg->uni);
AllocVar(gp);
gp->y = sg->meds[i-1];
if(gp->y > maxDataVal && gp->y < dataValCeiling)
    maxDataVal = gp->y;
gp->x = i;
gp->name = cloneString(name);
gp->hName = cloneString(name);
gp->groupName = cloneString(name);
return gp;
}

/**
  Uses the sage experiment data (sageExp) and sage data itself (sage)
to construct the basic gnuPlot2D data type, specifically creating the
graphPoint2D data lists for each element in sgList */
struct gnuPlot2D *createSagePlot(struct sageExp *seList, struct sage *sgList)
{
struct gnuPlot2D *gp = createGnuPlot2D();
struct sage *sg = NULL;
int i;
for(sg = sgList; sg != NULL; sg = sg->next)
    {
    struct graphPoint2D *gList = NULL;
    for (i=0; i< sg->numExps; i++)
        {
        struct graphPoint2D *gTemp = NULL;
        gTemp = createSageGraphPoint(sg, i+1);
	slAddHead(&gList, gTemp);
	}
    gptAddGraphPointList(gp, gList, "linesp");
    }
return gp;
}

/* Creates a string which tells gnuPlot what label to put on
   xTics and where to put them. Will Chop "SAGE" from name if present */
char * constructXticsFromExps(struct sageExp *seList)
{
char *ret = cloneString("set xtics rotate(");
static char buff[256];
int count =1;
struct sageExp *se;
for(se = seList; se != NULL; se = se->next)
    {
    if(se->next == NULL)
	sprintf(buff, " \"%d)%s\" %d ", count, strstr(se->exp,"SAGE_") ? strstr(se->exp,"_") : se->exp , count);
    else
	sprintf(buff, " \"%d)%s\" %d, ", count, strstr(se->exp,"SAGE_") ? strstr(se->exp, "_") : se->exp, count);
    dynamicStrncat(&ret, buff);
    count++;
    }
dynamicStrncat(&ret, ")\n");
return ret;
}

/* Prints the header appropriate for the title
 * passed in. Links html to chucks stylesheet for
 * easier maintaince
 */
void chuckHtmlStart(char *title)
{
printf("<html><head>");
//FIXME blueStyle should not be absolute to genome-test and should bae called by:
// webIncludeResourceFile("blueStyle.css");
printf("<LINK REL=STYLESHEET TYPE=\"text/css\" href=\"http://genome-test.cse.ucsc.edu/style/blueStyle.css\" title=\"Chuck Style\">\n");
printf("<title>%s</title>\n</head><body bgcolor=\"#f3f3ff\">",title);
}

/**
Generates the data plot and associated html
*/
void doCountsPage(struct sageExp *seList, struct sage *sgList)
{
struct gnuPlot2D *gp = needMem(sizeof(struct gnuPlot2D*));

char *cmd = NULL;
double xSize;
double ySize;
char *title = NULL;
char *xTics = NULL;
char plotSize[256]; // = cloneString("set size .75,.75\n");
struct tempName pngTn;

chuckHtmlStart("Sage Graph");
printf("<center>");
makeTempName(&pngTn, "sageDat", ".png");
gp = createSagePlot(seList, sgList);
gp->fileName = pngTn.forCgi;
xTics = constructXticsFromExps(seList);
title = cloneString("Sage Data for Unigene Clusters");
xSize = 0.020*slCount(gp->gpList[0]);
ySize = 0.075*slCount(sgList);
if(ySize < 1.25) ySize = 1.25;
if(xSize <.75) xSize = .75;
sprintf(plotSize, "set size %g, %g\n", xSize,ySize);
gp->other = cloneString(plotSize);
dynamicStrncat(&gp->other, xTics);
gp->ylabel = cloneString("Median Counts");
gp->xlabel = cloneString("Experiment");
gp->title = cloneString(title);
gp->xMax = slCount(gp->gpList[0]);
gp->yMax = maxDataVal;
cmd = gptGenerateCmd(gp);
gptPlotFromCmd(cmd);
doPlotPrintOut(pngTn.forHtml);
htmlEnd();
}


/** load the sage data by constructing a query based on the names in nmList
 */
struct sage *loadSageData(char *table, struct slName *nmList)
{
char *user = cfgOption("db.user");
char *password = cfgOption("db.password");
struct sqlConnection *sc = NULL;
struct dyString *query = newDyString(2048);
struct sage *sgList = NULL, *sg=NULL;
struct slName *nm =NULL;
char *db = cgiUsualString("db", "hgFixed");
char **row;
int count=0;
struct sqlResult *sr = NULL;
sc = sqlConnectRemote("localhost", user, password, db);
sqlDyStringPrintf(query, "select * from sage where ");
for(nm=nmList;nm!=NULL;nm=nm->next)
    {
    if (count++)
        {
        sqlDyStringPrintf(query," or uni=%s ", nm->name );
        }
    else
	{
	sqlDyStringPrintf(query," uni=%s ", nm->name);
	}
    }
sr = sqlGetResult(sc,query->string);
while((row = sqlNextRow(sr)) != NULL)
    {
    sg = sageLoad(row);
    slAddHead(&sgList,sg);
    }
sqlFreeResult(&sr);
sqlDisconnect(&sc);
slReverse(&sgList);
freeDyString(&query);
return sgList;
}

/** load the sage experiment data
 */
struct sageExp *loadSageExps(char *tableName, struct slName *nmList)
{
char *user = cfgOption("db.user");
char *password = cfgOption("db.password");
struct sqlConnection *sc = NULL;
char query[256];
struct sageExp *seList = NULL, *se=NULL;
char **row;
struct sqlResult *sr = NULL;
char *db = cgiUsualString("db", "hgFixed");
sc = sqlConnectRemote("localhost", user, password, db);
sqlSafef(query, sizeof query, "select * from sageExp order by num");
sr = sqlGetResult(sc,query);
while((row = sqlNextRow(sr)) != NULL)
    {
    se = sageExpLoad(row);
    slAddHead(&seList,se);
    }
sqlFreeResult(&sr);
sqlDisconnect(&sc);
slReverse(&seList);
return seList;
}

/** print usage and quit */
void usage()
{
errAbort("sageVisCGI - create a graph of median sage counts\n"
	 "usage:\n\t sageVisCGI u=<uniGeneNum> u=<uniGeneNum2> u=<uniGeneNum3> etc. \n");
}

/** main html generating function for htmShell */
void doHtml()
{
struct sageExp *seList;
struct sage *sgList;
struct slName *unis = cgiStringList("u");
if(unis == NULL)
    usage();
dataValCeiling = cgiOptionalInt("md",50);
seList = loadSageExps("sageExp", unis);
sgList = loadSageData("sage", unis);
doCountsPage(seList, sgList);
sageExpFreeList(&seList);
sageFreeList(&sgList);
slFreeList(&unis);
}


int main(int argc, char *argv[])
{
cgiSpoof(&argc, argv);
printf("Content-Type: text/html\n\n");
htmEmptyShell(doHtml, NULL);
return 0;
}

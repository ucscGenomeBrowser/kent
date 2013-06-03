/* spliceProbeVis.c - Visualize some plots for data. Since R is slow
 to call build up entire script to make all plots and then call R
 once, have to build up html in a buffer too or else browser starts
 looking for files that aren't there yet.
*/
#include "common.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "portable.h"
#include "dMatrix.h"
#include "dystring.h"
#include "mouseAPSetEventMap.h"

/* Don't start printing web page until R creates plots. */
struct dyString *html = NULL;

/* The event that this splicing event is represented by. */
struct mouseAPSetEventMap *altEvent = NULL;
char *abbv = "abbv = c( 'ce1','ce2','ce3','ch1','ch2','ch3','co1','co2','co3','ct1','ct2','ct3',\n"
"'es1','es2','es3','he1','he2','he3','hb1','hb2','hb3','ki1','ki2','ki3',\n"
"'li1','li2','li3','lu1','lu2','lu3','mg1','mg2','mg3','ob1','ob2','ob3',\n"
"'ov1','ov2','ov3','pi1','pi2','pi3','sg1','sg2','sg3','sm1','sm2','sm3','sm4',\n"
"'si1','si2','si3','sc1','sc2','sc3','sp1','sp2','sp3','st1','st2','st3',\n"
"'te1','te2','te3','th1','th2','th3');\n";

char *key = "Tisues: cerebellum (ce), cerebral hemisphere (ch), colon (co), cortex (ct), esophagus (es), heart (he), hind brain (hb), kidney (ki), liver (li), lung (lu), mammary gland (mg), olfactory bulb (ob), ovary (ov), pineal gland (pg), salivary gland (sg), skeletal muscle (sm), small intestine (si), spinal cord (sc), spleen (sp), stomach (st), testicle (te), thalamus (th)";

/* char *brainTissues = "brainTissues = c(1,  2,  3,  4,  5,  6, 10, 11, 12, 19, 20, 21, 34, 35, 36, 40, 41, 42, 65, 66, 67);\n"; */
/* char *nonBrainTissues = "nonBrainTissues = c(7,  8,  9, 13, 14, 15, 16, 17, 18, 22, 23, 24, 25, 26, 27, \n" */
/* "28, 29, 30, 31, 32, 33, 37, 38, 39, 43, 44, 45, 46, 47, 48, \n" */
/* "49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);\n"; */

char *isBrain = "isBrain = c(1,1,1,1,1,1,0,0,0,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1);\n";
char *isMuscle = "isBrain =c(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);\n";

char *rHeader = "library(MASS);\n"
"rlmZero <- function(x,y){\n"
"# Compute the rlm  of all points\n"
"# constrained to go through the origin.\n"
"fit <- rlm(y ~ x + 0)\n"
"coef <- c(0,0);\n"
"coef[1]<-0\n"
"coef[2]<-fit$coef[1]\n"
"res<-y-coef[2]*x-coef[1]\n"
"list(coef=coef,residuals=res)\n"
"}    \n";

char *plotRegression = 
"rX = range(incDat);\n"
"rY = range(skipDat);\n"
"incBrain = incDat[isBrain == 1 & expressed];\n"
"skipBrain = skipDat[isBrain == 1 & expressed];\n"
"incNotBrain = incDat[isBrain == 0 & expressed];\n"
"skipNotBrain = skipDat[isBrain == 0 & expressed];\n"
"plot.default(-1, -1, xlim=rX, ylim=rY, pch=21, main='%s %s - Red, Not %s - Blue', \n"
"	     col = 'white', xlab='Include intensity', ylab='Skip intensity');\n"
"if(length(abbv[isBrain == 1 & expressed == T]) != 0) {\n"
"text(incBrain, skipBrain, abbv[isBrain == 1 & expressed], col='#A52A2A')\n"
"}\n"
"if(length(abbv[isBrain == 1 & expressed == F]) != 0) {\n"
"text(incDat[isBrain == 1 & expressed == F], skipDat[isBrain == 1 & expressed == F], abbv[isBrain == 1 & expressed == F], col='#F38291')\n"
"}\n"
"if(length(abbv[isBrain == 0 & expressed == T]) != 0) {\n"
"text(incNotBrain, skipNotBrain, abbv[isBrain == 0 & expressed], col='blue');\n"
"}\n"
"if(length(abbv[isBrain == 0 & expressed == F]) != 0) {\n"
"text(incDat[isBrain == 0 & expressed == F], skipDat[isBrain == 0 & expressed == F], abbv[isBrain == 0 & expressed == F], col='#88D6FB')\n"
"}\n"
"if(length(incBrain) != 0 && length(incNotBrain) != 0) {\n"
"abline(rlmZero(incBrain,skipBrain)$coef, col='#A52A2A');\n"
"abline(rlmZero(incNotBrain,skipNotBrain)$coef, col='blue');\n"
"}\n"
;

void makePlotLink(char *type, char *fileName)
/* Write out an image or pdf link depending on pdf or image. */
{
if(cgiBoolean("pdf"))
    dyStringPrintf(html, "<a href='%s'>%s</a><br>\n", 
		   fileName, type);
else
    dyStringPrintf(html, "<img src='%s'><br>\n", fileName);
}

void initPlotOutput(struct dyString *script, char *fileName)
/* Init eithe a pdf or bitmap device. */
{
if(cgiBoolean("pdf"))
    dyStringPrintf(script, "pdf(file='%s', width=11, height=8);\n", fileName);
else
    dyStringPrintf(script, "bitmap('%s', width=4.5, height=3, res=200);\n", fileName);
}


void initRegPlotOutput(struct dyString *script, char *fileName)
/* Init eithe a pdf or bitmap device. */
{
if(cgiBoolean("pdf"))
    dyStringPrintf(script, "pdf(file='%s', width=6, height=6);\n", fileName);
else
    dyStringPrintf(script, "bitmap('%s', width=3, height=3, res=200);\n", fileName);
}

void constructQueryForEvent(struct dyString *query, char *skipPSet, char *tableName)
/* Construct a query for all the probe sets in a particular event for
   a matrix table. */
{
int i = 0;
char eventQuery[256];
struct mouseAPSetEventMap *event = NULL;
struct sqlConnection *conn = hAllocConn();
sqlSafef(eventQuery, sizeof(eventQuery), 
      "select * from mouseAPSetEventMap where skipPSet = '%s';",
      skipPSet);

/* Check our little cache. */
if(altEvent == NULL)      
    altEvent = event = mouseAPSetEventMapLoadByQuery(conn, eventQuery);
else 
    event = altEvent;

if(event == NULL)
    errAbort("Couldn't find an alternative events for: %s", skipPSet);

assert(query);
dyStringClear(query);
sqlDyStringPrintf(query, "select * from %s where ", tableName);
for(i = 0; i < event->incCount; i++) 
    dyStringPrintf(query, "name like '%s' or ", event->incPSets[i]);
for(i = 0; i < event->geneCount; i++) 
    dyStringPrintf(query, "name like '%s' or ", event->genePSets[i]);
dyStringPrintf(query, "name like '%s' order by name;", skipPSet);
hFreeConn(&conn);
}

void closePlotOutput(struct dyString *script) 
/* Turn off device. */
{
dyStringPrintf(script, "dev.off();\n");
}

struct dMatrix *dataFromTable(char *tableName, char *query)
/* Read data from table name with rowname. */
{
struct sqlConnection *conn = hAllocConn();
struct slName *colNames = sqlListFields(conn, tableName);
struct slName *name = NULL;
int count = 0;
struct dMatrix *dM = NULL;
struct sqlResult *sr = NULL;
char **row = NULL;
int colIx = 0;


/* Allocate some initial memory. */
AllocVar(dM);
dM->colCount = slCount(colNames) -1;
dM->rowCount = 1;
AllocArray(dM->matrix, dM->rowCount);
AllocArray(dM->rowNames, dM->rowCount);
AllocArray(dM->matrix[0], dM->colCount);
AllocArray(dM->colNames, dM->colCount);
for(name = colNames->next; name != NULL; name = name->next)
    dM->colNames[count++] = cloneString(name->name);

/* Execute our query. */
sr = sqlGetResult(conn, query);
count = 0;

/* Read out the data. */
while((row = sqlNextRow(sr)) != NULL)
    {
    /* Expand matrix data as necessary. */
    if(count > 0)
	{
	ExpandArray(dM->matrix, dM->rowCount, dM->rowCount+1);
	AllocArray(dM->matrix[dM->rowCount], dM->colCount);
	ExpandArray(dM->rowNames, dM->rowCount, dM->rowCount+1);
	dM->rowCount++;
	}
    dM->rowNames[count] = cloneString(row[0]);
    for(colIx = 0; colIx < dM->colCount; colIx++) 
	dM->matrix[count][colIx] = atof(row[colIx+1]);
    count++;
    }
if(count == 0)
    errAbort("Didn't find any results for query:\n%s", query);
sqlFreeResult(&sr);
hFreeConn(&conn);
return dM;
}

void plotMatrixRows(struct dMatrix *mat, struct dyString *script, 
		    char *title, char *fileName, char *type, boolean linesOnly) 
/* Write a file which will produce an R script to 
   make a plot. */
{
int i = 0, j = 0;
double minVal = BIGNUM, maxVal = 0;
initPlotOutput(script, fileName);

/* Write out all the data. */
dyStringPrintf(script, "\n dat = c(");
for(i = 0; i < mat->rowCount; i++) 
    {
    for(j = 0; j < mat->colCount; j++) 
	{
	dyStringPrintf(script, "%.2f,", mat->matrix[i][j]);
	maxVal = max(mat->matrix[i][j], maxVal);
	minVal = min(mat->matrix[i][j], minVal);
	}
    dyStringPrintf(script, "\n");
    }

dyStringPrintf(script, ");\n");

/* Write the row names. */
dyStringPrintf(script, "rownames = c(");
for(i = 0; i < mat->rowCount; i++)
    dyStringPrintf(script, "'%s',", mat->rowNames[i]);
dyStringPrintf(script, ");\n");

/* Write the column names. */
dyStringPrintf(script, "colnames = c(");
for(i = 0; i < mat->colCount; i++)
    dyStringPrintf(script, "'%s',", mat->colNames[i]);
dyStringPrintf(script, ");\n");

/* Create the matrix. */
dyStringPrintf(script, "mat = matrix(data=dat, nrow=%d, ncol=%d, byrow=T, dimnames=list(rownames, colnames));\n",
	       mat->rowCount, mat->colCount);
dyStringPrintf(script, "colorIx = 0;\n");
dyStringPrintf(script, "par(mar=c(10, 4, 4, 2) + 0.1);\n");
/* Create initial plot. */
dyStringPrintf(script, 
	       "cols = rainbow(length(unique(rownames)));\n"
	       "plot.default( 0, 0, ylim=c(%.2f, %.2f), xlim=c(1,%d), "
	       "main='%s',"
	       "col='white',"
	       "axes=F, ylab='%s', xlab=''"
	       ");\n",
	       minVal, maxVal, 
	       mat->colCount,
	       title,
	       type);
dyStringPrintf(script, "lastRowName = 'first';\n");
/* Loop through and plot individual rows. */
dyStringPrintf(script, "for(row in 1:%d) {\n", mat->rowCount);
dyStringPrintf(script, "if(lastRowName != rownames[row]) { colorIx = colorIx+1; lastRowName = rownames[row]; }\n");
dyStringPrintf(script, "lines(c(1:%d), mat[row,], col=cols[colorIx], pch=20, type='%c');\n", 
	       mat->colCount, linesOnly ? 'l' : 'b');
dyStringPrintf(script, "}\n");

/* Do the axis and the legend. */
dyStringPrintf(script, "axis(2);\n");
dyStringPrintf(script, "axis(1, at=c(1:%d), labels=colnames, las=3);\n", mat->colCount);
dyStringPrintf(script, "legend(1, .95 * %.2f, unique(rownames), fill=cols);\n", maxVal);
closePlotOutput(script);
}

void touchBlank(char *fileName)
/* Make an empty file. */
{
FILE *out = NULL;
assert(fileName);
out = mustOpen(fileName, "w");
carefulClose(&out);
}

void doRegressionPlot(struct dyString *script, char *skipPset, 
		      char *incTable, char *skipTable, char *geneTable)
/* Put up a regression plot. */
{
struct dMatrix *skip = NULL, *inc = NULL, *gene = NULL;
char query[256];
int i = 0;
struct tempName regPlot;
double thresh = .75;
if(cgiBoolean("allPoints"))
    thresh = -1;
if(cgiBoolean("pdf"))
    makeTempName(&regPlot, "sp", ".pdf");
else
    makeTempName(&regPlot, "sp", ".png");
touchBlank(regPlot.forCgi);

sqlSafef(query, sizeof(query), "select * from %s where name like '%%%s%%';", 
      incTable, skipPset);
inc = dataFromTable(incTable, query);

sqlSafef(query, sizeof(query), "select * from %s where name like '%%%s%%';", 
      skipTable, skipPset);
skip = dataFromTable(skipTable, query);

sqlSafef(query, sizeof(query), "select * from %s where name like '%%%s%%';", 
      geneTable, skipPset);
gene = dataFromTable(geneTable, query);

initRegPlotOutput(script, regPlot.forCgi);
dyStringPrintf(script, "incDat = c(");
for(i = 0; i< inc->colCount; i++)
    dyStringPrintf(script, "%.4f,", inc->matrix[0][i]);
dyStringPrintf(script, ");\n");

dyStringPrintf(script, "skipDat = c(");
for(i = 0; i< skip->colCount; i++)
    dyStringPrintf(script, "%.4f,", skip->matrix[0][i]);
dyStringPrintf(script, ");\n");

dyStringPrintf(script, "geneDat = c(");
for(i = 0; i< gene->colCount; i++)
    dyStringPrintf(script, "%.4f,", gene->matrix[0][i]);
dyStringPrintf(script, ");\n");

dyStringPrintf(script, "expressed = geneDat > %.4f;\n", thresh);

dyStringPrintf(script, abbv);
if(cgiBoolean("muscle"))
    {
    dyStringPrintf(script, isMuscle);
    dyStringPrintf(script, plotRegression, altEvent->geneName, "Muscle", "Muscle");
    }
else
    {
    dyStringPrintf(script, isBrain);
    dyStringPrintf(script, plotRegression, altEvent->geneName, "Brain", "Brain");
    }
closePlotOutput(script);
makePlotLink("Include vs. Skip", regPlot.forCgi);
dyStringPrintf(html, "<table width=600><tr><td>\n");
dyStringPrintf(html, "Tissues for which the gene expression was too low to be considered are in lighter colors.<br><br>\n");
dyStringPrintf(html, "%s\n", key);
dyStringPrintf(html, "</td></tr></table><br>\n");
//dyStringPrintf(html, "<img src='%s'><br>\n", regPlot.forCgi);
}

void invokeR(struct dyString *script)
/* Call R on our script. */
{
struct tempName rScript;
FILE *out = NULL;
char command[256];
assert(script);
makeTempName(&rScript, "sp", ".R");
out = mustOpen(rScript.forCgi, "w");
fprintf(out, "%s", script->string);
carefulClose(&out);
safef(command, sizeof(command), "R --vanilla < %s >& /dev/null ", rScript.forCgi);
system(command);
}



void doMatrixPlot(struct dyString *script, 
		  char *skipPSet, char *tableName, char *type,
		  boolean linesOnly)
/* Print out a matrix plot for a particular table. */
{
struct tempName plotFile;
struct dyString *query = newDyString(256);
struct dMatrix *dM = NULL;
char title[256];
safef(title, sizeof(title), "%s - %s", altEvent->geneName, type);
if(cgiBoolean("pdf"))
    makeTempName(&plotFile, "sp", ".pdf");
else
    makeTempName(&plotFile, "sp", ".png");
touchBlank(plotFile.forCgi);
makePlotLink(type, plotFile.forCgi);
//dyStringPrintf(html, "<img src='%s'><br>\n", plotFile.forCgi);
constructQueryForEvent(query, skipPSet, tableName);
dM = dataFromTable(tableName, query->string);
plotMatrixRows(dM, script, title, plotFile.forCgi, type, linesOnly) ;
}

void doProbeSet(struct dyString *script, char *skipPSet)
/* Plot the probe set intensity estimates. */
{
doMatrixPlot(script, skipPSet, "mouseAPSetInten", "PSet Intensity", FALSE);
}

void doProbeSetPVals(struct dyString *script, char *skipPSet)
/* Plot the p-values. */
{
doMatrixPlot(script, skipPSet, "mouseAPSetPval", "PSet (1 - p-val)", FALSE);
}

void doNormProbes(struct dyString *script, char *skipPSet)
/* Plot the normalized version of the raw probe values. */
{
doMatrixPlot(script, skipPSet, "mouseAProbeNormInten", "Probe Intensity", TRUE);
}

void doPlots()
/* Do all the plots. */
{
struct dyString *script = newDyString(2048);
struct dyString *dummy = newDyString(256);
char *skipPSet = cgiUsualString("skipPName", "G7153846@J939317_RC@j_at");
html = newDyString(1024);
hSetDb("mm5");
dyStringPrintf(script, "%s", rHeader);

/* Call query once to initialize altEvent. */
constructQueryForEvent(dummy, skipPSet, "dummy");
assert(altEvent);
fprintf(stdout, "<h3>Plots for %s (%s)</h3>\n", altEvent->geneName, skipPSet);
fflush(stdout);
/* Probe set plots. */
doRegressionPlot(script, skipPSet, 
		 "mouseAEvent_inc_intensity", 
		 "mouseAEvent_skip_intensity",
		 "mouseAEvent_gene_prob");
doProbeSet(script, skipPSet);
doProbeSetPVals(script, skipPSet);
doNormProbes(script, skipPSet);
fflush(stdout);
fflush(stderr);
invokeR(script);
fputs(html->string, stdout);
}


int main(int argc, char *argv[]) 
/* Everybody's favorite function. */
{
cgiSpoof(&argc, argv);
htmShell("Alt Event Data.", doPlots, NULL);
return 0;
}

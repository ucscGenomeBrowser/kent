#include "common.h"
#include "geneGraph.h"
#include "genePred.h"
#include "altGraphX.h"
#include "ggMrnaAli.h"
#include "psl.h"
#include "dnaseq.h"
#include "hdb.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "bed.h"
#include "affyPres.h"
#include "psl.h"


void usage()
{
errAbort("correlateWithExpression - Takes a altGraphX file and finds the corresponding\n"
	 "affymetrix ids from the database. Then checks with the experimental results\n"
	 "to make sure that the 'gene' is being expressed.\n"
	 "usage:\n\t correlateWithExpression <db> <agxFile> <experimental file> <min present>\n");
}

void startHtml(FILE *html)
{
fprintf(html, "<HTML><BODY><TABLE border=1><TR><TH>Affy Probe Id</TH><TH>Rep Sequence</TH><TH>Ave Exp</TH><TH>Num Tissues</TH><TH>Coordinates</TH></TR>\n");
}

void endHtml(FILE *html)
{
fprintf(html, "</BODY></HTML></TABLE>\n");
}

void writeBrowserLink(FILE *html, char *db, struct altGraphX *ag, char *id, char *info, float ave, int count, int edge)
{
int start = ag->vPositions[ag->edgeStarts[edge]];
int end = ag->vPositions[ag->edgeEnds[edge]];
char *chrom = ag->tName;
static int c =0;
if(c == 0)
    {
    fprintf(html, "<tr><td>%s</td><td><font size=-3>%s</font></td><td>%.2f</td><td>%d</td><td><a href=\"http://genome-test.gi.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">%s:%d-%d</a><br>\n</td></tr>",
	id, info, ave, count, db, ag->tName, start, end, ag->tName, start, end);
    c =1;
    }
else 
    {
    fprintf(html, "<tr bgcolor=\"#eaefef\"><td>%s</td><td><font size=-3>%s</font></td><td>%.2f</td><td>%d</td><td><a href=\"http://genome-test.gi.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">%s:%d-%d</a><br>\n</td></tr>",
	id, info, ave, count, db, ag->tName, start, end, ag->tName, start, end);
    c =0;
    }
}

int countCalls(struct affyPres *ap)
{
int i;
int count =0;
for(i=0; i<ap->callCount; i++)
    {
    if(sameWord("p", ap->calls[i]))
	count++;
    }
return count;
}

float calcAve(struct affyPres *ap)
{
int i;
float ave = 0;
int count =0;
for(i=0; i<ap->valCount; i++)
    {
    if(sameWord("p", ap->calls[i]))
	{
	ave += ap->vals[i];
	count++;
	}
    }
if(count > 0)
    return ave/count;
return 0;
}

void correlateWithExpression(char *db, char *agxName, char *expName, char *minNum)
{
FILE *html = NULL;
FILE *log = NULL;
int minCount = atoi(minNum);
struct sqlConnection *conn = NULL;
struct altGraphX *agxList = NULL, *agx = NULL;
struct affyPres *apList = NULL, *ap = NULL;

struct hash *apHash = newHash(12);
char buff[strlen(agxName) + 10];
int foundCount=0, passCount=0;
float minConfidence = cgiOptionalDouble("minConf", 2.0);
hSetDb(db);
conn = hAllocConn();
snprintf(buff, (strlen(agxName) + 10), "%s.html", agxName);
html = mustOpen(buff, "w");
snprintf(buff, (strlen(agxName) + 10), "%s.log", agxName);
log = mustOpen(buff, "w");
startHtml(html);
warn("Loading altGraphs.");
agxList = altGraphXLoadAll(agxName);
warn("Loaded %d altGraphXs", slCount(agxList));

warn("Loading affy experimental.");
apList = affyPresLoadAll(expName);
warn("Loaded %d affy experimentals", slCount(apList));

hashApList(apHash, apList);

for(agx = agxList; agx != NULL; agx = agx->next)
    {
    char *affyId = findAffyId(agx, conn, apHash);
    ap = NULL;
    if(affyId == NULL)
	warn("Can't find affyId for %s:%d-%d", agx->tName, agx->tStart, agx->tEnd);
    else
	{
	ap = hashFindVal(apHash, affyId);
	if(ap == NULL)
	    warn("Yikes, Can't find %s in apHash", affyId);
	}
    if(affyId != NULL && ap != NULL)
	{
	int count = countCalls(ap);
	float ave = calcAve(ap);
	foundCount++;
	warn("Found %s for %s:%d-%d", affyId, agx->tName, agx->tStart, agx->tEnd);
	if(count > minCount || (ave > 250 && count > 0)) 
	    {
	    int i;
	    for(i=0;i<agx->edgeCount; i++)
		{
		if(agx->edgeTypes[i] == ggCassette)
		    {
		    float confidence = altGraphCassetteConfForEdge(agx, i, 10);
		    if(confidence >= minConfidence) 
			{
			char *string = cloneString(ap->info);
			fprintf(log, "%f\t%d\n", ave, count);
			passCount++;
			subChar(string, '_', ' ');
			freez(&agx->name);
			agx->name = cloneString(ap->probeId);
			altGraphXTabOut(agx, stdout);
			writeBrowserLink(html, db, agx, ap->probeId, string, ave, count, i);
			freez(&string);
			}
		    }
		}
	    }
	}
    }
warn("%d total, %d found, %d passed criteria", slCount(agxList), foundCount, passCount);
hashFree(&apHash);
affyPresFreeList(&apList);
altGraphXFreeList(&agxList);
carefulClose(&log);
carefulClose(&html);
}

int main(int argc, char *argv[]) 
{
if(argc < 5)
    usage();
cgiSpoof(&argc, argv);
correlateWithExpression(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

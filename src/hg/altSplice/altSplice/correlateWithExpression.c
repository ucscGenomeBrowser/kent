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
    fprintf(html, "<tr><td>%s</td><td><font size=-3>%s</font></td><td>%.2f</td><td>%d</td><td><a href=\"http://genome-test.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">%s:%d-%d</a><br>\n</td></tr>",
	id, info, ave, count, db, ag->tName, start, end, ag->tName, start, end);
    c =1;
    }
else 
    {
    fprintf(html, "<tr bgcolor=\"#eaefef\"><td>%s</td><td><font size=-3>%s</font></td><td>%.2f</td><td>%d</td><td><a href=\"http://genome-test.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">%s:%d-%d</a><br>\n</td></tr>",
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


boolean isExon(struct altGraphX *agx, int edge)
{
if((agx->vTypes[agx->edgeStarts[edge]] == ggHardStart ||
    agx->vTypes[agx->edgeStarts[edge]] == ggSoftStart) &&
   (agx->vTypes[agx->edgeEnds[edge]] == ggHardEnd ||
    agx->vTypes[agx->edgeEnds[edge]] == ggSoftEnd))
    return TRUE;
return FALSE;
}

int calcDistance(struct psl *pslList, struct altGraphX *ag)
{
int distance = 0;
int minDiff = BIGNUM;
struct psl *psl = NULL;
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    if(sameString(ag->strand, "+")) 
	distance = abs(ag->tEnd - psl->tStart);
    else
	distance = abs(ag->tStart - psl->tStart);
    if(distance < minDiff && sameString(ag->tName, psl->tName) && distance < 1000)
	minDiff = distance;
    }
return minDiff;
}

int calcOverlap(struct psl *pslList, struct altGraphX *ag)
{
int i,j,overlap=0;
struct psl *psl = NULL;
if(pslList == NULL)
    return 0;
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    for(i=0; i< ag->edgeCount; i++)
	{
	if(isExon(ag,i))
	    {
	    for(j=0; j<psl->blockCount; j++)
		{
		overlap += positiveRangeIntersection(ag->vPositions[ag->edgeStarts[i]], ag->vPositions[ag->edgeEnds[i]], psl->tStarts[j], (psl->tStarts[j] + psl->blockSizes[j]));
		}
	    }
	}
    }
return overlap;
}

struct psl *getPslForId(char *acc, struct sqlConnection *conn)
{
struct psl *pslList = NULL, *psl = NULL;
struct sqlResult *sr = NULL;
char **row;
char *id = NULL;
char *table = NULL;
char buff[256];
if(strstr(acc, "NM_") != NULL)
    table = "refSeqAli";
else
    {

    int type = -1;
    snprintf(buff, sizeof(buff), "select type from mrna where acc = '%s'", acc);
    type = sqlQuickNum(conn, buff);
    if(type == 0)
	table = "all_est";
    else
	table = "all_mrna";
    }
snprintf(buff, sizeof(buff), "select * from %s where qName = '%s'", table, acc);
sr = sqlGetResult(conn, buff);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+1);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
return pslList;
}



char *resolveTies(struct altGraphX *ag, struct psl *pslList, struct hash *apHash, struct sqlConnection *conn)
/* Return the first id whose representative sequence we can find in our ag. */
{
struct psl *psl = NULL, *tmpPsl=NULL;
struct affyPres *ap = NULL;
int maxOverlap = 0;
static char buff[256];
static char closeBuff[256];
int minDiff = BIGNUM;
*buff = 0;
*closeBuff =0;
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    ap = hashFindVal(apHash, psl->qName);
    if(ap == NULL)
	warn("Can't find %s in apHash", psl->qName);
    else
	{
	char *info = cloneString(ap->info);
	char *repString = NULL;
	if(strstr(info, "gb:"))
	    {
	    repString = strstr(info, "gb:");
	    repString += 3;
	    }
	else if(strstr(info, "Cluster_Incl._"))
	    {
	    repString = strstr(info, "Cluster_Incl._");
	    repString += 14;
	    }
	else if (strstr(info, "gb|"))
	    {
	    repString = strstr(info, "gb|");
	    repString += 3;
	    }
	if(repString != NULL)
	    {
	    char *tmp = strstr(repString+4, "_");
	    int overlap = 0;
	    int distance = 0;
	    if(tmp != NULL)
		{
		int i;
		*tmp = 0;
		tmp = strstr(repString, ".");
		if(tmp != NULL)
		    *tmp = 0;
		tmpPsl = getPslForId(repString, conn);
		/* First try direct overlap. */
		overlap = calcOverlap(tmpPsl, ag);
		/* If the overlap isn't good enough try overlapping the probe. */
		if(overlap <= maxOverlap)
		    {
		    struct psl *tmp = psl->next;
		    psl->next = NULL;
		    overlap = calcOverlap(psl, ag);
		    psl->next = tmp;
		    }
		if(overlap > maxOverlap)
		    {
		    snprintf(buff, sizeof(buff), "%s", ap->probeId);
		    maxOverlap = overlap;
		    }
		/* then try distance. */
		distance = calcDistance(tmpPsl, ag);
		if(distance < minDiff)
		    snprintf(closeBuff, sizeof(closeBuff), "%s", ap->probeId);
		pslFreeList(&tmpPsl);
		}
	    }	
	freez(&info);
	}
    }
if(maxOverlap == 0 && minDiff == BIGNUM)
    warn("Out of %d psl none overlapped for %s:%d-%d", slCount(pslList), ag->tName, ag->tStart, ag->tEnd);

if(*buff == 0 && *closeBuff != 0)
    return closeBuff;
return buff;
}
	
char *findAffyId(struct altGraphX *ag, struct sqlConnection *conn, struct hash *apHash)
{
struct psl *pslList = NULL, *psl = NULL;
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
int pslCount = 0;
char buff[256];
char *id = NULL;
snprintf(buff, sizeof(buff), "strand = '%s'", ag->strand);
sr = hRangeQuery(conn, "affy_HG_U133A", ag->tName, ag->tStart, ag->tEnd, buff, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
    slAddHead(&pslList, psl);
    }
sqlFreeResult(&sr);
pslCount = slCount(pslList);
if(pslCount == 0)
    return NULL;
if(pslCount == 1)
    return pslList->qName;
id =  resolveTies(ag, pslList, apHash, conn);
if(pslCount > 0 && id == NULL)
    warn("Out of %d psls couldn't find probe for %s:%d-%d", ag->tName, ag->tStart, ag->tEnd);
return id;
}

void hashApList(struct hash *apHash, struct affyPres *apList)
{
struct affyPres *ap = NULL;
for(ap=apList; ap!=NULL; ap=ap->next)
    {
    hashAddUnique(apHash, ap->probeId, ap);
    }
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
		    float confidence = altGraphConfidenceForEdge(agx, i, 10);
		    if(confidence > minConfidence) 
			{
			char *string = cloneString(ap->info);
			fprintf(log, "%f\t%d\n", ave, count);
			passCount++;
			subChar(string, '_', ' ');
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

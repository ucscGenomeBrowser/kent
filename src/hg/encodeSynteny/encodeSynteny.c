/* encodeSynteny - create HTML files to compare syntenic predictions from liftOver and Mercator */
#include "encodeSynteny.h"

void usage()
{
errAbort("encodeSynteny - summarize synteny predictions from liftOver and mercator.\n"
	 "                generates an HTML table with links to the browser to STDOUT.\n"
	 "usage:\n\tencodeSynteny\n");
}

struct sizeList *getEncodeNameSize()
/* Get encode region names. */
{
struct sqlConnection *conn = sqlConnect("hg16");
struct sqlResult *sr = sqlGetResult(conn, "select distinct name, chromEnd-chromStart as size from hg16.encodeRegions order by name");
char **row;
struct sizeList *list = NULL, *sl;

while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(sl);
    sl->name = cloneString(row[0]);
    sl->size = atoi(row[1]);
    slAddHead(&list, sl);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&list);
return list;
}

struct namedRegion *getNamedRegions(char *database, char *table, char *where)
/* Get encode regions from input table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
struct namedRegion *list = NULL, *region;
char query[1024];

if (where==NULL)
    where = cloneString(" ");
safef(query, sizeof(query), "select chrom,chromStart,chromEnd,name from %s%s order by name", table, where);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->chrom      = cloneString(row[0]);
    region->chromStart = atoi(row[1]);
    region->chromEnd   = atoi(row[2]);
    region->name       = cloneString(row[3]);
    slAddHead(&list, region);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&list);
return list;
}

struct mercatorSummary *getMercatorRegions(char *database, char *name)
/* Get encode syntenic mercator regions from input table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char query[1024], **row;
struct mercatorSummary *list = NULL, *region;
char *thisChrom=NULL;

safef(query, sizeof(query),
      "select   e.name, e.chrom, e.chromStart, e.chromEnd, min(m.chromStart), max(m.chromEnd) "
      "from     encodeRegionLior m, encodeRegions e "
      "where    e.name=substring(m.name,1,6) and e.name='%s' and e.chrom=m.chrom "
      "group by e.name, e.chrom, e.chromStart, e.chromEnd "
      "order by e.name, chrom", name);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->name       = cloneString(row[0]);
    region->chrom      = cloneString(row[1]);
    region->chromStart = atoi(row[4]);
    region->chromEnd   = atoi(row[5]);
    region->diffStart  = atoi(row[4]) - atoi(row[2]);
    region->diffEnd    = atoi(row[5]) - atoi(row[3]);
    slAddHead(&list, region);
    thisChrom          = cloneString(row[1]);
    }
safef(query, sizeof(query),
	      "select   substring(name,1,6), chrom, min(chromStart),max(chromEnd) "
	      "from     encodeRegionLior "
	      "where    chrom<>'%s' and substring(name,1,6)='%s' "
	      "group by substring(name,1,6), chrom "
	      "order by substring(name,1,6), chrom", thisChrom, name);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->name       = cloneString(row[0]);
    region->chrom      = cloneString(row[1]);
    region->chromStart = atoi(row[2]);
    region->chromEnd   = atoi(row[3]);
    region->diffStart  = 999999999;
    region->diffEnd    = 999999999;
    slAddHead(&list, region);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&list);
return list;
}

void printHtmlHeader()
{
printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
printf("<HTML>\n  <HEAD>\n    <TITLE>\n      ");
printf("ENCODE Syntenic Region comparison: Mercator vs. Chain/Net\n    </TITLE>\n");
printf("  </HEAD>\n\n  <BODY>\n");
}

void printLink(char *assembly, char *chrom, int chromStart, int chromEnd, char *color)
{
char URL[]="http://hgwdev-daryl.cse.ucsc.edu/cgi-bin/hgTracks?";
char options[]="chainHg16=squish&chromInfo=full&encodeRegionLior"
    "=pack&encodeRegions=pack&gap=dense&netHg16=full&rmsk=dense&ruler=dense";

printf("          <A HREF=\"%s",URL);
printf("db=%s&position=%s:%d-%d&%s\"><FONT color=%s>%s:%d-%d</FONT></A><BR>\n",
       assembly,chrom,chromStart,chromEnd,options,color,chrom,chromStart,chromEnd);
}

void printColumnSeparator()
{
printf("        </TD>\n        <TD>\n");
}

void printTableHeader(int printMercatorSegments)
{
printf("    <TABLE rules=all>\n"
       "      <TR>\n"
       "        <TD>Assembly               </TD>\n"
       "        <TD>Name & hg16 size       </TD>\n"
       "        <TD>LiftOver Ranges        </TD>\n");
if (printMercatorSegments)
    printf("        <TD>Mercator Ranges        </TD>\n");
printf("        <TD>Mercator Summary       </TD>\n"
       "        <TD>MercStart<BR> - LoStart</TD>\n"
       "        <TD>MercEnd<BR> - LoEnd    </TD>\n"
       "        <TD>Consensus              </TD>\n"
       "        <TD>Consensus Sizes        </TD>\n"
//       "        <TD>Notes                    </TD>\n"
       "      </TR>\n");
}

void printRowAndRegion(char *assembly, char *name, int size)
{
printf("      <TR>\n        <TD>\n          <B>%s</B>\n", assembly);
printColumnSeparator();
printf("          <B>%s</B>: %d\n", name, size);
printColumnSeparator();
}

void summarizeRegions()
{
int assemblyCount = 4;
//char *assembly[6] = {"mm3", "rn3", "panTro1", "danRer1", "galGal2", "fr1"};
char  *assembly[4] = {"mm3", "rn3", "panTro1", "galGal2"};
char   where[256], query[1024], diff[2048], buf[2048];
char   spacer[]= "<BR>\n          ";
struct dyString *mStartDiffs = dyStringNew(0), *mEndDiffs = dyStringNew(0);
struct sizeList *region, *encodeRegions = getEncodeNameSize();
struct namedRegion *loRegion, *liftOver, *meRegion, *mercator;
struct mercatorSummary *mes, *meSummary;
struct consensusRegion *cr=NULL, *crList=NULL;
int    i, printMercatorSegments=0;

/* Loop through each assembly. */
for (i=0; i < assemblyCount; i++)
    {
    printTableHeader(printMercatorSegments);
//    assem = cloneString(assembly[i]);
    /* Loop through each ENCODE region. */
    for (region = encodeRegions; region != NULL; region = region->next)
	{
	cr=NULL;
	crList=NULL;

	// 1: print row start and region name/size
	printRowAndRegion(assembly[i], region->name, region->size);

	// 2: print LiftOver regions
	safef(where, sizeof(where), " where name = '%s'", region->name);
	liftOver = getNamedRegions(assembly[i], "encodeRegions", where);
	if (liftOver == NULL)
	    printf("          <FONT color=red>(no synteny predicted)</FONT><BR>\n");
	for (loRegion = liftOver; loRegion != NULL; loRegion = loRegion->next)
	    {
	    printLink(assembly[i],loRegion->chrom,loRegion->chromStart,loRegion->chromEnd,"blue");
	    AllocVar(cr);
	    cr->chrom      = cloneString(loRegion->chrom);
	    cr->chromStart = loRegion->chromStart;
	    cr->chromEnd   = loRegion->chromEnd;
	    slAddHead(&crList, cr);
	    }
	printColumnSeparator();

	// 3: print Mercator regions
	if (printMercatorSegments)
	    {
	    safef(where, sizeof(where), " where name like '%s%%'", region->name);
	    mercator = getNamedRegions(assembly[i], "encodeRegionLior", where);
	    if (mercator == NULL)
		printf("          <FONT color=red>(no synteny predicted)</FONT><BR>\n");
	    for (meRegion = mercator; meRegion != NULL; meRegion = meRegion->next)
		printLink(assembly[i],meRegion->chrom,meRegion->chromStart,meRegion->chromEnd,"blue");
	    printColumnSeparator();
	    }

	// 4: print Mercator summary (and get differences)
	meSummary = getMercatorRegions(assembly[i], region->name);

	ZeroVar(mStartDiffs);
	ZeroVar(mEndDiffs);
	if (meSummary == NULL)
	    printf("          <FONT color=red>(no synteny predicted)</FONT><BR>\n");
	for (mes = meSummary; mes != NULL; mes = mes->next)
	    {
	    printLink(assembly[i],mes->chrom,mes->chromStart,mes->chromEnd,"blue");
	    // save the start difference
	    if (mes->diffStart==999999999)
		safef(diff, sizeof(diff), "<FONT color=red>(null)</FONT>");
	    else if (mes->diffStart==0)
		safef(diff, sizeof(diff), "<FONT color=green><B>0</B></FONT>");
	    else
		safef(diff, sizeof(diff), "%d", mes->diffStart);
	    dyStringAppend(mStartDiffs, diff);
	    // save the end difference
	    if (mes->diffEnd==999999999)
		safef(diff, sizeof(diff), "<FONT color=red>(null)</FONT>");
	    else if (mes->diffEnd==0)
		safef(diff, sizeof(diff), "<FONT color=green><B>0</B></FONT>");
	    else
		safef(diff, sizeof(diff), "%d", mes->diffEnd);
	    dyStringAppend(mEndDiffs, diff);
	    // append spacer if needed
	    if (mes->next != NULL)
		{
		dyStringAppend(mStartDiffs, spacer);
		dyStringAppend(mEndDiffs, spacer);
		}
	    // if chrom exists, update coords.  Otherwise, store new consensusRegion
	    for (cr=crList; cr!=NULL; cr=cr->next)
		{
		if (sameWord(cr->chrom, mes->chrom))
		    break;
		}
	    if (cr==NULL) /* coordinates on this chrom not previously stored */
		{
		AllocVar(cr);
		cr->chrom      = cloneString(mes->chrom);
		cr->chromStart = mes->chromStart;
		cr->chromEnd   = mes->chromEnd;
		slAddHead(&crList, cr);
		}
	    else
		{
		if (cr->chromStart >  mes->chromStart)
		    cr->chromStart = mes->chromStart;
		if (cr->chromEnd   < mes->chromEnd)
		    cr->chromEnd   = mes->chromEnd;
		}
	    }
	slReverse(&crList);
	printColumnSeparator();

	// 5 and 6: print MeStart-LoStart and MeEnd-LoEnd
	if (mStartDiffs->string==NULL)
	    {
	    printf("          <FONT color=red>%s</FONT color=red>\n", mStartDiffs->string);
	    printColumnSeparator();
	    printf("          <FONT color=red>%s</FONT color=red>\n", mEndDiffs->string);
	    }
	else
	    {
	    printf("          %s\n", mStartDiffs->string);
	    printColumnSeparator();
	    printf("          %s\n", mEndDiffs->string);
	    }
	printColumnSeparator();

	// 7: print Consensus Ranges (and get sizes)
	if (crList==NULL)
	    printf("          <FONT color=red>(null)</FONT> \n");
	for (cr=crList; cr!=NULL; cr=cr->next)
	    printLink(assembly[i], cr->chrom, cr->chromStart, cr->chromEnd,"blue");
	printColumnSeparator();

	// 8: print Consensus Sizes
	if (crList==NULL)
	    printf("          <FONT color=red>(null)</FONT> \n");
	for (cr=crList; cr!=NULL; cr=cr->next)
	    printf("          %d<BR>\n",cr->chromEnd-cr->chromStart-1);
	slFreeList(&crList);
	printColumnSeparator();

	// 9: print Notes ??
	printf("        </TD>\n");

	printf("      </TR>\n");
	}
    printf("    </TABLE>\n  <BR>\n  <BR>\n");
    }
dyStringFree(&mStartDiffs);
dyStringFree(&mEndDiffs);
printf("  </BODY>\n</HTML>\n");
}

int main(int argc, char *argv[])
{
if(argc != 1)
    usage();
printHtmlHeader();
summarizeRegions();
return 0;
}

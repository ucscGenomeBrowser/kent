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
struct sqlResult *sr = sqlGetResult(conn, 
	    "select distinct name, chrom, chromStart, chromEnd, chromEnd-chromStart as size "
	    "from   hg16.encodeRegions order by name");
char **row;
struct sizeList *list = NULL, *sl;

while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(sl);
    sl->name       = cloneString(row[0]);
    sl->chrom      = cloneString(row[1]);
    sl->chromStart = atoi(row[2]);
    sl->chromEnd   = atoi(row[3]);
    sl->size       = atoi(row[4]);
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
safef(query, sizeof(query), 
      "select chrom, chromStart, chromEnd, name, notes "
      "from   %s%s order by name", table, where);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->notes      = dyStringNew(0);
    region->chrom      = cloneString(row[0]);
    region->chromStart = atoi(row[1]);
    region->chromEnd   = atoi(row[2]);
    region->name       = cloneString(row[3]);
    if (row[4]) 
	{
	dyStringAppend(region->notes,row[4]);
	dyStringAppend(region->notes,"; ");
	}
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
      "select   e.name, e.chrom, e.chromStart, e.chromEnd, min(m.chromStart), max(m.chromEnd), max(m.notes) \n"
      "from     encodeRegionLior m, encodeRegions e \n"
      "where    e.name=substring(m.name,1,6) and e.name='%s' and e.chrom=m.chrom \n"
      "group by e.name, e.chrom, e.chromStart, e.chromEnd \n"
      "order by e.name, chrom\n", name);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->notes      = dyStringNew(0);
    region->name       = cloneString(row[0]);
    region->chrom      = cloneString(row[1]);
    region->chromStart = atoi(row[4]);
    region->chromEnd   = atoi(row[5]);
    region->diffStart  = atoi(row[4]) - atoi(row[2]);
    region->diffEnd    = atoi(row[5]) - atoi(row[3]);
    if (row[6]) 
	{
	dyStringAppend(region->notes, row[6]);
	dyStringAppend(region->notes,"; ");
	}
    slAddHead(&list, region);
    thisChrom          = cloneString(row[1]);
    }
safef(query, sizeof(query),
	      "select   substring(name,1,6), chrom, min(chromStart),max(chromEnd), max(notes) \n"
	      "from     encodeRegionLior \n"
	      "where    chrom<>'%s' and substring(name,1,6)='%s' \n"
	      "group by substring(name,1,6), chrom \n"
	      "order by substring(name,1,6), chrom \n", thisChrom, name);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->notes      = dyStringNew(0);
    region->name       = cloneString(row[0]);
    region->chrom      = cloneString(row[1]);
    region->chromStart = atoi(row[2]);
    region->chromEnd   = atoi(row[3]);
    region->diffStart  = 999999999;
    region->diffEnd    = 999999999;
    if (row[4] != NULL) 
	{
	dyStringAppend(region->notes, row[4]);
	dyStringAppend(region->notes, "; ");
	}
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

printf("          <A TARGET=\"_BLANK\" HREF=\"%s",URL);
printf("db=%s&position=%s:%d-%d&%s\"><FONT color=%s>%s:%d-%d</FONT></A><BR>\n",
       assembly,chrom,chromStart,chromEnd,options,color,chrom,chromStart,chromEnd);
}

void printColumnSeparator()
{
printf("        </TD>\n        <TD NOWRAP>\n");
}

void printTableHeader(int printMercatorSegments)
{
printf("    <TABLE rules=all>\n"
       "      <TR>\n"
       "        <TD NOWRAP>Assembly               </TD>\n"
       "        <TD NOWRAP>Name & hg16 size       </TD>\n"
       "        <TD NOWRAP>LiftOver Ranges        </TD>\n");
if (printMercatorSegments)
    printf("        <TD NOWRAP>Mercator Ranges        </TD>\n");
printf("        <TD NOWRAP>Mercator Summary       </TD>\n"
       "        <TD NOWRAP>MercStart<BR> - LoStart</TD>\n"
       "        <TD NOWRAP>MercEnd<BR> - LoEnd    </TD>\n"
       "        <TD NOWRAP>Consensus              </TD>\n"
       "        <TD NOWRAP>Consensus Sizes        </TD>\n"
       "        <TD NOWRAP>Notes                    </TD>\n"
       "      </TR>\n");
}

void printRowAndRegion(char *assembly, struct sizeList *region)
{
char URL[]="http://hgwdev-daryl.cse.ucsc.edu/cgi-bin/hgTracks?";
char options[]="chainHg16=squish&chromInfo=full&encodeRegionLior"
    "=pack&encodeRegions=pack&gap=dense&netHg16=full&rmsk=dense&ruler=dense";

printf("      <TR>\n        <TD ALIGN=\"RIGHT\" NOWRAP>\n          <B>%s</B>\n", assembly);
printColumnSeparator();
printf("          <A TARGET=\"_BLANK\" HREF=\"%s",URL);
printf("db=hg16&position=%s:%d-%d&%s\"><B>%s</B></A>: %d<BR>\n",
       region->chrom,region->chromStart,region->chromEnd,options, region->name, region->size);
printColumnSeparator();
}

void summarizeRegions()
{
int assemblyCount = 4;
//char *assembly[6] = {"mm3", "rn3", "panTro1", "danRer1", "galGal2", "fr1"};
char  *assembly[4] = {"mm3", "rn3", "panTro1", "galGal2"};
char   where[256], query[1024], diff[2048], buf[2048];
char   spacer[]= "<BR>\n          ";
struct dyString *notes = dyStringNew(0), *mStartDiffs = dyStringNew(0), *mEndDiffs = dyStringNew(0);
struct sizeList *region, *encodeRegions = getEncodeNameSize();
struct namedRegion *loRegion, *liftOver, *meRegion, *mercator;
struct mercatorSummary *mes, *meSummary;
struct consensusRegion *cr=NULL, *crList=NULL;
int    i, printMercatorSegments=0;
int    newStart, newEnd;
char URL[]="http://hgwdev-daryl.cse.ucsc.edu/cgi-bin/hgTracks?";
char options[]="chainHg16=squish&chromInfo=full&encodeRegionLior"
    "=pack&encodeRegions=pack&gap=dense&netHg16=full&rmsk=dense&ruler=dense";

/* Loop through each assembly. */
for (i=0; i < assemblyCount; i++)
    {
    printTableHeader(printMercatorSegments);

    /* Loop through each ENCODE region. */
    for (region = encodeRegions; region != NULL; region = region->next)
	{
	cr=NULL;
	crList=NULL;

	// 1: print row start, region name, and hg16 size
	printRowAndRegion(assembly[i], region);

	// 2: print LiftOver regions
	safef(where, sizeof(where), " where name = '%s'", region->name);
	liftOver = getNamedRegions(assembly[i], "encodeRegions", where);
	if (liftOver == NULL)
	    printf("          <FONT color=red>(no synteny predicted)</FONT><BR>\n");
	for (loRegion = liftOver; loRegion != NULL; loRegion = loRegion->next)
	    {
	    printLink(assembly[i],loRegion->chrom,loRegion->chromStart,loRegion->chromEnd,"blue");
	    AllocVar(cr);
	    cr->notes      = dyStringNew(0);
	    cr->chrom      = cloneString(loRegion->chrom);
	    cr->chromStart = loRegion->chromStart;
	    cr->chromEnd   = loRegion->chromEnd;
	    if (loRegion->notes->string != NULL) 
		{
		dyStringAppend(cr->notes, loRegion->notes->string);
		dyStringAppend(cr->notes, "; ");
		}
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
		safef(diff, sizeof(diff), "<FONT color=black>(null)</FONT>");
	    else if (mes->diffStart==0)
		safef(diff, sizeof(diff), "<FONT color=green><B>0</B></FONT>");
	    else
		{
		if (mes->diffStart<0)
		    {
		    newStart = mes->chromStart;
		    newEnd   = mes->chromStart - mes->diffStart;
		    }
		else
		    {
		    newStart = mes->chromStart - mes->diffStart;
		    newEnd   = mes->chromStart;
		    }
		safef(diff, sizeof(diff),
		      "          <A TARGET=\"_BLANK\" HREF=\"%sdb=%s&position=%s:%d-%d&%s\">"
		      "<FONT color=red>%d</FONT></A>\n",
		      URL,assembly[i],mes->chrom,newStart,newEnd,options,mes->diffStart);
		}
	    dyStringAppend(mStartDiffs, diff);
	    // save the end difference
	    if (mes->diffEnd==999999999)
		safef(diff, sizeof(diff), "<FONT color=black>(null)</FONT>");
	    else if (mes->diffEnd==0)
		safef(diff, sizeof(diff), "<FONT color=green><B>0</B></FONT>");
	    else
		{
		if (mes->diffEnd>0)
		    {
		    newStart = mes->chromEnd - mes->diffEnd;
		    newEnd   = mes->chromEnd;
		    }
		else
		    {
		    newStart = mes->chromEnd;
		    newEnd   = mes->chromEnd - mes->diffEnd;
		    }		    
		safef(diff, sizeof(diff),
		      "          <A TARGET=\"_BLANK\" HREF=\"%sdb=%s&position=%s:%d-%d&%s\">"
		      "<FONT color=red>%d</FONT></A>\n",
		      URL,assembly[i],mes->chrom,newStart,newEnd,options,mes->diffEnd);
		}
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
		cr->notes      = dyStringNew(0);
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
	    if (mes->notes != NULL) 
		{
		dyStringAppend(cr->notes, mes->notes->string);
		dyStringAppend(cr->notes, "; ");
		}
	    }
	slReverse(&crList);
	printf("        </TD>\n        <TD NOWRAP ALIGN=\"RIGHT\">\n");

	// 5 and 6: print MeStart-LoStart and MeEnd-LoEnd
	if (mStartDiffs->string==NULL)
	    {
	    printf("          <FONT color=red>%s</FONT>\n", mStartDiffs->string);
	    printf("        </TD>\n        <TD NOWRAP ALIGN=\"RIGHT\">\n");
	    printf("          <FONT color=red>%s</FONT>\n", mEndDiffs->string);
	    }
	else
	    {
	    printf("          %s\n", mStartDiffs->string);
	    printf("        </TD>\n        <TD NOWRAP ALIGN=\"RIGHT\">\n");
	    printf("          %s\n", mEndDiffs->string);
	    }
	    printColumnSeparator();

	// 7: print Consensus Ranges (and get sizes)
	if (crList==NULL)
	    printf("          <FONT color=red>(null)</FONT> \n");
	for (cr=crList; cr!=NULL; cr=cr->next)
	    {
	    if (cr->chromEnd-cr->chromStart<2000)
		printLink(assembly[i], cr->chrom, cr->chromStart, cr->chromEnd,"red");
	    else
		printLink(assembly[i], cr->chrom, cr->chromStart, cr->chromEnd,"blue");
	    }
	printColumnSeparator();

	// 8: print Consensus Sizes
	if (crList==NULL)
	    printf("          <FONT color=red>(null)</FONT> \n");
	for (cr=crList; cr!=NULL; cr=cr->next)
	    {
	    if (cr->chromEnd-cr->chromStart<2000 || cr->chromEnd-cr->chromStart > (2*region->size))
		printf("          <FONT COLOR=RED>%d</FONT><BR>\n",cr->chromEnd-cr->chromStart-1);
	    else
		printf("          %d<BR>\n",cr->chromEnd-cr->chromStart-1);
	    }
	printColumnSeparator();

	// 9: print Notes
	if (crList==NULL)
	    printf("          No notes. \n");
	for (cr=crList; cr!=NULL; cr=cr->next)
	    printf("          %s<BR>\n",cr->notes->string);
	slFreeList(&crList);
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

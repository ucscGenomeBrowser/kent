/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"


static char const rcsid[] = "$Id: hgData.c,v 1.1.2.28 2009/04/23 05:01:31 mikep Exp $";
// #/g/project/(data|summary)/{project}/{pi}?/{lab}?/{datatype}?/{track}?/{genome}?
// RewriteRule ^/g/project/(data|summary)/([^/]+)(/([^/]+)(.*))?$ /$5?cmd=$1\&project=$2\&pi=$4 [C]
// RewriteRule ^/(/([^/]+)(/([^/]+)(/([^/]+)(/([^/]+))?)?)?)?$ /cgi-bin/hgData?lab=$2\&datatype=$4\&track=$6\&genome=$8 [PT,QSA,L,E=ETag:%{HTTP:If-None-Match},E=Modified:%{HTTP:If-Modified-Since},E=Authorization:%{HTTP:Authorization}]

// curl  -v --data-binary @test.bed http://mikep/g/project/data/wgEncode/Gingeras/Helicos/RnaSeq/Alignments/K562,cytosol,longNonPolyA/hg18?filename=testing.bed

struct slPair *nameValueListToPair(char *s)
{
// format: a=b,c=d,e=f        result-> b,d,f
struct slPair *vars;
char *t = cloneString(s);
memSwapChar(t, strlen(t), ',', ' ');
vars = slPairFromString(t);
freez(&t);
return vars;
}

boolean hashAddPair(struct hash *h, char *s)
/* If string s contains a pair in the format "name: value"
 * Add the name:value pair to the hash and return TRUE
 * Otherwise return FALSE */
{
char *words[2];
char *ss = cloneString(s);
boolean ret = FALSE;
if ( (chopByChar(ss, ':', words, sizeof(words))) == 2)
    {
    MJP(2);verbose(2,"header=[%s:%s]\n", trimSpaces(words[0]), trimSpaces(words[1]));
    hashAdd(h, trimSpaces(words[0]), trimSpaces(words[1]));
    ret = TRUE;
    }
freez(&ss);
return ret;
}

void doUpdate(char *method)
{
long i = 0;
int len;
boolean ok;
struct hash *h = newHash(0);
struct hash *hHeader = newHash(0);
FILE *f;
struct lineFile *lf;
char *line;
int lineSize;
char *boundary;
struct cgiVar *cgi = NULL;
char *url = getenv("SCRIPT_URL");
char *query = cloneString(getenv("QUERY_STRING"));
char *contLenStr = getenv("CONTENT_LENGTH");
long contLen = sqlUnsigned(contLenStr);
char *cmd, *project, *pi, *lab, *datatype, *view, *track, *genome;
struct dyString *tableName = newDyString(0);
struct dyString *fileName = newDyString(0);
struct slName *vars, *var;
MJP(2);verbose(2,"query_string=[%s] url=[%s] content_type=[%s] content_length=[%s = %ld]\n", 
    query, url, getenv("CONTENT_TYPE"), contLenStr, contLen);
if (!query || !cgiParseInput(query, &h, &cgi))
    errAbort("Could not parse query string [%s]\n", query);
cmd      = hashMustFindVal(h, "cmd");
project  = hashFindVal(h, "project");
pi       = hashFindVal(h, "pi");
lab      = hashFindVal(h, "lab");
datatype = hashFindVal(h, "datatype");
view     = hashFindVal(h, "view");
track    = hashFindVal(h, "track");
genome   = hashFindVal(h, "genome");
//verboseSetLevel(sqlUnsigned(hashOptionalVal(h, "verbose", "1")));
MJP(2);verbose(2,"cmd=%s, project=%s, pi=%s, lab=%s, datatype=%s, view=%s, track=%s, genome=%s.\n", 
    cmd, project, pi, lab, datatype, view, track, genome);
// split track into list of names
// capitalize each of the names
// join together into tableName using project....etc.
// create bigbed using this data
// Then:-
// count how many bytes: 
// - do without gzip
// - with gzip
// to see if on-the-fly compression works

// if (filename == NULL)
//     errAbort("filename not found\n");
//wgEncode-Helicos-RnaSeq-Alignments-K562,Cytosol,Longpolya
vars = slNameListFromString(track, ',');
dyStringAppend(tableName, project);
dyStringAppend(tableName, lab);
dyStringAppend(tableName, datatype);
dyStringAppend(tableName, view);
for (var = vars ; var ; var = var->next)
    dyStringAppend(tableName, var->name);
dyStringAppend(fileName, "../trash/");
dyStringAppend(fileName, tableName->string);
MJP(2);verbose(2,"writing file [%s]\n", fileName->string);
if (sameOk(method, "POST"))
    f = mustOpen(fileName->string, "wb"); // POST = Append
else if (sameOk(method, "PUT"))
    f = mustOpen(fileName->string, "wb"); // PUT = Write
lf = lineFileStdin(TRUE);
// get the multipart/form-data boundary string
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("no boundary line\n");
boundary = cloneStringZ(line, lineSize);
MJP(2);verbose(2,"boundary=[%s]\n", boundary);
// get the headers for the first 
while ( (ok = lineFileNext(lf, &line, &lineSize)) && hashAddPair(hHeader, line))
    ;
if (!ok)
    errAbort("no more input after headers\n");
MJP(2);verbose(2,"ok=%d size=%d strlen=%d [%1x,%1x,%1x] line=[%s]\n", ok, lineSize, (int)strlen(line), line[0], line[1], line[2], line);
// data is terminated by <CR><LF> and then the boundary
// the <CR><LF> is interpreted as a line of length 0
i = 0;
while ((ok = lineFileNext(lf, &line, &lineSize)) && (len = strlen(line)) > 0)
    {
    ++i;
    MJP(2);verbose(2,"%3d strlen=%d [%s] lf->nlType=[%d]\n", lineSize, len, line, lf->nlType);
    mustWrite(f, line, len);
    mustWrite(f, "\n", 1);
    }
if (!ok)
    errAbort("no blank line after input\n");
// Check boundary matches. Note that this boundary has 2 more dashes on the end compared to the initial boundary.
if (lineFileNext(lf, &line, &lineSize))
    {
    if (!sameStringN(line, boundary, strlen(boundary)) && !sameString("--", line+strlen(boundary)))
	errAbort("line doesnt match boundary [%s] <> [%s]\n", line, boundary);
    }
else
    errAbort("no boundary line after blank line after data\n");
carefulClose(&f);
okSendHeader(0, 0);
printf("bytes read=%ld\ncontent_length=%ld\ncompression=%f%%\nurl=%s\n", i, contLen, 100.0*contLen/i, url);
MJP(2);verbose(2,"wrote %ld bytes to file [%s]\n", i, fileName->string);
slFreeList(&vars);
freeDyString(&fileName);
freeDyString(&tableName);
}

void doGet()
{
// these optional cgi strings are set to NULL if not present
logTime(NULL);
struct tm *thisTime, *latestTime;
struct chromInfo *ci = NULL;
struct trackDb *tdb = NULL;
struct dbDbClade *dbs = NULL;
struct hTableInfo *hti = NULL;
struct bed *b = NULL;
struct hgPositions *hgp = NULL;
struct wiggleDataStream *wds = NULL;
//struct sqlConnection *conn = NULL;
//char *authorization = getenv("Authorization");
//verboseSetLevel(2);
char *reqEtag = getenv("ETag");
time_t reqModified = strToTime(getenv("Modified"), "%a, %d %b %Y %H:%M:%S GMT");// Thu, 08 Jan 2009 17:45:18 GMT
char *cmd = cgiUsualString(CMD_ARG, "");
char *format = cgiUsualString(FORMAT_ARG, "");
char *genome = cgiUsualString(GENOME_ARG, "");
char *track = cgiUsualString(TRACK_ARG, "");
char *term = cgiUsualString(TERM_ARG, "");
char *strand = cgiUsualString(STRAND_ARG, "+");
if (!*strand)
    strand = "+";
char *chrom = cgiUsualString(CHROM_ARG, "");
int start = stripToInt(cgiUsualString(START_ARG, NULL)); // MJP FIXME: should do redirect to the new url if start matched [,.]
int end = stripToInt(cgiUsualString(END_ARG, NULL)); // MJP FIXME: should do redirect to the new url if end matched [,.]
char rootName[HDB_MAX_TABLE_STRING];
char parsedChrom[HDB_MAX_CHROM_STRING];
char *trackType[4] = {NULL,NULL,NULL,NULL};
int typeWords;
time_t modified = 0, genomeMod = 0, trackMod = 0;
AllocVar(thisTime);
AllocVar(latestTime);
// list information about all active genome databases
logTime("init(%s,%s,%s,%s:%c:%d-%d)", cmd, genome, track, chrom, *strand, start, end);
MJP(2); verbose(2,"cmd=[%s] genome=[%s] track=[%s] pos=[%s:%c:%d-%d]\n", cmd, genome, track, chrom, *strand, start, end);
if (*genome)
    {
    genomeMod = validateGenome(genome);
    if (*track)
	trackMod = validateLoadTrackDb(genome, track, &tdb);
    if (*chrom)
	validateChrom(genome, chrom, &ci);
    }
if (!*cmd)
    {
    // get modified time from CVS $Date field in file where code resides
    printUsage(reqEtag, reqModified);
    }
else if (sameOk(GENOMES_CMD, cmd)) 
    {
    logTime("%s(genome=%s)", cmd, genome);
    // no genome -> get all genomes
    // genome, no chrom => get genome & all chroms
    // genome, chrom => get genome & one chrom
    // check for changes in dbDb, genomeClade, clade
    modified = hGetLatestUpdateTimeDbClade();
    if (*genome)
	{
	modified = max(modified, genomeMod);
	if (!notModifiedResponse(reqEtag, reqModified, modified))
	    {
	    if (!(dbs = hGetIndexedDbClade(genome)))
		errAbort("Db and clade not found for genome [%s]\n", genome);
	    if (!*chrom)
		{
		if (!(ci = getAllChromInfo(genome)))
		    ERR_NO_CHROMS_FOUND(genome);
		// TEST FOR number of chroms 
		// if (hChromCount(genome) < MAX_CHROM_COUNT || cgiBoolean(ALLCHROMS_ARG)
		}
	    printGenomes(genome, chrom, dbs, ci, modified);
	    }
	}
    else
	{
	if (!notModifiedResponse(reqEtag, reqModified, modified))
	    {
	    if (!(dbs = hGetIndexedDbClade(NULL)))
		ERR_NO_GENOMES_FOUND;
	    printGenomes(genome, chrom, dbs, ci, modified);
	    }
	}
    }
else if (sameOk(TRACKS_CMD, cmd))
    {
    logTime("%s(genome=%s,track=%s)", cmd, genome, track);
    if (*genome)
	{
	// validate genome & track
	// modified date is greatest of all trackDb dates or genome.chromInfo 
	modified = max(genomeMod, trackDbLatestUpdateTime(genome));
	logTime("trackDbLatestUpdateTime(%s)", genome);
	if (*track)
	    modified = max(modified, trackMod);
	if (!notModifiedResponse(reqEtag, reqModified, modified))
	    {
	    if (*track)
		{
		hParseTableName(genome, tdb->tableName, rootName, parsedChrom);
		if (!(hti = hFindTableInfo(genome, chrom, rootName)))
		    ERR_TABLE_INFO_NOT_FOUND(tdb->tableName, chrom, rootName, genome);
		logTime("hFindTableInfo(%s,%s,%s)", genome, chrom, rootName);
		}
	    else 
		{
		if ( !(tdb = hTrackDb(genome, NULL)) )
		    ERR_TRACK_INFO_NOT_FOUND("<any>", genome);
		logTime("hTrackDb(%s,NULL)",genome);
		}
	    printTrackInfo(genome, track, tdb, hti, modified);
	    }
	}
    else
	{
        ERR_NOT_IMPLEMENTED("List of all tracks in all genomes");
	}
    }
else if (sameOk(SEARCH_CMD, cmd))
    {
    logTime("%s(genome=%s)", cmd, genome);
    if (*genome)
	{
	// If no search term, send empty result
	if (!*term)
	    searchTracks(0, NULL, NULL, NULL, NULL);
	// if a track was specified just search that
	if (*track)
	    { // check if genome, track, or spec table(s) have changed
	    modified = max(genomeMod, trackMod);
	    modified = max(modified, findSpecLatestUpdateTime(genome));
	    logTime("findSpecLatestUpdateTime(%s)", genome);
	    if (!notModifiedResponse(reqEtag, reqModified, modified))
		{
		char where[512];
		safef(where, sizeof(where), "searchTable = '%s'", track);
		hgp = hgPositionsFindWhere(genome, term, "dummyCgi", "dummyApp", NULL, TRUE, where);
		logTime("hgPositionsFindWhere(%s,%s,dummyCgi,dummyApp,NULL,TRUE,where=%s)", genome, term, where);
		//hgp = hgPositionsFindWhere(genome, term, NULL, NULL, NULL, TRUE, where);
		searchTracks(modified, hgp, genome, track, term);
		}
	    }
	else // we are searching through many tracks, so dont use cache (dont set last-modified)
	    {
	    hgp = hgPositionsFind(genome, term, "dummyCgi", "dummyApp", NULL, TRUE);
	    logTime("hgPositionsFind(%s,%s,dummyCgi,dummyApp,NULL,TRUE)", genome, term);
	    //hgp = hgPositionsFind(genome, term, NULL, NULL, NULL, TRUE);
	    searchTracks(0, hgp, genome, track, term);
	    }
	}
    else
	ERR_NOT_IMPLEMENTED("Search over all genomes");
    }
else if (sameOk(META_SEARCH_CMD, cmd))
    {
    }
else if (sameOk(COUNT_CMD, cmd) || sameOk(RANGE_CMD, cmd))
    {
    logTime("%s(genome=%s,track=%s)", cmd, genome, track);
    if (!*genome)
	ERR_NO_GENOME;
    if (!*track)
	ERR_NO_TRACK;
    if (*format && 
	  (differentString(format, ANNOJ_FLAT_FMT) && 
	   differentString(format, ANNOJ_NESTED_FMT)))
	ERR_BAD_FORMAT(format);
    // check genome.chromInfo, the track, or the trackDbs for changes
    modified = max(genomeMod, trackMod);
    modified = max(modified, trackDbLatestUpdateTime(genome));
    logTime("trackDbLatestUpdateTime(%s)", genome);
    if (!notModifiedResponse(reqEtag, reqModified, modified))
	{
	hParseTableName(genome, tdb->tableName, rootName, parsedChrom);
	if (!*chrom)
	    ERR_NO_CHROM;
	if (!(ci = hGetChromInfo(genome, chrom)))
	    ERR_CHROM_NOT_FOUND(genome, chrom);
	logTime("hGetChromInfo(%s,%s)", genome, chrom);
	if (end<start)
	    ERR_START_AFTER_END(start, end);
	if (start<0) // MJP FIXME: should do redirect to the new url with start d set to 0
	    start = 0;
	if (end<0) // MJP FIXME: should do redirect to the new url with end set to 0
	    end = 0;
	else if (end==0)
	    end = ci->size; // MJP FIXME: potential overflow problem, int <- unsigned
	else if (end > ci->size) // MJP FIXME: should do redirect to the new url with end set to chromSize
	    end = ci->size;

	if (!(hti = hFindTableInfo(genome, chrom, rootName)))
	    ERR_TABLE_NOT_FOUND(tdb->tableName, chrom, rootName, genome);
	logTime("hFindTableInfo(%s,%s,%s)", genome, chrom, rootName);
	// test how many rows we are about to receive
	if (sameOk(COUNT_CMD, cmd))
	    {
	    int n = hGetBedRangeCount(genome, tdb->tableName, chrom, start, end, NULL);
	    logTime("hGetBedRangeCount(%s,%s,%s,%d,%d,NULL) -> %d", genome, tdb->tableName, chrom, start, end, n);
	    if (differentString(track, tdb->tableName))
		errAbort("track name (%s) does not match table name (%s)\n", track, tdb->tableName);
	    printBedCount(genome, track, chrom, start, end, ci->size, hti, n, modified);
	    }
	else if (sameOk(RANGE_CMD, cmd))
	    {
	    typeWords = chopByWhite(tdb->type, trackType, sizeof(trackType));
	    // handle different track types
	    if (sameOk(trackType[0], "bed") || sameOk(trackType[0], "genePred"))  
		{
		int n = hGetBedRangeCount(genome, tdb->tableName, chrom, start, end, NULL);
		logTime("hGetBedRangeCount(%s,%s,%s,%d,%d,NULL) -> %d", genome, tdb->tableName, chrom, start, end, n);
		b = hGetBedRange(genome, tdb->tableName, chrom, start, end, NULL);
		logTime("hGetBedRange(%s,%s,%s,%d,%d,NULL) -> %d", genome, tdb->tableName, chrom, start, end);
		printBed(genome, track, tdb->type, chrom, start, end, ci->size, hti, n, b, format, modified);
		}
	    else if (sameOk(trackType[0], "wigMaf"))  
		{
		printMaf(genome, track, chrom, ci->size, start, end, strand);
		}
	    else if (sameOk(trackType[0], "wig"))  
		{
		int n;
		// options: wigFetchStats || wigFetchRawStats || wigFetchBed || wigFetchDataArray || wigFetchAscii
		wds = wigOutRegion(genome, track, chrom, start, end, MAX_WIG_LINES, 
			wigFetchDataArray|wigFetchAscii|wigFetchBed, &n);
		// ascii: asciiData->count
		// bed:   slCount(wds->bed)
		logTime("wigOutRegion(%s,%s,%s,%d,%d,%d,options=%p) -> %d", genome, track, chrom, start, end, 
			MAX_WIG_LINES, wigFetchDataArray|wigFetchAscii|wigFetchBed, n);
		printWig(genome, track, chrom, ci->size, start, end, strand, n, wds);
		}
	    else
		{
		ERR_BAD_TRACK_TYPE(track, tdb->type);
		}
	    }
	}
    }
else
    {
    ERR_INVALID_COMMAND(cmd);
    }
dbDbCladeFreeList(&dbs);
chromInfoFreeList(&ci);
trackDbFree(&tdb);
freez(&hti);
bedFreeList(&b);
hgPositionsFree(&hgp);
wiggleDataStreamFree(&wds);
logTime("complete.");
}

int main(int argc, char *argv[])
/* Process command line.
 * Stateless server; No "CART" required for REST interface */
{
cgiSpoof(&argc, argv); // spoof cgi vars if running from command line

char *method = getenv("REQUEST_METHOD");
//initCgiInputMethod(cgiRequestMethod()); // otherwise initialize from request_method
//verboseSetLevel(cgiOptionalInt("verbose", 1));
verboseSetLevel(2);

//if (sameOk(cgiRequestMethod(), "GET"))
if (sameOk(method, "GET"))
    doGet();
else if (sameOk(method, "POST")||sameOk(method, "PUT"))
    doUpdate(method);
else
    errAbort("request method %s not implemented\n", method);
return 0;
}


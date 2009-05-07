/* hgData - simple RESTful interface to genome data. */
#include "common.h"
#include "hgData.h"
#include "asParse.h"
#include "bigBed.h"
#include "localmem.h"
#include "ra.h"

// Rough speed estimates; 
// Can process 125,000 lines per second to make a Bigbed on hgwdev 
// using input to/from hive
// Want to process data immediately if it would probably take less  than BIGBED_MAX_SECS
#define BIGBED_LINES_PER_SEC 125000
#define BIGBED_MAX_SECS 10

// suggested defaults from bigBed.h
#define BLOCKSIZE    1024
#define ITEMS_PER_SLOT 64

// use large blocks of memory as we expect to receive large bigBed files
#define LM_BLOCKSIZE 8*1024*1024

// paths to required config files
#define CHROM_FILE_HG18 "/cluster/home/mikep/kent/src/hg/encode/validateFiles/hg18_chromInfo.txt"
#define AS_FILE_PATH "/cluster/bin/sqlCreate/"
#define CV_CONFIG_FILE "/cluster/data/encode/pipeline/config/cv.ra"

static char const rcsid[] = "$Id: hgData.c,v 1.1.2.31 2009/05/07 07:25:26 mikep Exp $";

struct hash *hAsFile = NULL;
struct hash *hCvRa = NULL;
struct hash *hChrFile = NULL;

struct asObject *readAsFile(char *asFileName)
// Read as file
{
struct asObject *as = NULL;
if (asFileName != NULL)
    {
    /* Parse it and do sanity check. */
    as = asParseFile(asFileName);
    if (as->next != NULL)
        errAbort("Can only handle .as files containing a single object.");
    }
return as;
}

/*struct slPair *nameValueStringToPair(char *s, char delim)
{
// format: a=b<delim> c=d<delim>e=f        result-> b,d,f
struct slPair *vars;
char *t = cloneString(s);
memSwapChar(t, strlen(t), delim, ' ');
vars = slPairFromString(t);
freez(&t);
return vars;
}
*/

// struct slPair *nameValueListToPair(char *s)
// {
// // format: a=b,c=d,e=f        result-> b,d,f
// return nameValueStringToPair(s, ',');
// }

char *contentDispositionFileName(char *content)
// Parse a "Content-Disposition" header of the format:
// [form-data; name="a_file"; filename="test.100m.bed"]
// into its own list of pairs to find filename paramter
{
char *filename;
struct slPair *cl;
MJP(2);verbose(2,"(%s)\n", content);
// parse this header into its own pairs, possibly terminated by ';'
if (!startsWith("form-data; ", content))
    errAbort("Content-Disposition not form-data [%s]\n", content);
content = content+11; // skip over this bit to name=value pairs section
MJP(2);verbose(2," len=%lu content=[%s]\n", strlen("form-data; "), content);
cl = slPairFromString(content);
if ( (filename = slPairFindVal(cl, "filename")) == NULL)
    errAbort("Could not find filename in Content-Disposition header [%s]\n", content);
filename = cloneString(filename);
// strip off trailing ';' if any
if (strlen(filename) > 0 && filename[strlen(filename)-1] == ';')
    filename[strlen(filename)-1] = '\0';
// strip off surrounding double-quotes
stripChar(filename, '"');
slPairFreeValsAndList(&cl);
return filename;
}

boolean hashAddPair(struct hash *h, char *s, char delim)
/* If string s contains a pair in the format "name<delim> value"
 * Add the (name,value) to the hash and return TRUE
 * Otherwise return FALSE */
{
char *words[2];
char *ss = cloneString(s);
boolean ret = FALSE;
MJP(2);verbose(2,"s=[%s]\n", s);
if ( (chopByChar(ss, delim, words, sizeof(words))) == 2)
    {
    MJP(2);verbose(2,"header [%s]=[%s]\n", trimSpaces(words[0]), trimSpaces(words[1]));
    hashAdd(h, trimSpaces(words[0]), trimSpaces(words[1]));
    ret = TRUE;
    }
freez(&ss);
return ret;
}

char *myFindVarData(struct hash *inputHash, char *varName)
/* Get the string associated with varName from the query string. 
 * Similar to cheapcgi.c:findVarData() but takes cgi hash as input,
 * and doesnt have the call to initCgiInput() */
{
struct cgiVar *var;

//initCgiInput();
if ((var = hashFindVal(inputHash, varName)) == NULL)
    return NULL;
return var->val;
}

boolean readMultipartHeaders(struct lineFile *lf, struct hash *h)
// Read headers from lineFile
// Header values are saved in hash h keyed by header name
// Returns TRUE if if the header was terminated by a blank line
// and FALSE if header terminated with end of file
{
boolean ok;
char *line;
int lineSize;
MJP(2);verbose(2,"reading headers\n");
while ( (ok = lineFileNext(lf, &line, &lineSize)) )
    {
    // if we cant find any more pairs, must be end of headers
    MJP(2);verbose(2,"header line=[%s]\n", line);
    if (!hashAddPair(h, line, ':'))
	break;
    }
return ok;
}


boolean expect100Continue()
// Return true if headers include
//   "Expect: 100-continue"
{
MJP(2);verbose(2,"Expet=[%s]\n", getenv("Expect"));
return sameOk(getenv("Expect"), "100-continue");
}


void processBigBed(char *fileName, struct lineFile *lf, struct hash *chromHash, 
    char *sourceFile, char *track, char *asFileName, char *locationUrl)
// Process lines of input from lf until the first blank line or end of file
// Build up a list of ppBed objects maintaining the count and average size
// If the number of lines is low enough to process within a time threshold
// then build the bigBed directly and return '201 Created' response.
// Otherwise return a '202 Accepted' response, close the connection to the 
// client and continue and create the bigBed.
{
struct ppBed *pbList = NULL;
struct lm *lm = lmInit(LM_BLOCKSIZE);
struct asObject *as = NULL;
boolean ok;
int fieldCount = 0, fieldAlloc=0;
char *line;
int lineSize, len;
char **row = NULL;
char *prevChrom = NULL; // set after first time through loop
bits32 prevStart = 0;   // ditto
double totalSize = 0.0;
long count = 0;
bits64 fullSize = 0;
bits16 definedFieldCount = 0;
boolean delayedCreate = FALSE;
// lineSize and strlen(line) can mismatch because linefile has <CR><LF> line terminators
// in the header, and <LF> lines in the request body (for example if the BED file is from UNIX)
while ((ok = lineFileNext(lf, &line, &lineSize)) && (len = strlen(line)) > 0)
    {
    if ((count % 100000) == 0)
	{
	MJP(2);verbose(2,"lines=%ld\n", count);
	}
    MJP(3);verbose(4,"%3d strlen=%d [%s] lf->nlType=[%d]\n", lineSize, len, line, lf->nlType);
    /* First time through figure out the field count, and if not set, the defined field count. */
    if (fieldCount == 0)
	{
	if (as == NULL)
	    {
	    fieldCount = chopByWhite(line, NULL, 0);
	    if (definedFieldCount == 0)
		definedFieldCount = fieldCount;
	    if (as == NULL)
		{
		char *asText = bedAsDef(definedFieldCount, fieldCount);
		as = asParseText(asText);
		freeMem(asText);
		}
	    }
	else
	    {
	    fieldCount = slCount(as->columnList);
	    }
	fieldAlloc = fieldCount+1;
	AllocArray(row, fieldAlloc);
	}
    int wordCount = chopByWhite(line, row, fieldAlloc);
    lineFileExpectWords(lf, fieldCount, wordCount);
    bits64 diskSize = 0;
    ++count;
    struct ppBed *pb = ppBedLoadOne(row, fieldCount, lf, chromHash, lm, as, &diskSize);
    if (pb->start > pb->end)
	errAbort("Start > end (%d > %d) at line #%ld\n", 
	    pb->start, pb->end, count); 
    // first time through the loop prevChrom is NULL so test fails
    if (prevChrom && prevChrom == pb->chrom && pb->start < prevStart)
	errAbort("Input is not sorted line #%ld [previous=(%s,%d) current=(%s,%d)]\n", 
	    count, prevChrom, prevStart, pb->chrom, pb->start); 
    prevChrom = pb->chrom;
    prevStart = pb->start;
    totalSize += pb->end - pb->start;
    fullSize += diskSize;
    slAddHead(&pbList, pb);
    }
MJP(2);verbose(2,"read lines=%ld\n", count);
slReverse(&pbList);
// Now we have the full dataset.
// If it is not too big, process it immediately and send "201 Created" 
//   Include a "Location: " header pointing to the track status info.
// If it is too big, return a "202 Accepted" with guesstimate of the time it will take
//   Include a "Location: " header pointing to the track status info.
//   Then, close the in & out files so apache will return the reponse to the client,
//   and continue on writing the bigBed.
// See HTTP 1.1 for status codes: http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
if ( (count / BIGBED_LINES_PER_SEC) <= BIGBED_MAX_SECS)
    {
    MJP(2);verbose(2,"Creating bigbed now (%ld lines, avgsize=%.2f, diskSize=%lld, sourceFile=%s)\n", 
	count, totalSize/count, fullSize, sourceFile);
    bigBedFileCreateDetailed(pbList, count, totalSize/count, track, chromHash, BLOCKSIZE, 
	ITEMS_PER_SLOT, definedFieldCount, fieldCount, asFileName, as, fullSize, fileName);
    send2xxHeader(201, 0, 0, "", locationUrl);
    }
else
    {
    MJP(2);verbose(2,"Creating bigbed LATER (%ld lines, avgsize=%.2f, diskSize=%lld)\n", count, totalSize/count, fullSize);
    send2xxHeader(202, 0, 0, "", locationUrl);
    printf("Estimated time to complete = %ld minutes\n", 1 + ((count / BIGBED_LINES_PER_SEC) / 60));
    lineFileClose(&lf);
    if (fclose(stdin))
	errnoAbort("Could not close stdin\n");
    if (fclose(stdout))
	errnoAbort("Could not close stdout\n");
    sleep(30);
    bigBedFileCreateDetailed(pbList, count, totalSize/count, track, chromHash, BLOCKSIZE, 
	ITEMS_PER_SLOT, definedFieldCount, fieldCount, asFileName, as, fullSize, fileName);
    delayedCreate = TRUE;
    }
if (!delayedCreate)
    {
    printf("url=%s\n", locationUrl);
    MJP(2);verbose(2,"wrote %ld lines to file [%s]\n", count, fileName);
    }
lmCleanup(&lm);
}

void doUpdate(char *method)
{
boolean ok = FALSE;
struct hash *h = newHash(0);
struct hash *hHeader = newHash(0);
struct lineFile *lf;
char *line;
int lineSize;
char *boundary = NULL;
char *sourceFile = NULL;
struct cgiVar *cgi = NULL;
char *url = cgiRequestUri();// getenv("SCRIPT_URL");
char *query = cloneString(cgiQueryString());
char *contLenStr = cgiRequestContentLength();
long contLen = contLenStr ? sqlUnsigned(contLenStr) : 0;
char *cmd, *action, *type, *project, *pi, *lab, *datatype, *view, *track, *genome, *variables;
char *asFileName;
cmd = action = type = project = pi = lab = datatype = view = track = genome = variables = asFileName = NULL;
struct hash *chromHash = NULL;
// turn off buffering of stdin, as it messes with lineFile unbuffered IO
if (setvbuf(stdin, NULL, _IONBF, 0))
    errnoAbort("Could not unbuffer stdin\n");
MJP(2);verbose(2,"query_string=[%s] url=[%s] content_length=[%s = %ld]\n", query, url, contLenStr, contLen);
if (!query || !cgiParseInput(query, &h, &cgi))
    errAbort("Could not parse query string [%s]\n", query);
if (!(cmd = myFindVarData(h, "cmd")) )
    errAbort("Malformed url: no command supplied (u=%s q=%s)\n", url, query);
if (sameString(cmd,"project"))
    {
    if ( !(project = myFindVarData(h, "project")) )
	errAbort("No project supplied\n");
    if ( !(action = myFindVarData(h, "action")) )
	errAbort("No action supplied\n");
    if ( !(track = myFindVarData(h, "track")) )
	errAbort("No track supplied\n");
    if ( !(genome = myFindVarData(h, "genome")) )
	errAbort("No genome supplied\n");
    type = myFindVarData(h, "type");
    // if wgEncode project we have some extra metadata
    if (sameString(project, "wgEncode"))
	{
	// valid values specified in DAF
	if ( !(pi = myFindVarData(h, "pi")) ) // PI (Principal Investigator)
	    errAbort("No pi supplied\n");
	if ( !(lab = myFindVarData(h, "lab")) )
	    errAbort("No lab supplied\n");
	if ( !(datatype = myFindVarData(h, "datatype")) )
	    errAbort("No datatype supplied\n");
	if ( !(variables = myFindVarData(h, "variables")) )
	    errAbort("No variables supplied\n");
	if ( !(view = myFindVarData(h, "view")) )
	    errAbort("No view supplied\n");
	if ( !(sourceFile = myFindVarData(h, "sourceFile")) )
	    errAbort("No sourceFile supplied\n");
	}
    MJP(2);verbose(2,"cmd=%s, project=%s, action=%s, track=%s, type=%s, genome=%s, pi=%s, lab=%s, datatype=%s, variables=%s, view=%s, sourceFile=%s.\n", cmd, project, action, track, type, genome, pi, lab, datatype, variables, view, sourceFile);
    // Load a track
    if (sameString(action, "tracks")) 
	{
	// we need the type of the track
	if ( !(type = myFindVarData(h, "type")) )
	    errAbort("No type supplied for track [%s]\n", track);
	chromHash = bbiChromSizesFromFile((char *)hashMustFindVal(hChrFile, genome)); // FIXME
	MJP(2);verbose(2, "Read %d chromosomes and sizes from %s\n",  chromHash->elCount, (char *)hashMustFindVal(hChrFile, genome));

	// check the request content-type and read headers
	lf = lineFileStdin(TRUE);
	if ( startsWith("multipart/form-data", cgiRequestContentType()) )
	    {
	    // get the multipart/form-data boundary string
	    if (!lineFileNext(lf, &line, &lineSize))
		errAbort("no boundary line\n");
	    boundary = cloneStringZ(line, lineSize);
	    MJP(2);verbose(2,"boundary=[%s]\n", boundary);
	    // get the multipart/form-data headers for the first part
	    ok = readMultipartHeaders(lf, hHeader);
	    if ( differentString(hashMustFindVal(hHeader, "Content-Type"),"text/plain") )
		errAbort("Unknown content-type [%s] in form-data\n", 
		    (char *)hashMustFindVal(hHeader, "Content-Type"));
	    // data is terminated by <CR><LF> and then the boundary
	    // the <CR><LF> is interpreted as a line of length 0
	    }
	else if ( sameString("text/plain", cgiRequestContentType()) )
	    {
	    }
	else
	    {
	    errAbort("Unknown request content-type [%s]\n", cgiRequestContentType());
	    }

	// Now process content
	if (sameString(type, "tagAlign"))
	    {
	    char fileName[1024];
	    //unsigned long lastUpdate;
	    safef(fileName, sizeof(fileName), "../trash/uploadedData/%s/projects/%s/tracks/", genome, project);
	    MJP(2);verbose(2,"makeDirsOnPath(%s)\n", fileName);
	    makeDirsOnPath(fileName);
	    safef(fileName, sizeof(fileName), "../trash/uploadedData/%s/projects/%s/tracks/%s.bb", genome, project, track);
	    // 
	    if ( sameString(method,"POST") && fileSize(fileName) != -1)
		{
		ERR_TRACK_EXISTS(track);
		}
	    if ( !(asFileName = (char *)hashMustFindVal(hAsFile, type)) )
		errAbort("Cant find as file for track type [%s]\n", type);
	    // if this is an expect request then respond OK, otherwise process request
	    if (expect100Continue())
		okSend100ContinueHeader();
	    else
		processBigBed(fileName, lf, chromHash, sourceFile, track, asFileName, "LOCATION URL");
	    }
	// End of (first) content block
	// Check boundary matches. Note that this boundary has 2 more dashes on the end 
	// compared to the initial boundary.
	if (boundary)
	    {
	    if (!ok)
		errAbort("no blank line after input\n");
	    if (lineFileNext(lf, &line, &lineSize))
		{
		if (!sameStringN(line, boundary, strlen(boundary)) && !sameString("--", line+strlen(boundary)))
		    errAbort("line doesnt match boundary [%s] <> [%s]\n", line, boundary);
		}
	    else
		errAbort("no boundary line after blank line after data\n");
	    }
	}
    }
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
verboseSetLevel(2);
// turn off buffering of stderr
if (setvbuf(stderr, NULL, _IONBF, 0))
    errnoAbort("Could not unbuffer stderr\n");
MJP(2);verbose(2,"cgiSpoof\n");
cgiSpoof(&argc, argv); // spoof cgi vars if running from command line

char *method = getenv("REQUEST_METHOD");
MJP(2);verbose(2,"method=%s\n", method);
//initCgiInputMethod(cgiRequestMethod()); // otherwise initialize from request_method
//verboseSetLevel(cgiOptionalInt("verbose", 1));
hChrFile = hashNew(0);
hashAdd(hChrFile, "hg18", CHROM_FILE_HG18);
hAsFile = hashNew(0);
hashAdd(hAsFile, "tagAlign", AS_FILE_PATH "tagAlign.as");
hCvRa = raReadAll(CV_CONFIG_FILE, "term");
/* Return hash that contains all ra records in file keyed
 * by given field, which must exist.  The values of the
 * hash are themselves hashes. */

MJP(2);verbose(2,"starting (%s)\n", method);
//if (sameOk(cgiRequestMethod(), "GET"))
if (sameOk(method, "GET"))
    doGet();
else if (sameOk(method, "POST")||sameOk(method, "PUT"))
    doUpdate(method);
else
    errAbort("request method %s not implemented\n", method);
return 0;
}


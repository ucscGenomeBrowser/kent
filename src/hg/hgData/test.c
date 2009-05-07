#include "common.h"
#include "options.h"
#include "linefile.h"
#include "net.h"
#include "hash.h"
#include "base64.h"
#include "hgData.h"
#include "zlib.h"
#include <limits.h>
#include <time.h>

#define CHUNK 128*1024
#define MAX_TRACK_NAME 64

static int const gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - track loader\n"
  "usage:\n"
  "   test project pi lab datatype variables view inputfile type genome [server]\n"
  "parameters:\n"
  "    project    : Name of the project, for example:\n"
  "                 'wgEncode' - Whole genome ENCODE project\n"
  "    pi         : PI (Principal Investigator, Gingeras, etc)\n"
  "    lab        : Lab (eg, SUNY, RIKEN, etc)\n"
  "    datatype   : Project Datatype (eg, RnaSeq, DnaseSeq, etc)\n"
  "    variables  : Variables (eg, K562,cytosol,longNonPolyA)\n"
  "    view       : View (eg, Alignments, RawSignal, RawData)\n"
  "    inputfile  : Name of the source data file which is to be sent\n"
  "                 - must be a single file (not a tar file)\n"
  "                 - must be sorted on (chrom,start) if it is BED type data\n"
  "                 - can be a '.gz' (gzip'ed) file\n"
  "                 - the file will be transmitted in compressed format\n" 
  "    type       : Type of data in the source file (eg, tagAlign, wig, broadPeak)\n"
  "    genome     : Genome this data refers to (eg, hg18)\n"
  "    server     : Webserver to send data to. Specify protocol, user, password and \n"
  "                 alternative port (default 80) if required. Format: \n"
  "                   http://[user:password@]hostname[:port]\n"
  "                 - If server is not specified then only validate the input file\n"
  "options:\n"
  "    -rep=N            : Replicate number, if any.\n"
  "    -sendUncompressed : If the file is uncompressed, send it as-is, dont compress it.\n"
  "    -append           : If data already exists for this track, append this data to it, dont overwrite it.\n"
  );
}

static struct optionSpec options[] = {
    {"sendUncompressed", OPTION_BOOLEAN},
    {"append", OPTION_BOOLEAN},
    {"rep", OPTION_INT},
    {NULL, 0},
};


void mustWriteFd(int fd, void *buf, size_t size)
/* Write to a file or squawk and die. */
{
size_t res;
if (size != 0)
    {
    if ( (res = write(fd, buf, size)) != size)
	{
	if (res == -1)
	    errnoAbort("Error writing %lld bytes\n", (long long)size);
	else
	    errAbort("Error writing %lld bytes (wrote %lld)\n", (long long)size, (long long)res);
	}
    }
}


boolean hashAddHeader(struct hash *h, char *s, int size)
/* If string s[0..size-1] contains a pair in the format "name: value\r\n"
 * names are converted to upper-case 
 * Add the (name,value) to the hash and return TRUE
 * Otherwise return FALSE */
{
char *val;
char ss[1024];
safencpy(ss, sizeof(ss), s, size);
if ( !(val = memchr(ss, ':', size)) )
    return FALSE;
*val++ = '\0'; // zero the delimiter
strUpper(ss); // uppercase name
// strip leading and trailing whitespace from val
val = trimSpaces(val);
MJP(2);verbose(2,"hdr(%s -> %s)\n", ss, val);
hashAdd(h, ss, cloneString(val));
return TRUE;
}


boolean readHttpStatus(struct lineFile *lf, struct hash *h)
// Read HTTP status line from lineFile
// Value is saved in hash h keyed by STATUS
// Returns TRUE if a line was read from the file
// and FALSE otherwise
{
char *line;
int lineSize;
char status[1024];
if ( !lineFileNext(lf, &line, &lineSize) )
    {
    lineSize = read(lf->fd, status, sizeof(status));
    status[lineSize]=0;
    lineSize = read(lf->fd, status, sizeof(status));
    status[lineSize]=0;
    errnoAbort("failed read(fd=%d, size=%d, status=[%s] - what is the error ?\n", lf->fd, lineSize, status);
    MJP(2);verbose(2,"no line; lineSize=%d\n", lineSize);
    return FALSE;
    }
if (lineSize < 2)
    errAbort("Invalid responsed line [%d] [%s]\n", lineSize, line);
safencpy(status, sizeof(status), line, lineSize-2);
MJP(2);verbose(2,"lineSize=%d line=[%s]\n", lineSize, status);
if (!startsWith("HTTP/1.1 ", status))
    errAbort("Not HTTP/1.1 response [%s]\n", status);
else
    hashAdd(h, "STATUS", cloneString(status));
return TRUE;
}


boolean readHttpResponseHeaders(struct lineFile *lf, struct hash *h)
// Read HTTP response from lineFile
// Header values are saved in hash h keyed by header name
// Returns TRUE if if the header was terminated by a blank line
// and FALSE if header terminated with end of file
{
boolean ok;
char *line;
int lineSize;
if (lf->zTerm)
    errAbort("zTerm must be FALSE\n");
if ( !(ok = readHttpStatus(lf, h)) )
    errAbort("Could not find status line\n");
while ( (ok = lineFileNext(lf, &line, &lineSize)))
    {
    if (lineSize == 2 && line[0] == '\r' && line[1] == '\n')
	break; // found terminating line
    else if (lineSize<2)
	errAbort("Short line (%d) in headers\n", lineSize);
    if (!hashAddHeader(h, line, lineSize-2))
	errAbort("Found line which does not look like header (size=%d) [%s]\n", lineSize, line);
    }
return ok;
}


void progressMeter(int elapsed, long soFar, long total)
{
int divisor = (soFar >= 1024*1024 ? 1024*1024 : (soFar >= 1024 ? 1024 : 1));
elapsed = max(elapsed,1);
char *units = (divisor == 1 ? "bytes" : (divisor == 1024 ? "Kbytes" : (divisor == 1024*1024 ? "Mbytes" : "")));
if (total)
    printf("%9ld %s/s %12ld %s %3ld%% (%d secs, %ld remaining)          \r", soFar/divisor/elapsed, units, 
	soFar/divisor, units, soFar*100/total, elapsed, (total-soFar)*elapsed/soFar);
else
    printf("%9ld %s/s %12ld %s (%d secs)          \r", soFar/divisor/elapsed, units, 
	soFar/divisor, units, elapsed);
}


/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files.
   Code from: http://www.zlib.net/zlib_how.html

   Data is sent in 'chunked' format as we dont want to have to compress
   it all in advance.
 */
// void netWriteDeflatedFileChunked(int sd, char *inFile)
// {
// int ret, flush;
// unsigned have;
// ssize_t sent;
// z_stream strm;
// unsigned char in[CHUNK];
// unsigned char out[CHUNK];
// int chunkSizeLen;
// int i;
// char chunkSize[1024];
// /* allocate deflate state */
// strm.zalloc = Z_NULL;
// strm.zfree = Z_NULL;
// strm.opaque = Z_NULL;
// ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
// if (ret != Z_OK)
//     errAbort("Could not initialze deflator (err=%d)\n", ret);
// /* compress until end of file */
// do 
//     {
//     strm.avail_in = fread(in, 1, CHUNK, inFile);
//     if (ferror(inFile)) 
// 	{
// 	(void)deflateEnd(&strm);
// 	errnoAbort("Could not read %d bytes from input\n", CHUNK);
// 	}
//     flush = feof(inFile) ? Z_FINISH : Z_NO_FLUSH;
//     strm.next_in = in;
//     /* run deflate() on input until output buffer not full, finish
// 	compression if all of source has been read in */
//     do 
// 	{
// 	strm.avail_out = CHUNK;
// 	strm.next_out = out;
// 	ret = deflate(&strm, flush);    /* no bad return value */
// 	assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
// 	have = CHUNK - strm.avail_out;
// 	//if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
// 	chunkSizeLen = safef(chunkSize, sizeof(chunkSize), "%x\r\n", have);
// 	fprintf(stderr,"> chunk=[%d/%x]\n> ", have, have);
// 	for (i=0; i<have ; ++i)
// 	    fprintf(stderr,"%c_%2x ", isprint(out[i]) ? out[i] : '_', out[i]);
// 	fprintf(stderr,"\n");
// 	if ( (sent = write(sd, chunkSize, chunkSizeLen)) != chunkSizeLen) 
// 	    {
// 	    if (sent == -1)
// 		errnoAbort("Error while writing %d bytes of chunk header\n", chunkSizeLen);
// 	    else
// 		errAbort("Could not write %d bytes of chunk header (wrote=%d)\n", chunkSizeLen, (int)sent);
// 	    }
// 	if ( (sent = write(sd, out, have)) != have) 
// 	    {
// 	    (void)deflateEnd(&strm);
// 	    if (sent == -1)
// 		errnoAbort("Error while writing %d bytes\n", have);
// 	    else
// 		errAbort("Could not write %d bytes (wrote=%d)\n", have, (int)sent);
// 	    }
// 	} while (strm.avail_out == 0);
//     assert(strm.avail_in == 0);     /* all input will be used */
//     /* done when last data in file processed */
//     } while (flush != Z_FINISH);
// assert(ret == Z_STREAM_END);        /* stream will be complete */
// // Write zero byte for end of chunked encoding
// if ( (sent = write(sd, "0\r\n", 3)) != 3) 
//     {
//     if (sent == -1)
// 	errnoAbort("Error while writing %d bytes of chunk footer\n", 3);
//     else
// 	errAbort("Could not write %d bytes of chunk footer (wrote=%d)\n", 3, (int)sent);
//     }
// /* clean up and return */
// (void)deflateEnd(&strm);
// }


void netWriteGzFileChunked(int sd, char *inFile, long inFileSize, void (*progress)(int elapsed, long soFar, long total))
/* Write a file which is already gzipped 
 * Need to skip over any extra gzip header fields
 * and write the simple 10-byte header with FLAGS=0 that apache understands 
 * http://www.faqs.org/rfcs/rfc1952.html
 * RFC1952 - GZIP file format specification version 4.3
 */
{
FILE *f;
int res;
unsigned char buf[CHUNK+3]; // space for CRLF \0
char szBuf[1024];  // hex size + CRLF
long totSent = 10; // header
time_t start = time(NULL);

// open the file (and discard the gzip header)
f = mustOpen(inFile, "rb");
// write 15 bytes containing chunk size 'a' (10) & basic 10-byte header & 2 CRLFs
safef(szBuf, sizeof(szBuf), "%x\r\n%c%c%c%c%c%c%c%c%c%c\r\n", 10, gz_magic[0], gz_magic[1],
	    Z_DEFLATED, 0 /*flags*/, 0,0,0,0 /*time*/, 0 /*xflags*/, 
	    0x03 /* default OS_CODE to 'UNIX' */);
mustWriteFd(sd, szBuf, 15);
// write the rest of the compressed file chunk by chunk
// leave space in buf for the <CR><LF> chunk-data terminator
//while ( (res = gzread (f, buf, CHUNK)) > 0)
unsigned char *msg = NULL;
while ( (res = fread(buf, 1, CHUNK, f)) > 0)
    {
    if (ferror(f))
	errnoAbort("error reading %s\n", inFile);
    if (!msg)
	{
	MJP(2);verbose(2,"HEADER=[ID=%x.%x (GZIP is 1f8b)][CM=%x (8=deflate)][FLAGS=%x (0=no extra hdrs)][Mtime=%x.%x.%x.%x][XFL=%x][OS=%x (3=Unix)]\n", buf[0], buf[1], buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9]);
	// skip header plus any extra headers determined by FLAGS field
	msg = buf+10;
	if (buf[3] != 0)
	    {
	    if (buf[3] & 4) // FEXTRA bit 2
		{
		MJP(2);verbose(2,"bit2 skipping 2 + 0x%x + 256*0x%x = %d bytes\n", msg[0], msg[1], 2 + msg[0] + 256*msg[1] );
		msg += 2 + msg[0] + 256*msg[1];
		}
	    if (buf[3] & 8) // FNAME bit 3
		{
		unsigned char *s = msg;
		while (*(msg++))
		    ;
		MJP(2);verbose(2,"bit3 skipping %ld bytes\n", msg-s);
		}
	    if (buf[3] & 16) // FCOMMENT bit 4
		{
		unsigned char *s = msg;
		while (*(msg++))
		    ;
		MJP(2);verbose(2,"bit4 skipping %ld bytes\n", msg-s);
		}
	    if (buf[3] & 2) // FHCRC bit 1
		{
		msg += 2;
		MJP(2);verbose(2,"bit1 skipping 2 bytes\n");
		}
	    }
	MJP(2);verbose(2,"skipped %ld bytes\n", msg-buf);
	res = res - (msg-buf);
	}
    // write chunk size
    safef(szBuf, sizeof(szBuf), "%x\r\n", res);
    mustWriteFd(sd, szBuf, strlen(szBuf));
    // append <CR><LF> to chunk-data and write
    msg[res]   = '\r';
    msg[res+1] = '\n';
    mustWriteFd(sd, msg, res+2);
    msg = buf;
    totSent += res;
/*unsigned char *x = msg+(res-8);
MJP(2);verbose(2,"print crc\n");
printf("bufsize=%d res=%d CRC=[%x.%x.%x.%x] ISIZE=[%x.%x.%x.%x]\n", CHUNK, res, x[0], x[1], x[2],x[3],x[4],x[5],x[6],x[7]);*/
    if (progress)
	(*progress)(time(NULL) - start, totSent, inFileSize);
    }
mustWriteFd(sd, "0\r\n\r\n", 5);
if (res != 0)
    errnoAbort("Read error in file [%s]\n", inFile);
carefulClose(&f);
if (progress)
    printf("\n");
}


void netWriteFileChunked(int sd, char *inFile, long inFileSize, void (*progress)(int elapsed, long soFar, long total))
/* Write a file directly to sd in chunked format */
{
FILE *f;
size_t res;
long totSent = 0;
time_t start = time(NULL);
unsigned char buf[CHUNK];
char tmpBuf[1024];
f = mustOpen(inFile, "rb");
/* compress until end of file */
// write the rest of the compressed file chunk by chunk
// leave space in buf for the <CR><LF> chunk-data terminator
do 
    {
    res = fread(buf, 1, CHUNK-3, f);
    if (ferror(f))
	errnoAbort("Could not read %d bytes from input\n", CHUNK);
    if (res > 0)
	{
	// write chunk size
	safef(tmpBuf, sizeof(tmpBuf), "%x\r\n", (unsigned) res);
	mustWriteFd(sd, tmpBuf, strlen(tmpBuf));
	// append <CR><LF> to chunk-data and write
	buf[res]   = '\r';
	buf[res+1] = '\n';
	buf[res+2] = '0';
	MJP(2);verbose(2,"chunk mustWriteFd(sd=%d, buf=%s, %d)\n", sd, buf, (int)res+2);
	mustWriteFd(sd, buf, res+2);
	totSent += res;
	if (progress)
	    (*progress)(time(NULL) - start, totSent, inFileSize);
	}
    } while (!feof(f));
// write the final zero chunk and terminator bytes
MJP(2);verbose(2,"zero mustWriteFd(sd=%d, 0rn, %d)\n", sd, 5);
mustWriteFd(sd, "0\r\n\r\n", 5);
MJP(2);verbose(2,"close(f)\n");
if (progress)
    printf("\n");
carefulClose(&f);
}


void netHttpSendHeaders(int sd, char *method, boolean sendExpectContinue, long contentSize, 
		struct netParsedUrl *npu, boolean keepAlive, char *extraHeaders)
/* Send a request, possibly with Keep-Alive. 
   If the transfer content size is unknown then pass -1 for chunked Transfer-Encoding */
{
struct dyString *dy = newDyString(1024);
ssize_t sent;
/* Ask remote server for the file/query. */
dyStringPrintf(dy, "%s %s HTTP/1.1\r\n", method, npu->file);
dyStringPrintf(dy, "Host: %s:%s\r\n", npu->host, npu->port);
dyStringPrintf(dy, "User-Agent: genome.ucsc.edu/net.c\r\n");
if (!sameString(npu->user,""))
    {
    char up[256];
    char *b64up = NULL;
    safef(up,sizeof(up), "%s:%s", npu->user, npu->password);
    b64up = base64Encode(up, strlen(up));
    dyStringPrintf(dy, "Authorization: Basic %s\r\n", b64up);
    freez(&b64up);
    }
dyStringAppend(dy, "Accept: */*\r\n");
if (keepAlive)
    {
    dyStringAppend(dy, "Connection: Keep-Alive\r\n");
    dyStringAppend(dy, "Connection: Persist\r\n");
    }
else
    dyStringAppend(dy, "Connection: close\r\n");
if (extraHeaders)
    dyStringAppend(dy, extraHeaders);
if (sendExpectContinue)
    {
    dyStringAppend(dy, "Expect: 100-continue\r\n");
    }
else
    {
    if (contentSize >= 0)
	dyStringPrintf(dy, "Content-length: %ld\r\n", contentSize);
    else
	dyStringAppend(dy, "Transfer-Encoding: chunked\r\n"); // we dont know the size ein advance
    }
dyStringAppend(dy, "\r\n");
// write headers
if ( (sent = write(sd, dy->string, dy->stringSize)) == -1)
    errnoAbort("Could not write header\n");
else
    {
    MJP(2);verbose(2,"Send %d bytes of headers: \n[%s]\n", dy->stringSize, dy->string);
    }
if (sent != dy->stringSize)
    errAbort("Could not write full %d header (wrote %ld bytes)\n", dy->stringSize, sent);
/* Clean up. */
dyStringFree(&dy);
} /* netHttpGet */


char *encodeTrackName(char *project, char *lab, char *datatype, char *view, int rep, char *variables)
// Build encode track name
// Uppercase the first character of lab, view and variables
// If rep is not 0, add 'RepN'
// Variables are a comma-delimited list; uppercase the first character of each and append to name
{
char *track;
struct dyString *t = newDyString(65);
dyStringAppend(t, project);
strUcFirst(lab);
dyStringAppend(t, lab);
dyStringAppend(t, datatype);
strUcFirst(view);
dyStringAppend(t, view);
if (rep > 0)
    dyStringPrintf(t, "Rep%d", rep);
struct slName *var, *vars = slNameListFromString(variables, ',');
for (var = vars ; var ; var = var->next)
    {
    strUcFirst(var->name);
    dyStringAppend(t, var->name);
    }
track = dyStringCannibalize(&t);
slFreeList(&vars);
if (strlen(track) > MAX_TRACK_NAME)
    errAbort("Track name %s too long (got %d, max is %d)\n", track, (int)strlen(track), MAX_TRACK_NAME);
return track;
}


void buildUrl(char *buf, int bufSize, char *project, char *pi, char *lab, char *datatype, char *view, 
    char *variables, int rep, char *inputfile, char *type, char *genome, char *track)
{
safef(buf, bufSize, "/g/project/%s/tracks/%s/%s?type=%s&pi=%s&lab=%s&datatype=%s&variables=%s&view=%s&rep=%d&sourceFile=%s",
    project, genome, track, type, pi, lab, datatype, variables, view, rep, inputfile);
}


long mustReadHex(char *s)
// Read a hex number into a long int
// Do error checking as per strtol man page
{
char *endptr;
errno = 0;
long val = strtol(s, &endptr, 16);
if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0))
    errnoAbort("Error converting [%s] to number\n", s);
if (endptr == s) 
    errAbort("No digits were found in [%s] (val=%ld)\n", s, val);
return val;
}


char *readChunked(struct lineFile *lf)
// Read chunked response
{
char *line;
int lineSize;
struct dyString *data = newDyString(100);
for (;;)
    {
    long totalBytes = 0;
    if (!lineFileNext(lf, &line, &lineSize))
	errAbort("No chunk line\n");
    long bytes = mustReadHex(line);
    struct dyString *chunk = newDyString(100);
    while (totalBytes < bytes)
	{
	if (!lineFileNext(lf, &line, &lineSize))
	    errAbort("No data line\n");
	totalBytes += lineSize;
	dyStringAppendN(chunk, line, lineSize);
	}
    if (!lineFileNext(lf, &line, &lineSize) || lineSize != 2)
	errAbort("No blank after chunk (size=%d, %s)\n", lineSize, line);
    dyStringAppendN(data, chunk->string, totalBytes);
    freeDyString(&chunk);
    if (bytes == 0)
	break;
    }
return dyStringCannibalize(&data);
}


char *readContentLength(struct lineFile *lf, int length)
// Read body based on content-length field
{
char *line;
int lineSize;
int totalBytes = 0;
struct dyString *data = newDyString(100);
while (totalBytes < length)
    {
    if (!lineFileNext(lf, &line, &lineSize))
	errAbort("No data line\n");
    totalBytes += lineSize;
    dyStringAppendN(data, line, lineSize);
    }
return dyStringCannibalize(&data);
}


boolean check100Continue(struct lineFile *lf)
{
char *msg = "";
struct hashEl *el;
struct hash *hHeader = newHash(0);
boolean ok = readHttpResponseHeaders(lf, hHeader);
if ( !strstrNoCase( (char *)hashMustFindVal(hHeader, "STATUS"), "100 Continue") )
    {
    if ( (el = hashLookupUpperCase(hHeader, "TRANSFER-ENCODING")) && sameString((char *)el->val, "chunked") )
	msg = readChunked(lf);
    else if ( (el = hashLookupUpperCase(hHeader, "Content-Length")) )
	msg = readContentLength(lf, sqlUnsigned((char *)el->val));
    errAbort("Error: %s\n%s\n", (char *)hashFindVal(hHeader, "STATUS"), msg);
    }
freeHashAndVals(&hHeader);
return ok;
}


int main(int argc, char *argv[])
{
struct netParsedUrl *npu = NULL;
int sd;
boolean isGz = FALSE, sendUncompressed = FALSE;
char *method, *track, *project, *pi, *lab, *datatype, *variables, 
    *view, *inputfile, *type, *genome;
int rep;
long inputSize;
struct lineFile *lf;
struct hash *hHeader = hashNew(0);

optionInit(&argc, argv, options);
--argc; 
++argv;
if (argc < 9 || argc > 10)
    usage();
MJP(2);verbose(2,"started\n");
project   = argv[0];
pi        = argv[1];
lab       = argv[2];
datatype  = argv[3];
variables = argv[4];
view      = argv[5];
inputfile = argv[6];
type      = argv[7];
genome    = argv[8];
    MJP(2);verbose(2,"argc=%d\n", argc);
if (argc == 10)
    {
    MJP(2);verbose(2,"netParseUrl=[%s]\n", argv[9]);
    AllocVar(npu);
    netParseUrl(argv[9], npu);
    }
rep = optionInt("rep", 0);
method = optionExists("append") ? "POST" : "PUT";
sendUncompressed = optionExists("sendUncompressed");

MJP(2);verbose(2,"method=[%s] file=[%s] host=[%s]\n", method, inputfile, npu ? npu->host : "none");
MJP(2);verbose(2,"project=[%s] pi=[%s] lab=[%s] datatype=[%s] variables=[%s] view=[%s] inputfile=[%s] type=[%s] genome=[%s]\n", 
    project, pi, lab, datatype, variables, view, inputfile, type, genome);
track = encodeTrackName(project, lab, datatype, view, rep, variables);

if ( (inputSize = fileSize(inputfile)) == -1)
    errAbort("File [%s] does not exist\n", inputfile);
if (endsWith(inputfile, "gz"))
    isGz = TRUE;
if (isGz && sendUncompressed)
    errAbort("Option -sendUncompressed does not work with compressed 'gz' input files\n");

///////////////
// VALIDATE FILE HERE
///////////////
if (!npu)
    exit(0);

///////////////
// Send the file
buildUrl(npu->file, sizeof(npu->file), project, pi, lab, datatype, view, variables, rep, 
    inputfile, type, genome, track);
MJP(2);verbose(2,"proto=[%s] user=[%s] pass=[%s] host=[%s] port=[%s] file=[%s]\n", npu->protocol,
    npu->user, npu->password, npu->host, npu->port, npu->file);
if (!sameString(npu->protocol, "http"))
    errAbort("Sorry, can only netOpen http's currently");
if (isGz)
    {
    sd = netMustConnect(npu->host, atoi(npu->port));
    MJP(2);verbose(2,"done netConnect(%s,%s) -> [%d]\n",npu->host, npu->port, sd );
    lf = lineFileAttach(npu->file, FALSE, sd);
    MJP(2);verbose(2,"send headers (gzip)\n");
    netHttpSendHeaders(sd, method, TRUE, 0, npu, FALSE, "Content-Type: text/plain\r\n");
    check100Continue(lf);
    printf("Sending %ldkb file %s\n", inputSize/1024, inputfile);
    lineFileClose(&lf);
    // full request can proceed
    sd = netMustConnect(npu->host, atoi(npu->port));
    lf = lineFileAttach(npu->file, FALSE, sd);
    netHttpSendHeaders(sd, method, FALSE, -1, npu, FALSE, "Content-Encoding: gzip\r\nContent-Type: text/plain\r\n");
    MJP(2);verbose(2,"netWriteGzFileChunked(%s)\n", inputfile);
    netWriteGzFileChunked(sd, inputfile, inputSize, progressMeter);
    }
else
    {
    if (sendUncompressed)
	{
	sd = netMustConnect(npu->host, atoi(npu->port));
	lf = lineFileAttach(npu->file, FALSE, sd);
	netHttpSendHeaders(sd, method, TRUE, -1, npu, FALSE, "Content-Type: text/plain\r\n");
	check100Continue(lf);
	printf("Sending %ldkb file %s\n", inputSize/1024, inputfile);
	lineFileClose(&lf);
	// full request can proceed
	sd = netMustConnect(npu->host, atoi(npu->port));
	lf = lineFileAttach(npu->file, FALSE, sd);
	netHttpSendHeaders(sd, method, FALSE, -1, npu, FALSE, "Content-Type: text/plain\r\n");
	netWriteFileChunked(sd, inputfile, inputSize, progressMeter);
	}
    else
	{
	sd = netMustConnect(npu->host, atoi(npu->port));
	lf = lineFileAttach(npu->file, FALSE, sd);
	MJP(2);verbose(2,"send headers (send compressed)\n");
	netHttpSendHeaders(sd, method, TRUE, -1, npu, FALSE, "Content-Type: text/plain\r\n");
	check100Continue(lf);
	printf("Sending %ldkb file %s\n", inputSize/1024, inputfile);
	lineFileClose(&lf);
	// full request can proceed
	sd = netMustConnect(npu->host, atoi(npu->port));
	lf = lineFileAttach(npu->file, FALSE, sd);
	MJP(2);verbose(2,"netWriteDeflatedFileChunked(%s)\n", inputfile);
	netHttpSendHeaders(sd, method, FALSE, -1, npu, FALSE, "Content-Type: text/plain\r\n");
	//netWriteDeflatedFileChunked(sd, inputfile, inputSize, progressMeter);
	}
    }
// get the headers
MJP(2);verbose(2,"freeHashAndVals(&hHeader)\n");
freeHashAndVals(&hHeader);
hHeader = newHash(0);
MJP(2);verbose(2,"readHttpResponseHeaders(lf, hHeader)\n");
if (!readHttpResponseHeaders(lf, hHeader))
    errnoAbort("no blank line after headers\n");
MJP(2);verbose(2,"hashLookupUpperCase(hHeader, Content-Type)\n");
struct hashEl *el = hashLookupUpperCase(hHeader, "Content-Type");
if ( !el || !(sameOk(el->val,"text/plain") || sameOk(el->val,"application/json")
      || startsWith("text/html", el->val) ) )
    errAbort("Unexpected Content-Type [%s] inside request body\n", (char *)el->val);
MJP(2);verbose(2,"hashLookupUpperCase(hHeader, Transfer-Encoding)\n");
char *msg = "";
if ( (el = hashLookupUpperCase(hHeader, "Transfer-Encoding")) && sameString((char *)el->val, "chunked") )
    msg = readChunked(lf);
else if ( (el = hashLookupUpperCase(hHeader, "Content-Length")) )
    msg = readContentLength(lf, sqlUnsigned((char *)el->val));
printf("Result: %s\n%s\n", (char *)hashFindVal(hHeader, "STATUS"), msg);
MJP(2);verbose(2,"Done.\n");
freeHashAndVals(&hHeader);
close(sd);
exit(0);
}

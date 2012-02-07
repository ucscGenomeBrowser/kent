/* Routines for processing MIME from a descriptor.
 *   For cgi post, the MIME descriptor is stdin.
 *   We want to parse it as it comes in so that 
 *   we can handle very large files if needed.
 *   Large data are saved to tempfiles.
 *   Small data stays as ptr+size in memory.
 *
 * This file is copyright 2005 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "portable.h"
#include "errabort.h"
#include "mime.h"

/* 
 * Note: MIME is a nested structure that makes a tree that streams in depth-first.
 */

#define MAXPARTSIZE 64LL*1024*1024*1024  /* max size before gets put in a tempfile to save memory. It currently has been set so large that it will not be triggered. */
#define MAXPARTLINESIZE 1024 /* header lines should be small, so bad if bigger than this */
#define MAXDATASIZE 64LL*1024*1024*1024 /* max size allowable for large uploads */
#define MAXBOUNDARY 72+5     /* max size of buffer for boundary 72+--""0 */

enum nlType nlType = nlt_undet;

static void setEopMB(struct mimeBuf *b)
/* do a search for boundary, set eop End Of Part if found */
{
if (b->blen > 0)
    b->eop = memMatch(b->boundary, b->blen, b->i, b->eoi - b->i);
else
    b->eop = NULL;
}

static void setEodMB(struct mimeBuf *b)
/* set end of data - eoi minus (boundary-size -1) */
{
if (b->blen > 1 && b->eoi == b->eom) 
    {
    b->eod = b->eoi - (b->blen-1);
    }
else
    {
    b->eod = b->eoi;
    }
}

static void setBoundaryMB(struct mimeBuf *b, char *boundary)
/* set boundary in b */
{
b->boundary = boundary;
b->blen = boundary ? strlen(b->boundary) : 0;
setEopMB(b);
setEodMB(b);
}

#ifdef DEBUG
static void dumpMB(struct mimeBuf *b)
/* debug dump */
{
int i=0;

fprintf(stderr,"b->i  =%lu "
      "b->eop=%lu "
      "b->eod=%lu "
      "b->eoi=%lu "
      "b->eom=%lu "
      "%s "
      "%d "
      "\n", 
    (unsigned long) b->i,
    (unsigned long) b->eop,
    (unsigned long) b->eod,
    (unsigned long) b->eoi,
    (unsigned long) b->eom,
    b->boundary,
    b->blen
    );
fprintf(stderr,"*");    
for(i=0;i<MIMEBUFSIZE;++i)
    {
    fprintf(stderr,"%c", (b->buf[i] < 31 || (unsigned) b->buf[i] > 127) ? '.' : b->buf[i] );
    }
fprintf(stderr,"\n\n");    
}
#endif

static void moreMimeBuf(struct mimeBuf *b)
{
int bytesRead = 0, bytesToRead = 0;
if (b->blen > 1)
    {
    int r = b->eoi - b->i;
    memmove(b->buf, b->i, r);
    b->eoi = b->buf+r;
    }
else
    {
    b->eoi = b->buf;
    }
b->i = b->buf+0;
bytesToRead = b->eom - b->eoi;
while (bytesToRead > 0)
    {
    bytesRead = read(b->d, b->eoi, bytesToRead);
    if (bytesRead < 0)
        errnoAbort("moreMimeBuf: error reading MIME input descriptor");
    b->eoi += bytesRead;
    if (bytesRead == 0)
        break;
    bytesToRead = bytesToRead - bytesRead;
    }
setEopMB(b);
setEodMB(b);
//debug
//fprintf(stderr,"post-moreMime dumpMB: ");
//dumpMB(b);  //debug
}

static char getcMB(struct mimeBuf *b)
/* read just one char from MIME buffer */
{
if (b->i >= b->eoi && b->eoi < b->eom)  /* at end of input */
    errAbort("getcMB error - requested input beyond end of MIME input.");
if (b->i >= b->eod && b->eoi == b->eom) /* at end of buffer */
    moreMimeBuf(b);
    
//fprintf(stderr,"b->buf:%lu b->i:%lu %c \n",
//    (unsigned long) b->buf,
//    (unsigned long) b->i,
//    *b->i
//    );
//fprintf(stderr,"%c",*b->i);
//fflush(stderr); 
return *b->i++;    
}

static void putBackMB(struct mimeBuf *b)
/* Rewind just one char back in MIME buffer.
 * Do not use except for distinguishing line type initially */
{
if (b->i == b->buf)  /* at beginning of buffer */
    errAbort("putBackMB error - requested pushback beyond beginning buffer.");
b->i--;    
}

static char *getLineMB(struct mimeBuf *b)
/* Reads one line up to CRLF, returned string does not include CRLF however. 
   Use freeMem when done with string. */
{
char line[MAXPARTLINESIZE];
int i = 0;
char c = 0;
line[0]=0;
while(TRUE)
    {
    c =getcMB(b);
    if ((c == 0x0d) || (c == 0x0a))  /* CR or LF is end of line */
	break;
    line[i++] = c;
    if (i >= MAXPARTLINESIZE)
	errAbort("getLineMB error - MIME input header too long, "
		    "greater than %d chars",MAXPARTLINESIZE);
    }
line[i] = 0; /* terminate string */ 
if (nlType == nlt_undet)  /* determine newline type */
    {
    if (c == 0x0d)
	{
	nlType = nlt_mac;
	c = getcMB(b);
	if (c == 0x0a)
	    nlType = nlt_dos;
	else
    	    putBackMB(b);
	}
    else
	{
	nlType = nlt_unix;
	}
    }
else if (nlType == nlt_dos)
    {
    if (c == 0x0d)
    	getcMB(b); /* just waste the LF */
    else
	nlType = nlt_unix;
    }
return cloneString(line);
}


static void getChunkMB(struct mimeBuf *b, char **address, int *size, boolean *hasZeros)
/* Pass back address and size of chunk, and whether it contains embedded zeros.
   The chunk is the largest piece of data left in the buffer up to the eod or eop. */
{
char *eoc = b->eop ? b->eop : b->eod; /* end of chunk */
//debug
//fprintf(stderr,"pre-getChunkMB dumpMB: ");
//dumpMB(b);  //debug
*address=b->i;
*size=eoc - b->i;
*hasZeros = (memMatch("", 1,*address, *size) != NULL);
b->i = eoc;
}

static void readPartHeaderMB(struct mimeBuf *b, struct mimePart *p, char *altHeader)
/* Reads the header lines of the mimePart,
   saves the header settings in a hash.  */
{
struct dyString *fullLine = dyStringNew(0);
char *key=NULL, *val=NULL;
struct lineFile *lf = NULL;
char *line = NULL;
char *lineAhead = NULL;
int size = 0;
p->hdr = newHash(3);
	//debug
    	//fprintf(stderr,"headers dumpMB: ");
	//dumpMB(b);  //debug
if (altHeader)
    {
    lf = lineFileOnString("MIME Header", TRUE, altHeader);
    }
/* read ahead one line, skipping any leading blanks lines */   
do
    {
    if (altHeader)
	lineFileNext(lf, &lineAhead, &size);
    else
	lineAhead = getLineMB(b);
    } 
    while (sameString(lineAhead,""));

do
    {
    /* accumulate a full header line - some emailers split into mpl lines */
    dyStringClear(fullLine);
    do 
	{
	line = lineAhead;
	if (altHeader)
	    lineFileNext(lf, &lineAhead, &size);
	else
	    lineAhead = getLineMB(b);
	dyStringAppend(fullLine,line);    
	if (!altHeader) 
	    freez(&line);
	} while (isspace(lineAhead[0]));
    line = fullLine->string;
    //fprintf(stderr,"found a line! [%s]\n",line);  //debug
    key = line;
    val = strchr(line,':');
    if (!val)
	errAbort("readPartHeaderMB error - header-line colon not found, line=[%s]",line);
    *val = 0;
    val++;
    key=trimSpaces(key);
    // since the hash is case-sensitive, convert to lower case for ease of matching
    tolowers(key);  
    val=trimSpaces(val);
    hashAdd(p->hdr,key,cloneString(val));
    
    //debug
    //fprintf(stderr,"MIME header: key=[%s], val=[%s]\n",key,val);
    //fflush(stderr); 
    
    } while (!sameString(lineAhead,""));
if (altHeader)
    {
    if (nlType == nlt_undet)
	nlType = lf->nlType;
    lineFileClose(&lf);
    }
else
    {
    freez(&lineAhead);
    }
dyStringFree(&fullLine);
    
}


struct mimeBuf * initMimeBuf(int d)
/* d is a descriptor for a file or socket or some other descriptor 
   that the MIME input can be read from. 
   Initializes the mimeBuf structure. */
{
struct mimeBuf *b=AllocA(*b);
b->d = d;
b->boundary = NULL;
b->blen = 0;
b->eom = b->buf+MIMEBUFSIZE;
b->eoi = b->eom;
b->eod = b->eom;
b->i = b->eom;
moreMimeBuf(b);
return b;
}

char *getMimeHeaderMainVal(char *header)
/* Parse a typical mime header line returning the first
 * main value up to whitespace, punctuation, or end. 
 * freeMem the returned string when done */
{
char value[1024]; 
char *h = header;
int i = 0;
char *puncChars = ",;: \t\r\n"; /* punctuation chars */
i=0;
/* The header should have already been trimmed of leading and trailing spaces */
while(TRUE)
    {
    char c = *h++;
    if (c==0 || strchr(puncChars,c))
	break;
    value[i++] = c;
    if (i >= sizeof(value))
	errAbort("error: main value too long (>%lu) in MIME header Content-type:%s",(unsigned long)sizeof(value),header);
    }
value[i] = 0;    

return cloneString(value);

}


char *getMimeHeaderFieldVal(char *header, char *field)
/* Parse a typical mime header line looking for field=
 * and return the value which may be quoted.
 * freeMem the returned string when done */
{
char value[1024]; 
char *fld = header;
int i = 0;
char *puncChars = ",;: \t\r\n"; /* punctuation chars */
while (TRUE)
    {
    fld = strstr(fld,field);
    if (!fld)
	return NULL;
    if (fld > header && strchr(puncChars,fld[-1]))
	{
	fld+=strlen(field);
	if (*fld == '=')
	    {
	    ++fld;
	    break;
	    }
	}    
    else
	{
	++fld;
	}
    }	
if (*fld == '"')
    {
    puncChars = "\"";  /* quoted */
    ++fld;
    }
i=0;
while(TRUE)
    {
    char c = *fld++;
    if (c==0 || strchr(puncChars,c))
	break;
    value[i++] = c;
    if (i >= sizeof(value))
	errAbort("error: %s= value too long (>%lu) in MIME header Content-type:%s",field,(unsigned long)sizeof(value),header);
    }
value[i] = 0;    

return cloneString(value);

}

char *getNewLineByType()
/* just use global nlType setting */
{
switch (nlType)
    {
    case nlt_dos:
	//debug
    	//fprintf(stderr,"nlType=nlt_dos\n");
	return "\x0d\x0a";
    case nlt_mac:
	//debug
    	//fprintf(stderr,"nlType=nlt_mac\n");
	return "\x0d";
    case nlt_unix:
    default:
	//debug
    	//fprintf(stderr,"nlType=nlt_unix\n");
	return "\x0a";
    }
}

struct mimePart *parseMultiParts(struct mimeBuf *b, char *altHeader)
/* This is a recursive function.  It parses multipart MIME messages.
   Data that are binary or too large will be saved in mimePart->filename
   otherwise saved as a c-string in mimePart->data.  If multipart,
   then first child is mimePart->child, subsequent sibs are in child->next.
   altHeader is a string of headers that can be fed in if the headers have
   already been read off the stream by an earlier process, i.e. apache.
 */
{ 
struct mimePart *p=AllocA(*p);
char *parentboundary = NULL, *boundary = NULL;
char *ct = NULL;
boolean autoBoundary = FALSE;


//debug
//fprintf(stderr,"altHeader=[%s]\n",altHeader);

if (sameOk(altHeader, "autoBoundary"))
    { /* process things with no explicit header.
       *  look for *MIME* \n\n-- */
    struct dyString *dy = dyStringNew(0);
    char *prevPrevLine = NULL;
    char *prevLine = NULL;
    char *line = NULL;
    boolean found = FALSE;
    autoBoundary = TRUE;
    while (TRUE)
	{
	if (b->i >= b->eoi && b->eoi < b->eom)  /* at end of input */
	    break;
	line = getLineMB(b);
	if (line && startsWith("--",line) // && 
	    //sameString(prevLine,"") && 
	    //prevPrevLine &&
	    //stringIn("MULTI",prevPrevLine) && 
	    //stringIn("MIME",prevPrevLine) 
	    )
	    {
	    found = TRUE;
	    break;
	    }
	freez(&prevPrevLine);
	prevPrevLine = prevLine;
	prevLine = line;
	if (prevPrevLine)
	    touppers(prevPrevLine);
	}
    if (!found)
	errAbort("autoBoundary: No initial boundary found.");

    dyStringPrintf(dy, "CONTENT-TYPE:multipart/form-data; boundary=%s%s%s", 
	line+2, getNewLineByType(), getNewLineByType() );
    altHeader = dyStringCannibalize(&dy); 
    
    //debug
    //fprintf(stderr,"autoBoundary altHeader = [%s]\n",altHeader);
    //fflush(stderr); 

    freez(&prevPrevLine);	    
    freez(&prevLine);	    
    freez(&line);	    
    }

//debug
//fprintf(stderr,"\n");
readPartHeaderMB(b,p,altHeader);

ct = hashFindVal(p->hdr,"content-type");  /* use lowercase key */
//debug
//fprintf(stderr,"ct from hash:%s\n",ct);
//fflush(stderr); 

if (ct && startsWith("multipart/",ct))
    {
    char bound[MAXBOUNDARY]; 
    char *bnd = NULL;
    struct mimePart *child = NULL;

    /* these 3 vars just for processing epilog chunk: */
    char *bp=NULL;
    int size=0;
    boolean hasZeros=FALSE;

    /* save */
    parentboundary = b->boundary;

    boundary = getMimeHeaderFieldVal(ct,"boundary");
    if (strlen(boundary) >= MAXBOUNDARY)
	errAbort("error: boundary= value too long in MIME header Content-type:%s",ct);
    safef(bound, sizeof(bound), "--%s",boundary);  /* do not prepend CRLF to boundary yet */
    freez(&boundary);
    boundary = cloneString(bound);
    //debug
    //fprintf(stderr,"initial boundary parsed:%s\n",boundary);
    //fflush(stderr); 

    if (!autoBoundary)
	{
	/* skip any extra "prolog" before the initial boundary marker */
	while (TRUE)
	    {
	    bnd = getLineMB(b);
	    if (sameString(bnd,boundary)) 
	       break;
	    freez(&bnd);
	    }
	    //debug
	    //fprintf(stderr,"initial boundary found:%s\n",bnd);
	    //fflush(stderr); 
	freez(&bnd);
	}

    /* include crlf in the boundary so bodies won't have trailing a CRLF
     * this is done here so that in case there's no extra CRLF
     * between the header and the boundary, it will still work,
     * so we only prepend the CRLF to the boundary after initial found */
    safef(bound,sizeof(bound),"%s%s", getNewLineByType(), boundary);
    freez(&boundary);
    boundary=cloneString(bound);
    
    setBoundaryMB(b, boundary);

    while(TRUE)
	{
	int i = 0;
	char c1 = ' ', c2 = ' ';
    	child = parseMultiParts(b,NULL);
	slAddHead(&p->multi,child);
	//call getLine, compare to boundary 
	/* skip extra initial boundary marker - it's moot anyway */
	freez(&bnd);
	    //debug
	    //fprintf(stderr,"post-parse pre-getLineMB dumpMB: ");
	    //dumpMB(b);  //debug
	for (i=0;i<strlen(boundary);++i)
	    bound[i] = getcMB(b);
	bound[i] = 0;    
	if (!sameString(bound,boundary))
	    errAbort("expected boundary %s, but found %s in MIME",boundary,bound);
	//debug
    	//fprintf(stderr,"\nfound boundary:%s\n",bound);
	//fflush(stderr); 
    	c1 = getcMB(b);
	if (c1 == '-')
	    {
	    c2 = getcMB(b);
	    if (c2 == '-')
    		break;  /* last boundary found */
	    else		    
	    	errAbort("expected -- after boundary %s, but found %c%c in MIME",boundary,c1,c2);
	    }
	if (nlType == nlt_dos)
	    c2 = getcMB(b);
	switch (nlType)
	    {
	    case nlt_dos:
		if (c1 == 0x0d && c2 == 0x0a)
		    break;
		else		    
		    errAbort("expected CRLF after boundary %s, but found %c%c in MIME",boundary,c1,c2);
	    case nlt_unix:
		if (c1 == 0x0a)
		    break;
		else		    
		    errAbort("expected LF after boundary %s, but found %c in MIME",boundary,c1);
	    case nlt_mac:
		if (c1 == 0x0d)
		    break;
		else		    
		    errAbort("expected CR after boundary %s, but found %c in MIME",boundary,c1);
	    default:
		    errAbort("unexpected nlType %d after boundary %s",nlType,boundary);
	    }
	setEopMB(b);
	}	
    freez(&bnd);
    slReverse(&p->multi);
    /* restore */
    freez(&boundary);
    boundary = parentboundary;
	//debug
    	//fprintf(stderr,"restoring parent boundary = %s\n",boundary);
    setBoundaryMB(b, boundary);

    /* dump any "epilog" that may be between the 
     * end of the child boundary and the parent boundary */
    getChunkMB(b, &bp, &size, &hasZeros);
    //debug
    //fprintf(stderr,"epilog size=%d\n",size);
    	   
    
    }
else
    {
    char *bp=NULL;
    int size=0;
    boolean hasZeros=FALSE;
    boolean toobig=FALSE;
    boolean asFile=FALSE;
    boolean convert=FALSE;
    FILE *f = NULL;
    struct dyString *dy=newDyString(1024);
    //debug
    //fprintf(stderr,"starting new part (non-multi), dumpMB: \n");
    //dumpMB(b);  //debug
    
    //debug
    //ct = hashFindVal(p->hdr,"content-transfer-encoding");  /* use lowercase key */
    //fprintf(stderr,"cte from hash:%s\n",ct);
	
    while(TRUE)
	{
	// break if eop, eod, eoi
    	getChunkMB(b, &bp, &size, &hasZeros);
	//debug
    	//fprintf(stderr,"bp=%lu size=%d, hasZeros=%d \n", 
	//    (unsigned long) bp,
	//    size,
	//    hasZeros);
	if (hasZeros)
	    {
	    p->binary=TRUE;
	    }
	//if (hasZeros && !asFile)
	//    {
	//    convert=TRUE;
	//    }
	if (!asFile && p->size+size > MAXPARTSIZE)
	    {
	    toobig = TRUE;
	    convert=TRUE;
	    }
	if (convert)
	    {
	    struct tempName uploadedData;
	    convert=FALSE;
	    asFile = TRUE;
	    makeTempName(&uploadedData, "hgSs", ".cgi");
	    p->fileName=cloneString(uploadedData.forCgi);
	    f = mustOpen(p->fileName,"w");
	    mustWrite(f,dy->string,dy->stringSize);
	    freeDyString(&dy);
	    }
	if (asFile)
	    {
	    mustWrite(f,bp,size);
	    }
	else
	    {
    	    dyStringAppendN(dy,bp,size);
	    }
	p->size+=size;
	if (p->size > MAXDATASIZE)
	    errAbort("max data size allowable for upload in MIME exceeded %llu",(unsigned long long)MAXDATASIZE);
	    
	
	if (b->eop && b->i == b->eop)  /* end of part */
	    {
	    break;
	    }
	if (b->i == b->eoi && b->eoi < b->eom) /* end of data */
	    {
	    break;
	    }
	moreMimeBuf(b);
	}
    if (dy)
	{
	p->data=needLargeMem(dy->stringSize+1);
	memcpy(p->data,dy->string,dy->stringSize);
	p->data[dy->stringSize] = 0;
    	freeDyString(&dy);
	}
    if (f)
	carefulClose(&f);

    //debug
    //fprintf(stderr,"p->fileName=%s p->data=[%s]\n",p->fileName,p->data);

    }

return p;
}



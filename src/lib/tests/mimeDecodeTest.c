/* mimeDecodeTest.c for parsing MIME from a descriptor (stdin).
 *
 * This file is copyright 2006 Galt Barber, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "options.h"
#include "dystring.h"
#include "errAbort.h"
#include "hash.h"
#include "cheapcgi.h"
#include "mime.h"
#include "base64.h"
#include "quotedP.h"

/* 
 * Note: MIME is a nested structure that makes a tree that streams in depth-first.
 */

int noNameCount = 0;
char noName[256];
char outPath[256];

char *altHeader = NULL;
boolean cidPass = FALSE;
struct hash *cidHash = NULL;
struct dyString *dy = NULL;
char *outDir = ".";

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n\n"
    "mimeDecode - parse and decode mime input on stdin\n"
    "\n"
    "Usage:\n"
    "   mimeDecode [options] outDir\n"
    "where outDir is the output directory.\n"
    "Options:\n"
    "  -altHeader='CONTENT_TYPE=\"multipart/form-data, boundary=AaB03x\"'\n"
    "    be sure to specify the same boundary pattern that is used \n"
    "    in the stdin input.\n"
    "  -autoBoundary - no boundary given, scan for --.\n"
    "  -cid - process html replacing cid urls with filenames.\n"
    "  -noNames - ignore given filenames, use noNames for all.\n"
    "  --help - this help screen\n",
    msg);
}

static struct optionSpec options[] =
{
    {"altHeader", OPTION_STRING},
    {"autoBoundary", OPTION_BOOLEAN},
    {"cid", OPTION_BOOLEAN},
    {"noNames", OPTION_BOOLEAN},
    {"-help", OPTION_BOOLEAN},
    {NULL, 0},
};


static void handleCID(char **html, char *ctMain)
/* Handle CID replacements if needed */
{
if (optionExists("cid") && ctMain && sameWord(ctMain,"text/html"))
    {
    struct hashEl *el, *list = hashElListHash(cidHash);
    for(el=list;el;el=el->next)
	{
	char *cid=addSuffix("cid:",el->name);
	if (stringIn(cid,*html))
	    {
	    char *new = replaceChars(*html, cid, el->val);
	    freez(html);
	    *html = new;
	    }
	freez(&cid);
	// support for content-location
	if (stringIn(el->name,*html))
	    {
	    char *new = replaceChars(*html, el->name, el->val);
	    freez(html);
	    *html = new;
	    }
	}
    hashElFreeList(&list);
    }
}
		

void printMimeInfo(struct mimePart *mp, FILE *out, int level)
/* print mimeParts info and decode and detach recursively if needed */
{

char *cd = NULL, *cdMain = NULL, *cdName = NULL, *cdFileName = NULL, 
 *ct = NULL, *ctMain = NULL, *ctCharset = NULL, *ctName = NULL,
 *ce = NULL, *ceMain = NULL, *cid = NULL, *cidMain = NULL, *cl = NULL, *clMain = NULL;
char *margin = needMem(level+1);
int i = 0;
for(i=0;i<level;++i)
    margin[i] = ' ';
margin[level] = 0;    

cd = hashFindVal(mp->hdr,"content-disposition");
ct = hashFindVal(mp->hdr,"content-type");
ce = hashFindVal(mp->hdr,"content-transfer-encoding");
cid = hashFindVal(mp->hdr,"content-id");
cl = hashFindVal(mp->hdr,"content-location");

if (cd)
    {
    dyStringPrintf(dy,"%scontent-disposition: %s\n",margin,cd);
    cdMain=getMimeHeaderMainVal(cd);
    cdName=getMimeHeaderFieldVal(cd,"name");
    dyStringPrintf(dy,"%smain:[%s]\n",margin,cdMain);
    dyStringPrintf(dy,"%sname:[%s]\n",margin,cdName);
    cdFileName=getMimeHeaderFieldVal(cd,"filename");
    if (cdFileName)
    	dyStringPrintf(dy,"%sfilename:[%s]\n",margin,cdFileName);
    }
if (ct)
    {
    dyStringPrintf(dy,"%scontent-type: %s\n",margin,ct);
    ctMain=getMimeHeaderMainVal(ct);
    ctCharset=getMimeHeaderFieldVal(ct,"charset");
    ctName=getMimeHeaderFieldVal(ct,"name");
    }
if (ce)
    {
    dyStringPrintf(dy,"%scontent-transer-encoding: %s\n",margin,ce);
    ceMain=getMimeHeaderMainVal(ce);
    }
if (cid)
    {
    dyStringPrintf(dy,"%scontent-id: %s\n",margin,cid);
    if (cid[0]=='<' && lastChar(cid)=='>')
	cidMain = cloneStringZ(cid+1,strlen(cid)-2);
    else    
	cidMain = cloneString(cid);
    }
if (cl)
    {  // this is a url and needs decoding
    cgiDecode(cl, cl, strlen(cl));
    dyStringPrintf(dy,"%scontent-location: %s\n",margin,cl);
    clMain=getMimeHeaderMainVal(cl);
    }

if (cd)
    {
    dyStringPrintf(dy,"%ssize:[%llu]\n",margin,(unsigned long long) mp->size);
    if (mp->binary)
    	dyStringPrintf(dy,"%sbinary (contains zeros)\n",margin);
    if (mp->fileName)
	dyStringPrintf(dy,"%sfileName=[%s]\n",margin, mp->fileName);
    if (!cidPass)
    	dyStringPrintf(dy,"%sdata:[%s]\n",margin,
	    mp->binary && mp->data ? "<binary data not safe to print>" : mp->data);
    dyStringPrintf(dy,"\n");
    }


if (mp->data) /* typical case in ram */
    {
    char *outName = NULL;
    char *ext = "";
    if (ctMain)
	{
	if (sameWord(ctMain,"text/plain"))
	    ext = ".txt";
	if (sameWord(ctMain,"text/html"))
	    ext = ".html";
	if (sameWord(ctMain,"image/jpeg"))
	    ext = ".jpg";
	}
    safef(noName, sizeof(noName), "noName%d%s", noNameCount, ext);
    if (cdFileName) 
	outName = cdFileName;
    else if (ctName) 
	outName = ctName;
    if (optionExists("noNames") || outName==NULL)
	{
	outName = noName;
	++noNameCount;
	}
    if (cidPass)
	{
	if (cidMain)
    	    hashAdd(cidHash,cidMain,cloneString(outName));
	if (clMain)
    	    hashAdd(cidHash,clMain,cloneString(outName));
	}
    else
	{
	FILE *f = NULL;
	safef(outPath, sizeof(outPath), "%s/%s", outDir, outName);
	f = mustOpen(outPath,"w");
	if (ceMain && sameWord(ceMain,"base64"))
	    {
	    size_t newSize = 0;
	    char *decoded = NULL;
	    boolean base64Ok = base64Validate(mp->data); /* also removes whitespace */
	    if (!base64Ok)
		dyStringPrintf(dy,"%swarning: base64Validate failed on mp->data\n",margin);
	    decoded = base64Decode((char *)mp->data, &newSize);
	    mustWrite(f, decoded, newSize);
	    freez(&decoded);
	    }
	else if (ceMain && sameWord(ceMain,"quoted-printable"))
	    {
	    char *decoded = quotedPrintableDecode((char *)mp->data);
	    handleCID(&decoded, ctMain);
	    mustWrite(f, decoded, strlen(decoded));
	    freez(&decoded);
	    }
	else
	    {
	    handleCID(&mp->data, ctMain);
	    mustWrite(f, mp->data, mp->size);
	    }
	carefulClose(&f);
	}
    	
    }	    
else if (mp->fileName)  /* huge file triggers tempfile */
    {
    }
else if (mp->multi)
    {
    dyStringPrintf(dy,"%snested MIME structure\n\n",margin);
    for(mp=mp->multi;mp;mp=mp->next)
	printMimeInfo(mp, out, level+1);
    }
else
    {
    errAbort("mp-> type not data,fileName, or multi - unexpected MIME structure");
    }


freez(&cidMain);
freez(&ceMain);
freez(&ctName);
freez(&ctCharset);
freez(&ctMain);

freez(&cdName);
freez(&cdFileName);
freez(&margin);

}




static struct mimePart * cgiParseMultipart(int fd, FILE *out, boolean autoBoundary)
/* process a multipart form */
{
char h[1024];  /* hold mime header line */
char *s = NULL;
struct mimeBuf *mb = NULL;
struct mimePart *mp = NULL;
char **env = NULL;

if (altHeader)
    {
    /* find the CONTENT_ environment strings, use to make Alternate Header string for MIME */
    for(env=environ; *env; env++)
	if (startsWith("CONTENT_",*env))
	    {
	    //debug
	    //fprintf(stderr,"%s\n",*env);  //debug
	    safef(h,sizeof(h),"%s",*env);
	    /* change env syntax to MIME style header, from _ to - */
	    s = strchr(h,'_');    
	    if (!s)
		errAbort("expecting '_' parsing env var %s for MIME alt header", *env);
	    *s = '-';
	    /* change env syntax to MIME style header, from = to : */
	    s = strchr(h,'=');
	    if (!s)
		errAbort("expecting '=' parsing env var %s for MIME alt header", *env);
	    *s = ':';
	    dyStringPrintf(dy,"%s\r\n",h);
	    }
    dyStringAppend(dy,"\r\n"); /* blank line at end means end of headers */
    fprintf(out,"Alternate Header Text:\n%s",dy->string);
    }

mb = initMimeBuf(fd);

if (altHeader)
	/* The Alternate Header will get freed */
    mp = parseMultiParts(mb, cloneString(dy->string)); 
else if (autoBoundary)
    mp = parseMultiParts(mb, "autoBoundary"); 
else    
    mp = parseMultiParts(mb, NULL); 

if(!mp->multi) /* expecting multipart child parts */
    errAbort("Malformatted multipart-form.");


freez(&mb);


if (optionExists("cid"))
    { /* make a preliminary pass to populate cid hash */
    noNameCount = 0;
    cidPass = TRUE;
    dyStringClear(dy);
    printMimeInfo(mp, out, 0);
    }
noNameCount = 0;
cidPass = FALSE;
dyStringClear(dy);
printMimeInfo(mp, out, 0);
fprintf(out,"%s",dy->string);

return mp;
}


static void freeMimeParts(struct mimePart **pMp)
/* free struct mimePart* recursively if needed */
{
struct mimePart *next = NULL, *mp = *pMp; 

while (mp)
    {
    next = mp->next;
    if (mp->multi)
	freeMimeParts(&mp->multi);
    freeHashAndVals(&mp->hdr);
    freez(&mp->data);
    freez(&mp->fileName);
    freez(pMp);
    mp = next;
    pMp = &mp;
    }

}

int main(int argc, char *argv[])
{
struct mimePart *mp = NULL;
optionInit(&argc, argv, options);
if (argc != 2)
    usage("wrong number of args");
if (optionExists("-help"))
    usage("help");
if (optionExists("altHeader") && optionExists("autoBoundary"))
    usage("-altHeader and -autoBoundary may not be used together");    
altHeader  = optionVal("altHeader",altHeader);
outDir = argv[1];

if (altHeader)
    putenv(altHeader);

cidHash = hashNew(5);
dy = dyStringNew(0);

mp = cgiParseMultipart(STDIN_FILENO, stdout, optionExists("autoBoundary"));
freeMimeParts(&mp);

hashFree(&cidHash);
freeDyString(&dy);
return 0;
}


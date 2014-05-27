/* mimeTester - test program for mime parser */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "pipeline.h"
#include "linefile.h"
#include "dystring.h"
#include "mime.h"
extern char **environ;


char *altHeader = NULL;

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n\n"
    "mimeTester - test program for mime parser\n"
    "\n"
    "Usage:\n"
    "   mimeTester [options]\n"
    "Options:\n"
    "  -altHeader='CONTENT_TYPE=\"multipart/form-data, boundary=AaB03x\"'\n"
    "    be sure to specify the same boundary pattern that is used \n"
    "    in the stdin input.\n"
    "  -autoBoundary - no boundary given, scan for --.\n"
    "  -sizeSeries=n - run a series of mime messages to parse of increasing size\n"
    "    with dna field size 0 to n\n"
    "  --help - this help screen\n",
    msg);
}

int sizeSeries = -1;

static struct optionSpec options[] =
{
    {"altHeader", OPTION_STRING},
    {"autoBoundary", OPTION_BOOLEAN},
    {"sizeSeries", OPTION_INT},
    {"-help", OPTION_BOOLEAN},
    {NULL, 0},
};



void printMimeInfo(struct mimePart *mp, FILE *out, int level)
/* print mimeParts recursively if needed */
{

char *cd = NULL, *cdMain = NULL, *cdName = NULL, *cdFileName = NULL, 
 *ct = NULL, *ce = NULL;
char *margin = needMem(level+1);
int i = 0;
for(i=0;i<level;++i)
    margin[i] = ' ';
margin[level] = 0;    


cd = hashFindVal(mp->hdr,"content-disposition");
ct = hashFindVal(mp->hdr,"content-type");
ce = hashFindVal(mp->hdr,"content-transfer-encoding");

if (cd)
    {
    fprintf(out,"%scontent-disposition: %s\n",margin,cd);
    cdMain=getMimeHeaderMainVal(cd);
    cdName=getMimeHeaderFieldVal(cd,"name");
    fprintf(out,"%smain:[%s]\n",margin,cdMain);
    fprintf(out,"%sname:[%s]\n",margin,cdName);
    cdFileName=getMimeHeaderFieldVal(cd,"filename");
    if (cdFileName)
    	fprintf(out,"%sfilename:[%s]\n",margin,cdFileName);
    }
if (ct)
    fprintf(out,"%scontent-type: %s\n",margin,ct);
if (ce)
    fprintf(out,"%scontent-transer-encoding: %s\n",margin,ce);

if (cd)
    {
    fprintf(out,"%ssize:[%llu]\n",margin,(unsigned long long) mp->size);
    if (mp->binary)
    	fprintf(out,"%sbinary (contains zeros)\n",margin);
    if (mp->fileName)
	fprintf(out,"%sfileName=[%s]\n",margin, mp->fileName);
    fprintf(out,"%sdata:[%s]\n",margin,
    	mp->binary && mp->data ? "<binary data not safe to print>" : mp->data);
    fprintf(out,"\n");
    }

if (mp->data) 
    {
    }	    
else if (mp->fileName)
    {
    }
else if (mp->multi)
    {
    fprintf(out,"%snested MIME structure\n\n",margin);
    for(mp=mp->multi;mp;mp=mp->next)
	printMimeInfo(mp, out, level+1);
    }
else
    {
    errAbort("mp-> type not data,fileName, or multi - unexpected MIME structure");
    }

freez(&cdMain);
freez(&cdName);
freez(&cdFileName);
freez(&margin);

}




static struct mimePart * cgiParseMultipart(int fd, FILE *out, boolean autoBoundary)
/* process a multipart form */
{
char h[1024];  /* hold mime header line */
char *s = NULL;
struct dyString *dy = newDyString(256);
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
    mp = parseMultiParts(mb, cloneString(dy->string)); /* The Alternate Header will get freed */
else if (autoBoundary)    
    mp = parseMultiParts(mb, "autoBoundary"); 
else    
    mp = parseMultiParts(mb, NULL); 

if(!mp->multi) /* expecting multipart child parts */
    errAbort("Malformatted multipart-form.");

freeDyString(&dy);

freez(&mb);


printMimeInfo(mp, out, 0);

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

#define TESTFILENAME "mimeTester.tmp"
#define TESTFILEOUT "mimeTester.out"

static void makeTestFile(int size)
/* make a tmp test file as input to mime parser */
{
int i = 0;
FILE *f = mustOpen(TESTFILENAME,"w");
char *dna = NULL;
fprintf(f,"%s",
"Content-type: multipart/form-data; boundary=----------0xKhTmLbOuNdArY\r\n"
"\r\n"
"------------0xKhTmLbOuNdArY\r\n"
"Content-Disposition: form-data; name=\"hgsid\"\r\n"
"\r\n"
"63932244\r\n"
"------------0xKhTmLbOuNdArY\r\n"
"Content-Disposition: form-data; name=\"org\"\r\n"
"\r\n"
"Human\r\n"
"------------0xKhTmLbOuNdArY\r\n"
"Content-Disposition: form-data; name=\"db\"\r\n"
"\r\n"
"hg17\r\n"
"------------0xKhTmLbOuNdArY\r\n"
"Content-Disposition: form-data; name=\"type\"\r\n"
"\r\n"
"BLAT's guess\r\n"
"------------0xKhTmLbOuNdArY\r\n"
"Content-Disposition: form-data; name=\"sort\"\r\n"
"\r\n"
"query,score\r\n"
"------------0xKhTmLbOuNdArY\r\n"
"Content-Disposition: form-data; name=\"output\"\r\n"
"\r\n"
"hyperlink\r\n"
"------------0xKhTmLbOuNdArY\r\n"
"Content-Disposition: form-data; name=\"userSeq\"\r\n"
"\r\n"
">NM_000230 (LEP)\n"
);

dna = needMem(size+1);
for(i=0;i<size;++i)
    {
    dna[i] = 'A';
    }
dna[size] = 0;    
fprintf(f,"%s",dna);
freez(&dna);

fprintf(f,"%s",
"\r\n"
"------------0xKhTmLbOuNdArY\r\n"
"Content-Disposition: form-data; name=\"Submit\"\r\n"
"\r\n"
"Submit\r\n"
"------------0xKhTmLbOuNdArY\r\n"
"Content-Disposition: form-data; name=\"seqFile\"; filename=\"\"\r\n"
"\r\n"
"\r\n"
"------------0xKhTmLbOuNdArY--\r\n"
);

carefulClose(&f);
}


static void doSizeSeries()
/* test a range of increasing sizes for the mime parser */
{
int fd = -1;
FILE *out = NULL;
struct mimePart *mp = NULL; 
int i = 0;
for (i=0;i<sizeSeries;++i)
    {
    out = mustOpen(TESTFILEOUT, "w");
    makeTestFile(i);
    fd = open(TESTFILENAME, O_RDONLY);
    mp = cgiParseMultipart(fd, out, FALSE);
    fprintf(stderr,"ouput %d ok\n",i);
    close(fd);
    fd = -1;
    carefulClose(&out);
    freeMimeParts(&mp);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 1)
    usage("wrong number of args");
if (optionExists("-help"))
    usage("help");
if (optionExists("altHeader") && optionExists("sizeSeries"))
    usage("-altHeader and -sizeSeries may not be used together");
if (optionExists("altHeader") && optionExists("autoBoundary"))
    usage("-altHeader and -autoBoundary may not be used together");
if (optionExists("autoBoundary") && optionExists("sizeSeries"))
    usage("-autoBoundary and -sizeSeries may not be used together");
altHeader  = optionVal("altHeader",altHeader);
sizeSeries = optionInt("sizeSeries",sizeSeries);

if (altHeader)
    putenv(altHeader);

if (optionExists("sizeSeries"))
    {
    doSizeSeries();
    }
else    
    {
    struct mimePart *mp = cgiParseMultipart(STDIN_FILENO, stdout, optionExists("autoBoundary"));
    freeMimeParts(&mp);
    }

return 0;
}

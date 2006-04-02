/* mysqlEscape - GALT BARBER 2006-03-31
 * Reads data formatted with long string field and line terminators.
 *  it can be used to just parse the data and count the fields and rows,
 *  but it can also be used to do much more, i.e. convert to another set of terminators,
 *  insert \N to mark NULL values for mysql, and escape the other fields generally.
 *  It also serves as a model of how to parse for non-single-character punctuators
 *  capable of operating quickly on large files while using a modest amount of memory
 *  in a rolling overlapped buffer.
 */

#include "common.h"
#include "options.h"
#include "dystring.h"

#include <mysql.h>

#define BUFSIZE 32768

boolean noOutput  = FALSE;
boolean noEscape  = FALSE;
boolean noNullFix = FALSE;
boolean tabSep    = FALSE;

#define TTERM "<UCSCtabUCSC>"
#define NTERM "<UCSCnewlineUCSC>"
#define NULLMARK "\\N"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"noNullFix"  , OPTION_BOOLEAN},
    {"noEscape"   , OPTION_BOOLEAN},
    {"noOutput"   , OPTION_BOOLEAN},
    {"tabSep"     , OPTION_BOOLEAN},
    {"-help"      , OPTION_BOOLEAN},
    {NULL, 0}
};


static void usage(char *self)
/* print usage message and exit */
{
errAbort("Purpose:\n\tperform mysql escape on stdin, write to stdout.\n"
    "Usage:\n\t%s < input > output\n"
    "options:\n"
    " -noNullFix suppresses fixing NULLs for MySQL\n"
    " -noEscape  suppresses escaping for MySQL\n"
    " -noOutput  suppresses writing output\n"
    " -tabSep causes tab and newline to be used instead \n"
    ,self);
}


static void putOut(char *buf, int *pi, char *more, int len)
/* buffer output to stdout to increase speed, use len==-1 to flush */
{
int i = *pi;
int bufSize = BUFSIZE*2 + 1;
int leftover = len;
while(leftover!=0)
    {
    len = leftover;
    if (len > bufSize)
	{
	len = bufSize;
	}
    if (((bufSize - i) < len) || (len < 0))
	{
	mustWrite(stdout, buf, i);
	i = 0;
	}
    if (len>0)
	{
	memcpy(buf+i,more,len);
	i+=len;
	}
    leftover -= len;
    more += len;
    }    
*pi = i;
}


int main(int argc, char *argv[])
{
char *buf=NULL;
char *fix=NULL;
char *out=NULL;
int bi = 0;    /* current index 0-based - where span starts */  
int bii = 0;    /* current index 0-based - where span ends */
int bn = -1;   /* next newline token */
int bt = -1;   /* next tab token */
int be = 0;    /* end of data 0-based */
int nread = -1;
int size = 0;
int fieldCount = 0;
int rowCount = 1;
int lastFC = -1;
char *match = NULL;
int ns = strlen(NTERM);
int ts = strlen(TTERM);
int is = 0;
int blimit = 0;
int overlap = max(strlen(TTERM),strlen(NTERM)) - 1;
int oi = 0;
boolean tagLeft = TRUE;
struct dyString *field = dyStringNew(0);
char *tOut = TTERM;  // this can be changed to desired character or string
char *nOut = NTERM; 
char *iOut = NULL;

optionInit(&argc, argv, optionSpecs);
if (argc != 1 || optionExists("-help"))
    usage(argv[0]);
noNullFix = optionExists("noNullFix");
noEscape  = optionExists("noEscape");
noOutput  = optionExists("noOutput");
tabSep    = optionExists("tabSep");

if ((tabSep && noOutput) || (tabSep && noEscape))
    errAbort("-tabSep should not be used with -noOutput or -noEscape");

if (tabSep)
    {
    tOut = "\t";
    nOut = "\n";
    }

buf=needMem(BUFSIZE);
out=needMem(BUFSIZE*2+1);
dyStringClear(field);

while (nread != 0)
    {
    //verbose(2,"reading more be=%d\n",be);
    while (be < BUFSIZE && (nread = fread(buf+be, 1, BUFSIZE-be, stdin))>0)
	{
	be += nread;	
    	//verbose(2,"read %d bytes, be=%d\n",nread,be);
	}
    if (nread < 0)    
	errAbort("Error reading %lld bytes: %s", (long long)BUFSIZE, strerror(ferror(stdin)));
    blimit = be;
    if (be == BUFSIZE)
    	blimit = BUFSIZE - overlap;
    bn = -1;
    bt = -1;
    while (bi < blimit)
	{
	if (bn < 0)
	    {
	    match = memMatch(NTERM, ns, buf+bi, be-bi);
	    if (match) 
		bn = match - buf;
	    else
		bn = BUFSIZE;  
	    }
	if (bt < 0)
	    {
	    match = memMatch(TTERM, ts, buf+bi, be-bi);
	    if (match) 
		bt = match - buf;
	    else
		bt = BUFSIZE;  
	    }
	if (bt < bn)  /* tab field separator is next */
	    {
	    bii = bt;
	    bt = -1;
	    is = ts;
	    iOut = tOut;
	    ++fieldCount;		
	    }
	else if (bn < bt)  /* newline is next */
	    {
	    bii = bn;
	    bn = -1;
	    is = ns;
	    iOut = nOut;
	    ++fieldCount;
	    if (lastFC == -1)
		lastFC = fieldCount;
	    if (lastFC != fieldCount)
		verbose(1,"expected %d fields, found %d in row %d\n", lastFC, fieldCount, rowCount);
	    fieldCount = 0;
	    ++rowCount;
	    }
	else      /* just a fragment continuing */
	    {
	    bii = blimit;
	    is = 0;
	    }

    	dyStringAppendN(field, buf+bi, bii - bi);
	if (is > 0) /* we have a complete tag and output desired */
	    {
	    if (field->stringSize==0)
		{
		if (!noNullFix)
		    dyStringAppend(field, NULLMARK);  /* mark as NULL for mysql, when field is empty */
		}
	    else
		{
		if (!noEscape)
		    {
		    /* Don't call the function in jksql.c because it assumes it is a string 
		     * i.e. that has a terminating 0,
		     * whereas in theory the input data itself could contain a nul. */
		    fix=needMem(field->stringSize*2+1);
		    mysql_escape_string(fix, field->string, field->stringSize);
		    dyStringClear(field);
		    dyStringAppendN(field, fix, strlen(fix));
		    freez(&fix);
		    if (strchr(field->string, '\t'))  /* have to escape tabs separately */
			{
			fix=replaceChars(field->string, "\t", "\\t");
			dyStringClear(field);
			dyStringAppendN(field, fix, strlen(fix));
			freez(&fix);
			}
		    }
		}
	    if (!noOutput)
		{
		putOut(out, &oi, field->string, field->stringSize);  /* field value     */
		putOut(out, &oi, iOut, strlen(iOut));                /* field separator */
		}
	    dyStringClear(field);
	    }
	    
	bii += is; /* skip over input separator */ 
	    
	bi = bii;  /* this becomes last */

	}

    memcpy(buf,buf+bi,be-bi);  /* scroll remainder overlap */
    be -= bi;	    
    bi = 0;
	
    }	    

/* any residual field value - actually it's probably some unescaped garbage, do not output.
  putOut(out, &oi, field->string, field->stringSize);  
*/  

/* flush to stdout */
putOut(out, &oi, NULL, -1);

verbose(1,"#fields=%d (fieldCount var=%d), #rows=%d\n", lastFC, fieldCount, rowCount-1);

}


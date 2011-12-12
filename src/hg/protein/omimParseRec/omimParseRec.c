/* omimParseRec - parse text of OMIM records by different field types */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"


FILE *fh2; /* 2nd file handle pointing to the OMIM text file */
FILE *recFh;
FILE *fieldFh;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "omimParseRec - parse OMIM records by different field types\n"
  "usage:\n"
  "   omimParseRec omimText outRecFile outFieldFile\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct omimRecord
/* An OMIM record. */
    {
    char *id;		/* OMIM ID */
    char type;		/* OMIM type */
    char *title;	/* OMIM title */
    char *geneSymbol;	/* gene symbol */

    int  startPos;
    int  endPos;
    
    char *av;
    char *cd;
    char *cn;
    char *cs;
    char *ed;
    char *rf;
    char *sa;
    char *tx;
  };

struct omimField
/* An OMIM field. */
    {
    char *omimId;	/* OMIM ID */
    char *type;		/* field type */
    char *title;	/* field title */

    int  startPos;
    int  endPos;
    };

struct omimLitRef 
/* A OMIM literature reference. */
    {
    struct omimLitRef *next;
    char *title;	/* Title of article. */
    char *cite;		/* Journal/book/patent citation. */
    struct slName *authorList;	/* Author names in lastName, F.M. format. */
    char *rp;		/* Somewhat complex 'Reference Position' line. */
    struct hashEl *rxList; /* Cross-references. */
    struct hashEl *rcList; /* TISSUE=XXX X; STRAIN=YYY; parsed out. */
    char *pubMedId;	/* pubMed ID, may be NULL. */
    char *medlineId;	/* Medline ID, may be NULL. */
    char *doiId;	/* DOI ID, may be NULL. */
    };

char buffer[80*10000];

void printField(struct omimField *omimFd, FILE *outf)
/* print out field info */
{
long bytesRead;

fprintf(outf, "%s\t%s\t%d\t%d\n", 
       omimFd->omimId, omimFd->type, omimFd->startPos, omimFd->endPos - omimFd->startPos + 1);
       fflush(stdout);
	
if (((omimFd->endPos - omimFd->startPos + 1) + 1) >sizeof(buffer))
    {
    fprintf(stderr, 
    	    "field %s for OMIM record %s needs %d bytes, which exceeded buffer size of %d\n", 
	    omimFd->type, omimFd->omimId, 
	    (omimFd->endPos - omimFd->startPos + 1) + 1, (int)sizeof(buffer));
    exit(1);
    }

fseek(fh2, (long)(omimFd->startPos), SEEK_SET);
bytesRead = (long)fread(buffer, (size_t)1, (size_t)(omimFd->endPos - omimFd->startPos + 1), fh2);
*(buffer+bytesRead) = '\0';
}
    
struct omimRecord *omimRecordNext(struct lineFile *lf, 
	struct lm *lm, 	/* Local memory pool for this structure. */
	struct dyString *dy,	/* Scratch string to use. */
	FILE *recFh,
	FILE *fieldFh)
/* Read next record from file and parse it into omimRecord structure
 * that is allocated in memory. */
{
char *line;
struct omimRecord *omimr;
char *chp;
int lineSize;
char *fieldType;
boolean recDone, fieldDone;
char *row[1];

boolean endOfFile = FALSE;
boolean endOfRec  = FALSE;

boolean firstFieldDone = FALSE;

struct omimField *omimFd = NULL;

recDone = FALSE;

/* Parse record number and title lines. */
    if (!lineFileRow(lf, row))
	return NULL;
    if (!sameString(row[0], "*RECORD*"))
	{
	errAbort("Expecting *RECORD* line %d of %s", lf->lineIx, lf->fileName);
    	}
    
    lmAllocVar(lm, omimr);
    omimr->startPos = lf->lineStart + lf->bufOffsetInFile;
    

    if (!lineFileNext(lf, &line, &lineSize))
	return NULL;
    
    if (!sameString(line, "*FIELD* NO"))
	errAbort("Expecting *FIELD* NO line %d of %s", lf->lineIx, lf->fileName);
    
    if (!lineFileNextReal(lf, &line)) errAbort("%s ends in middle of a record", lf->fileName);
    omimr->id = lmCloneString(lm, line);
   
    if (!lineFileNextReal(lf, &line)) errAbort("%s ends in middle of a record", lf->fileName);
    if (!sameString(line, "*FIELD* TI"))
	errAbort("Expecting *FIELD* TI line %d of %s ---%s---", lf->lineIx, lf->fileName, line);
    if (!lineFileNext(lf, &line, &lineSize)) errAbort("%s ends in middle of a record", lf->fileName);
    if (!isdigit(*line)) 
    	{
	omimr->type = *line;
	}
    else
    	{
	omimr->type = '\0';
	}
   
    /* some records may not have gene symbol */
    omimr->geneSymbol = "";
    chp = strstr(line, ";");
    if (chp != NULL)
    	{
	*chp = '\0';
	chp++;
	if (*chp == ' ') chp++;
	if (*chp != '\0') omimr->geneSymbol = cloneString(chp);
	}
    chp = strstr(line, omimr->id);
    if (chp == NULL)
    	{
	errAbort("Expecting TI line for record %s, %d of %s ---%s===", omimr->id, lf->lineIx, 
	lf->fileName, line);
   	} 
    chp = chp + strlen(omimr->id); chp++;
    omimr->title = lmCloneString(lm, chp);
    
    // !!! need to enhance it later to include startPos and length info.
    fprintf(recFh, "%s\t%c\t%s\t%s\n", omimr->id, omimr->type, omimr->geneSymbol, omimr->title);
   
    /* !!! temporarily skip lines before first FIELD after title */
    /* further processing TBD */
    while (strstr(line, "*FIELD*") == NULL)
    	{
	lineFileNext(lf, &line, &lineSize);
	}
    lineFileReuse(lf);
    
    /* process a field */
    firstFieldDone = FALSE;
    fieldDone = FALSE;
    while (!recDone)
    	{
        if (!lineFileNext(lf, &line, &lineSize)) 
	    {
	    endOfFile = TRUE;
	    }
	    
	/* "*THEEND*" signals the end of the OMIM text file */
	if (sameWord(line, "*THEEND*")) endOfFile = TRUE;
	
	if (!endOfFile && (strstr(line, "*RECORD*") != NULL)) 
	    {
	    endOfRec = TRUE;
	    }
	    
	/* handle termination of record here */    
	if (endOfFile || endOfRec)
	    {
	    lineFileReuse(lf);
	    recDone = TRUE;
    	    if (firstFieldDone) 
	    	{
		/* extra minus 1 to get rid of empty line at the end */
		omimr ->endPos = lf->lineStart + lf->bufOffsetInFile - 1 - 1;
		omimFd->endPos = lf->lineStart + lf->bufOffsetInFile - 1 - 1;
		
	   	/* End of record or end of line also means end of a field.  Print the field info */
		printField(omimFd, fieldFh);
		} 
	    break;
	    }
	
	chp = strstr(line, "*FIELD*");
	if (chp != NULL)
	    {
	    fieldType = chp + strlen("*FIELD* ");
	    if (!firstFieldDone)
	    	{
    	    	lmAllocVar(lm, omimFd);
		firstFieldDone = TRUE;
		/* do not print anything at the first "*FIELD*" line */
		}
	    else
	    	{
		/* extra minus 1 to get rid of empty line at the end */
		omimFd->endPos = lf->lineStart + lf->bufOffsetInFile - 1 - 1;
	   	/* print previous field info */
		printField(omimFd, fieldFh);
		}
		
	    /* fill in current field info */		
	    omimFd->omimId   = cloneString(omimr->id);
	    omimFd->type     = cloneString(fieldType);
	    omimFd->startPos = lf->lineEnd + lf->bufOffsetInFile;
            }
	}
if (endOfFile) return NULL;    
return omimr;
}

void omimParseRec(char *omimTextFile, FILE *recFh, FILE *fieldFh)
/* omimParseRec - parse OMIM flat text file. */
{
struct lineFile *lf = lineFileOpen(omimTextFile, TRUE);
struct omimRecord *omimr;
struct dyString *dy = newDyString(4096);

for (;;)
    {
    struct lm *lm = lmInit(8*1024);
    omimr = omimRecordNext(lf, lm, dy, recFh, fieldFh);
    if (omimr == NULL)
        break;
    lmCleanup(&lm);
    }
dyStringFree(&dy);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

fh2     = mustOpen(argv[1], "r");
recFh   = mustOpen(argv[2], "w");
fieldFh = mustOpen(argv[3], "w");

omimParseRec(argv[1], recFh, fieldFh);

return 0;
}

/* omimParseRec - parse text of OMIM records into files according to different field types */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"

static char const rcsid[] = "$Id: omimParseRec.c,v 1.1 2006/10/09 21:21:13 fanhsu Exp $";

FILE *omimTitle;
FILE *omimTitleAlt;

FILE *omimAv;
FILE *omimCd;
FILE *omimCn;
FILE *omimCs;
FILE *omimEd;
FILE *omimRf;
FILE *omimSa;
FILE *omimTx;

#define MAX_TX_SEC_TYPE 27

char txSecType[MAX_TX_SEC_TYPE][80]={
"ANIMAL MODEL",
"BIOCHEMICAL FEATURES",
"CLINICAL FEATURES",
"CLINICAL MANAGEMENT",
"CLONING",
"CYTOGENETICS AND MAPPING",
"CYTOGENETICS",
"DESCRIPTION",
"DIAGNOSIS",
"EVOLUTION",
"GENE FAMILY",
"GENE FUNCTION",
"GENE STRUCTURE",
"GENE THERAPY",
"GENETIC VARIABILITY",
"GENOTYPE",
"GENOTYPE/PHENOTYPE CORRELATIONS",
"HETEROGENEITY",
"HISTORY",
"INHERITANCE",
"MAPPING",
"MOLECULAR GENETICS",
"NOMENCLATURE",
"OTHER FEATURES",
"PATHOGENESIS",
"PHENOTYPE",
"POPULATION GENETICS"
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "omimParseRec - parse text of OMIM records into files according to different field types\n"
  "usage:\n"
  "   omimParseRec omimText outDir\n"
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

    char *av;
    char *cd;
    char *cn;
    char *cs;
    char *ed;
    char *rf;
    char *sa;
    char *tx;
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

struct omimRecord *omimRecordNext(struct lineFile *lf, 
	struct lm *lm, 	/* Local memory pool for this structure. */
	struct dyString *dy)	/* Scratch string to use. */
/* Read next record from file and parse it into omimRecord structure
 * that is allocated in memory. */
{
char *line, *word, *type;
struct omimRecord *omimr;
struct slName *n;
char *chp;
int j=0;
int lineSize;
char *fieldType;
FILE *fh = NULL;
boolean recDone, fieldDone;
char *row[1];
char *upperStr;

char avPosLine[255];

char avPosLine1[255];
char avPosLine2[255];
char avPosLine3[255];
char avPosLine4[255];

recDone = FALSE;
    /* Parse record number and title lines. */
    if (!lineFileRow(lf, row))
	return NULL;
    if (!sameString(row[0], "*RECORD*"))
	{
	errAbort("Expecting *RECORD* line %d of %s", lf->lineIx, lf->fileName);
    	}
    if (!lineFileNext(lf, &line, &lineSize))
	return NULL;
    if (!sameString(line, "*FIELD* NO"))
	errAbort("Expecting *FIELD* NO line %d of %s", lf->lineIx, lf->fileName);
    
    if (!lineFileNextReal(lf, &line)) errAbort("%s ends in middle of a record", lf->fileName);
    lmAllocVar(lm, omimr);
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
    fprintf(omimTitle, "%s\t%c\t%s\t%s\n", omimr->id, omimr->type, omimr->geneSymbol, omimr->title);
    fflush(omimTitle); 
   
    printf("%s|%s|%s\n", omimr->id, omimr->title, omimr->geneSymbol);
    
    /* !!! temporarily skip lines before first FIELD after title */
    /* further processing TBD */
    while (strstr(line, "*FIELD*") == NULL)
    	{
	//printf("skipping ... %s\n", line);fflush(stdout);
	lineFileNext(lf, &line, &lineSize);
	}
    lineFileReuse(lf);
    
    /* process a field */
    fieldDone = FALSE;
    while (!recDone)
    	{
        if (!lineFileNext(lf, &line, &lineSize)) break;
	if (strstr(line, "*RECORD*") != NULL) 
	    {
	    lineFileReuse(lf);
	    recDone = TRUE;
	    break;
	    }
	
	chp = strstr(line, "*FIELD*");
	if (chp != NULL)
	    {
	    fieldType = chp + strlen("*FIELD* ");
	   
	    fh = NULL;
	    
	    if (sameWord(fieldType, "AV")) fh = omimAv;	 
	    if (sameWord(fieldType, "CD")) fh = omimCd;	 
	    if (sameWord(fieldType, "CN")) fh = omimCn;	 
	    if (sameWord(fieldType, "CS")) fh = omimCs;	 
	    if (sameWord(fieldType, "ED")) fh = omimEd;	 
	    if (sameWord(fieldType, "RF")) fh = omimRf;	 
	    if (sameWord(fieldType, "SA")) fh = omimSa;	 
	    if (sameWord(fieldType, "TX")) fh = omimTx;	 
	    
	    if (fh == NULL)
	    	{
		fprintf(stderr, "Uncognized field type: %s from line: %s\n", fieldType, line);
		exit(1);
		}
		
	    fprintf(fh, "***\t%s\t%s\n", omimr->id, fieldType);
           
	    fieldDone = FALSE;
	    while (!fieldDone)
	    	{
	    	if (!lineFileNext(lf, &line, &lineSize)) 
		   {
		   fieldDone = TRUE;
		   recDone   = TRUE;
		   break;
	    	   }
		if (strstr(line, "*FIELD*") != NULL) 
	    	   {
		   fieldDone = TRUE;
	    	   lineFileReuse(lf);
		   break;
		   }
		if (strstr(line, "*RECORD*") != NULL) 
	    	    {
	    	    fieldDone = TRUE;
		    recDone = TRUE;
		    lineFileReuse(lf);
	    	    break;
	    	    }
	        if (lineSize > 0)
		    {
		    /* further processing of TX field */
		    /* TBD */
		    if (sameWord(fieldType, "TX"))
		    	{
			for (j=0; j<MAX_TX_SEC_TYPE; j++)
			    {
			    if (strcmp(line, txSecType[j])==0) 
			    	{
				//printf("\t%s\n", line);fflush(stdout);
				}
			    }
		    	}
			
		    /* further processing of AV field */
		    /* TBD */
		    if (sameWord(fieldType, "AV"))
		    	{
			}
		    fprintf(fh, "%s\n", line);
		    }
		else
		    {
		    fprintf(fh, "\n");
		    }
		}
	    }
	else
	    {
	    printf("GOT %s\n", line);fflush(stdout);
	    errAbort("expecting a FIELD line \n");
	    }
	fflush(fh);
	}
    
return omimr;
}

/* ------------- Start main program ------------------  */

FILE *createAt(char *dir, char *file)
/* Open file in dir. */
{
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s.txt", dir, file);
return mustOpen(path, "w");
}

void omimParseRec(char *datFile, char *tabDir)
/* omimParseRec - Create a relational database out of OMIM flat file. */
{
struct lineFile *lf = lineFileOpen(datFile, TRUE);
struct omimRecord *omimr;
struct dyString *dy = newDyString(4096);

/* We have many tables to make this fully relational and not
 * lose any info. Better start opening files. */

makeDir(tabDir);

omimTitle    = createAt(tabDir, "omimTitle");
omimTitleAlt = createAt(tabDir, "omimTitleAlt");
omimAv 	     = createAt(tabDir, "omimAv");
omimCd 	     = createAt(tabDir, "omimCd");
omimCn 	     = createAt(tabDir, "omimCn");
omimCs 	     = createAt(tabDir, "omimCs");
omimEd 	     = createAt(tabDir, "omimEd");
omimRf 	     = createAt(tabDir, "omimRf");
omimSa 	     = createAt(tabDir, "omimSa");
omimTx 	     = createAt(tabDir, "omimTx");

for (;;)
    {
    struct lm *lm = lmInit(8*1024);
    char *acc;
    struct slName *n;
    omimr = omimRecordNext(lf, lm, dy);
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
if (argc != 3)
    usage();
omimParseRec(argv[1], argv[2]);

fclose(omimAv);
fclose(omimCd);
fclose(omimCn);
fclose(omimCs);
fclose(omimEd);
fclose(omimRf);
fclose(omimSa);
fclose(omimTx);

return 0;
}

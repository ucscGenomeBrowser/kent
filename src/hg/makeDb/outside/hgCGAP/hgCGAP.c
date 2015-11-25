/* This program parse CGAP data file */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#define      ALIAS	1
#define   BIOCARTA	2
#define   CYTOBAND	3
#define         GO	4
#define    HOMOLOG	5
#define       KEGG	6
#define    LOCUSID	7
#define  MGC_CLONE	8
#define      MOTIF	9
#define       OMIM	10
#define   SEQUENCE	11
#define        SNP	12
#define   SV_CLONE	13
#define     SYMBOL	14
#define      TITLE	15
#define    UNIGENE	16

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int done;

char line[2000];
FILE *inf;
char id[40];
int attr;

FILE *fBIOCARTAdesc;
FILE *fKEGGdesc;

FILE *fALIAS;
FILE *fBIOCARTA;
FILE *fCYTOBAND;
FILE *fGO;
FILE *fHOMOLOG;
FILE *fKEGG;
FILE *fLOCUSID;
FILE *fMGC_CLONE;
FILE *fMOTIF;
FILE *fOMIM;
FILE *fSEQUENCE;
FILE *fSNP;
FILE *fSV_CLONE;
FILE *fSYMBOL;
FILE *fTITLE;
FILE *fUNIGENE;

int doALIAS(char *data)
    {
    if ((strcmp(data, "NA") != 0) && (strcmp(data, "na") != 0) )
	{
    	fprintf(fALIAS, "%s\t%s\n",id, data);
    	}
    return(0);
    }

int doBIOCARTA(char *data)
    {
    char *chp;
    char *desc;

    chp = strstr(data, "|");
    *chp = '\0';
    desc = chp+1;

    fprintf(fBIOCARTA, "%s\t%s\n",id, data);
    fprintf(fBIOCARTAdesc, "%s\t%s\n",data, desc);
    return(0);
    }

int doCYTOBAND(char *data)
    {
    fprintf(fCYTOBAND, "%s\t%s\n",id, data);
    return(0);
    }

int doGO(char *data)
    {
    fprintf(fGO, "%s\t%s\n",id, data);
    return(0);
    }

int doHOMOLOG(char *data)
    {
    fprintf(fHOMOLOG, "%s\t%s\n",id, data);
    return(0);
    }

int doKEGG(char *data)
    {
    char *chp;
    char *desc;

    chp = strstr(data, "|");
    *chp = '\0';
    desc = chp+1;

    fprintf(fKEGG, "%s\t%s\n",id, data);
    fprintf(fKEGGdesc, "%s\t%s\n",data, desc);
    return(0);
    }

int doLOCUSID(char *data)
    {
    fprintf(fLOCUSID, "%s\t%s\n",id, data);
    return(0);
    }

int doMGC_CLONE(char *data)
    {
    fprintf(fMGC_CLONE, "%s\t%s\n",id, data);
    return(0);
    }

int doMOTIF(char *data)
    {
    fprintf(fMOTIF, "%s\t%s\n",id, data);
    return(0);
    }

int doOMIM(char *data)
    {
    fprintf(fOMIM, "%s\t%s\n",id, data);
    return(0);
    }

int doSEQUENCE(char *data)
    {
    fprintf(fSEQUENCE, "%s\t%s\n",id, data);
    return(0);
    }

int doSNP(char *data)
    {
    fprintf(fSNP, "%s\t%s\n",id, data);
    return(0);
    }

int doSV_CLONE(char *data)
    {
    fprintf(fSV_CLONE, "%s\t%s\n",id, data);
    return(0);
    }

int doSYMBOL(char *data)
    {
    if ((strcmp(data, "NA") != 0) && (strcmp(data, "na") != 0) )
    	{
	fprintf(fSYMBOL, "%s\t%s\n",id, data);
    	}
    return(0);
    }

int doTITLE(char *data)
    {
    fprintf(fTITLE, "%s\t%s\n",id, data);
    return(0);
    }

int doUNIGENE(char *data)
    {
    fprintf(fUNIGENE, "%s\t%s\n",id, data);
    return(0);
    }

int processRecord(char *attrStr, char *data)
{
if (strcmp(attrStr, "ALIAS") == 0)
    {
    attr = ALIAS;
    goto setDone;
    }

if (strcmp(attrStr, "BIOCARTA") == 0)
    {
    attr = BIOCARTA;
    goto setDone;
    }

if (strcmp(attrStr, "CYTOBAND") == 0)
    {
    attr = CYTOBAND;
    goto setDone;
    }

if (strcmp(attrStr, "GO") == 0)
    {
    attr = GO;
    goto setDone;
    }

if (strcmp(attrStr, "HOMOLOG") == 0)
    {
    attr = HOMOLOG;
    goto setDone;
    }

if (strcmp(attrStr, "KEGG") == 0)
    {
    attr = KEGG;
    goto setDone;
    }

if (strcmp(attrStr, "LOCUSID") == 0)
    {
    attr = LOCUSID;
    goto setDone;
    }

if (strcmp(attrStr, "MGC_CLONE") == 0)
    {
    attr = MGC_CLONE;
    goto setDone;
    }

if (strcmp(attrStr, "MOTIF") == 0)
    {
    attr = MOTIF;
    goto setDone;
    }

if (strcmp(attrStr, "OMIM") == 0)
    {
    attr = OMIM;
    goto setDone;
    }

if (strcmp(attrStr, "SEQUENCE") == 0)
    {
    attr = SEQUENCE;
    goto setDone;
    }

if (strcmp(attrStr, "SNP") == 0)
    {
    attr = SNP;
    goto setDone;
    }

if (strcmp(attrStr, "SV_CLONE") == 0)
    {
    attr = SV_CLONE;
    goto setDone;
    }

if (strcmp(attrStr, "SYMBOL") == 0)
    {
    attr = SYMBOL;
    goto setDone;
    }

if (strcmp(attrStr, "TITLE") == 0)
    {
    attr = TITLE;
    goto setDone;
    }

if (strcmp(attrStr, "UNIGENE") == 0)
    {
    attr = UNIGENE;
    goto setDone;
    }

setDone:

switch (attr)
{
case ALIAS:
    doALIAS(data);
    break;
case BIOCARTA:
    doBIOCARTA(data);
    break;
case CYTOBAND:
    doCYTOBAND(data);
    break;
case GO:
    doGO(data);
    break;
case HOMOLOG:
    doHOMOLOG(data);
    break;
case KEGG:
    doKEGG(data);
    break;
case LOCUSID:
    doLOCUSID(data);
    break;
case MGC_CLONE:
    doMGC_CLONE(data);
    break;
case MOTIF:
    doMOTIF(data);
    break;
case OMIM:
    doOMIM(data);
    break;
case SEQUENCE:
    doSEQUENCE(data);
    break;
case SNP:
    doSNP(data);
    break;
case SV_CLONE:
    doSV_CLONE(data);
    break;
case SYMBOL:
    doSYMBOL(data);
    break;
case TITLE:
    doTITLE(data);
    break;
case UNIGENE:
    doUNIGENE(data);
    break;
}

return(0);
}

int do1Record()
    {
    char *chp;
    char *attr;
    char *data;

    // line shoud be like ">>12345"
    if (line[0] != '>')
	{
	printf("Wrong first line:%s\n", line);fflush(stdout);
	exit(1);
	}

    strcpy(id, line+2);
    
    while (fgets(line, sizeof(line), inf) != NULL)
	{
	*(line + strlen(line) - 1 ) = '\0';
    	chp = strstr(line, ":");
	if (chp != NULL)
	    {
	    *chp = '\0';
	    attr = line;
	    data = chp + 2;
	   
	    processRecord(attr, data);
	    }
	else
	    {
    	    if ((line[0] != '>') || (line[1] != '>'))
		{
		printf("CGAP ID line not found:\n%s\n", line);
	    	break;
		}
	    else
		{
		return(0);
		}
	    }
	}

    done = 1;
    return(0);
    }

int main(argc, argv)
	int argc;
	char **argv;
	{
	char **p;
	char *arg1;

if (argc==2) 
	{
	p=argv;

	p++;
	arg1=*p;

	fALIAS      = fopen("cgapALIAS.tab", "w");
	fBIOCARTA   = fopen("cgapBIOCARTA.tab", "w");
	fCYTOBAND   = fopen("cgapCYTOBAND.tab", "w");
	fGO         = fopen("cgapGO.tab", "w");
	fHOMOLOG    = fopen("cgapHOMOLOG.tab", "w");
	fKEGG       = fopen("cgapKEGG.tab", "w");
	fLOCUSID    = fopen("cgapLOCUSID.tab", "w");
	fMGC_CLONE  = fopen("cgapMGC_CLONE.tab", "w");
	fMOTIF      = fopen("cgapMOTIF.tab", "w");
	fOMIM       = fopen("cgapOMIM.tab", "w");
	fSEQUENCE   = fopen("cgapSEQUENCE.tab", "w");
	fSNP        = fopen("cgapSNP.tab", "w");
	fSV_CLONE   = fopen("cgapSV_CLONE.tab", "w");
	fSYMBOL     = fopen("cgapSYMBOL.tab", "w");
	fTITLE      = fopen("cgapTITLE.tab", "w");
	fUNIGENE    = fopen("cgapUNIGENE.tab", "w");

	fBIOCARTAdesc = fopen("cgapBIOCARTAdesc.tab", "w"); 
	fKEGGdesc = fopen("cgapKEGGdesc.tab", "w"); 

	if ((inf = fopen(arg1, "r")) == NULL)
		{		
		fprintf(stderr, "Can't open file %s.\n", arg1);
		exit(8);
		}

    	if (fgets(line, sizeof(line), inf) == NULL)
	    fprintf(stderr, "first fgets failed\n");
        *(line + strlen(line) - 1) = '\0';
	
        while (!done)
		{
		do1Record();
		}
	}

fclose(fALIAS);
fclose(fBIOCARTA);
fclose(fCYTOBAND);
fclose(fGO);
fclose(fHOMOLOG);
fclose(fKEGG);
fclose(fLOCUSID);
fclose(fMGC_CLONE);
fclose(fMOTIF);
fclose(fOMIM);
fclose(fSEQUENCE);
fclose(fSNP);
fclose(fSV_CLONE);
fclose(fSYMBOL);
fclose(fTITLE);
fclose(fUNIGENE);

fclose(fBIOCARTAdesc);
fclose(fKEGGdesc);
return(0);
}


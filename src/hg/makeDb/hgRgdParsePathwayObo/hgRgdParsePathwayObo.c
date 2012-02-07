/* hgRgdParsePathwayObo - Create a relational database out of RGD pathway .obo file. */
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgRgdParsePathwayObo - Parse RGD pathway .obo file\n"
  "usage:\n"
  "   hgRgdParsePathwayObo pathway.obo\n"
  );
}

char *line;
int lineSize;

FILE *namef;
FILE *deff;
FILE *is_af;

FILE *createAt(char *file)
/* Open file */
{
char path[PATH_LEN];
safef(path, sizeof(path), "%s", file);
return mustOpen(path, "w");
}

void pathwayRecordNext(struct lineFile *lf)
{
char emptyString[2] = {""};

char *chp;
char *pathwayId;
char *name = NULL;
char *def, *comment;
char *is_a;

while (!strstr(line, "[Term]"))
    {
    if (!lineFileNext(lf, &line, &lineSize)) exit(1);
    }

lineFileNext(lf, &line, &lineSize);
chp = strstr(line, "id: ");
chp = chp + strlen("id: ");
pathwayId = strdup(chp);

lineFileNext(lf, &line, &lineSize);
if (strstr(line, "name: ") != NULL)
    {
    chp = strstr(line, "name: ");
    chp = chp + strlen("name: ");
    name= strdup(chp);
    if (strstr(pathwayId, "PW:") != NULL) fprintf(namef, "%s\t%s\n", pathwayId, name);fflush(namef);
    }
else
    {
    errAbort("name: expected");
    }

def     = emptyString;
comment = emptyString;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (strstr(line, "[Term]") != NULL)break;

    if (strstr(line, "def: ") != NULL)
    	{
    	chp = strstr(line, "def: ");
    	chp = chp + strlen("def: ") + 1;
    	def = strdup(chp);
    	chp = def + strlen(def);
    	*chp = '\0';
    	if (strstr(pathwayId, "PW:") != NULL) fprintf(deff, "%s\t%s\t%s\n", pathwayId, name, def);fflush(deff);
    	}		
    else
    	{
    	if (strstr(line, "is_a: ") != NULL) 
    	    {
            chp = strstr(line, "is_a: ");
    	    chp = chp + strlen("is_a: ");
	    is_a = strdup(chp);
            chp = strstr(is_a, " !");
	    *chp = '\0';
	    if (strstr(pathwayId, "PW:") != NULL) fprintf(is_af, "%s\t%s\n", pathwayId, is_a);fflush(is_af);
            }  
    	}	
    }
}

void hgRgdParsePathwayObo(char *datFile)
/* hgRgdParsePathwayObo - Create a relational database out of .obo pathway file. */
{
struct lineFile *lf = lineFileOpen(datFile, TRUE);

namef = createAt("rgdPathway.tab");
deff  = createAt("rgdPathwayDef.tab");
is_af = createAt("rgdPathway_isa.tab");

lineFileNext(lf, &line, &lineSize);
for (;;)
    {
    pathwayRecordNext(lf);
    }

carefulClose(&namef);
carefulClose(&deff);
carefulClose(&is_af);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
hgRgdParsePathwayObo(argv[1]);
return 0;
}

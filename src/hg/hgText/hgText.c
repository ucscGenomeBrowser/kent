#include "common.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "knownInfo.h"
#include "hdb.h"
#include "hgConfig.h"
#include "hgFind.h"

boolean findGenomePos(char *spec, char **retChromName, 
		    int *retWinStart, int *retWinEnd)
{
	struct hgPositions *hgp;
	struct hgPos *pos;
	struct dyString *ui;

	hgp = hgPositionsFind(spec, "");
	if (hgp == NULL || hgp->posCount == 0)
		    {
				    hgPositionsFree(&hgp);
					    errAbort("Sorry, couldn't locate %s in genome database\n", spec);
						    return TRUE;
							    }
	if ((pos = hgp->singlePos) != NULL)
		    {
				    *retChromName = pos->chrom;
					    *retWinStart = pos->chromStart;
						    *retWinEnd = pos->chromEnd;
							    hgPositionsFree(&hgp);
								    return TRUE;
									    }
	else
		    {
				    hgPositionsHtml(hgp, stdout);
					    hgPositionsFree(&hgp);
						    return FALSE;
							    }
	freeDyString(&ui);
}

/* returns true if the string only has letters number and underscores */
boolean allLetters(char* s)
{
int i;
for(i = 0; i < strlen(s); i++)
	if(!isalnum(s[i]) && s[i] != '_')
		return FALSE;

return TRUE;
}

/* this function makes sure that the given table can be accessed 
 * if true, name can safely be used in SQL calls */
boolean validTable(char* name)
{
struct sqlConnection *conn = sqlConnect("hg7");
char query[256];

/* prevent string overflow hack */
if(strlen(name) > 200)
	return FALSE;
/* make sure that there are not SQL control characters */	
if(!allLetters(name))
	return FALSE;

/* create the query */
sprintf(query, "SELECT * FROM browserTable WHERE PRIVATE = 0 AND tableName = '%s'", name);

/* see if any tables match */
return sqlExists(conn, query);
}

void doMiddle()
/* Print middle parts of web page. */
{
char* table = cgiOptionalString("table");
struct cgiVar* current = cgiVarList();

char *chromName;        /* Name of chromosome sequence . */
int winStart;           /* Start of window in sequence. */
int winEnd;         /* End of window in sequence. */

char* position = cgiOptionalString("position");
findGenomePos(position, &chromName, &winStart, &winEnd);

hDefaultConnect();	/* read in the default connection options */

puts("<TABLE>");
while(current != 0)
	{
	puts("\t<TR>");
	printf("\t\t<TD>%s</TD><TD>%s</TD>\n", current->name, current->val);
	puts("\t</TR>");

	current = current->next;
	}
printf("%s from %d to %d\n", chromName, winStart, winEnd);
puts("</TABLE>");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (!cgiIsOnWeb())
   {
   warn("This is a CGI script - attempting to fake environment from command line");
   warn("Reading configuration options from the user's home directory.");
   cgiSpoof(&argc, argv);
   }
htmShell("Genome Text Browser", doMiddle, NULL); 
return 0;
}

/* edwWebCreateUser - Allows one user to create another - vampire mode - over web.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "edwLib.h"

char *oldUserEmail = NULL;

void doMiddle()
/* Write what goes between BODY and /BODY */
{
printf("<FORM ACTION=\"edwWebCreateUser\" METHOD=POST>\n");
printf("<B>Add new user to ENCODE Data Warehouse</B><BR>");
if (oldUserEmail == NULL)
    {
    printf("First an existing user needs to sign in:");
    printf("<INPUT TYPE=BUTTON NAME=\"signIn\" VALUE=\"sign in\" id=\"signin\">");
    }
else if (cgiVarExists("newUser"))
    {
    char *newUser = trimSpaces(cgiString("newUser"));
    edwCreateNewUser(newUser);
    printf("ENCODE Data Warehouse account for %s created.<BR>\n", newUser);
    printf("Note %s will need to create a Persona account as well if they don't have one. ", 
	newUser);
    printf("They will be led through the process when they sign in.<BR>");
    printf("<INPUT TYPE=BUTTON NAME=\"signOut\" VALUE=\"sign out %s\" id=\"signout\">", 
	oldUserEmail);
    printf(" ");
    cgiMakeButton("submit", "Add another user");
    }
else
    {
    printf("%s is authorized to create a new user<BR>\n", oldUserEmail);
    printf("Email of new user:\n");
    cgiMakeTextVar("newUser", NULL, 40);
    cgiMakeSubmitButton();
    }
printf("</FORM>\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (!cgiIsOnWeb())
    errAbort("edwWebCreateUser is a cgi script not meant to be run from command line.\n");
oldUserEmail = findCookieData("email");
edwWebHeaderWithPersona("ENCODE Data Warehouse Create User");
htmEmptyShell(doMiddle, NULL);
edwWebFooterWithPersona();
return 0;
}

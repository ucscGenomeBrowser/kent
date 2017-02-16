/* edwWebDeprecate - A cgi script to mark files as deprecated.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmshell.h"
#include "errAbort.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

char *userEmail;	/* Filled in by authentication system. */
char *fileList = "";
char *reason = "";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwWebDeprecate - A cgi script to mark files as deprecated.\n"
  "usage:\n"
  "   edwWebDeprecate cgiVar=value cgiVar2=value2\n"
  );
}

boolean okToShowAllowBox(struct sqlConnection *conn, char *userEmail)
{
return edwUserIsAdmin(conn,userEmail);
};

boolean checkOwnership(struct sqlConnection *conn, int fId, char *userEmail)
/* Return true if file to be deprecated was submitted by this user. */
{
char *email = edwFindOwnerNameFromFileId(conn, fId);
if (sameString(email, userEmail))
    return TRUE;
else
    return FALSE;
}

boolean okToDeprecateThisFile(struct sqlConnection *conn, int fId, char *userEmail)
/* Return true if it is OK to deprecate this file */
{
if (checkOwnership(conn, fId, userEmail))
    return TRUE;
else if (cgiVarExists("allowBox"))
    return TRUE;
else
    return FALSE;
}

void logIn()
/* Put up name.  No password for now. */
{
printf("Deprecate files in ENCODE Data Warehouse.<BR>");
printf("Please sign in via Persona.");
printf("<INPUT TYPE=BUTTON NAME=\"signIn\" VALUE=\"sign in\" id=\"signin\">");
}

void getFileListAndReason(struct sqlConnection *conn)
/* Put up URL. */
{
edwMustGetUserFromEmail(conn, userEmail);
printf("Please enter in accessions (ENCFF identifiers) for files to deprecate:<BR>");
cgiMakeTextArea("fileList", fileList, 4, 60);
printf("<BR>");
printf("Please enter in reason for deprecating files:<BR>");
cgiMakeTextArea("reason", reason, 4, 60);
printf("<BR>");
if (okToShowAllowBox(conn, userEmail))
    {
    printf("Allow me to deprecate files not uploaded by me:  ");
    cgiMakeCheckBox("allowBox", FALSE);
    printf("<BR>");
    }
cgiMakeButton("submit", "submit");
}

static void localWarn(char *format, va_list args)
/* A little warning handler to override the one with the button that goes nowhere. */
{
printf("<FONT color='red'>");
vfprintf(stdout, format, args);
printf("</FONT><BR>\n");
}

void deprecateFileList(struct sqlConnection *conn, struct slInt *idList, char *reason)
/* Mark list of files defined by ID list as deprecated for given reason */
{
struct dyString *dy = dyStringNew(0);
struct slInt *id;
for (id = idList; id != NULL; id = id->next)
    {
    dyStringClear(dy);
    sqlDyStringPrintf(dy, "update edwFile set deprecated='%s' where id=%d", reason, id->val);
    sqlUpdate(conn, dy->string);
    }
dyStringFree(&dy);
}

void tryToDeprecate(struct sqlConnection *conn)
/* CGI variables are set - if possible deprecate, otherwise put up error message. */
{
pushWarnHandler(localWarn);
fileList = cgiString("fileList");
reason = cloneString(trimSpaces(cgiString("reason")));
if (isEmpty(reason))
   {
   warn("Please enter a reason for deprecation.");
   getFileListAndReason(conn);
   }
else
   {
   /* Go through list of accessions and make sure they are all well formed and correspond to files that exist. */
   boolean ok = TRUE;
   struct slName *accList = slNameListOfUniqueWords(cloneString(fileList), FALSE);
   struct slName *acc;
   struct slInt *idList = NULL, *idEl;
   for (acc = accList; acc != NULL; acc = acc->next)
       {
       char *licensePlate = acc->name;
       char *prefix = edwLicensePlateHead(conn);
       if (!startsWith(prefix, licensePlate))
           {
	   ok = FALSE;
	   warn("%s is not an accession, doesn't start with %s", licensePlate, prefix);
	   break;
	   }
	char query[256];
	sqlSafef(query, sizeof(query), "select fileId from edwValidFile where licensePlate='%s'", licensePlate);
	int id = sqlQuickNum(conn, query);
	if (id == 0)
	   {
	   ok = FALSE;
	   warn("%s - no such accession. ", licensePlate);
	   break;
	   }
	/* check to see is it ok tor deprecate this file */
	if (!okToDeprecateThisFile(conn, id, userEmail))
	    {
	    ok = FALSE;
	    warn("You can not deprecate %s which was originally uploaded by %s.\n",
	    licensePlate, edwFindOwnerNameFromFileId(conn, id));
	    if (okToShowAllowBox(conn, userEmail))
		warn("Please click the check box below to override this rule.");
	    break;
	    }

	idEl = slIntNew(id);
	slAddTail(&idList, idEl);
	}

    if (accList == NULL)
        {
	warn("Please enter some file accessions");
	ok = FALSE;
	}

    /* If a problem then put up page to try again,   otherwise do deprecation. */
    if (!ok)
        getFileListAndReason(conn);
    else
        {
	deprecateFileList(conn, idList, reason);
	printf("Deprecated %d files<BR>\n", slCount(idList));
	cgiMakeButton("submit", "Deprecate More Files");
	printf(" ");
	edwPrintLogOutButton();
	}
    }
}


void doMiddle()
/* doMiddle - put up middle part of web page, not including http and html headers/footers */
{
printf("<FORM ACTION=\"../cgi-bin/edwWebDeprecate\" METHOD=GET>\n");
struct sqlConnection *conn = edwConnectReadWrite(edwDatabase);
userEmail = edwGetEmailAndVerify();
if (userEmail == NULL)
    logIn();
else if (cgiVarExists("fileList") && cgiVarExists("reason"))
    tryToDeprecate(conn);
else
    getFileListAndReason(conn);
printf("</FORM>");
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean isFromWeb = cgiIsOnWeb();
if (!isFromWeb && !cgiSpoof(&argc, argv))
    usage();

/* Put out HTTP header and HTML HEADER all the way through <BODY> */
edwWebHeaderWithPersona("Deprecate files in ENCODE Data Warehouse");

/* Call error handling wrapper that catches us so we write /BODY and /HTML to close up page
 * even through an errAbort. */
htmEmptyShell(doMiddle, NULL);

edwWebFooterWithPersona();
return 0;
}

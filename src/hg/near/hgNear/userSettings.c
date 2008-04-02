/* userSettings - pages and control sets for loading/saving
 * user settings. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cart.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: userSettings.c,v 1.9 2008/03/26 00:36:57 galt Exp $";

static char *catAndClone(char *a, char *b)
/* Return concatenation of a and b in dynamic memory. */
{
int aLen = strlen(a);
int bLen = strlen(b);
char *buf = needMem(aLen + bLen + 1);
memcpy(buf, a, aLen);
memcpy(buf+aLen, b, bLen);
return buf;
}

static char *settingsVarName(char *prefix, char *label)
/* Given user readable label construct var name */
{
char *symName = cloneString(label);
char *varName;
spaceToUnderbar(symName);
varName = catAndClone(prefix, label);
freez(&symName);
return varName;
}

static char *settingsLabel(struct userSettings *us, char *varName)
/* Given varName return corresponding label. */
{
char *spacedString = cloneString(varName + strlen(us->savePrefix));
subChar(spacedString, '_', ' ');
return spacedString;
}

struct userSettings *userSettingsNew(
	struct cart *cart,	/* Persistent variable cart. */
	char *formTitle,	/* Heading of settings save form. */
	char *formVar,		/* Name of button variable on save form. */
	char *localVarPrefix)   /* Prefix to use for internal cart variables. 
		                 * No periods allowed because of javascript. */
/* Make new object to help manage sets of user settings. */
{
struct userSettings *us;
AllocVar(us);
us->cart = cart;
us->formTitle = cloneString(formTitle);
us->formVar = cloneString(formVar);
us->savePrefix = catAndClone(localVarPrefix, "named_");
us->nameVar = catAndClone(localVarPrefix, "name");
us->listDisplayVar = catAndClone(localVarPrefix, "displayList");
return us;
}

void userSettingsCaptureVar(struct userSettings *us, char *varName)
/* Add a single variable to list of variables to capture. */
{
struct slName *n = slNameNew(varName);
slAddHead(&us->saveList, n);
}

void userSettingsCapturePrefix(struct userSettings *us, char *prefix)
/* Capture all variables that start with prefix. */
{
struct hashEl *el, *list = cartFindPrefix(us->cart, prefix);
for (el = list; el != NULL; el = el->next)
     {
    struct slName *n = slNameNew(el->name);
    slAddHead(&us->saveList, n);
    }
slFreeList(&list);
}

boolean userSettingsAnySaved(struct userSettings *us)
/* Return TRUE if any user settings are saved. */
{
struct hashEl *list = cartFindPrefix(us->cart, us->savePrefix);
boolean any = (list != NULL);
slFreeList(&list);
return any;
}

void userSettingsUseNamed(struct userSettings *us, char *setName)
/* Use named collection of settings. */
{
struct cart *cart = us->cart;
char *varName = settingsVarName(us->savePrefix, setName);
char *settings = cartOptionalString(cart, varName);
if (settings != NULL)
    {
    struct hash *hash = hashVarLine(settings, 1);
    struct hashEl *list = hashElListHash(hash);
    struct hashEl *el;
    for (el = list; el != NULL; el = el->next)
	cartSetString(cart, el->name, el->val);
    slFreeList(&list);
    hashFree(&hash);
    }
freez(&varName);
}

void userSettingsUseSelected(struct userSettings *us)
/* Use currently selected user settings. */
{
char *setName = cartOptionalString(us->cart, us->nameVar);
if (setName != NULL)
    {
    userSettingsUseNamed(us, setName);
    }
}

static void printLabelList(struct userSettings *us, struct hashEl *list)
/* Print list of available settings as options. */
{
struct hashEl *el;
char *curSetting = cartUsualString(us->cart, us->nameVar, "");
for (el = list; el != NULL; el = el->next)
    {
    char *label = settingsLabel(us, el->name);
    hPrintf("<OPTION%s VALUE=\"%s\">%s</OPTION>\n", 
	    (sameString(curSetting, label) ? " SELECTED" : ""),
	    label, label);
    freez(&label);
    }
}

void userSettingsSaveForm(struct userSettings *us)
/* Put up controls that let user name and save the current
 * set. */
{
struct hashEl *list = cartFindPrefix(us->cart, us->savePrefix);

/* Start form/save session/print title. */
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" NAME=\"usForm\" METHOD=GET>\n");
cartSaveSession(us->cart);
hPrintf("<H2>Save %s</H2>\n", us->formTitle);

/* Put up controls that are always there. */
hPrintf("Please name this setup:\n");
cartMakeTextVar(us->cart, us->nameVar, "", 16);
hPrintf(" ");
cgiMakeButton(us->formVar, "save");
hPrintf(" ");
cgiMakeButton(us->formVar, "cancel");

/* Put up additional controls if have saved settings already. */
if (list != NULL)
    {
    struct dyString *js = newDyString(0);

    htmlHorizontalLine();
    slSort(&list, hashElCmp);
    hPrintf("Existing Setups:");
    dyStringPrintf(js, "document.usForm.%s.value=", us->nameVar);
    dyStringPrintf(js, "document.usForm.%s.options", us->listDisplayVar);
    dyStringPrintf(js, "[document.usForm.%s.selectedIndex].value;", us->listDisplayVar);

    hPrintf("<SELECT NAME=\"%s\" SIZE=%d onchange=\"%s\">",
    	us->listDisplayVar, slCount(list), js->string);
    printLabelList(us, list);
    hPrintf("</SELECT>\n");

    cgiMakeButton(us->formVar, "delete existing setup");
    }

/* Cleanup. */
hPrintf("</FORM>\n");
slFreeList(&list);
}

void userSettingsLoadForm(struct userSettings *us)
/* Put up controls that let user name and save the current
 * set. */
{
struct hashEl *list = cartFindPrefix(us->cart, us->savePrefix);

/* Start form/save session/print title. */
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" NAME=\"usForm\" METHOD=GET>\n");
cartSaveSession(us->cart);
hPrintf("<H2>Load %s</H2>\n", us->formTitle);

hPrintf("<TABLE><TR><TD>\n");
hPrintf("<SELECT NAME=\"%s\" SIZE=%d>",
    us->nameVar, slCount(list));
printLabelList(us, list);
hPrintf("</SELECT>");
hPrintf("</TD><TD>");
cgiMakeButton(us->formVar, "load");
hPrintf(" ");
cgiMakeButton(us->formVar, "cancel");
hPrintf("</TD></TR></TABLE>");

/* Cleanup. */
hPrintf("</FORM>\n");
slFreeList(&list);
}


static void dyStringAppendQuoted(struct dyString *dy, char *s)
/* Append s to dyString, surrounding with quotes and escaping
 * internal quotes. */
{
char c;
dyStringAppendC(dy, '"');
while ((c = *s++) != 0)
    {
    if (c == '"')
       dyStringAppendC(dy, '\\');
    dyStringAppendC(dy, c);
    }
dyStringAppendC(dy, '"');
dyStringAppendC(dy, ' ');
}

static void saveSettings(struct userSettings *us, char *varName)
/* Save captured settings to varName. */
{
struct cart *cart = us->cart;
struct slName *capture;
struct dyString *dy = dyStringNew(4*1024);
for (capture = us->saveList; capture != NULL; capture = capture->next)
    {
    char *name = capture->name;
    char *val = cartOptionalString(cart, name);
    if (val != NULL)
        {
	dyStringPrintf(dy, "%s=", name);
	dyStringAppendQuoted(dy, val);
	}
    }
cartSetString(cart, varName, dy->string);
dyStringFree(&dy);
}

boolean userSettingsProcessForm(struct userSettings *us)
/* Handle button press in userSettings form. 
 * If this returns TRUE then form is finished processing 
 * and you can call something to make the next page. */
{
struct cart *cart = us->cart;
char *command = cartString(cart, us->formVar);
boolean retVal = TRUE;
char *name = cartNonemptyString(cart, us->nameVar);

command = trimSpaces(command);
if (sameWord(command, "save") && name != NULL)
    {
    char *varName = settingsVarName(us->savePrefix, name);
    saveSettings(us, varName);
    freez(&varName);
    }
else if (startsWith(command, "load") && name != NULL)
    {
    userSettingsUseSelected(us);
    }
else if (startsWith("delete", command))
    {
    char *which = cartOptionalString(cart, us->listDisplayVar);
    if (which != NULL)
        {
	char *varName = settingsVarName(us->savePrefix, which);
	cartRemove(cart, varName);
	userSettingsSaveForm(us);
	retVal = FALSE;
	}
    }
cartRemove(cart, us->formVar);
return retVal;
}

void userSettingsDropDown(struct userSettings *us)
/* Display list of available saved settings . */
{
struct hashEl *list = cartFindPrefix(us->cart, us->savePrefix);
if (list != NULL)
    {
    hPrintf("<SELECT NAME=\"%s\">\n", us->nameVar);
    slSort(&list, hashElCmp);
    printLabelList(us, list);
    slFreeList(&list);
    hPrintf("</SELECT>");
    }
}


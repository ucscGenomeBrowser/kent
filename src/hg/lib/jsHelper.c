// jsHelper.c - helper routines for interface between CGIs and client-side javascript

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include <regex.h>
#include "dystring.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hPrint.h"
#include "hash.h"
#include "jsHelper.h"
#include "web.h"
#include "hui.h"
#include "hgConfig.h"
#include "portable.h"

static boolean jsInited = FALSE;

/* mainForm/hiddenForm code supports the following: when the user selects
 * something from a pull-down, it will go hit the server to
 * figure out how to reload other control options based on the choice.
 * (For instance if they change the group, which items in the track
 * drop-down need to change).
 *
 * We accomplish this by maintaining two forms - a mainForm and a
 * hiddenForm.  The hiddenForm maintains echo's of all the variables
 * in the main form, which get updated onChange of controls that need
 * to 'ripple' to other controls.  The onChange also submits the
 * control. */

void jsInit()
/* If this is the first call, set window.onload to the operations
 * performed upon loading a page and print supporting javascript.
 * Currently this just sets the page vertical position if specified on
 * CGI, and includes jsHelper.js.
 * Subsequent calls do nothing, so this can be called many times. */
{
if (! jsInited)
    {
    // jsh_pageVertPos trick taken from
    // http://www.softcomplex.com/docs/get_window_size_and_scrollbar_position.html
    puts("<INPUT TYPE=HIDDEN NAME=\"jsh_pageVertPos\" VALUE=0>");
    int pos = cgiOptionalInt("jsh_pageVertPos", 0);
    if (pos > 0)
	{
	jsInlineF("window.onload = function () { window.scrollTo(0, %d); }\n", pos);
	}
    jsInited = TRUE;
    jsIncludeFile("jsHelper.js", NULL);
    }
}

struct dyString *jsOnChangeStart()
/* Start up an onChange string */
{
struct dyString *dy = dyStringNew(1024);
return dy;
}

char *jsOnChangeEnd(struct dyString **pDy)
/* Finish up javascript onChange command. */
{
dyStringAppend(*pDy, "document.hiddenForm.submit();");
return dyStringCannibalize(pDy);
}

void jsDropDownCarryOver(struct dyString *dy, char *var)
/* Add statement to carry-over drop-down item to dy. */
{
dyStringPrintf(dy, "document.hiddenForm.%s.value=", var);
dyStringPrintf(dy, "document.mainForm.%s.options", var);
dyStringPrintf(dy, "[document.mainForm.%s.selectedIndex].value; ", var);
}

void jsTextCarryOver(struct dyString *dy, char *var)
/* Add statement to carry-over text item to dy. */
{
dyStringPrintf(dy,
    "document.hiddenForm.%s.value=document.mainForm.%s.value; ",
    var, var);
}

void jsTrackingVar(char *jsVar, char *val)
/* Emit a little Javascript to keep track of a variable.
 * This helps especially with radio buttons. */
{
jsInlineF("var %s='%s';\n", jsVar, val);
}

void jsMakeTrackingRadioButtonExtraHtml(char *cgiVar, char *jsVar,
                                        char *val, char *selVal, char *extraHtml)
/* Make a radio button with extra HTML attributes that also sets tracking variable
 * in javascript. */
{
char id[256];
safef(id, sizeof id, "%s_%s", cgiVar, val);
hPrintf("<INPUT TYPE=RADIO NAME='%s' ID='%s'", cgiVar, id);
hPrintf(" VALUE=\"%s\"", val);
if (isNotEmpty(extraHtml))
    hPrintf(" %s", extraHtml);
jsOnEventByIdF("click", id, "%s='%s';", jsVar, val);
if (sameString(val, selVal))
    hPrintf(" CHECKED");
hPrintf(">");
}

void jsMakeTrackingRadioButton(char *cgiVar, char *jsVar,
	char *val, char *selVal)
/* Make a radio button that also sets tracking variable
 * in javascript. */
{
jsMakeTrackingRadioButtonExtraHtml(cgiVar, jsVar, val, selVal, NULL);
}

void jsMakeTrackingCheckBox(struct cart *cart,
	char *cgiVar, char *jsVar, boolean usualVal)
/* Make a check box filling in with existing value and
 * putting a javascript tracking variable on it. */
{
char buf[256];
boolean oldVal = cartUsualBoolean(cart, cgiVar, usualVal);
jsInlineF("var %s=%d;\n", jsVar, oldVal);
hPrintf("<INPUT TYPE=CHECKBOX NAME='%s' ID='%s' VALUE=1", cgiVar, cgiVar);
if (oldVal)
    hPrintf(" CHECKED");
hPrintf(">");
jsOnEventByIdF("click", cgiVar, "%s=(%s+1)%%2;", jsVar, jsVar);
safef(buf, sizeof(buf), "%s%s", cgiBooleanShadowPrefix(), cgiVar);
cgiMakeHiddenVar(buf, "0");
}

void jsTrackedVarCarryOver(struct dyString *dy, char *cgiVar, char *jsVar)
/* Carry over tracked variable (radio button?) to hidden form. */
{
dyStringPrintf(dy, "document.hiddenForm.%s.value=%s; ", cgiVar, jsVar);
}

char *jsRadioUpdate(char *cgiVar, char *jsVar, char *val)
/* Make a little javascript to check and uncheck radio buttons
 * according to new value.  To use this you must have called
 * jsInit somewhere, and also must use jsMakeTrackingRadioButton
 * to make the buttons. */
{
static char buf[256];
safef(buf, sizeof(buf),
    "setRadioCheck('%s', '%s'); %s='%s'", cgiVar, val, jsVar, val);
return buf;
}

void jsCreateHiddenForm(struct cart *cart, char *scriptName,
	char **vars, int varCount)
/* Create a hidden form with the given variables */
{
int i;
hPrintf(
    "<FORM ACTION=\"%s\" "
    "METHOD=\"GET\" NAME=\"hiddenForm\">\n", scriptName);
cartSaveSession(cart);
for (i=0; i<varCount; ++i)
    hPrintf("<input type=\"hidden\" name=\"%s\" value=\"\">\n", vars[i]);
puts("</FORM>");
}

char *jsSetVerticalPosition(char *form)
/* Returns a javascript statement for storing the vertical position of the
 * page; typically this would go just before a document submit.
 * jsInit must be called first.
 * Do not free return value!   */
{
if (! jsInited)
    errAbort("jsSetVerticalPosition: jsInit must be called first.");
static char vertPosSet[2048];
//TODO XSS filter
safef(vertPosSet, sizeof(vertPosSet),
      "document.%s.jsh_pageVertPos.value = f_scrollTop(); ", form);
return vertPosSet;
}

void jsMakeCheckboxGroupSetClearButton(char *buttonVar, boolean isSet)
/* Make a button for setting or clearing a set of checkboxes with the same name.
 * Uses only javascript to change the checkboxes, no resubmit. */
{
char id[256];
char javascript[256];
safef(javascript, sizeof(javascript), "var list = document.getElementsByName('%s'); "
      "for (var ix = 0; ix < list.length; ix++) {list[ix].checked = %s}", buttonVar,
      isSet ? "true" : "false");
safef(id, sizeof id, "%s_grp%sBut", buttonVar, isSet ? "Set" : "Clr");
cgiMakeOnClickButton(id, javascript, isSet ? JS_SET_ALL_BUTTON_LABEL : JS_CLEAR_ALL_BUTTON_LABEL);
}

void jsMakeSetClearContainer()
/* Begin a wrapper div with class setClearContainer, plus 'Set all' and 'Clear all' buttons.
 * This should be followed by a bunch of checkboxes, and then a call to jsEndContainer. */
{
puts("<div class=\"setClearContainer\">\n"
     "<input type=button value=\""JS_SET_ALL_BUTTON_LABEL"\" title=\"Check all checkboxes\">\n"
     "<input type=button value=\""JS_CLEAR_ALL_BUTTON_LABEL"\" title=\"Uncheck all checkboxes\">\n"
     "<br>"
     );
}

void jsEndContainer()
/* End a wrapper div. */
{
puts("</div>");
}

char *jsPressOnEnter(char *button)
/* Returns a javascript statement that clicks button when the Enter key
 * has been pressed; typically this would go in a text input.
 * jsInit must be called first.
 * Do not free return value!  */
{
if (! jsInited)
    errAbort("jsPressOnEnter: jsInit must be called first.");
static char poe[2048];
safef(poe, sizeof(poe), "return pressOnEnter(event, %s);", button);
return poe;
}

void jsIncludeFile(char *fileName, char *noScriptMsg)
{
/* Prints out html to include given javascript file from the js directory; suppresses redundant
 *  <script ...> tags if called repeatedly.
 * <noscript>...</noscript> tags are provided automatically. The noscript message may be specified via
 * the noScriptMsg parameter (the string may contain HTML markup). A default msg is provided
 * if noScriptMsg == NULL; noscript msg is suppressed if noScriptMsg == "" (this is useful
 * if you want to more carefully control where the message will appear on the page). */
char *link = webTimeStampedLinkToResourceOnFirstCall(fileName,TRUE);
if (link != NULL)
    {
    static boolean defaultWarningShown = FALSE;
    if(noScriptMsg == NULL && !defaultWarningShown)
        {
        noScriptMsg = "<b>JavaScript is disabled in your web browser</b></p><p>You must have JavaScript enabled in your web browser to use the Genome Browser";
        defaultWarningShown = TRUE;
        }
    if(noScriptMsg && strlen(noScriptMsg))
        hPrintf("<noscript><div class='noscript'><div class='noscript-inner'><p>%s</p></div></div></noscript>\n", noScriptMsg);
    hPrintf("%s",link);
    freeMem(link);
    }
}

void jsIncludeReactLibs()
/* Prints out <script src="..."> tags for external libraries including ReactJS & ImmutableJS
 * and our own model libs, React mixins and components. */
{
// We need a module system... webpack?
jsIncludeFile("es5-shim.4.0.3.min.js", NULL);
jsIncludeFile("es5-sham.4.0.3.min.js", NULL);
jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
puts("<script src=\"//code.jquery.com/jquery-1.9.1.min.js\"></script>");
puts("<script src=\"//code.jquery.com/ui/1.10.3/jquery-ui.min.js\"></script>");
jsIncludeFile("react-with-addons-0.12.2.min.js", NULL);
jsIncludeFile("immutable.3.7.4.min.js", NULL);
jsIncludeFile("jquery.bifrost.1.0.1.min.js", NULL);
jsIncludeFile("BackboneExtend.js", NULL);
jsIncludeFile("cart.js", NULL);
jsIncludeFile("ImModel.js", NULL);
jsIncludeFile("CladeOrgDbMixin.js", NULL);
jsIncludeFile("PositionSearchMixin.js", NULL);
jsIncludeFile("UserRegionsMixin.js", NULL);
jsIncludeFile("PathUpdate.js", NULL);
jsIncludeFile("PathUpdateOptional.js", NULL);
jsIncludeFile("ImmutableUpdate.js", NULL);
jsIncludeFile("reactLibBundle.js", NULL);
}

void jsIncludeDataTablesLibs()
/* Prints out <script src="..."> tags for external libraries: jQuery 1.12.3, the jQuery DataTables
 * plugin (version 1.10.12), and the accompanying standard CSS file for DataTables. */
{
puts("<link rel=\"stylesheet\" type=\"text/css\" "
    "href=\"https://cdn.datatables.net/1.10.12/css/jquery.dataTables.min.css\">\n");
puts("<script type=\"text/javascript\" "
    "src=\"https://code.jquery.com/jquery-1.12.3.min.js\"\"></script>");
puts("<script type=\"text/javascript\" "
    "src=\"https://cdn.datatables.net/1.10.12/js/jquery.dataTables.min.js\"\"></script>");
}

char *jsDataTableStateSave (char *cartPrefix)
/* Prints out a javascript function to save the state of a DataTables jQuery plugin-enabled
 * table to the cart, using the specified cart prefix to help name the variable. */
{
static char saveFunction[4096];
safef(saveFunction, sizeof(saveFunction),
    "function saveTableState (settings, data) { "
    "var cartVarName  = \"%s\".concat(\"%s\"); "
    "var stateString = JSON.stringify(data); "
    "setCartVar(cartVarName,  encodeURIComponent(stateString)); "
    "}"
    , cartPrefix, dataTableStateName);
return saveFunction;
}

char *jsDataTableStateLoad (char *cartPrefix, struct cart *cart)
/* Prints out a javascript function to load the state of a DataTables jQuery plugin-enabled
 * table from the cart variable whose prefix is specified in the first argument */
{
    char *stateVariable = catTwoStrings(cartPrefix, "DataTableState");
    char *stateString = cartUsualString(cart, stateVariable, "{}");
    static char loadFunction [4096];
    safef(loadFunction, sizeof(loadFunction),
        "function loadTableState (settings) { "
        "var stateString = decodeURIComponent(\"%s\"); "
        "var data = JSON.parse(stateString); "
        "return data; "
        "}"
        , stateString);
    return loadFunction;
}

char *jsCheckAllOnClickHandler(char *idPrefix, boolean state)
/* Returns javascript for use as an onclick attribute value to check all/uncheck all
 * all checkboxes with given idPrefix.
 * state parameter determines whether to "check all" or "uncheck all" (TRUE means "check all"). */
{
static char buf[512];
jsIncludeFile("utils.js", NULL);
safef(buf, sizeof(buf), "setCheckBoxesWithPrefix(this, '%s', %s); return false", idPrefix, state ? "true" : "false");
return buf;
}

/* cgiMakeCheckAllSubmitButton really belongs in cheapcgi.c, but that is compiled without access to jsHelper.h */

void cgiMakeCheckAllSubmitButton(char *name, char *value, char *id, char *idPrefix, boolean state)
/* Make submit button which uses javascript to apply check all or uncheck all to all
 * checkboxes with given idPrefix.
 * state parameter determines whether to "check all" or "uncheck all" (TRUE means "check all").
 * id parameter may be NULL */
{
cgiMakeOnClickSubmitButton(jsCheckAllOnClickHandler(idPrefix, state), name, value);
}

char *stripRegEx(char *str, char *regEx, int flags)
{
/* Strip out text matching regEx from str.
   flags is passed through to regcomp as the cflags argument.
   Returned string should be free'ed after use. */
return replaceRegEx(str, NULL, regEx, flags);
}

char *replaceRegEx(char *str, char *replace, char *regEx, int flags)
{
/* Replace text matching regEx in str with replace string.
   flags is passed through to regcomp as the cflags argument.
   Returned string should be free'ed after use. */
regex_t re;
regmatch_t match[1];
int err = regcomp(&re, regEx, flags);
if(err)
    errAbort("regcomp failed; err: %d", err);
struct dyString *dy = newDyString(0);
size_t len = strlen(str);
size_t offset = 0;
while(offset < len && !regexec(&re, str + offset, 1, match, 0))
    {
    dyStringAppendN(dy, str + offset, match[0].rm_so);
    if(replace != NULL)
        dyStringAppend(dy, replace);
    offset += match[0].rm_eo;
    }
if(offset < len)
    {
    dyStringAppend(dy, str + offset);
    }
regfree(&re);
return dyStringCannibalize(&dy);
}

char *jsStripJavascript(char *str)
/* Strip out anything that looks like javascript in html string.
   This function is designed to cleanup user input (e.g. to avoid XSS attacks).
   In reality, we cannot remove javascript with 100% accuracy, b/c there are many browser
   specific ways of embedding javascript; see http://ha.ckers.org/xss.html for many, many examples.
   Returned string should be free'ed after use. */
{
char *regExs[] = {"<script\\s*>[^<]*</script\\s*>",
                   "<script[^>]*>" // handles case where they have an un-closed script tag with a src attribute
			};
int i;
str = cloneString(str);
for(i=0;i<ArraySize(regExs);i++)
    {
    char *tmp = str;
    str = stripRegEx(str, regExs[i], REG_ICASE);
    freeMem(tmp);
    }
return str;
}

static void jsBeginCollapsibleSectionFull(struct cart *cart, char *track, char *section,
                                   char *sectionTitle, boolean isOpenDefault, char *fontSize,
                                   boolean oldStyle)
/* Make the hidden input, collapse/expand button and <TR id=...> needed for utils.js's
 * setTableRowVisibility().  Caller needs to have already created a <TABLE> and <FORM>. 
 * With support for style variation. "oldStyle" has blue background color in section header
 * (as in web.c sections). "fontSize" is ignored if oldStyle is TRUE */
{
char collapseGroupVar[512];
safef(collapseGroupVar, sizeof(collapseGroupVar), "%s.section_%s_close", track, section);
boolean isOpen = !cartUsualBoolean(cart, collapseGroupVar, !isOpenDefault);

// Both plus button and title are now in same <TD>
// but still colspan=2 because we are lib code and callers own the table.
puts("<TR");
if (oldStyle)
    puts(" class='subheadingBar'");
puts (">");
printf("<TD colspan=2 style='text-align:left;'>\n");
printf("<input type='hidden' name='%s' id='%s' value='%s'>\n",
       collapseGroupVar, collapseGroupVar, isOpen ? "0" : "1");
char *buttonImage = (isOpen ? "../images/remove_sm.gif" : "../images/add_sm.gif");
char id[256];
safef(id, sizeof id, "%s_button", section);
printf("<IMG height='18' width='18' "
       "id='%s' src='%s' alt='%s' title='%s this section' class='bigBlue'"
       " style='cursor:pointer;'>\n",
       id, buttonImage, (isOpen ? "-" : "+"), (isOpen ? "Collapse": "Expand"));
jsOnEventByIdF("click", id, "return setTableRowVisibility(this, '%s', '%s.section', 'section', true);", 
       section, track);
if (oldStyle || fontSize == NULL)
    printf("&nbsp;%s</TD></TR>\n", sectionTitle);
else
    printf("<B style='font-size:%s;'>&nbsp;%s</B>", fontSize, sectionTitle);
puts("</TD></TR>\n");
printf("<TR %s id='%s-%d'><TD colspan=2>", isOpen ? "" : "style='display: none' ", section, 1);
}

void jsBeginCollapsibleSectionOldStyle(struct cart *cart, char *track, char *section,
				       char *sectionTitle, boolean isOpenDefault)
/* Make the hidden input, collapse/expand button and <TR id=...> needed for utils.js's
 * setTableRowVisibility().  Caller needs to have already created a <TABLE> and <FORM>. 
 * With support for varying font size */
{
jsBeginCollapsibleSectionFull(cart, track, section, sectionTitle, isOpenDefault, NULL, TRUE);
}

void jsBeginCollapsibleSectionFontSize(struct cart *cart, char *track, char *section,
				       char *sectionTitle, boolean isOpenDefault, char *fontSize)
/* Make the hidden input, collapse/expand button and <TR id=...> needed for utils.js's
 * setTableRowVisibility().  Caller needs to have already created a <TABLE> and <FORM>. 
 * With support for varying font size */
{
jsBeginCollapsibleSectionFull(cart, track, section, sectionTitle, isOpenDefault, fontSize, FALSE);
}

void jsBeginCollapsibleSection(struct cart *cart, char *track, char *section, char *sectionTitle,
			       boolean isOpenDefault)
/* Make the hidden input, collapse/expand button and <TR id=...> needed for utils.js's
 * setTableRowVisibility().  Caller needs to have already created a <TABLE> and <FORM>. */
{
jsBeginCollapsibleSectionFull(cart, track, section, sectionTitle, isOpenDefault, "larger", FALSE);
}

void jsEndCollapsibleSection()
/* End the collapsible <TR id=...>. */
{
puts("</TD></TR>");
}

void jsReloadOnBackButton(struct cart *cart)
/* Add some javascript to detect that the back button (or reload) has been pressed,
 * and to resubmit in that case to redraw the page with the latest cart contents. */
// __detectback trick from
// http://siphon9.net/loune/2009/07/detecting-the-back-or-refresh-button-click/
// Yes, I know this along with every other inline <script> here belongs in a .js module
{
jsInlineF(
       "document.write(\"<form style='display: none'><input name='__detectback' id='__detectback' "
       "value=''></form>\");\n"
       "function checkPageBackOrRefresh() {\n"
       "  if (document.getElementById('__detectback').value) {\n"
       "    return true;\n"
       "  } else {\n"
       "    document.getElementById('__detectback').value = 'been here';\n"
       "    return false;\n"
       "  }\n"
       "}\n"
       "window.onload = function() { "
       "  if (checkPageBackOrRefresh()) { \n"
       "    if (window.location.search == '?%s') { \n"
	      // We already have the hgsid-only URL that we want, reload it.
	      // (necessary for IE because IE doesn't reload on replace,
	      //  unless window.location and/or window.search changes)
       "      window.location.reload(true);\n"
       "    } else { \n"
       "      window.location.replace('%s?%s');\n"
       "    } \n"
       "  } "
       "};\n"
       , cartSidUrlString(cart), cgiScriptName(), cartSidUrlString(cart));
}

static char *makeIndentBuf(int indentLevel)
{
if (indentLevel < 0)
    return "";
char *indentBuf;
indentBuf = needMem(indentLevel + 1);
memset(indentBuf, '\t', indentLevel);
indentBuf[indentLevel] = 0;
return indentBuf;
}

static void jsonDyStringPrintRecurse(struct dyString *dy, struct jsonElement *ele, int indentLevel)
{
if (indentLevel >= -1) // Note that < -1 will result in no indenting
    indentLevel++;
char *tab = "\t";
char *nl = "\n";
if (indentLevel < 0)
    {
    tab = "";
    nl = "";
    }
char *indentBuf = makeIndentBuf(indentLevel);
switch (ele->type)
    {
    case jsonObject:
        {
        dyStringPrintf(dy,"{%s",nl);
        if(hashNumEntries(ele->val.jeHash))
            {
            struct hashEl *el, *list = hashElListHash(ele->val.jeHash);
            slSort(&list, hashElCmp);
            for (el = list; el != NULL; el = el->next)
                {
                struct jsonElement *val = el->val;
                dyStringPrintf(dy,"%s%s\"%s\": ", indentBuf, tab, el->name);
                jsonDyStringPrintRecurse(dy, val, indentLevel);
                dyStringPrintf(dy,"%s%s", el->next == NULL ? "" : ",",nl);
                }
            hashElFreeList(&list);
            }
        dyStringPrintf(dy,"%s}", indentBuf);
        break;
        }
    case jsonList:
        {
        struct slRef *el;
        dyStringPrintf(dy,"[%s",nl);
        if(ele->val.jeList)
            {
            for (el = ele->val.jeList; el != NULL; el = el->next)
                {
                struct jsonElement *val = el->val;
                dyStringPrintf(dy,"%s%s", indentBuf,tab);
                jsonDyStringPrintRecurse(dy, val, indentLevel);
                dyStringPrintf(dy,"%s%s", el->next == NULL ? "" : ",",nl);
                }
            }
        dyStringPrintf(dy,"%s]", indentBuf);
        break;
        }
    case jsonString:
        {
        dyStringPrintf(dy,"\"%s\"", jsonStringEscape(ele->val.jeString));
        break;
        }
    case jsonBoolean:
        {
        dyStringPrintf(dy,"%s", ele->val.jeBoolean ? "true" : "false");
        break;
        }
    case jsonNumber:
        {
        char buf[256];
        safef(buf, sizeof(buf), "%ld", ele->val.jeNumber);
        dyStringPrintf(dy,"%s", buf);
        break;
        }
    case jsonDouble:
        {
        char buf[256];
        safef(buf, sizeof(buf), "%g", ele->val.jeDouble);
        dyStringPrintf(dy,"%s", buf);
        break;
        }
    default:
        {
        errAbort("jsonPrintRecurse; invalid type: %d", ele->type);
        break;
        }
    }
if (indentLevel >= 0)
    freez(&indentBuf);
}

void jsonDyStringPrint(struct dyString *dy, struct jsonElement *json, char *name, int indentLevel)
// dyStringPrint out a jsonElement, indentLevel -1 means no indenting
{

char *indentBuf = makeIndentBuf(indentLevel);
if(name != NULL)
    {
    if (indentLevel >= 0 )
        dyStringPrintf(dy, "// START %s\n%s", name, indentBuf);
    dyStringPrintf(dy, "var %s = ", name);
    }
jsonDyStringPrintRecurse(dy, json, (indentLevel - 1)); // will increment back to indentLevel
if(name != NULL)
    {
    dyStringPrintf(dy, "%s;\n", indentBuf);
    if (indentLevel >= 0 )
        dyStringPrintf(dy, "// END %s\n", name);
    }
if (indentLevel >= 0)
    freez(&indentBuf);
}

void jsonPrint(struct jsonElement *json, char *name, int indentLevel)
// print out a jsonElement, indentLevel -1 means no indenting
{
struct dyString *dy = dyStringNew(1024);
jsonDyStringPrint(dy, json, name, indentLevel);
hPrintf("%s", dy->string);
dyStringFree(&dy);
}

void jsonErrPrintf(struct dyString *ds, char *format, ...)
//  Printf a json error to a dyString for communicating with ajax code; format is:
//  {"error": error message here}
{
va_list args;
va_start(args, format);
dyStringPrintf(ds, "{\"error\": \"");
struct dyString *buf = newDyString(1000);
dyStringVaPrintf(buf, format, args);
dyStringAppend(ds, jsonStringEscape(dyStringCannibalize(&buf)));
dyStringPrintf(ds, "\"}");
va_end(args);
}


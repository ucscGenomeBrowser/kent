/* cheapcgi.h - turns variables passed from the web form into
 * something that C understands. 
 * 
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef CHEAPCGI_H
#define CHEAPCGI_H

#include "dystring.h"

#ifndef HASH_H
#include "hash.h"
#endif


//============ javascript inline-separation routines ===============

void jsInlineFinish();
/* finish outputting accumulated inline javascript */

void jsInline(char *javascript);
/* Add text to output file or memory structure */

void jsInlineF(char *format, ...)
/* Add javascript text to output file or memory structure */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

void jsOnEventById(char *event, char *idText, char *jsText);
/* Add js mapping for inline event */

void jsOnEventByIdF(char *event, char *idText, char *format, ...)
/* Add js mapping for inline event with printf formatting */
#if defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
;

void jsInlineReset();  
/* used by genomeSpace to repeatedly output multiple pages to stdout */

//============ END of javascript inline-separation routines ===============

#define COLOR_BG_DEFAULT         "#FFFEE8"
#define COLOR_BG_ALTDEFAULT      "#FFF9D2"
#define COLOR_BG_DEFAULT_DARKER  "#FCECC0"
#define COLOR_BG_DEFAULT_DARKEST "#EED5B7"
#define COLOR_BG_GHOST           "#EEEEEE"
#define COLOR_BG_PALE            "#F8F8F8"
#define COLOR_BG_HEADER_LTBLUE   "#D9E4F8"
#define COLOR_DARKGREEN          "#008800"
#define COLOR_LTGREEN            "#CCFFCC"
#define COLOR_DARKBLUE           "#000088"
#define COLOR_BLUE_BUTTON        "#91B3E6"
#define COLOR_DARKGREY           "#666666"
#define COLOR_LTGREY             "#CCCCCC"
#define COLOR_YELLOW             "#FFFF00"
#define COLOR_LTYELLOW           "#FFF380"
#define COLOR_WHITE              "#FFFFFF"
#define COLOR_RED                "#AA0000"
#define COLOR_TRACKLIST_LEVEL1   COLOR_BG_DEFAULT
#define COLOR_TRACKLIST_LEVEL2   COLOR_BG_ALTDEFAULT
#define COLOR_TRACKLIST_LEVEL3   COLOR_BG_DEFAULT_DARKER
#define COLOR_TRACKLIST_LEVEL4   COLOR_BG_DEFAULT_DARKEST

void initSigHandlers(boolean dumpStack);
/* set handler for various terminal signals for logging purposes.
 * if dumpStack is TRUE, attempt to dump the stack. */

struct cgiVar
/* Info on one cgi variable. */
    {
    struct cgiVar *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash. */
    char *val;  		/* Value - also not allocated here. */
    boolean saved;		/* True if saved. */
    };

struct cgiVar* cgiVarList();
/* return the list of cgiVar's */

struct cgiDictionary
/* Stuff to encapsulate parsed out CGI vars. */
    {
    struct cgiDictionary *next;	    /* Next in list if we have multiple */
    char *stringData;		    /* Where values if cgi-vars live. */
    struct hash *hash;		    /* Keyed by cgi-var name, value is cgiVar */
    struct cgiVar *list;	    /* List of all vars. */
    };

void cgiDictionaryFree(struct cgiDictionary **pD);
/* Free up resources associated with dictionary. */

void cgiDictionaryFreeList(struct cgiDictionary **pList);
/* Free up a whole list of cgiDictionaries */

struct cgiDictionary *cgiDictionaryFromEncodedString(char *encodedString);
/* Giving a this=that&this=that string,  return cgiDictionary parsed out from it. 
 * This does *not* destroy input like the lower level cgiParse functions do. */

char *findCookieData(char *varName);
/* Get the string associated with varName from the cookie string. */

void dumpCookieList();
/* Print out the cookie list. */

boolean cgiIsOnWeb();
/* Return TRUE if looks like we're being run as a CGI. */

char *cgiRequestMethod();
/* Return CGI REQUEST_METHOD (such as 'GET/POST/PUT/DELETE/HEAD') */

char *cgiRequestUri();
/* Return CGI REQUEST_URI */

char *cgiRequestContentLength();
/* Return HTTP REQUEST CONTENT_LENGTH if available*/

char *cgiScriptName();
/* Return name of script so libs can do context-sensitive stuff. */

char *cgiServerName();
/* Return name of server, better to use cgiServerNamePort() for
   actual URL construction */

char *cgiServerPort();
/* Return port number of server */

char *cgiServerNamePort();
/* Return name of server with port if different than 80 */

boolean cgiServerHttpsIsOn();
/* Return true if HTTPS is on */

char *cgiAppendSForHttps();
/* if running on https, add the letter s to the url protocol */

char *cgiRemoteAddr();
/* Return IP address of client (or "unknown"). */

char *cgiUserAgent();
/* Return remote user agent (HTTP_USER_AGENT) or NULL if remote user agent is not known */

enum browserType
/* How to look at a track. */
    {
    btUnknown=0, // Not yet known
    btOpera=1,   // Opera
    btIE=2,      // MS Internet Explorer
    btFF=3,      // Firefox
    btChrome=4,  // Google Chrome
    btSafari=5,  // Safari
    btOther=6    // Anything else
    };

enum osType
/* How to look at a track. */
    {
    osUnknown=0, // Not yet known
    osWindows=1, // The evil empire
    osLinux=2,   // Workhorse
    osMac=3,     // ashion or Religion
    osOther=4    // Anything else
    };

enum browserType cgiClientBrowser(char **browserQualifier, enum osType *clientOs, 
                                  char **clientOsQualifier);
/* These routines abort the html output if the input isn't
 * there or is misformatted. */
#define cgiBrowser() cgiClientBrowser(NULL,NULL,NULL)

char *cgiString(char *varName);
int cgiInt(char *varName);
double cgiDouble(char *varName);

boolean cgiBoolean(char *varName);
/* The cgiBoolean is a little problematic.  If the variable
 * is TRUE it exists, but if it is false it is simply not
 * defined.   cgiBoolean() thus returns FALSE if the CGI
 * variable doesn't exist or if it is set to FALSE.  To
 * work around this when need be use cgiBooleanDefined(),
 * which relies on the fact that when we define a boolean
 * variable we also define a hidden variable. */

boolean cgiBooleanDefined(char *name);
/* Return TRUE if boolean variable is defined (by
 * checking for shadow). */

char *cgiBooleanShadowPrefix();
/* Prefix for shadow variable set with boolean variables. */

void cgiMakeHiddenBoolean(char *name, boolean on);
/* Make hidden boolean variable. Also make a shadow hidden variable so we
 * can distinguish between variable not present and
 * variable set to false. */

char *cgiMultListShadowPrefix();
/* Prefix for shadow variable set with multi-select inputs. */

int cgiIntExp(char *varName);
/* Evaluate an integer expression in varName and
 * return value. */

char *cgiOptionalString(char *varName);
/* Return value of string if it exists in cgi environment, else NULL */

char *cgiUsualString(char *varName, char *usual);
/* Return value of string if it exists in cgi environment.
 * Otherwiser return 'usual' */

struct slName *cgiStringList(char *varName);
/* Find list of cgi variables with given name.  This
 * may be empty.  Free result with slFreeList(). */

int cgiOptionalInt(char *varName, int defaultVal);
/* This returns value of varName if it exists in cgi environment,
 * otherwise it returns defaultVal. */

double cgiOptionalDouble(char *varName, double defaultVal);
/* Returns double value. */

#define cgiUsualInt cgiOptionalInt
#define cgiUsualDouble cgiOptionalDouble

struct cgiChoice
/* Choice table */
    {
    char *name;
    int value;
    };

int cgiOneChoice(char *varName, struct cgiChoice *choices, int choiceSize);
/* Returns value associated with string variable in choice table. */

boolean cgiVarExists(char *varName);
/* Returns TRUE if the variable was passed in. */

void cgiBadVar(char *varName);
/* Complain about a variable that's not there. */

void cgiDecode(char *in, char *out, int inLength);
/* Decode from cgi pluses-for-spaces format to normal.
 * Out will be a little shorter than in typically. */

void cgiDecodeFull(char *in, char *out, int inLength);
/* Out will be a cgi-decoded version of in (no space from plus!).
 * Out will be a little shorter than in typically, and
 * can be the same buffer. */

char *cgiEncode(char *inString);
/* Return a cgi-encoded version of inString.
 * Alphanumerics kept as is, space translated to plus,
 * and all other characters translated to %hexVal.
 * You can free return value with freeMem(). */

char *cgiEncodeFull(char *inString);
/* Return a cgi-encoded version of inString (no + for space!).
 * Alphanumerics/./_ kept as is and all other characters translated to
 * %hexVal. */

void cgiEncodeHash(struct hash *hash, struct dyString *dy);
/* Put a cgi-encoding of a string valued hash into dy.  Tags are always
 * alphabetical to make it easier to compare if two hashes are same. */

void cgiMakeSubmitButtonMaybePressed(char *name, char *value, char *msg, 
                char *onClick, boolean pressed);
/* Make 'submit' type button, with optional messsage and onclick javascript, boolean pressed.
   Set styling to indicate whether button has been pressed (for buttons that change browser mode).
 */

void cgiMakeButtonWithMsg(char *name, char *value, char *msg);
/* Make 'submit' type button. Display msg on mouseover, if present*/

void cgiMakeButtonWithOnClick(char *name, char *value, char *msg, char *onClick);
/* Make 'submit' type button, with onclick javascript */

void cgiMakeButton(char *name, char *value);
/* Make 'submit' type button. */

void cgiMakeOnClickButton(char *id, char *command, char *value);
/* Make 'push' type button with client side onClick (java)script. */

void cgiMakeOnClickSubmitButton(char *command, char *name, char *value);
/* Make submit button with both variable name and value with client side
 * onClick (java)script. */

void cgiMakeOptionalButton(char *name, char *value, boolean disabled);
/* Make 'submit' type button that can be disabled. */

void cgiMakeRadioButton(char *name, char *value, boolean checked);
/* Make radio type button.  A group of radio buttons should have the
 * same name but different values.   The default selection should be
 * sent with checked on. */

void cgiMakeOnEventRadioButtonWithClass(char *name, char *value, boolean checked,
    char *class, char *event, char *command);
/* Make radio type button with an event and an optional class attribute.
 *  A group of radio buttons should have the
 * same name but different values.   The default selection should be
 * sent with checked on. If class is non-null it is included. */

void cgiMakeCheckBoxUtil(char *name, boolean checked, char *msg, char *id);
/* Make check box - can be called directly, though it was originally meant
 * as the common code for all lower level checkbox routines.
 * However, it's util functionality has been taken over by
 * cgiMakeCheckBoxWithIdAndOptionalHtml() */

void cgiMakeCheckBox(char *name, boolean checked);
/* Make check box. */

void cgiMakeCheckBoxWithMsg(char *name, boolean checked, char *msg);
/* Make check box, which includes a msg. */

void cgiMakeCheckBoxWithId(char *name, boolean checked, char *id);
/* Make check box, which includes an ID. */

void cgiMakeCheckBoxMore(char *name, boolean checked, char *moreHtml);
/* Make check box with moreHtml */

void cgiMakeCheckBoxEnabled(char *name, boolean checked, boolean enabled);
/* Make check box, optionally enabled/disabled. */

void cgiMakeCheckBoxIdAndMore(char *name, boolean checked, char *id, char *moreHtml);
/* Make check box with ID and extra (non-javascript) html. */

void cgiMakeCheckBoxFourWay(char *name, boolean checked, boolean enabled, char *id, 
                            char *classes, char *moreHtml);
/* Make check box - with fourWay functionality (checked/unchecked by enabled/disabled
 * Also makes a shadow hidden variable that supports the 2 boolean states. */

void cgiMakeTextArea(char *varName, char *initialVal, int rowCount, int columnCount);
/* Make a text area with area rowCount X columnCount and with text: intialVal. */

void cgiMakeTextAreaDisableable(char *varName, char *initialVal, int rowCount, int columnCount, boolean disabled);
/* Make a text area that can be disabled. The rea has rowCount X
 * columnCount and with text: intialVal */

void cgiMakeTextVar(char *varName, char *initialVal, int charSize);
/* Make a text control filled with initial value.  If charSize
 * is zero it's calculated from initialVal size. */

void cgiMakeTextVarWithJs(char *varName, char *initialVal, int width, char *event, char *javascript);
/* Make a text control filled with initial value. */

void cgiMakeOnKeypressTextVar(char *varName, char *initialVal, int charSize,
			      char *script);
/* Make a text control filled with initial value, with a (java)script
 * to execute every time a key is pressed.  If charSize is zero it's
 * calculated from initialVal size. */

void cgiMakeIntVarWithExtra(char *varName, int initialVal, int maxDigits, char *extra);
/* Make a text control filled with initial value and optional extra HTML.  */

void cgiMakeIntVar(char *varName, int initialVal, int maxDigits);
/* Make a text control filled with initial integer value.  */

#define NO_VALUE            -96669
void cgiMakeIntVarInRange(char *varName, int initialVal, char *title, int width, 
                          char *min, char *max);
/* Make a integer control filled with initial value.
   If min and/or max are non-NULL will enforce range
   Requires utils.js jQuery.js and inputBox class */
void cgiMakeIntVarWithLimits(char *varName, int initialVal, char *title, int width, 
                             int min, int max);
void cgiMakeIntVarWithMin(char *varName, int initialVal, char *title, int width, int min);
void cgiMakeIntVarWithMax(char *varName, int initialVal, char *title, int width, int max);
#define cgiMakeIntVarNoLimits(varName,initialVal,title,width) \
        cgiMakeIntVarInRange(varName,initialVal,title,width,NULL,NULL)
/* All four of these call cgiMakeIntVarInRange() and therefore require utils.js */

void cgiMakeDoubleVar(char *varName, double initialVal, int maxDigits);
/* Make a text control filled with initial floating-point value.  */

void cgiMakeDoubleVarInRange(char *varName, double initialVal, char *title, int width, 
                             char *min, char *max);
/* Make a floating point control filled with initial value.
   If min and/or max are non-NULL will enforce range
   Requires utils.js jQuery.js and inputBox class */
void cgiMakeDoubleVarWithLimits(char *varName, double initialVal, char *title, int width, 
                                double min, double max);
void cgiMakeDoubleVarWithMin(char *varName, double initialVal, char *title, int width, double min);
void cgiMakeDoubleVarWithMax(char *varName, double initialVal, char *title, int width, double max);
#define cgiMakeDoubleVarNoLimits(varName,initialVal,title,width) \
        cgiMakeDoubleVarInRange(varName,initialVal,title,width,NULL,NULL)
/* All four of these call cgiMakeDoubleVarInRange() and therefore require utils.js */

void cgiMakeDropListClass(char *name, char *menu[], int menuSize, char *checked, char *class);
/* Make a drop-down list with names and style sheet class. */

void cgiMakeDropList(char *name, char *menu[], int menuSize, char *checked);
/* Make a drop-down list with names.
 * uses style "normalText" */

void cgiMakeDropListClassWithIdStyleAndJavascript(char *name, char *id, char *menu[],
        int menuSize, char *checked, char *class, char *style, struct slPair *events);
/* Make a drop-down list with name, id, text class, style and javascript. */

void cgiMakeDropListClassWithStyleAndJavascript(char *name, char *menu[],
                                                int menuSize, char *checked, char *class,
                                                char *style, struct slPair *events);
/* Make a drop-down list with names, text class, style and javascript. */

void cgiMakeDropListClassWithStyle(char *name, char *menu[],
	int menuSize, char *checked, char *class, char *style);
/* Make a drop-down list with names, text class and style. */

void cgiMakeDropListWithVals(char *name, char *menu[], char *values[],
                         int menuSize, char *checked);
/* Make a drop-down list with names and values. In this case checked
 * corresponds to a value, not a menu. */

void cgiMakeDropListFullExt(char *name, char *menu[], char *values[],
                         int menuSize, char *checked, char *event, char *javascript, char *style, char *class);
/* Make a drop-down list with names and values.
 * Optionally include values for style and class */

void cgiMakeDropListFull(char *name, char *menu[], char *values[],
                         int menuSize, char *checked, char *event, char *javascript);
/* Make a drop-down list with names and values. */

void cgiDropDownWithTextValsAndExtra(char *name, char *text[], char *values[],
                                     int count, char *selected, char *extra);
/* Make a drop-down list with both text and values. */

char *cgiMakeSelectDropList(boolean multiple, char *name, struct slPair *valsAndLabels,
     char *selected, char *anyAll,char *extraClasses, char *event, char *javascript, char *style, char *id);
// Returns allocated string of HTML defining a drop-down select
// (if multiple, REQUIRES ui-dropdownchecklist.js)
// valsAndLabels: val (pair->name) must be filled in but label (pair->val) may be NULL.
// selected: if not NULL is a val found in the valsAndLabels (multiple then comma delimited list).
//           If null and anyAll not NULL, that will be selected
// anyAll: if not NULL is the string for an initial option. It can contain val and label,
//         delimited by a comma
// event: click, etc.
// javacript: what to execute when the event happens.
#define cgiMakeMultiSelectDropList(name,valsAndLabels,selected,anyAll,extraClasses,event,javascript,style,id) \
        cgiMakeSelectDropList(TRUE,(name),(valsAndLabels),(selected),(anyAll),\
                              (extraClasses),(event),(javascript),(style),(id))
#define cgiMakeSingleSelectDropList(name,valsAndLabels,selected,anyAll,extraClasses,event,javascript,style,id) \
        cgiMakeSelectDropList(FALSE,(name),(valsAndLabels),(selected),(anyAll),\
                              (extraClasses),(event),(javascript),(style),(id))

void cgiMakeMultList(char *name, char *menu[], int menuSize, struct slName *checked, int length);
/* Make a list of names which can have multiple selections.
 * Same as drop-down list except "multiple" is added to select tag */

void cgiMakeCheckboxGroup(char *name, char *menu[], int menuSize, struct slName *checked,
			  int tableColumns);
/* Make a table of checkboxes that have the same variable name but different
 * values (same behavior as a multi-select input). */

void cgiMakeCheckboxGroupWithVals(char *name, char *menu[], char *values[], int menuSize,
				  struct slName *checked, int tableColumns);
/* Make a table of checkboxes that have the same variable name but different
 * values (same behavior as a multi-select input), with nice labels in menu[]. */

void cgiMakeHiddenVarWithExtra(char *varName, char *string, char *extra);
/* Store string in hidden input for next time around. */

#define cgiMakeHiddenVar(name,val) cgiMakeHiddenVarWithExtra((name),(val),NULL)
/* Store string in hidden input for next time around. */

void cgiContinueHiddenVar(char *varName);
/* Write CGI var back to hidden input for next time around.
 * (if it exists). */

void cgiContinueAllVars();
/* Write back all CGI vars as hidden input for next time around. */

void cgiVarExclude(char *varName);
/* If varName exists, remove it. */

void cgiVarExcludeExcept(char **varNames);
/* Exclude all variables except for those in NULL
 * terminated array varNames.  varNames may be NULL
 * in which case nothing is excluded. */

void cgiVarSet(char *varName, char *val);
/* Set a cgi variable to a particular value. */

struct dyString *cgiUrlString();
/* Get URL-formatted that expresses current CGI variable state. */

void cgiEncodeIntoDy(char *var, char *val, struct dyString *dy);
/* Add a CGI-encoded &var=val string to dy. */

boolean cgiSpoof(int *pArgc, char *argv[]);
/* Use the command line to set up things as if we were a CGI program.
 * User types in command line (assuming your program called cgiScript)
 * like:
 *        cgiScript nonCgiArg1 var1=value1 var2=value2 var3=value3 nonCgiArg2
 * or like
 *        cgiScript nonCgiArg1 var1=value1&var2=value2&var3=value3 nonCgiArg2
 * (The non-cgi arguments can occur anywhere.  The cgi arguments (all containing
 * the character '=') are erased from argc/argv.  Normally you call this
 *        cgiSpoof(&argc, argv);
 */

boolean cgiFromCommandLine(int *pArgc, char *argv[], boolean preferWeb);
/* Use the command line to set up things as if we were a CGI program.
 * If preferWeb is TRUE will choose real CGI variables over command
 * line ones. */

void useTempFile();
/* tell cheapcgi to use temp files */

boolean cgiFromFile(char *fileName);
/* Set up a cgi environment using parameters stored in a file.
 * Takes file with arguments in the form:
 *       argument1=someVal
 *       # This is a comment
 *       argument2=someOtherVal
 *       ...
 * and puts them into the cgi environment so that the usual
 * cgiGetVar() commands can be used. Useful when a program
 * has a lot of possible parameters.
 */

boolean cgiParseInput(char *input, struct hash **retHash,
	struct cgiVar **retList);
/* Parse cgi-style input into a hash table and list.  This will alter
 * the input data.  The hash table will contain references back
 * into input, so please don't free input until you're done with
 * the hash. Prints message and returns FALSE if there's an error.
 * To clean up - slFreeList, hashFree, and only then free input. */

void cgiParseInputAbort(char *input, struct hash **retHash,
        struct cgiVar **retList);
/* Parse cgi-style input into a hash table and list as above but abort if there's an error. */

char *cgiStringNewValForVar(char *cgiIn, char *varName, char *newVal);
/* Return a cgi-encoded string with newVal in place of what was oldVal.
 * It is an error for var not to exist.   Do a freeMem of this string
 * when you are through. */

void cgiSimpleTableStart();
/* start HTML table  -- no customization. Leaves room
 * for a fancier implementation */

void cgiTableEnd();
/* end HTML table */

void cgiMakeSubmitButton();
/* Make 'submit' type button. */

void cgiMakeResetButton();
/* Make 'reset' type button. */

void cgiMakeClearButton(char *form, char *field);
/* Make button to clear a text field. */

void cgiMakeClearButtonNoSubmit(char *form, char *field);
/* Make button to clear a text field, without resubmitting the form. */

void cgiMakeFileEntry(char *name);
/* Make file entry box/browser */

void cgiSimpleTableRowStart();
/* Start table row */

void cgiTableRowEnd();
/* End table row */

void cgiSimpleTableFieldStart();
/* Start table field */

void cgiTableFieldStartAlignRight();
/* Start table field */

void cgiTableFieldEnd();
/* End table field */

void cgiTableField(char *text);
/* Make table field entry */

void cgiTableFieldWithMsg(char *text, char *msg);
/* Make table field entry with mouseover */

void cgiParagraph(char *text);
/* Make text paragraph */

void logCgiToStderr();
/* Log useful CGI info to stderr */

void cgiResetState();
/* This is for reloading CGI settings multiple times in the same program
 * execution.  No effect if state has not yet been initialized. */

void cgiDown(float lines);
// Drop down a certain number of lines (may be fractional)

char *commonCssStyles();
/* Returns a string of common CSS styles */

char *javaScriptLiteralEncode(char *inString);
/* Use backslash escaping on newline
 * and quote chars, backslash and others.
 * Intended that the encoded string will be
 * put between quotes at a higher level and
 * then interpreted by Javascript. */

boolean cgiParseNext(char **pInput, char **retVar, char **retVal);
/* Parse out next var/val in a var=val&var=val... cgi formatted string 
 * This will insert zeroes and other things into string. 
 * Usage:
 *     char *pt = cgiStringStart;
 *     char *var, *val
 *     while (cgiParseNext(&pt, &var, &val))
 *          printf("%s\t%s\n", var, val); */

struct cgiParsedVars
/* A parsed out cgi variable string */
    {
    struct cgiParsedVars *next;	/* In case want to make a list of these. */
    char *stringBuf;		/* Holds strings inside vars. */
    struct cgiVar *list;    /* List of variables. */
    struct hash *hash;	    /* Keyed by varName, value is just value, not cgiVar. */
    };

struct cgiParsedVars *cgiParsedVarsNew(char *cgiString);
/* Build structure containing parsed out cgiString */

void cgiParsedVarsFree(struct cgiParsedVars **pTags);
/* Free up memory associated with cgiParsedVars */

void cgiParsedVarsFreeList(struct cgiParsedVars **pList);
/* Free up list of cgiParsedVars */

char *cgiScriptDirUrl();
/* Return the cgi-bin directory path on this webserver.
 * This is not the local directory but the <path> part after the server
 * in external URLs to this webserver.
 * e.g. if CGI is called via http://localhost/subdir/cgi-bin/cgiTest
 * the returned string is /subdir/. Must be free'd. */
#endif /* CHEAPCGI_H */

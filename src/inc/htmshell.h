/* Htmshell.h - stuff to make it easier to generate HTML files on
 * the fly.  Typically included with cheapcgi.h in almost any
 * CGI program.
 *
 * To use this generally you should call the function htmShell()
 * very early inside of main().  You pass htmShell() a routine
 * which does most of the work of your web server-side applet.
 *
 * These routines will throw errors, which are caught by
 * htmShell, which then returns.  For the most part you just
 * want an error to cause an error message to be printed and
 * then terminate your CGI program, so this works fine.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

void htmlSetCookie(char* name, char* value, char* expires, char* path, char* domain, boolean isSecure);
/* create a cookie with the given stats */

void htmlParagraph(char *line, ...);
/* Print a line in it's own paragraph. */

void htmlVaParagraph(char *line, va_list args);
/* Print a line in it's own paragraph. */

void htmlCenterParagraph(char *line, ...);
/* Center a line in it's own paragraph. */

void htmlHorizontalLine();
/* Print a horizontal line. */

void htmlNbSpaces(int count);
/* Print a number of non-breaking spaces. */

void htmHorizontalLine(FILE *f);
/* Print a horizontal line. */

void htmTextOut(FILE *f, char *s);
/* Print out string to file, if necessary replacing > with &gt; and the like */

void htmlTextOut(char *s);
/* Print out string, if necessary replacing > with &gt; and the like */

char *htmlTextStripTags(char *s);
/* Returns a cloned string with all html tags stripped out */

char *htmlTextReplaceTagsWithChar(char *s, char ch);
/* Returns a cloned string with all html tags replaced with given char (useful for tokenizing) */

char *htmlEncodeText(char *s, boolean tagsOkay);
/* Returns a cloned string with quotes replaced by html codes.
   Changes ',",\n and if not tagsOkay >,<,& to code equivalents.
   This differs from cgiEncode as it handles text that will
   be displayed in an html page or tooltip style title.  */
#define htmlEncode(s) htmlEncodeText(s,FALSE)

char *attributeEncode(char *str);
// encode double and single quotes in a string to be used as an element attribute

void htmlMemDeath();
/* Complain about lack of memory and abort.  */

void htmlStart(char *title);
/* Write the start of a cgi-generated html file */

void htmStart(FILE *f, char *title);
/* Write the start of a stand alone .html file. */

void htmStartWithHead(FILE *f, char *head, char *title);
/* Write the start of a stand alone .html file, plus head info */

void htmStartDirDepth(FILE *f, char *title, int dirDepth);
/* Write the start of a stand alone .html file.  dirDepth is the number of levels
 * beneath apache root that caller's HTML will appear to the web client.
 * E.g. if writing HTML from cgi-bin, dirDepth is 1; if trash/body/, 2. */

void htmlEnd();
/* Write the end of a cgi-generated html file */

void htmEnd(FILE *f);
/* Write the end of a stand-alone html file */

extern char *htmlStyleUndecoratedLink;
/* Style that gets rid of underline of links. */

void htmlSetStyle(char *style);
/* Set document wide style. A favorite style to
 * use for many purposes is htmlStyleUndecoratedLink
 * which will remove underlines from links.
 * Needs to be called before htmlStart or htmShell. */

void htmlSetStyleSheet(char *styleSheet);
/* Set document wide style sheet by adding css name to HEAD part.
 * Needs to be called before htmlStart or htmShell. */

void htmlSetFormClass(char *formClass);
/* Set class in the BODY part. */


void htmlSetStyleTheme(char *style);
/* Set theme style, these styles can overwrite document wide styles.
 * Needs to be called before htmlStart or htmShell. */

void htmlSetBackground(char *imageFile);
/* Set background image - needs to be called before htmlStart
 * or htmShell. */

void htmlSetBgColor(int color);
/* Set background color - needs to be called before htmlStart
 * or htmShell. */

void htmlBadVar(char *varName);
/* Complain about input variables. */

void htmlImage(char *fileName, int width, int height);
/* Display centered image file. */

jmp_buf htmlRecover;  /* Error recovery jump. Exposed for cart's use. */

void htmlVaWarn(char *format, va_list args);
/* Write an error message.  (Generally you just call warn() or errAbort().
 * This is exposed mostly for the benefit of the cart.) */

char *htmlWarnStartPattern();
/* Return starting pattern for warning message. */

char *htmlWarnEndPattern();
/* Return ending pattern for warning message. */

void htmlWarnBoxSetup(FILE *f);
/* Creates an invisible, empty warning box than can be filled with errors
 * and then made visible. */

void htmlAbort();
/* Terminate HTML file.  Exposed for cart's use. */

void htmlPushEarlyHandlers();
/* Push stuff to close out web page to make sensible error
 * message during initialization. */

/* Wrap error recovery around call to doMiddle. */
void htmErrOnlyShell(void (*doMiddle)());

/* Wrap error recovery and and input processing around call to doMiddle. */
void htmEmptyShell(void (*doMiddle)(), char *method);

/* Wrap an html file around the passed in function.
 * The passed in function is already in the body. It
 * should just make paragraphs and return.
 * Method should be "query" or "get" or "post" (or NULL
 * if you don't care)..
 */
void htmShell( char *title, void (*doMiddle)(), char *method);

/* Wrap an html file around the passed in function.
 * The passed in function is already in the body. It
 * should just make paragraphs and return.
 * Method should be "query" or "get" or "post".
param title - The HTML page title
param head - The head text: can be a refresh directive or javascript
param method - The function pointer to execute in the middle
param method - The browser request method to use
 */
void htmShellWithHead( char *title, char *head, void (*doMiddle)(), char *method);

/* tell htmlOut to not escape special HTML chars '<', '>' */
void htmlNoEscape();

/* tell htmlOut to escape special HTML chars '<', '>' */
void htmlDoEscape();

/* Include an HTML file in a CGI.
 *   The file path is relative to the web server document root */
void htmlIncludeWebFile(char *file);

/* Include an HTML file in a CGI */
void htmlIncludeFile(char *path);

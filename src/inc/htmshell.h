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

void htmHorizontalLine(FILE *f);
/* Print a horizontal line. */

void htmlMemDeath();
/* Complain about lack of memory and abort.  */

void htmlStart(char *title);
/* Write the start of a cgi-generated html file */

void htmStart(FILE *f, char *title);
/* Write the start of a stand alone .html file. */

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

void htmlSetBackground(char *imageFile);
/* Set background image - needs to be called before htmlStart
 * or htmShell. */

void htmlSetBgColor(int *color);
/* Set background color - needs to be called before htmlStart
 * or htmShell. */

void htmlEchoInput();
/* Echo the input string to the output. */

void htmlBadVar(char *varName); 
/* Complain about input variables. */

void htmlImage(char *fileName, int width, int height); 
/* Display centered image file. */

jmp_buf htmlRecover;  /* Error recovery jump. Exposed for cart's use. */

void htmlVaWarn(char *format, va_list args);
/* Write an error message.  (Generally you just call warn() or errAbort().
 * This is exposed mostly for the benefit of the cart.) */

void htmlAbort();
/* Terminate HTML file.  Exposed for cart's use. */

/* Wrap error recovery around call to doMiddle. */
void htmErrOnlyShell(void (*doMiddle)());

/* Wrap error recovery and and input processing around call to doMiddle. */
void htmEmptyShell(void (*doMiddle)(), char *method);

/* Wrap an html file around the passed in function.
 * The passed in function is already in the body. It
 * should just make paragraphs and return. 
 * Method should be "query" or "get" or "post".
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

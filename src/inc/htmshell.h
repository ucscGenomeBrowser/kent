/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
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
 */

/* create a cookie with the given stats */
void htmlSetCookie(char* name, char* value, char* expires, char* path, char* domain, boolean isSecure);

/* Print a line in it's own paragraph. */
void htmlParagraph(char *line, ...);

void htmlVaParagraph(char *line, va_list args);
/* Print a line in it's own paragraph. */

/* Center a line in it's own paragraph. */
void htmlCenterParagraph(char *line, ...);

/* Print a horizontal line. */
void htmlHorizontalLine();

/* Print a horizontal line. */
void htmHorizontalLine(FILE *f);

/* Complain about lack of memory and abort.  */
void htmlMemDeath();

/* Write the start of a cgi-generated html file */
void htmlStart(char *title);

/* Write the start of a stand alone .html file. */
void htmStart(FILE *f, char *title);

/* Write the end of a cgi-generated html file */
void htmlEnd();

/* Write the end of a stand-alone html file */
void htmEnd(FILE *f);

void htmlSetBackground(char *imageFile);
/* Set background - needs to be called before htmlStart
 * or htmShell. */

/* Echo the input string to the output. */
void htmlEchoInput();

/* Complain about input variables. */
void htmlBadVar(char *varName); 

/* Display centered image file. */
void htmlImage(char *fileName, int width, int height); 

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

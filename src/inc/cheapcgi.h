/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Cheapcgi.h - turns variables passed from the web form into 
 * something that C understands. */

#ifndef DYSTRING_H
#include "dystring.h"
#endif 

struct cgiVar
/* Info on one cgi variable. */
    {
    struct cgiVar *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash. */
    char *val;  		/* Value - also not allocated here. */
    boolean saved;		/* True if saved. */
    };

/* return the list of cgiVar's */
struct cgiVar* cgiVarList();

/* Get the string associated with varName from the cookie string. */
char *findCookieData(char *varName);

/* Return TRUE if looks like we're being run as a CGI. */
boolean cgiIsOnWeb();

/* These routines abort the html output if the input isn't
 * there or is misformatted. */
char *cgiString(char *varName);
int cgiInt(char *varName);
double cgiDouble(char *varName);
boolean cgiBoolean(char *varName);

char *cgiOptionalString(char *varName);
/* Return value of string if it exists in cgi environment, else NULL */

char *cgiUsualString(char *varName, char *usual);
/* Return value of string if it exists in cgi environment.  
 * Otherwiser return 'usual' */

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

char *cgiEncode(char *inString);
/* Return a cgi-encoded version of inString. 
 * Alphanumerics kept as is, space translated to plus,
 * and all other characters translated to %hexVal. 
 * You can free return value with freeMem(). */

void cgiMakeButton(char *name, char *value);
/* Make 'submit' type button. */

void cgiMakeTextVar(char *varName, char *initialVal, int charSize);
/* Make a text control filled with initial value.  If charSize
 * is zero it's calculated from initialVal size. */

void cgiMakeIntVar(char *varName, int initialVal, int maxDigits);
/* Make a text control filled with initial value.  */

void cgiMakeDropList(char *name, char *menu[], int menuSize, char *checked);
/* Make a drop-down list. */

void cgiMakeHiddenVar(char *varName, char *string);
/* Store string in hidden input for next time around. */

void cgiContinueHiddenVar(char *varName);
/* Write CGI var back to hidden input for next time around. 
 * (if it exists). */

void cgiVarExclude(char *varName);
/* If varName exists, remove it. */

struct dyString *cgiUrlString();
/* Get URL-formatted that expresses current CGI variable state. */

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


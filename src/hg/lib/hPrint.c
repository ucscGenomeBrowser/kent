/* hPrint - turning html printing on and off, which is useful
 * when postscript and PDF images are being drawn  */

#include "hPrint.h"

static boolean suppressHtml = FALSE;
/* If doing PostScript output we'll suppress most of HTML output. */

boolean hPrintStatus()
/* is html printing on or off ?
   return TRUE for print is on, FALSE for printing is off */
{
return ! suppressHtml;
}

void hPrintDisable()
/* turn html printing off */
{
suppressHtml = TRUE;
}

void hPrintEnable()
/* turn html printing on */
{
suppressHtml = FALSE;
}

void hvPrintf(char *format, va_list args)
/* Suppressable variable args printf. Check for write error so we can
 * terminate if http connection breaks. */
{
if (suppressHtml)
    return;
vprintf(format, args);
if (ferror(stdout))
    noWarnAbort();
}

void hPrintf(char *format, ...)
/* Printf that can be suppressed if not making html. */
{
va_list(args);
va_start(args, format);
hvPrintf(format, args);
va_end(args);
}

void hPrintNonBreak(char *s)
/* Print out string but replace spaces with &nbsp; */
{
char c;

if (suppressHtml)
    return;
while ((c = *s++) != '\0')
    if (c == ' ')
	fputs("&nbsp;", stdout);
    else
        putchar(c);
}

void hPuts(char *string)
/* Puts that can be suppressed if not making html. */
{
if (!suppressHtml)
    puts(string);
}

void hPutc(char c)
/* putc that can be suppressed if not making html. */
{
if (!suppressHtml)
    fputc(c, stdout);
}

void hWrites(char *string)
/* Write string with no '\n' if not suppressed. */
{
if (!suppressHtml)
    fputs(string, stdout);
}

void hButton(char *name, char *label)
/* Write out button if not suppressed. */
{
if (!suppressHtml)
    cgiMakeButton(name, label);
}

void hButtonWithMsg(char *name, char *label, char *msg)
/* Write out button with msg if not suppressed. */
{
if (!suppressHtml)
    cgiMakeButtonWithMsg(name, label, msg);
}

void hOnClickButton(char *command, char *label)
/* Write out push button if not suppressed. */
{
if (!suppressHtml)
    cgiMakeOnClickButton(command, label);
}

void hTextVar(char *varName, char *initialVal, int charSize)
/* Write out text entry field if not suppressed. */
{
if (!suppressHtml)
    cgiMakeTextVar(varName, initialVal, charSize);
}

void hIntVar(char *varName, int initialVal, int maxDigits)
/* Write out numerical entry field if not supressed. */
{
if (!suppressHtml)
    cgiMakeIntVar(varName, initialVal, maxDigits);
}

void hDoubleVar(char *varName, double initialVal, int maxDigits)
/* Write out numerical entry field if not supressed. */
{
if (!suppressHtml)
    cgiMakeDoubleVar(varName, initialVal, maxDigits);
}

void hCheckBox(char *varName, boolean checked)
/* Make check box if not suppressed. */
{
if (!suppressHtml)
    cgiMakeCheckBox(varName, checked);
}

void hCheckBoxJS(char *varName, boolean checked, char *javascript)
/* Make check box if not suppressed, with javascript. */
{
if (!suppressHtml)
    cgiMakeCheckBoxJS(varName, checked, javascript);
}

void hDropListClassWithStyle(char *name, char *menu[], int menuSize, 
                                char *checked, char *class, char *style)
/* Make a drop-down list with names if not suppressed, 
 * using specified class and style */
{
if (!suppressHtml)
    cgiMakeDropListClassWithStyle(name, menu, menuSize, checked, class, style);
}

void hDropListClass(char *name, char *menu[], int menuSize, char *checked,
                        char *class)
/* Make a drop-down list with names if not suppressed, using specified class. */
{
hDropListClassWithStyle(name, menu, menuSize, checked, class, NULL);
}

void hDropList(char *name, char *menu[], int menuSize, char *checked)
/* Make a drop-down list with names if not suppressed. */
{
hDropListClass(name, menu, menuSize, checked, NULL);
}

void hPrintComment(char *format, ...)
/* Function to print output as a comment so it is not seen in the HTML
 * output but only in the HTML source. */
{
va_list(args);
va_start(args, format);
hWrites("\n<!-- DEBUG: ");
hvPrintf(format, args);
hWrites(" -->\n");
fflush(stdout); /* USED ONLY FOR DEBUGGING BECAUSE THIS IS SLOW - MATT */
va_end(args);
}

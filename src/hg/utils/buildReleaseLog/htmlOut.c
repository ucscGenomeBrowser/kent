#include "common.h"
#include "dystring.h"
#include "htmlOut.h"

struct htmlOutFile *htmlOutNewFile(char *filename, char *title, char *rootPath)
/* Open a new HTML file for writing and write out the standard header for
 * static pages.  Leaks memory via a dyString. */
{
struct htmlOutFile *new = NULL;
AllocVar(new);
new->file = mustOpen(filename, "w");
new->filename = cloneString(filename);

struct dyString *header = dyStringNew(0);
dyStringPrintf(header,
    "<!DOCTYPE html>\n"
    "<!--#set var=\"TITLE\" value=\"%s\" -->\n"
    "<!--#set var=\"ROOT\" value=\"%s\" -->\n\n"
    "<!-- Relative paths to support mirror sites with non-standard GB docs install -->\n"
    "<!--#include virtual=\"$ROOT/inc/gbPageStart.html\" -->\n\n",
    title, rootPath);
mustWrite(new->file, dyStringContents(header), dyStringLen(header));
return new;
}


void htmlOutCloseFile(struct htmlOutFile **filePtr)
/* Write out the footer for a standard static doc HTML file and then close it. */
{
struct htmlOutFile *file = *filePtr;
char footer[] = "\n<!--#include virtual=\"$ROOT/inc/gbPageEnd.html\" -->";
mustWrite(file->file, footer, strlen(footer));
fclose(file->file);
free(file->filename);
freez(filePtr);
}

struct htmlOutBlock *htmlOutNewBlock(char *heading, char *anchor)
/* Allocate an html block and fill in the heading and anchor name fields
 * (a NULL value means no heading or anchor). The text block automatically opens
 * with a <p> tag. */
{
struct htmlOutBlock *new = NULL;
AllocVar(new);
new->heading = cloneString(heading);
new->anchor = cloneString(anchor);
new->blockText = dyStringNew(0);
return new;
}


void htmlOutFreeBlock(struct htmlOutBlock **bPtr)
/* Free the memory associated with an HTML block and zero the pointer */
{
struct htmlOutBlock *block = *bPtr;
if (block->heading != NULL)
    freez(&(block->heading));
if (block->anchor != NULL)
    freez(&(block->anchor));
dyStringFree(&(block->blockText));
freez(bPtr);
}


void htmlOutAddBlockContent(struct htmlOutBlock *block, char *format, ...)
/* Append to the text for with this block of HTML content */
{
if (format == NULL)
    return;
va_list args;
va_start(args, format);
dyStringVaPrintf(block->blockText, format, args);
va_end(args);
}


void htmlOutWriteBlock(struct htmlOutFile *file, struct htmlOutBlock *block)
/* Append a chunk of HTML content to a file.  First the block's anchor (if any)
 * is created, then the header (if any) and text content is written out,
 * and finally a closing </p> tag is added. */
{
if (block->anchor != NULL)
    {
    struct dyString *anchor = dyStringNew(0);
    dyStringPrintf(anchor, "<a name='%s'></a>\n", block->anchor);
    mustWrite(file->file, dyStringContents(anchor), dyStringLen(anchor));
    dyStringFree(&anchor);
    }
if (block->heading != NULL)
    {
    mustWrite(file->file, block->heading, strlen(block->heading));
    mustWrite(file->file, "\n", strlen("\n"));
    }
char *content = dyStringContents(block->blockText);
if ((content != NULL) && (strlen(content) > 0))
    {
    mustWrite(file->file, content, strlen(content));
    }
}

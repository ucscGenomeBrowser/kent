/* annoFormatTab -- collect fields from all inputs and print them out, tab-separated. */

#include "annoFormatTab.h"
#include "dystring.h"

struct annoFormatTab
{
    struct annoFormatter formatter;
    char *fileName;
    FILE *f;
    boolean needHeader;
    struct dyString *header;
    struct dyString *line;
};

void aftCollect(struct annoFormatter *vSelf, struct annoStreamer *source, struct annoRow *rows,
		boolean filterFailed)
/* Gather columns from a single source's row(s).
 * If filterFailed, forget what we've collected so far, time to start over. */
{
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
// handle filter failure:
if (filterFailed)
    {
    dyStringClear(self->line);
    dyStringClear(self->header);
    return;
    }
//#*** uh-oh -- we can have multiple rows of input!
char **words = rows->data;  //#*** formatter & source must agree here
struct annoColumn *col;
int i;
for (col = source->columns, i = 0;  col != NULL;  col = col->next, i++)
    {
    if (! col->included)
	continue;
    if (self->needHeader)
	{
	if (i > 0 || dyStringLen(self->header) > 0)
	    dyStringAppendC(self->header, '\t');
	else
	    dyStringAppendC(self->header, '#');
	dyStringAppend(self->header, col->def->name);
	}
    if (i > 0 || dyStringLen(self->line) > 0)
	dyStringAppendC(self->line, '\t');
    dyStringAppend(self->line, words[i]);
    }
}

void aftFormatOne(struct annoFormatter *vSelf)
/* Print out tab-separated columns that we have gathered in prior calls to aftCollect,
 * and start over fresh for the next line of output.  Write header if necessary. */
{
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
if (self->needHeader)
    {
    dyStringAppendC(self->header, '\n');
    fputs(self->header->string, self->f);
    self->needHeader = FALSE;
    }
dyStringAppendC(self->line, '\n');
fputs(self->line->string, self->f);
dyStringClear(self->line);
}

void aftClose(struct annoFormatter **pVSelf)
/* Close file handle, free self. */
{
if (pVSelf == NULL)
    return;
struct annoFormatTab *self = *(struct annoFormatTab **)pVSelf;
freeMem(self->fileName);
carefulClose(&(self->f));
dyStringFree(&(self->header));
dyStringFree(&(self->line));
annoFormatterFree(pVSelf);
}

struct annoFormatter *annoFormatTabNew(char *fileName)
/* Return a formatter that will write its tab-separated output to fileName. */
{
struct annoFormatTab *aft;
AllocVar(aft);
struct annoFormatter *formatter = &(aft->formatter);
formatter->getOptions = annoFormatterGetOptions;
formatter->setOptions = annoFormatterSetOptions;
formatter->collect = aftCollect;
formatter->formatOne = aftFormatOne;
formatter->close = aftClose;
aft->fileName = cloneString(fileName);
aft->f = mustOpen(fileName, "w");
aft->needHeader = TRUE;
aft->header = dyStringNew(0);
aft->line = dyStringNew(0);
return (struct annoFormatter *)aft;
}

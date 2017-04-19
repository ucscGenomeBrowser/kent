struct htmlOutFile
/* File object for a static HTML document */
    {
    FILE *file;
    char *filename;
    };

struct htmlOutBlock
/* A chunk of content (with optional header and anchor) to be written to a static HTML document.
 * To omit a header or anchor, set the pointers to NULL. */
    {
    char *heading; /* Content heading.  Should include any desired <h1>/<h2>/... tags. */
    char *anchor; /* Name of the HTML anchor for this content, e.g., "section1" */
    struct dyString *blockText;
    };

struct htmlOutFile *htmlOutNewFile(char *filename, char *title, char *rootPath);
/* Open a new HTML file for writing and write out the standard header for static pages. */

void htmlOutCloseFile(struct htmlOutFile **file);
/* Write out the footer for a standard static doc HTML file and then close it. */

struct htmlOutBlock *htmlOutNewBlock(char *heading, char *anchor);
/* Allocate an html block and fill in the heading and anchor name fields
 * (a NULL value means no heading or anchor). The text block automatically opens
 * with a <p> tag. */

void htmlOutFreeBlock(struct htmlOutBlock **bPtr);
/* Free the memory associated with an HTML block and zero the pointer */

void htmlOutAddBlockContent(struct htmlOutBlock *block, char *format, ...);
/* Append to the text for with this block of HTML content */

void htmlOutWriteBlock(struct htmlOutFile *file, struct htmlOutBlock *block);
/* Append a chunk of HTML content to a file.  First the block's anchor (if any)
 * is created, then the header (if any) and text content is written out,
 * and finally a closing </p> tag is added. */


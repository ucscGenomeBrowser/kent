/* tabFmt - Format a tab-seperated file for human readability. */
#include "common.h"
#include "linefile.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tabFmt - Format a tab-seperated file for human readability\n"
  "Usage:\n"
  "   tabFmt [options] [inFile] [outFile]\n"
  "\n"
  "Options:\n"
    "  -right - right-justify\n"
    "  -numRight - right-justify numeric appearing columns (text header allowed)\n"
    "  -passNoTab - pass through lines with no tabs without including them in the\n"
    "   formatting\n"
    "  -h,-help - help\n"
  );
}

/* Command line validation table. */
static struct optionSpec optionSpecs[] = {
    {"right", OPTION_BOOLEAN},
    {"numRight", OPTION_BOOLEAN},
    {"passNoTab", OPTION_BOOLEAN},
    {"h", OPTION_BOOLEAN},
    {"help", OPTION_BOOLEAN},
    {NULL, 0}
};
static boolean clRight = FALSE;
static boolean clNumRight = FALSE;
static boolean clPassNoTab = FALSE;

/* column data type guess */
enum colData {
    colUnknown = 0,
    colStr,
    colInt,
    colReal
};

/* Information about a column */
struct colInfo {
    int width;
    int numCnt;  /* rows, except first, that appear numeric */
    boolean nonNumeric;  /* doen't look numeric */
};

/* Description of table. */
struct tblInfo {
    int numCols;  /* current number of columns */
    int maxCols;  /* current maximum size */
    int numRows;  /* total number of rows */
    struct colInfo *cols;  /* array of column info */
};

static struct tblInfo* tblInfoNew()
/* allocated new column sizes */
{
struct tblInfo* ti;
AllocVar(ti);
ti->maxCols = 256;
AllocArray(ti->cols, ti->maxCols);
return ti;
}

static void tblInfoFree(struct tblInfo** tip)
/* free tblInfo object */
{
struct tblInfo* ti = *tip;
if (ti != NULL)
    {
    freeMem(ti->cols);
    freeMem(ti);
    *tip = NULL;
    }
}

static void adjustColWidth(struct tblInfo *ti, int iCol, int width)
/* set max on a column, expanding arrays as needed */
{
if (iCol >= ti->numCols)
    {
    ti->numCols++;
    if (ti->numCols > ti->maxCols)
        {
        int oldMax = ti->maxCols;
        ti->maxCols *= 2;
        ExpandArray(ti->cols, oldMax, ti->maxCols);
        }
    }
ti->cols[iCol].width = max(ti->cols[iCol].width, width);
}

static boolean checkNumericLen(char *str,
                               int len)
/* check if a string looks number. Does hex, and nn%,, but not scientific. */
{
char *p = str, *stop = str+len;
if (((stop-p) >= 1) && ((*p == '+') || (*p == '-')))
    p++;
if (((stop-p) >= 2) && (*p == '0') && ((*(p+1) == 'x') || (*(p+1) == 'X')))
    p += 2; /* looks hex */
if (p == stop)
    return FALSE;  /* empty string */
int dotCnt = 0;
for (; p != stop; p++)
    {
    if (!((('0' <= *p) && (*p <= '9')) || (*p == '.') ||
          ((*p == '%') && (*(p+1) == '\0'))))
        return FALSE;
    if (*p == '.')
        dotCnt++;
    if (dotCnt > 1)
        return FALSE;
    }
return TRUE;
}

/* check if a string looks number. Does hex, and nn%,, but not scientific. */
static boolean checkNumeric(char *str)
{
return checkNumericLen(str, strlen(str));
}

static boolean looksNumeric(char *col)
/* test if a column value looks numeric, handles scientific and hex
 * notation */
{
char *ePtr = strpbrk(col, "eE");
if (ePtr == NULL)
    return checkNumeric(col);
else
    {
    boolean ok = checkNumericLen(col, ePtr-col);
    if (ok)
        ok = checkNumeric(ePtr+1);
    return ok;
    }
}

static bool isIgnoredNoTabLine(char *line)
/* should this line be ignored due to -passNoTab option */
{
return clPassNoTab && (strchr(line, '\t') == NULL);
}

static void colInfoUpdate(struct tblInfo *ti, char *col, int iCol, int dataLineNum)
/* update information on a column */
{
adjustColWidth(ti, iCol, strlen(col));
if (clNumRight && (dataLineNum > 0) && !looksNumeric(col))
    ti->cols[iCol].nonNumeric = TRUE;
}

static void tblInfoUpdate(struct tblInfo *ti, char *line, int dataLineNum)
/* update tblInfo based on a line */
{
char *col = line;
char *tab;
int iCol = 0;
while ((tab = strchr(col, '\t')) != NULL)
    {
    *tab = '\0';
    colInfoUpdate(ti, col, iCol, dataLineNum);
    *tab = '\t';
    iCol++;
    col = tab+1;
    }
colInfoUpdate(ti, col, iCol, dataLineNum);
ti->numRows++;
}

static struct slName *readLines(struct lineFile *inLf, struct tblInfo* ti)
/* read lines and count tab widths */
{
struct slName *lines = NULL;
char *line;
int dataLineNum = 0;  // only count data lines, not ignored lines
while (lineFileNext(inLf, &line, NULL))
    {
    if (!isIgnoredNoTabLine(line))
        {
        tblInfoUpdate(ti, line, dataLineNum);
        dataLineNum++;
        }
    slSafeAddHead(&lines, slNameNew(line));
    }
slReverse(&lines);
return lines;
}

static void writeCol(FILE *outFh, struct tblInfo* ti, int iCol, char *data, int len, boolean isLast)
/* write a column, padding to width */
{
// isLast is an arg to allow variable number of columns
int w;
boolean rightFmt = clRight || (clNumRight && !ti->cols[iCol].nonNumeric);
if (isLast && !rightFmt)
    w = len; // last column, don't add training white-space
else
    w = (ti->cols[iCol].width) * (rightFmt ? 1 : -1);
if (iCol > 0)
    fputs(" ", outFh);
fprintf(outFh, "%*.*s", w, len, data);
}

static void writeLine(struct tblInfo* ti, char *line, FILE *outFh)
/* write one line */
{
char *prev = line;
char *tab;
int iCol = 0;
while ((tab = strchr(prev, '\t')) != NULL)
    {
    writeCol(outFh, ti, iCol, prev, tab-prev, FALSE);
    iCol++;
    prev = tab+1;
    }
writeCol(outFh, ti, iCol, prev, strlen(prev), TRUE);
fputc('\n', outFh);
}

static void writeLines(struct tblInfo* ti, struct slName *lines, FILE *outFh)
/* write rows, padding columns to column widths */
{
for (struct slName *line = lines; line != NULL; line = line->next)
    {
    if (isIgnoredNoTabLine(line->name))
        {
        fputs(line->name, outFh);
        fputc('\n', outFh);
        }
    else
        {
        writeLine(ti, line->name, outFh);
        }
    }
}

static void tabFmt(char *inFile, char *outFile)
/* format file */
{
struct tblInfo* ti = tblInfoNew();
struct lineFile *inLf = lineFileOpen(inFile, TRUE);
struct slName *lines = readLines(inLf, ti);
lineFileClose(&inLf);

FILE *outFh = mustOpen(outFile, "w");
writeLines(ti, lines, outFh);
carefulClose(&outFh);
slFreeList(&lines);
tblInfoFree(&ti);
}

int main(int argc, char **argv)
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (optionExists("h") || optionExists("help"))
    usage();

if (argc > 3)
    usage();
clRight = optionExists("right");
clNumRight = optionExists("numRight");
clPassNoTab = optionExists("passNoTab");

tabFmt(((argc >= 2) ? argv[1] : "stdin"),
       ((argc >= 3) ? argv[2] : "stdout"));
return 0;
}

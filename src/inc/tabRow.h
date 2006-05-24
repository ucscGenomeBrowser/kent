/* tabRow - a row from a database or a tab-separated file held in
 * memory.   Just a light wrapper around an array of strings. 
 * Also some routines to convert slLines to tabRows. */

#ifndef TABROW_H
#define TABROW_H

struct tabRow
/* A parsed out tableRow. */
    {
    struct tabRow *next;
    int colCount;
    char *columns[1];
    };

struct tabRow *tabRowNew(int colCount);
/* Return new row. */

int tabRowMaxColCount(struct tabRow *rowList);
/* Return largest column count */

struct tabRow *tabRowByWhite(struct slName *lineList, char *fileName,
	boolean varCol);
/* Convert lines to rows based on spaces.  If varCol is TRUE then not
 * all rows need to have same number of columns. */

struct tabRow *tabRowByChar(struct slName *lineList, char c, char *fileName,
	boolean varCol);
/* Convert lines to rows based on character separation.  If varCol is TRUE then not
 * all rows need to have same number of columns. */

struct tabRow *tabRowByFixedOffsets(struct slName *lineList, struct slInt *offList,
	char *fileName);
/* Return rows parsed into fixed width fields whose starts are defined by
 * offList. */

struct tabRow *tabRowByFixedGuess(struct slName *lineList, char *fileName);
/* Return rows parsed into fixed-width fields. */

struct slInt *tabRowGuessFixedOffsets(struct slName *lineList, char *fileName);
/* Return our best guess list of starting positions for space-padded fixed
 * width fields. */

#endif /* TABROW_H */


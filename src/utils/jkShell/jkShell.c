/* jkShell - Simple shell.  Reads a line. Executes a line. */
#include "common.h"
#include "hash.h"
#include "dlist.h"
#include "portable.h"
#include "obscure.h"
#include <process.h>
#include <direct.h>
#include <signal.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <conio.h>


boolean checkAbort();
/* Check and clear abort */

void maskControlC();
/* Make Control C not kill us. */

/* Begin with some general purpose functions that should probably
 * go into library some day. */

boolean isDir(char *fileName)
/* Returns TRUE if file is a directory. */
{
struct stat buf;
int result;

/* Get data associated with file */
result = stat( fileName, &buf );

/* Check if statistics are valid: */
if( result != 0 )
    {
    return FALSE;
    }
else
    {
    return ((buf.st_mode & _S_IFDIR) != 0);
    }
}

boolean fileExists(char *fileName)
/* Returns TRUE if file exists. */
{
struct stat buf;
return (stat(fileName, &buf) == 0);
}

boolean copyOpenFile(int s, int d)
/* Copy contents of s to d. */
{
int sizeRead;
char buf[8*1024];
boolean ok = TRUE;

for (;;)
    {
    sizeRead = read(s, buf, sizeof(buf));
    if (sizeRead > 0)
        {
        if (write(d, buf, sizeRead) < sizeRead)
            {
            fprintf(stderr, "Write failed.  Disk full?");
            ok = FALSE;
            break;
            }
        }
    if (sizeRead < sizeof(buf))
        break;
    }
return ok;
}

boolean copyFile(char *source, char *dest)
/* Do big copy from source to dest. Prints a warning and
 * returns FALSE if it fails. */
{
int s, d;
boolean ok = TRUE;

s = open(source, _O_RDONLY|_O_BINARY);
if (s == -1)
    {
    warn("Couldn't open %s", source);
    return FALSE;
    }

d = open(dest, _O_WRONLY|_O_CREAT|_O_TRUNC|_O_BINARY, _S_IREAD|_S_IWRITE );
if (d == -1)
    {
    warn("Couldn't create %s", dest);
    close(s);
    return FALSE;
    }
ok = copyOpenFile(s, d);
close(s);
close(d);
return ok;
}


char *fileNameOnly(char *path)
/* Returns file name part of path, ignoring
 * directory info. */
{
char *d1 = strrchr(path, '/');
char *d2 = strrchr(path, '\\');
char *d3 = strrchr(path, ':');
char *dirEnd = NULL;

/* Get Last directory character. */
dirEnd = d1;
if (d2 != NULL)
    {
    if (dirEnd == NULL || d2 > dirEnd)
        dirEnd = d2;
    }
if (d3 != NULL)
    {
    if (dirEnd == NULL || d3 > dirEnd)
        dirEnd = d3;
    }
if (dirEnd == NULL)
    return path;
else
    return dirEnd+1;
}


enum specialKeys
    {
    skLeft = 0x4b,
    skRight = 0x4d,
    skUp = 0x48,
    skDown = 0x50,
    skDel = 0x53,
    skHome = 0x47,
    skEnd = 0x4f,
    skPageUp = 0x49,
    skPageDown = 0x51,
    skInsert = 0x52,
    };

static void redrawLine(char *prompt, char *buf, int cursorPos, int blanks)
/* Redraw line for line editor. */
{
int bufLen = strlen(buf);
int extraChars = bufLen - cursorPos + blanks;
int i;

putch('\r');
cputs(prompt);
cputs(buf);
for (i=0; i<blanks; ++i)
    putch(' ');
if (extraChars)
    {
    for (i=0; i<extraChars; ++i)
        putch('\b');
    }
}

void editLine(struct dlList *history, char *prompt, char *buf, int cursorPos, int bufSize)
/* Read a line from console allowing some editing. */
{
int promptSize = strlen(prompt);
int bufCharCount = strlen(buf);
int c;
struct dlNode *curNode = NULL;
if (cursorPos > bufCharCount)
    cursorPos = bufCharCount;

redrawLine(prompt, buf, cursorPos, 0);
for (;;)
    {
    c = getch();
    if (c == 0 || c == 0xE0)
        {
        c = getch();
        if (c == skLeft)
            {
            if (cursorPos > 0)
                {
                --cursorPos;
                putch('\b');
                }
            }
        else if (c == skRight)
            {
            if (cursorPos < bufCharCount)
                {
                ++cursorPos;
                redrawLine(prompt, buf, cursorPos, 0);
                }
            }
        else if (c == skDel)
            {
            if (cursorPos < bufCharCount)
                {
                int endChars = bufCharCount - cursorPos - 1;
                memmove(buf+cursorPos, buf+cursorPos+1, endChars);
                --bufCharCount;
                buf[bufCharCount] = 0;
                redrawLine(prompt, buf, cursorPos, 1);
                }
            }
        else if (c == skUp)
            {
            boolean gotHistory = FALSE;
            if (history != NULL && history->tail->prev != NULL)
                {
                if (curNode == NULL)
                    {
                    curNode = history->tail;
                    gotHistory = TRUE;
                    }
                else
                    {
                    struct dlNode *pn = curNode->prev;
                    if (pn->prev != NULL)
                        {
                        curNode = pn;
                        gotHistory = TRUE;
                        }
                    }
                if (gotHistory)
                    {
                    int oldSize = bufCharCount;
                    int sizeDif;
                    strcpy(buf,curNode->val);
                    bufCharCount = cursorPos = strlen(buf);
                    sizeDif = oldSize - bufCharCount;
                    if (sizeDif < 0)
                        sizeDif = 0;
                    redrawLine(prompt, buf, cursorPos, sizeDif);
                    }
                }
            }
        else if (c == skDown)
            {
            if (curNode != NULL && curNode->next != NULL && curNode->next->next != NULL)
                {
                int oldSize = bufCharCount;
                int sizeDif;
                curNode = curNode->next;
                strcpy(buf,curNode->val);
                bufCharCount = cursorPos = strlen(buf);
                sizeDif = oldSize - bufCharCount;
                if (sizeDif < 0)
                    sizeDif = 0;
                redrawLine(prompt, buf, cursorPos, sizeDif);
                }
            else
                {
                buf[0] = 0;
                redrawLine(prompt, buf, 0, bufCharCount);
                cursorPos = bufCharCount = 0;
                }
            }
        else if (c == skHome)
            {
            cursorPos = 0;
            redrawLine(prompt, buf, cursorPos, 0);
            }
        else if (c == skEnd)
            {
            cursorPos = bufCharCount;
            redrawLine(prompt, buf, cursorPos, 0);
            }
        else
            {
            printf("\n0x%x\n", c);
            redrawLine(prompt, buf, cursorPos, 0);
            }
        }
    else if (c == '\r')
        return;
    else if (c == '\b')
        {
        if (cursorPos > 0)
            {
            if (cursorPos == bufCharCount)
                {
                putch('\b');
                putch(' ');
                putch('\b');
                --cursorPos;
                --bufCharCount;
                buf[bufCharCount] = 0;
                }
            else
                {
                int endChars = bufCharCount - cursorPos;
                memmove(buf+cursorPos-1, buf+cursorPos, endChars);
                --cursorPos;
                --bufCharCount;
                buf[bufCharCount] = 0;
                redrawLine(prompt, buf, cursorPos, 1);
                }
            }
        }
    else
        {
        if (bufCharCount < bufSize-1)
            {
            if (cursorPos == bufCharCount)
                {
                putch(c);
                buf[bufCharCount] = c;
                bufCharCount += 1;
                buf[bufCharCount] = 0;
                cursorPos += 1;
                }
            else
                {
                int endChars = bufCharCount - cursorPos;
                memmove(buf+cursorPos+1, buf+cursorPos, endChars);
                buf[cursorPos] = c;
                ++cursorPos;
                ++bufCharCount;
                buf[bufCharCount] = 0;
                redrawLine(prompt, buf, cursorPos, 0);
                }
            }
        }
    }
}


void enterLine(struct dlList *history, char *prompt, char *buf, int bufSize)
/* Get a line from user letting them edit it and use arrow keys for command history. */
{
*buf = 0;
editLine(history, prompt, buf, 0, bufSize);
putch('\r');
putch('\n');
}


boolean gotAbort = FALSE;       /* Set if got ^C abort. */

int handleControlC(int sig)
/* Signal handler for control C. */
{
gotAbort = TRUE;
maskControlC();    /* Apparently at least in Windows after catching signal you have
                    * to reset signal handler. */
return 0;
}

void clearAbort()
/* Clear last abort. */
{
gotAbort = FALSE;
}

boolean checkAbort()
/* Check and clear abort */
{
if (gotAbort)
    {
    clearAbort();
    return TRUE;
    }
else
    return FALSE;
}


int commandLimit = 1000;     /* Max length of command line (here because Windows will
                             * crash if you give it a really long one.). */

enum shellReturnCode
/* Return code values. */
    {
    slcEof = 0,     /* End of file. */
    slcOk = 1,      /* A-ok. */
    slcError = 2,   /* Problem. */
    };

int shellLine(struct shell *sh, char *line, FILE *f);
/* Do processing of one line of user input. Returns
 * slcEof to quit. */

void enterLine(struct dlList *history, char *prompt, char *buf, int bufSize);
/* Get a line from user letting them edit it and use arrow keys for command history. */

struct shell
/* Holds data structures for a shell. */
    {
    char *inBuf;         /* Holds input line. */
    int inBufAlloc;      /* Allocated size of input line. */
    char *outBuf;         /* Holds output line. */
    int outBufAlloc;     /* Allocates size of output line. */
    struct dlList *history; /* Recent commands. */
    int historySize;        /* Current length of history. */
    int maxHistorySize;     /* Number of commands to keep. */
    int commandsSeen;       /* Number of commands seen. */
    struct hash *variables; /* Variable hash table. */
    struct hash *aliases;   /* Alias hash table. */
    char *prompt;           /* Prompt string. */
    };

struct shell *newShell()
/* Return a new shell. */
{
struct shell *sh;
AllocVar(sh);
sh->inBufAlloc = 1024;
sh->inBuf = needMem(sh->inBufAlloc);
sh->outBufAlloc = 64*1024;
sh->outBuf = needMem(sh->outBufAlloc);
sh->history = newDlList();
sh->maxHistorySize = 200;
sh->variables = newHash(0);
sh->aliases = newHash(0);
sh->prompt = "pong% ";
return sh;
}

void freeShell(struct shell **pSh)
/* Free up a shell. */
{
struct shell *sh;
if ((sh = *pSh) != NULL)
    {
    freeMem(sh->inBuf);
    freeMem(sh->outBuf);
    freeDlListAndVals(&sh->history);
    freeHashAndVals(&sh->variables);    
    freeHashAndVals(&sh->aliases);
    }
}

struct slName *newTok(char *string, int size)
/* Allocat slName for string of give size. */
{
struct slName *n = needMem(sizeof(*n) + size);
memcpy(n->name, string, size);
return n;
}

bool isNameChar[256];

void makeIsNameChar()
/* Make array of flags that say whether something is
 * an acceptable part of a word or not. */
{
int i;
for (i=0; i<ArraySize(isNameChar); ++i)
    isNameChar[i] = TRUE;
isNameChar[' '] = FALSE;
isNameChar['\r'] = FALSE;
isNameChar['\n'] = FALSE;
isNameChar['\t'] = FALSE;
isNameChar['#'] = FALSE;
isNameChar['|'] = FALSE;
isNameChar['='] = FALSE;
isNameChar['>'] = FALSE;
isNameChar['<'] = FALSE;
isNameChar['&'] = FALSE;
isNameChar[')'] = FALSE;
isNameChar['('] = FALSE;
isNameChar[0] = FALSE;
}

char *firstNonName(char *s)
/* Return first non-name character in string */
{
char c;
for (;;)
    {
    c = *s;
    if (!isNameChar[c])
        return s;
    ++s;
    }
}

struct slName *tokenize(char *text)
/* Return a list of tokenized text. */
{
char c;
char *s = text;
struct slName *tokList = NULL, *tok;

while((c = *s++) != 0)
    {
    if (isspace(c))
        {
        char *start = s-1;
        while (isspace(*s))
            ++s;
        tok = newTok(start, s-start);
        slAddHead(&tokList, tok);
        }
    else if (c == '"')
        {
        /* Go look for next (unescaped) quote */
        char *start = s;
        char lastC = c;
        while ((c = *s) != 0)
            {
            if (c == '"' && lastC != '\\')
                {
                break;
                }
            lastC = c;
            ++s;
            }
        tok = newTok(start, s - start);
        slAddHead(&tokList, tok);
        if (c == '"')
            ++s;
        }        
    else if (isNameChar[c])
        {
        char *start = s-1;
        for (;;)
            {
            c = *s;
            if (!isNameChar[c])
                break;
            ++s;
            }
        tok = newTok(start, s-start);
        slAddHead(&tokList, tok);
        }
    else
        {
        tok = newTok(&c, 1);
        slAddHead(&tokList, tok);
        }
    }
slReverse(&tokList);
return tokList;
}

struct slName *stripSpaceTokens(struct slName *tokList)
/* Remove white space tokens from list. */
{
struct slName *newList = NULL, *tok, *next;

for (tok = tokList; tok != NULL; tok = next)
    {
    next = tok->next;
    if (isspace(tok->name[0]))
        freeMem(tok);
    else
        slAddHead(&newList, tok);
    }
slReverse(&newList);
return newList;
}

void flattenOutput(struct shell *sh, struct slName *tokList, char *spacer)
/* Copy token list to space-delimited sh->outBuf */
{
int totalSize = 1;  /* 1 extra for zero tag. */
struct slName *tok;
char *s;
int len;
int spaceLen = 0;

if (spacer != NULL)
    spaceLen = strlen(spacer);
for (tok = tokList; tok != NULL; tok = tok->next)
    totalSize += (strlen(tok->name) + spaceLen);
if (totalSize > sh->outBufAlloc)
    {
    sh->outBuf = needMoreMem(sh->outBuf, 0, totalSize);
    sh->outBufAlloc = totalSize;
    }
s = sh->outBuf;
for (tok = tokList; tok != NULL; tok = tok->next)
    {
    len = strlen(tok->name);
    memcpy(s, tok->name, len);
    s += len;
    if (spacer != NULL && tok->next != NULL)
        {
        memcpy(s, spacer, spaceLen);
        s += spaceLen;
        }
    }
*s++ = 0;
}

int doSystem(struct shell *sh, struct slName *tokList)
/* Put all the tokens back together in a space delimited
 * string and do a system call on it. */
{
int retVal = slcOk;
int sysRet;
flattenOutput(sh, tokList, " ");
sh->outBuf[commandLimit] = 0;

sysRet = _spawnlp(_P_WAIT, tokList->name, sh->outBuf, NULL );

//sysRet = system(sh->outBuf);
/* Sadly the system call only returns -1 in case of
 * an internal error, not if there was a problem
 * with the underlying program. */
if (sysRet != 0)
    {
    if (errno)
        perror(NULL);
    retVal = slcError;
    }
return retVal;
}

char *findUnescaped(char *s, char target)
/* Find first unescaped target char in s. */
{
char lastC = 0;
char c;

while ((c = *s) != 0)
    {
    if (c == target && lastC != '\\')
        return s;
    lastC = c;
    ++s;
    }
return NULL;
}


struct dlNode *relBack(struct shell *sh, int val)
/* Go back 'val' in history. Return NULL if can't go that
 * far back. */
{
struct dlNode *histNode;

for (histNode = sh->history->tail; histNode->prev != NULL; histNode = histNode->prev)
    {
    if (++val >= 0)
        break;
    }
if (histNode->next == NULL)
    {
    fprintf(stderr, "Sorry, that's before recorded history\n");
    histNode = NULL;
    }
return histNode;
}

struct slName *lastRealNode(struct slName *tokList)
/* Return last real node in list. */
{
struct slName *tok, *lastReal = NULL;
for (tok = tokList; tok != NULL; tok = tok->next)
    {
    if (!isspace(tok->name[0]))
        lastReal = tok;
    }
return lastReal;
}

boolean interpretHistory(struct shell *sh, char *firstBang)
/* Expand out some sort of bang! history. */
{
char *s = firstBang+1;
struct dlNode *histNode = NULL;
char c = *s;
char *hist = NULL;
struct slName *tokList = NULL, *tok;
boolean ok;

if (c == '$')
    {
    histNode = relBack(sh, -1);
    if (histNode != NULL)
        {
        tokList = tokenize(histNode->val);
        tok = lastRealNode(tokList);
        if (tok != NULL)
            hist = tok->name;
        }
    }
else if (c == '!')
    {
    histNode = relBack(sh, -1);
    }
else if (c == '-')
    {
    int val = atoi(s);
    if (val < 0)
        {
        histNode = relBack(sh, val);
        }
    }
else if (isdigit(c))
    {
    int val = atoi(s);
    histNode = relBack(sh, val - sh->commandsSeen - 1);
    }
else
    {
    char *end = skipToSpaces(s);
    int size;
    if (end == NULL)
        size = strlen(s);
    else
        size = end - s;
    for (histNode = sh->history->tail; histNode->prev != NULL; histNode = histNode->prev)
        {
        char *command = histNode->val;
        if (strncmp(command, s, size) == 0)
            break;
        }
    if (histNode->prev == NULL)
        {
        fprintf(stderr, "!%s not found\n", s);
        histNode = NULL;
        }
    }
if (hist == NULL && histNode != NULL && histNode->prev != NULL)
    {
    hist = histNode->val;
    }
if (hist != NULL)
    {
    int oldLen = strlen(sh->inBuf);
    int histLen = strlen(hist);
    int beforeLen = firstBang - sh->inBuf;
    char *after = firstNonName(s);
    int afterStart = after - sh->inBuf;
    int afterLen = oldLen - afterStart;
    int newLen = beforeLen + histLen + afterLen;
    if (newLen >= sh->inBufAlloc)
        {
        sh->inBuf = needMoreMem(sh->inBuf, oldLen, newLen+1);
        sh->inBufAlloc = newLen+1;
        }
    memmove(sh->inBuf + beforeLen + histLen, sh->inBuf + afterStart, afterLen);
    memcpy(sh->inBuf + beforeLen, hist, histLen);
    sh->inBuf[newLen] = 0;
    printf("%s\n", sh->inBuf);
    ok = TRUE;
    }
else
    ok = FALSE;
slFreeList(&tokList);
return ok;
}

boolean doHistory(struct shell *sh)
/* Deal with command history - adding and interpreting. */
{
char *s = trimSpaces(sh->inBuf);        
if (s != NULL)
    {
    /* Look for !.  If it's there deal with it before
     * adding to history. */
    char *bang = findUnescaped(s, '!');
    char *dupe;
    if (bang != NULL)
        {
        if (!interpretHistory(sh, bang))
            return FALSE;        
        }
    dupe = cloneString(sh->inBuf);
    dlAddValTail(sh->history, dupe);
    if (++sh->historySize == sh->maxHistorySize)
        {
        struct dlNode *old = dlPopHead(sh->history);
        freeMem(old->val);
        freeMem(old);
        sh->historySize -= 1;
        }
    ++sh->commandsSeen;
    }
return TRUE;
}

struct slName *lookupAliases(struct shell *sh, struct slName *tokList)
/* Look up first token to see if it's an alias. */
{
if (tokList == NULL)
    return NULL;
else
    {
    char *command = tokList->name;
    struct hashEl *hel = hashLookup(sh->aliases, command);
    if (hel == NULL)
        return tokList;
    else
        {
        /* We got alias.  Dummy up token list with aliased
         * head, flatten it out, and retokenize. */
        char *alias = hel->val;
        struct slName *newHead = newSlName(alias);
        newHead->next = tokList->next;
        flattenOutput(sh, newHead, NULL);
        freeMem(newHead);
        slFreeList(&tokList);
        tokList = tokenize(sh->outBuf);
        return tokList;
        }
    }
}

struct slName *lookupVariables(struct shell *sh, struct slName *tokList)
/* Look up variables (any tokens that start with $) */
{
struct slName *newList = NULL, *next, *tok;
struct hashEl *hel;
boolean gotAny = FALSE;
char *varStart;

for (tok = tokList; tok != NULL; tok = next)
    {
    next = tok->next;
    if ((varStart = findUnescaped(tok->name, '$')) != NULL)
        {
        /* Extract variable part of token, replace it, and put
         * back rest of token. */
        char *before = tok->name;              /* Start of part before var. */
        int beforeSize = varStart - before;
        char *after;                          /* Start of part after var. */
        char *colon;
        char colonCommand = 0;
        int afterSize;
        int varSize;
        char varNameBuf[256];


        varStart += 1;
        colon = findUnescaped(varStart, ':');
        if (colon == NULL)
            {
            after = varStart;
            while (isalpha(*after))
                ++after;
            varSize = after-varStart;
            }
        else
            {
            after = colon+2;
            varSize = colon-varStart;
            colonCommand = colon[1];
            }
        afterSize = strlen(after);
        if (varSize >= sizeof(varNameBuf))
            warn("%s too long\n", varStart);
        memcpy(varNameBuf, varStart, varSize);
        varNameBuf[varSize] = 0;
        if ((hel = hashLookup(sh->variables, varNameBuf)) != NULL)
            {
            struct slName *t;
            if (beforeSize > 0)
                {
                t = newTok(before, beforeSize);
                slAddHead(&newList, t);
                }
            if (colonCommand == 0)
                t = newSlName(hel->val);
            else 
                {
                char *s = hel->val;
                char *suffix = strrchr(s, '.');
                int prefixSize, suffixSize;
                int wholeSize = strlen(s);
                if (suffix == NULL)
                    {
                    suffix = s + wholeSize;
                    suffixSize = 0;
                    prefixSize = wholeSize;
                    }
                else
                    {
                    prefixSize = suffix - s;
                    suffix += 1;
                    suffixSize = wholeSize - prefixSize - 1;
                    }

                if (colonCommand == 'r')
                    t = newTok(s, prefixSize);
                else if (colonCommand == 'e')
                    t = newTok(suffix, suffixSize);
                else
                    t = newTok(s, wholeSize);
                }
            slAddHead(&newList, t);
            if (afterSize > 0)
                {
                t = newTok(after, afterSize);
                slAddHead(&newList, t);
                } 
            freez(&tok);
            gotAny = TRUE;
            }
        else
            {
            slAddHead(&newList, tok);
            }
        }
    else
        {
        slAddHead(&newList, tok);
        }
    }
slReverse(&newList);
if (gotAny)
    {
    flattenOutput(sh, newList, NULL);
    slFreeList(&newList);
    newList = tokenize(sh->outBuf);
    }
return newList;
}


boolean anyWild(char *s)
/* Returns TRUE if any characters in string are wildcards. */
{
char c;
while ((c = *s++) != 0)
    {
    if (c == '?' || c == '*')
        return TRUE;
    }
return FALSE;
}

struct slName *lookupWildcards(struct shell *sh, struct slName *tokList)
/* Expand wildcards */
{
struct slName *newList = NULL, *next, *tok;
boolean gotDir = FALSE;
boolean gotWild = FALSE;
int origCount;
int newCount;
char *s;

if (tokList == NULL)
    return NULL;
origCount = slCount(tokList);

for (tok = tokList; tok != NULL; tok = next)
    {
    next = tok->next;
    s = tok->name;
    if (anyWild(s))
        {
        char *pattern = fileNameOnly(s);
        char *dir;
        char endChar;
        struct slName *t, *nt, *dirList;

        gotWild = TRUE;
        /* Parse out directory if any and pattern. */
        if (pattern != s)
            {
            endChar = pattern[-1];
            pattern[-1] = 0;
            dir = s;
            gotDir = TRUE;
            }
        else
            {
            dir = ".";
            }

        /* Read directory and put it on new list in place
         * of the wildcard. */
        dirList = listDir(dir, pattern);
        if (gotDir)
            {
            char pathBuf[512];
            for (t = dirList; t != NULL; t = t->next)
                {
                sprintf(pathBuf, "%s%c%s", dir, endChar, t->name);
                nt = newSlName(pathBuf);                
                slAddHead(&newList, nt);
                }
            slFreeList(&dirList);
            }
        else
            {
            for (t = dirList; t != NULL; t = nt)
                {
                nt = t->next;
                slAddHead(&newList, t);
                }
            }
        freeMem(tok);
        }
    else
        {
        slAddHead(&newList, tok);
        }
    }
slReverse(&newList);
newCount = slCount(newList);
if (gotWild && (newCount == 0 || (newCount <= 1 && origCount > 1)))
    {
    fprintf(stderr, "No wildcard match.\n");
    slFreeList(&newList);
    }
return newList;
}


int noHistoryShell(struct shell *sh, char *fileName)
/* Interpret lines in file without putting onto
 * history or doing prompts. */
{
FILE *f = fopen(fileName, "r");
int lineCount = 0;
int retCode = slcOk;

if (f == NULL)
    {
    fprintf(stderr, "Couldn't open %s\n", fileName);
    return slcError;
    }

for (;;)
    {
    if (fgets(sh->inBuf, sh->inBufAlloc, f) == NULL)
        break;
    ++lineCount;
    retCode = shellLine(sh, trimSpaces(sh->inBuf), f);
    if (retCode != slcOk)
        break;
    }
fclose(f);
if (retCode == slcEof)
    return slcOk;
else
    return retCode;
}

void setUsage()
/* Explain usage of the set command. */
{
fprintf(stderr, "set - set a variable to a value\n"
                "usage\n"
                "    set var=val\n");
}


void forEachUsage()
/* Explain usage of the foreach command. */
{
fprintf(stderr, "foreach - do something a number of times\n"
                "usage\n"
                "    foreach var (list)\n"
                "        command1\n"
                "        command2\n"
                "          ...\n"
                "    end\n");
}

int doForEach(struct shell *sh, struct slName *foreachToks, FILE *f)
/* Interpret a foreach command. */
{
boolean isInteractive = (f == stdin);
int tokCount = slCount(foreachToks);
char *var;
struct slName *tok, *valList, *next;
struct slName *lineList = NULL, *line;
boolean interpretIt = TRUE;
char *s;
int retCode = slcOk;

/* Parse first line:
 *      foreach var ( ... ) */
if (tokCount < 4)
    {
    forEachUsage();
    return slcError;
    }
tok = foreachToks->next;
var = tok->name;
tok = tok->next;
if (!sameString(tok->name, "("))
    {
    forEachUsage();
    return slcError;
    }
valList = tok->next;
while ((next = tok->next) != NULL)
    tok = next;
if (!sameString(tok->name, ")"))
    {
    forEachUsage();
    return slcError;
    }

/* Store away lines until end. */
for (;;)
    {
    if (isInteractive)
        {
        enterLine(sh->history, "foreach: ", sh->inBuf, sh->inBufAlloc);
        if (!doHistory(sh))
            continue;
        }
    else if (fgets(sh->inBuf, sh->inBufAlloc, f) == NULL)
        {
        interpretIt = FALSE;
        fprintf(stderr, "foreach without end\n");
        retCode = slcError;
        break;
        }
    s = trimSpaces(sh->inBuf);
    if (sameWord(s, "end"))
        break;
    line = newSlName(sh->inBuf);
    slAddHead(&lineList, line);
    }
slReverse(&lineList);
if (interpretIt)
    {
    for (tok = valList; tok != NULL; tok = tok->next)
        {
        struct hashEl *hel;

        if (sameString(tok->name, ")"))
            break;
        hel = hashStore(sh->variables, var);
        if (hel != NULL)
            {
            freeMem(hel->val);
            hel->val = cloneString(tok->name);
            }
        for (line = lineList; line != NULL; line = line->next)
            {
            retCode = shellLine(sh, line->name, f);
            if (retCode != slcOk)
                break;
            }
        if (retCode != slcOk)
            break;
        }
    }
slFreeList(&lineList);
return retCode;
}


boolean cmRename(char *a, char *b)
/* Rename file a to b */
{
if (fileExists(b))
    remove(b);
if (rename(a, b) != 0)
    {
    fprintf(stderr, "Couldn't rename %s to %s\n", a, b);
    return FALSE;
    }
else
    return TRUE;
}

boolean cmCharCopy(char *source, char *dest)
/* Copy source file to dest. */
{
int c;
FILE *s, *d;
boolean ok = TRUE;

if ((s = fopen(source, "rb")) == NULL)
    {
    fprintf(stderr, "Couldn't open %s", source);
    perror("");
    return FALSE;
    }
if ((d = fopen(dest, "wb")) == NULL)
    {
    fprintf(stderr, "Couldn't create %s", dest);
    perror("");
    fclose(s);
    return FALSE;
    }
while ((c = getc(s)) != EOF)
    {
    if ((putc(c, d)) == EOF)
        {
        fprintf(stderr, "%s truncated", dest);
        perror("");
        ok = FALSE;
        break;
        }
    }
fclose(s);
fclose(d);

return ok;
}

void cpMvUsage(boolean isMv)
/* Print usage instructions for mv or copy command */
{
char *name
 = (isMv ? "mv" : "cp");
if (isMv)
    {
    fprintf(stderr, "mv - move files\n");
    name = "mv";
    }
else
    {
    fprintf(stderr, "cp - copy files\n");
    name = "cp";
    }
fprintf(stderr,    "usage:\n"
                    "   %s oldName newName\n"
                    "or\n"
                    "   %s file(s) dir\n",
                    name, name);
}


int doCpMv(struct slName *tokLine, int tokCount, char *line, boolean isMv)
/* Do mv or copy command (depending on cpMv function passed in. */
{
boolean (*cpMv)(char *a, char *b) = (isMv ? cmRename : copyFile);

if (tokCount < 3)
    {
    cpMvUsage(isMv);
    return slcError;
    }
else
    {
    struct slName *dest = lastRealNode(tokLine);
    if (isDir(dest->name))
        {
        char destPath[512];
        struct slName *source;
        char *sourcePath;
        for (source = tokLine->next; source != dest; source = source->next)
            {
            sourcePath = source->name;
            sprintf(destPath, "%s/%s", dest->name, fileNameOnly(sourcePath));
            if (!cpMv(sourcePath, destPath))
                {
                return slcError;
                }
            }
        }
    else
        {
        if (tokCount != 3)
            {
            cpMvUsage(isMv);
            return slcError;
            }
        else
            {
            char *oldName, *newName;
            struct slName *tok = tokLine->next;
            oldName = tok->name;
            newName = dest->name;
            if (!cpMv(oldName, newName))
                {
                return slcError;
                }
            }
        }
    }
return slcOk;
}

struct fileInfo
/* Full info about one file - used by ls. */
    {
    struct fileInfo *next;  /* Next in list. */
    char *name;             /* File name (not allocated here) */
    struct stat stat;       /* Result from stat system call. */
    };

int cmpFiSize(const void *va, const void *vb)
/* Compare two files by size. */
{
struct fileInfo **pA = (struct fileInfo **)va;
struct fileInfo **pB = (struct fileInfo **)vb;
struct fileInfo *a = *pA, *b = *pB;
return a->stat.st_size- b->stat.st_size;
}

int cmpFiTime(const void *va, const void *vb)
/* Compare two files by time. */
{
struct fileInfo **pA = (struct fileInfo **)va;
struct fileInfo **pB = (struct fileInfo **)vb;
struct fileInfo *a = *pA, *b = *pB;
return a->stat.st_mtime - b->stat.st_mtime;
}



int doLs(struct slName *tokLine, int tokCount, char *line)
/* List files. */
{
struct slName *tok;
char *name;
int result;
boolean doLong = FALSE;
boolean doOne = FALSE;
boolean sortSize = FALSE;
boolean sortTime = FALSE;
struct fileInfo *fiList = NULL, *fi;
int nameSize, maxNameSize = 0;
int lineSize = 80;
int perLine;
int linePos = 0;
struct slName *ourList = NULL;
int fileCount = 0;
double totalSize = 0;
int retCode = slcOk;

clearAbort();   /* This can take long enough that we check for ^C. */
/* Collect options and count real files. */
for (tok = tokLine->next; tok != NULL; tok = tok->next)
    {
    name = tok->name;
    if (name[0] == '-')
        {
        char *s = name+1;
        char c;
        while ((c = *s++) != 0)
            {
            c = tolower(c);
            if (c == 'l')
                doLong = TRUE;
            else if (c == '1')
                doOne = TRUE;
            else if (c == 's')
                sortSize = TRUE;
            else if (c == 't')
                sortTime = TRUE;
            }
        }
    else
        {
        ++fileCount;
        }
    }

/* If no real files then list current directory. */
if (fileCount == 0)
    {
    tok = ourList = listDir(".", "*");
    fileCount = slCount(ourList);
    if (checkAbort())
        {
        retCode = slcError;
        goto CLEANUP;
        }
        
    }
else
    tok = tokLine->next;
    
/* Collect info on all files. */
for (; tok != NULL; tok = tok->next)
    {
    name = tok->name;
    if (name[0] != '-')
        {
        AllocVar(fi);
        result = stat(name, &fi->stat);
        if (result != 0)
            {
            fprintf(stderr, "%s ", name);
            perror("");
            freeMem(fi);
            }
        else
            {
            nameSize = strlen(name);
            if (nameSize > maxNameSize)
                maxNameSize = nameSize;
            fi->name = name;
            slAddHead(&fiList, fi);
            }
        }
    }

/* Second pass - sort and print info. */
if (sortSize)
    slSort(&fiList, cmpFiSize);
if (sortTime)
    slSort(&fiList, cmpFiTime);

perLine = lineSize/(maxNameSize+2);
if (perLine < 1)
    perLine = 1;

for (fi = fiList; fi != NULL; fi = fi->next)
    {
    if (doLong)
        {
        /* Print something like:
           drwxr-xr-x  512 Feb  3 12:29:19 patJobs/ */
        struct tm *tm = localtime(&fi->stat.st_mtime);
        char dirChar = '-';
        char readChar = '-';
        char writeChar = '-';
        char exeChar = '-';

        if (fi->stat.st_mode & _S_IFDIR)
            dirChar = 'd';
        if (fi->stat.st_mode & _S_IREAD)
            readChar = 'r';
        if (fi->stat.st_mode & _S_IWRITE)
            writeChar = 'w';
        if (fi->stat.st_mode & _S_IEXEC)
            exeChar = 'x';
        printf("%c%c%c%c %10lu %02d/%02d/%d %02d:%02d:%02d %s\n", 
            dirChar, readChar, writeChar, exeChar,
            fi->stat.st_size, 
            tm->tm_mon, tm->tm_mday, tm->tm_year+1900,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            fi->name);
        totalSize += fi->stat.st_size;
        }
    else
        {
        char buf[256];
        char extra = ' ';
//        if (fi->stat.st_mode & _S_IEXEC)
//            extra = '*';
        if (fi->stat.st_mode & _S_IFDIR)
            extra = '/';
        if (doOne)
            extra = ' ';
        sprintf(buf, "%s%c", fi->name, extra);
        printf("%-*s", maxNameSize+1, buf);
        if (doOne)
            putchar('\n');
        else
            {
            if (++linePos == perLine)
                {
                putchar('\n');
                linePos = 0;
                }
            else
                putchar(' ');
            }
        }
    if (checkAbort())
        {
        putchar('\n');
        retCode = slcError;
        goto CLEANUP;
        }
    }
if (!doOne && !doLong && linePos != 0)
    putchar('\n');
if (doLong)
    printf("%d files %1.0f bytes\n", fileCount, totalSize);
CLEANUP:
    {
    slFreeList(&fiList);
    slFreeList(&ourList);
    return retCode;
    }
}


struct redirector
/* This keeps track of input/output redirection. */
    {
    int old[3];     /* Old stdin (0) stdout (1) stderr (2) */
    int new[3];     /* New stdin, out, err. */
    };

int cpToStdout(char *name)
/* Copy file to standard out (unbuffered) */
{
int s,d;
int sizeRead;
char buf[8*1024];
int bufSize = sizeof(buf);
int retCode = slcOk;

clearAbort();
d = 1;      /* Standard out. */
s = open(name, _O_RDONLY | _O_BINARY);
if (s == -1)
    {
    warn("Couldn't open %s", name);
    return slcError;
    }
for (;;)
    {
    sizeRead = read(s, buf, bufSize);
    if (sizeRead > 0)
        {
        if (write(d, buf, sizeRead) < sizeRead)
            {
            fprintf(stderr, "Write failed.  Disk full?");
            retCode = slcError;
            break;
            }
        }
    if (sizeRead < bufSize)
        break;
    if (checkAbort())
        {
        fprintf(stderr, "\nAborting cat from <control>C\n");
        retCode = slcError;
        break;
        }                
    }
close(s);
return retCode;
}

int doCat(struct slName *tokLine, int tokCount, char *line)
/* List files. */
{
int retCode = slcOk;
struct slName *tok;

if (tokCount < 2)
    {
    fprintf(stderr, "cat - copy file(s) to standard output\n");
    return slcError;
    }
fflush(stdout);
for (tok = tokLine->next; tok != NULL; tok = tok->next)
    {
    if ((retCode = cpToStdout(tok->name)) != slcOk)
        break;
    }
return retCode;
}


boolean redirectOne(struct redirector *rd, char *fileName, int fileIx, int openMode, int sMode)
/* Redirect standard input, output, or error depending on fileIx. 
 * Returns FALSE on failure. */
{
int fd;
if (rd->old[fileIx] != -1)
    {
    fprintf(stderr, "Only one < and one > allowed per command\n");
    return FALSE;
    }

rd->new[fileIx] = fd = open(fileName, openMode, sMode);
if (fd == -1)
    {
    fprintf(stderr, "Couldn't open %s\n", fileName);
    return FALSE;
    }
if ((rd->old[fileIx] = _dup(fileIx)) < 0)
    {
    fprintf(stderr, "Couldn't dup %d, we're hosed!\n", fileIx);
    return FALSE;
    }
if (_dup2(fd, fileIx) < 0)
    {
    fprintf(stderr, "Couldn't dup2 %s and %d, we're hosed\n", fileName, fileIx);
    return FALSE;
    }
return TRUE;
}

struct slName *doRedirection(struct shell *sh, struct slName *tokList, struct redirector *rd)
/* Handle redirection - do necessary duplications and such of file handles.
 * Remove redirection character and files from token list. */
{
struct slName *newList = NULL, *next, *tok;
int i;

for (i=0; i<3; ++i)
    rd->new[i] = rd->old[i] = -1;
for (tok = tokList; tok != NULL; tok = next)
    {
    next = tok->next;
    if (tok->name[0] == '<')
        {
        freeMem(tok);
        tok = next;
        if (tok == NULL)
            {
            fprintf(stderr, "Need a file name after <\n");
            goto ABORT;
            }
        next = tok->next;
        if (!redirectOne(rd, tok->name, 0, _O_RDONLY, 0))
            goto ABORT;
        freeMem(tok);
        }
    else if (tok->name[0] == '>')
        {
        boolean errToo = FALSE;
        freeMem(tok);
        tok = next;
        if (tok == NULL)
            {
            fprintf(stderr, "Need a file name after >\n");
            goto ABORT;
            }
        next = tok->next;
        fflush(stdout);
        if (tok->name[0] == '&')
            {
            errToo = TRUE;
            freeMem(tok);
            tok = next;
            if (tok == NULL)
                {
                fprintf(stderr, "Need a file name after >&\n");
                goto ABORT;
                }
            next = tok->next;
            }
        if (!redirectOne(rd, tok->name, 1, _O_WRONLY|_O_CREAT|_O_TRUNC, _S_IREAD|_S_IWRITE ))
            goto ABORT;
        if (errToo)
            {
            fflush(stderr);
            rd->old[2] = _dup(2);
            dup2(rd->new[1], 2);
            }
        freeMem(tok);
        }
    else
        {
        slAddHead(&newList, tok);
        }
    }
slReverse(&newList);
return newList;

ABORT:
    {
    slFreeList(&tok);
    slFreeList(&newList);
    return NULL;
    }
}

void undoRedirection(struct redirector *rd)
/* Restore old standard in, out and stuff. */
{
int i;
int oldFd, newFd;
boolean gotAny = FALSE;

if (rd->old[1] != -1)
    fflush(stdout);
if (rd->old[2] != -1)
    fflush(stderr);

for (i=0; i<3; ++i)
    {
    if ((oldFd = rd->old[i]) != -1)
        {
        newFd = rd->new[i];
        _dup2(oldFd, i);
        if (i != 2)
            _close(newFd);
        rd->old[i] = rd->new[i] = -1;
        gotAny = TRUE;
        }
    }                                                                   
fflush(stdout);
fflush(stderr);
}

int shellLine(struct shell *sh, char *line, FILE *f)
/* Do processing of one line of user input. Return FALSE
 * to quit. */
{
struct slName *tokLine;  /* Tokenized line. */
int retCode = slcOk;
struct redirector rd;

tokLine = tokenize(skipLeadingSpaces(line));
tokLine = lookupAliases(sh, tokLine);
tokLine = lookupVariables(sh, tokLine);
tokLine = stripSpaceTokens(tokLine);
tokLine = lookupWildcards(sh, tokLine);
tokLine = doRedirection(sh, tokLine, &rd);
if (tokLine != NULL)
    {
    char *command = tokLine->name;
    int tokCount = slCount(tokLine);

    if (sameWord(command, "quit") || sameWord(command, "exit"))
        retCode = slcEof;
    else if (sameString(command, "cd") && tokCount > 1)
        {
        struct slName *dirName = tokLine->next;
        char *dir = dirName->name;
        int err = chdir(dir);
        if (err < 0)
            {
            warn("Couldn't change directory to %s", dir);
            retCode = slcError;
            }
        }
    else if (sameString(command, "pwd"))
        {
        char line[512];
        getcwd(line, sizeof(line));
        puts(line);
        }
    else if (sameString(command, "history"))
        {
        int count = dlCount(sh->history);
        int start = sh->commandsSeen - count + 1;
        int i = start;
        struct dlNode *hNode;
        for (hNode = sh->history->head; hNode->next != NULL; hNode = hNode->next)
            {
            printf("%d %s\n", i, hNode->val);
            ++i;
            }
        }
    else if (sameString(command, "alias"))
        {
        if (tokCount < 3)
            {
            fprintf(stderr, "alias - creates a shortcut for a command\n"
                            "usage\n"
                            "   alias  shortcut  command\n");
            retCode = slcError;
            }
        else
            {
            struct slName *tok = tokLine->next;
            command = tok->name;
            flattenOutput(sh, tok->next, " ");
            hashAdd(sh->aliases, command, cloneString(sh->outBuf));
            }
        }
    else if (sameWord(command, "set"))
        {
        if (tokCount < 4)
            {
            setUsage();
            retCode = slcError;
            }
        else
            {
            struct slName *tok = tokLine->next;
            char *var = tok->name;
            tok = tok->next;
            if (!sameString(tok->name, "="))
                setUsage();
            else
                {
                flattenOutput(sh, tok->next, " ");
                hashAdd(sh->variables, var, cloneString(sh->outBuf));
                }
            }
        }
    else if (sameString(command, "source"))
        {
        if (tokCount != 2)
            {
            fprintf(stderr, "source - execute a batch file\n"
                            "usage:\n"
                            "    source file\n");
            retCode = slcError;
            }
        else
            {
            retCode = noHistoryShell(sh, tokLine->next->name);
            }
        }
    else if (sameString(command, "foreach"))
        {
        retCode = doForEach(sh, tokLine, f);
        }
    else if (sameString(command, "rm"))
        {
        if (tokCount < 2)
            {
            fprintf(stderr, "rm - remove files\n"
                            "usage:\n"
                            "    rm file(s)\n");
            }
        else
            {
            struct slName *tok = tokLine;
            while ((tok = tok->next) != NULL)
                {
                if (remove(tok->name) < 0)
                    {
                    fprintf(stderr, "Couldn't remove %s.\n", tok->name);
                    break;
                    }
                }
            }
        }
    else if (sameString(command, "mv"))
        {
        retCode = doCpMv(tokLine, tokCount, line, TRUE);
        }
    else if (sameString(command, "cp"))
        {
        retCode = doCpMv(tokLine, tokCount, line, FALSE);
        }
    else if (sameString(command, "ls"))
        {
        retCode = doLs(tokLine, tokCount, line);
        }
    else if (sameString(command, "cat"))
        {
        retCode = doCat(tokLine, tokCount, line);
        }
    else if (sameString(command, "echo"))
        {
        struct slName *tok;
        for (tok = tokLine->next; tok != NULL; tok = tok->next)
            {
            fputs(tok->name, stdout);
            if (tok->next != NULL)
                fputc(' ', stdout);
            }
        fputc('\n', stdout);
        }
    else
        {
        retCode = doSystem(sh, tokLine);
        }
    }
undoRedirection(&rd);
slFreeList(&tokLine);
return retCode;
}

void maskControlC()
/* Make controlC not terminate us. */
{
signal(SIGINT, handleControlC);
}

void interactiveShell(struct shell *sh)
/* Interpret shell on terminal input. */
{
maskControlC();
for (;;)
    {
    enterLine(sh->history, sh->prompt, sh->inBuf, sh->inBufAlloc);
    if (!doHistory(sh))
        continue;
    if (shellLine(sh, sh->inBuf, stdin) == slcEof)
        break;
    }
}


int main(int argc, char *argv[])
{
struct shell *sh;
char *cshrc = "C:\\csh.rc";

makeIsNameChar();
sh = newShell();

if (fileExists(cshrc))
    noHistoryShell(sh, cshrc);
if (argc == 2)
    {
    noHistoryShell(sh, argv[1]);
    }
else
    {
    interactiveShell(sh);
    }
return 0;
}





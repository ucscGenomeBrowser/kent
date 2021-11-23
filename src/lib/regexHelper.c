/* regexHelper: easy wrappers on POSIX Extended Regular Expressions (man 7 regex, man 3 regex) */

/* Copyright (C) 2012 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "regexHelper.h"
#include "hash.h"

const regex_t *regexCompile(const char *exp, const char *description, int compileFlags)
/* Compile exp (or die with an informative-as-possible error message).
 * Cache pre-compiled regex's internally (so don't free result after use). */
{
static struct hash *reHash = NULL;
struct hashEl *hel = NULL;
char key[512];
safef(key, sizeof(key), "%d.%s", compileFlags, exp);

if (reHash == NULL)
    reHash = newHash(10);
hel = hashLookup(reHash, key);
if (hel != NULL)
    return((regex_t *)hel->val);
else
    {
    regex_t *compiledExp = NULL;
    int errNum = 0;
    AllocVar(compiledExp);
    errNum = regcomp(compiledExp, exp, compileFlags);
    if (errNum != 0)
	{
	char errBuf[512];
	regerror(errNum, compiledExp, errBuf, sizeof(errBuf));
	errAbort("%s \"%s\" got regular expression compilation error %d:\n%s\n",
		 description, exp, errNum, errBuf);
	}
    hashAdd(reHash, key, compiledExp);
    return(compiledExp);
    }
}

static boolean regexMatchSubstrMaybeCase(const char *string, const char *exp,
					 regmatch_t substrArr[], size_t substrArrSize,
					 boolean isCaseInsensitive)
/* Return TRUE if string matches regular expression exp;
 * regexec fills in substrArr with substring offsets. */
{
if (string == NULL)
    return FALSE;
int compileFlags = REG_EXTENDED;
char desc[256];
safecpy(desc, sizeof(desc), "Regular expression");
if (isCaseInsensitive)
    {
    compileFlags |= REG_ICASE;
    safecat(desc, sizeof(desc), " (case insensitive)");
    }
if (substrArr == NULL)
    compileFlags |= REG_NOSUB;
else
    safecat(desc, sizeof(desc), " with substrings");

const regex_t *compiledExp = regexCompile(exp, desc, compileFlags);
return(regexec(compiledExp, string, substrArrSize, substrArr, 0) == 0);
}

boolean regexMatch(const char *string, const char *exp)
/* Return TRUE if string matches regular expression exp (case sensitive). */
{
return regexMatchSubstrMaybeCase(string, exp, NULL, 0, FALSE);
}

boolean regexMatchNoCase(const char *string, const char *exp)
/* Return TRUE if string matches regular expression exp (case insensitive). */
{
return regexMatchSubstrMaybeCase(string, exp, NULL, 0, TRUE);
}

boolean regexMatchSubstr(const char *string, const char *exp,
			 regmatch_t substrArr[], size_t substrArrSize)
/* Return TRUE if string matches regular expression exp (case sensitive);
 * regexec fills in substrArr with substring offsets. */
{
return regexMatchSubstrMaybeCase(string, exp, substrArr, substrArrSize, FALSE);
}

boolean regexMatchSubstrNoCase(const char *string, const char *exp,
			       regmatch_t substrArr[], size_t substrArrSize)
/* Return TRUE if string matches regular expression exp (case insensitive);
 * regexec fills in substrArr with substring offsets. */
{
return regexMatchSubstrMaybeCase(string, exp, substrArr, substrArrSize, TRUE);
}

void regexSubstringCopy(const char *string, const regmatch_t substr,
                        char *buf, size_t bufSize)
/* Copy a substring from string into buf using start and end offsets from substr.
 * If the substring was not matched then make buf an empty string. */
{
if (regexSubstrMatched(substr))
    safencpy(buf, bufSize, string + substr.rm_so, substr.rm_eo - substr.rm_so);
else
    *buf = '\0';
}

char *regexSubstringClone(const char *string, const regmatch_t substr)
/* Clone and return a substring from string using start and end offsets from substr.
 * If the substring was not matched then return a cloned empty string. */
{
char *clone = NULL;
if (regexSubstrMatched(substr))
    {
    int len = substr.rm_eo - substr.rm_so;
    clone = needMem(len + 1);
    regexSubstringCopy(string, substr, clone, len + 1);
    }
else
    clone = cloneString("");
return clone;
}

int regexSubstringInt(const char *string, const regmatch_t substr)
/* Return the integer value of the substring specified by substr.
 * If substr was not matched, return 0; you can check first with regexSubstrMatched() if
 * that's not the desired behavior for unmatched substr. */
{
int val = 0;
if (regexSubstrMatched(substr))
    {
    int len = substr.rm_eo - substr.rm_so;
    char buf[len+1];
    regexSubstringCopy(string, substr, buf, sizeof(buf));
    val = atoi(buf);
    }
else
    val = 0;
return val;
}

#ifdef NOTNOW
/* THIS CODE DOESN'T PARSE REGULAR EXPRESSIONS CORRECTLY IN THAT
 * IT ASSUMES THAT THE ORDER OF PARENTHETICAL EXPRESSIONS IN THE OUTPUT
 * IS THE SAME ORDER THEY APPEAR IN THE INPUT
 */
static struct regexSnippet *parseSnippets(char *input)
/* Generate a data structure that describes how the parenthetical
 * regular expressions should be substituted in the output.
 */
{
char *in = input;
struct regexSnippet *out = NULL;

char *prev = input;
for(;;)
    {
    if ((*in == 0) || ((*in == '\\') && isdigit(in[1])))
        {
        struct regexSnippet *snippet;
        AllocVar(snippet);

        int size = in - prev;
        if (size)
            {
            char buffer[size + 1];
            strncpy(buffer, prev, size);
            buffer[size] = 0;
            snippet->precursor = cloneString(buffer);
            snippet->precursorLen = size;
            prev = in;
            }

        if (*in)
            {
            in++;
            snippet->num = atoi(in);
            while (isdigit(*in))
                in++;
            }

        slAddHead(&out, snippet);
        if (*in == 0)
            break;
        }
    else
        in++;
    }

slReverse(&out);

return out;
}

static struct regexCompiledEdit *compileEdits(struct regexEdit *editArray, unsigned numEdits, boolean quiet)
/* Compile all the edits. */
{
struct regexCompiledEdit *compiledEdits, *compiledEdit;

AllocArray(compiledEdits, numEdits);
compiledEdit = compiledEdits;

for(; numEdits; numEdits--, editArray++, compiledEdit++)
    {
    regex_t *compiledExp = NULL;
    int errNum = 0;
    int compileFlags = 0;

    AllocVar(compiledExp);
    errNum = regcomp(compiledExp, editArray->query, compileFlags);

    if (errNum != 0)
        {
        if (quiet)
            return NULL;

        char errBuf[4096];
        regerror(errNum, compiledExp, errBuf, sizeof(errBuf));
        errAbort("regular expression compilation error %d: %s", errNum, errBuf);
        }

    compiledEdit->compiledExp = compiledExp;
    compiledEdit->snippets = parseSnippets(editArray->substitution);
    }

return compiledEdits;
}

static char *doSubEdits(struct regexSnippet *snippets, regmatch_t *matches, char *source, int *plength)
/* Do the substitions on parenthetical expressions. */
{
char output[40 * 1024], *out = output;
*out = 0;

for(; snippets ; matches++, snippets = snippets->next)
    {
    // copy the part before the match
    strncpy(out, snippets->precursor, snippets->precursorLen);
    out += snippets->precursorLen;
    *plength += snippets->precursorLen;

    if (matches->rm_so == -1)
        break;

    // copy in the part that matches the regular expression
    int size = matches->rm_eo - matches->rm_so;
    strncpy(out, &source[matches->rm_so], size);
    *plength += size;
    out += size;
    }
*out = 0;
return cloneString(output);
}

static char *doOneEdit( struct regexCompiledEdit *edit, char *input, boolean quiet)
/* Perform one edit on the input string.  Errabort if !quiet and there is an error. */
{
char buffer[40 * 1024];
char *source = input;
regmatch_t matches[1024];
int lastSrc = 0;
int offset = 0;

for(;;)
    {
    /* if there's not a match, we're done. */
    if (regexec(edit->compiledExp, source, ArraySize(matches), matches, 0))
        break;

    int size =  matches->rm_so;
    strncpy(&buffer[lastSrc], source, size);
    lastSrc += size;

    int subSize = 0;
    // do the substitions on any matching parenthetical expressions
    char *subEdit = doSubEdits(edit->snippets, matches+1, source, &subSize);

    strncpy(&buffer[lastSrc], subEdit, subSize);
    lastSrc += subSize;
    offset += matches->rm_eo;
    //
    // keep looking after the last match
    source = &input[offset];
    }

strcpy(&buffer[lastSrc], source);

return cloneString(buffer);
}

char *regexEdit(struct regexEdit *editArray, unsigned numEdits, char *input, boolean quiet)
/* Perform a list of edits on a string. */
{
struct regexCompiledEdit *compiledEdits = compileEdits(editArray, numEdits, quiet);

if (compiledEdits == NULL)
    return FALSE;

char *outString = input;
for(; numEdits && outString; compiledEdits++, numEdits--)
    outString = doOneEdit(compiledEdits, outString, quiet);

return outString;
}
#endif

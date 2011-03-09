/* regexHelper: easy wrappers on POSIX Extended Regular Expressions (man 7 regex, man 3 regex) */

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


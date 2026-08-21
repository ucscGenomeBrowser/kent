/* htmlSanitize - reduce a piece of HTML that came from outside to an allowlist of
 * elements, attributes and style properties.
 *
 * An element on the keep list survives with its allowlisted attributes.  A short list of
 * elements that carry nothing a reader needs, script and style and form among them, is
 * removed together with everything inside.  Every other element loses its tag but keeps
 * its text, which is what lets a whole pasted document come out as the article it was
 * meant to be.
 *
 * The parser here is deliberately forgiving.  It never aborts and it always returns
 * something, however broken the markup it is handed. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "htmlSanitize.h"

/* Elements we keep. */
static char *keepElements =
    "p br hr div span center address blockquote pre "
    "h1 h2 h3 h4 h5 h6 "
    "a b strong i em u s strike small big sub sup code tt kbd samp var cite "
    "abbr acronym dfn mark del ins q wbr font "
    "ul ol li dl dt dd "
    "table caption thead tbody tfoot tr th td col colgroup "
    "img figure figcaption "
    "section article header footer main aside nav details summary "
    "iframe";
    /* iframe is here but only survives when its source is a video host, see iframeSrcOk. */

/* Elements that go away with everything inside them. */
static char *killElements =
    "script style noscript template svg math frame frameset "
    "object embed applet param form input button select option optgroup "
    "textarea label fieldset legend base meta link title "
    "audio video source track canvas map area marquee dialog slot portal xml";

/* Elements written without a closing tag.  Killing one of these takes no content with it. */
static char *voidElements =
    "area base br col embed frame hr img input link meta param source track wbr";

/* Killed elements that never showed a reader anything, so there is nothing to tell the
 * hub author about. */
static char *silentKillElements =
    "title meta link base param source track xml slot portal template noscript";

/* Elements whose content is text rather than markup.  When one of these is never closed we
 * drop the rest of the input rather than pour its content onto the page as text. */
static char *rawTextElements = "script style textarea title noscript template xml";

/* Attributes allowed, by element.  The first row is for every kept element. */
struct attrRule
    {
    char *element;              /* Element name, or "*" for all of them. */
    char *attrs;                /* Space separated attribute names. */
    };

static struct attrRule attrRules[] = {
    {"*",        "title dir lang style id"},
    {"a",        "href target rel name"},
    {"img",      "src alt width height border align hspace vspace"},
    {"table",    "width border cellpadding cellspacing align bgcolor summary"},
    {"td",       "colspan rowspan align valign width height nowrap bgcolor scope"},
    {"th",       "colspan rowspan align valign width height nowrap bgcolor scope"},
    {"tr",       "align valign bgcolor"},
    {"col",      "span width align valign"},
    {"colgroup", "span width align valign"},
    {"ol",       "start type reversed"},
    {"ul",       "type"},
    {"li",       "type value"},
    {"font",     "color face size"},
    {"hr",       "width size align noshade"},
    {"p",        "align"},
    {"div",      "align"},
    {"span",     "align"},
    {"h1",       "align"},
    {"h2",       "align"},
    {"h3",       "align"},
    {"h4",       "align"},
    {"h5",       "align"},
    {"h6",       "align"},
    {"caption",  "align"},
    {"thead",    "align valign"},
    {"tbody",    "align valign"},
    {"tfoot",    "align valign"},
    {"iframe",   "src width height frameborder allowfullscreen allow loading"},
};

/* Properties allowed inside a style attribute. */
static char *styleProperties =
    "text-align text-align-last vertical-align white-space word-break word-wrap overflow-wrap "
    "padding padding-top padding-bottom padding-left padding-right "
    "margin margin-top margin-bottom margin-left margin-right "
    "border border-top border-bottom border-left border-right border-color border-style "
    "border-width border-radius border-collapse border-spacing "
    "width height min-width max-width min-height max-height "
    "color background-color "
    "font font-size font-weight font-style font-family font-variant "
    "line-height letter-spacing text-decoration text-transform text-indent "
    "list-style list-style-type list-style-position "
    "float clear display overflow overflow-x overflow-y opacity";

/* URL schemes allowed in href and src.  A URL with no scheme at all is allowed too. */
static char *urlSchemes = "http https mailto ftp";

/* Hosts an iframe may point at. */
static char *videoHosts =
    "www.youtube.com youtube.com www.youtube-nocookie.com youtube-nocookie.com youtu.be "
    "player.vimeo.com vimeo.com";

/* Nesting past this depth is not a document, it is a way to make us emit a huge page. */
#define maxNestDepth 256

static struct hash *keepHash = NULL, *killHash = NULL, *voidHash = NULL, *rawTextHash = NULL;
static struct hash *silentKillHash = NULL;
static struct hash *attrHash = NULL, *stylePropHash = NULL, *schemeHash = NULL, *videoHostHash = NULL;

static struct hash *hashOfWords(char *words, int sizePow2)
/* Return a hash holding each space separated word in words. */
{
struct hash *hash = hashNew(sizePow2);
char *dupe = cloneString(words);
char *word, *s = dupe;
while ((word = nextWord(&s)) != NULL)
    hashAdd(hash, word, NULL);
freeMem(dupe);
return hash;
}

static void initTables()
/* Build the lookup hashes on first use. */
{
if (keepHash != NULL)
    return;
killHash = hashOfWords(killElements, 7);
silentKillHash = hashOfWords(silentKillElements, 5);
voidHash = hashOfWords(voidElements, 6);
rawTextHash = hashOfWords(rawTextElements, 5);
stylePropHash = hashOfWords(styleProperties, 8);
schemeHash = hashOfWords(urlSchemes, 4);
videoHostHash = hashOfWords(videoHosts, 4);
attrHash = hashNew(9);
int i;
for (i = 0;  i < ArraySize(attrRules);  ++i)
    {
    char *dupe = cloneString(attrRules[i].attrs);
    char *attr, *s = dupe;
    while ((attr = nextWord(&s)) != NULL)
        {
        char key[256];
        safef(key, sizeof key, "%s.%s", attrRules[i].element, attr);
        hashAdd(attrHash, key, NULL);
        }
    freeMem(dupe);
    }
keepHash = hashOfWords(keepElements, 7);        /* last, it is the flag that we are built */
}

struct sanitizer
/* State of one pass over a piece of HTML. */
    {
    struct dyString *out;       /* Sanitized HTML accumulates here. */
    struct slName *openStack;   /* Elements opened and not yet closed, innermost first. */
    int depth;                  /* Length of openStack. */
    struct hash *exhausted;     /* Elements we have already looked for a closing tag of
                                 * and not found, so there is no point looking again. */
    boolean report;             /* Collecting messages about what we removed? */
    struct hash *seen;          /* Messages reported already. */
    struct slName *removed;     /* Messages, in the order we first hit them. */
    };

static void noteRemoved(struct sanitizer *san, char *format, ...)
/* Record a message naming something we took out, once per distinct message. */
{
if (!san->report)
    return;
char message[512];
va_list args;
va_start(args, format);
vsnprintf(message, sizeof message, format, args);
va_end(args);
if (hashLookup(san->seen, message) != NULL)
    return;
hashAdd(san->seen, message, NULL);
slNameAddHead(&san->removed, message);
}

static char *findTagEnd(char *s)
/* Given s just inside a '<', return the '>' that ends the tag, stepping over quoted
 * attribute values.  Return NULL if the tag is never closed.  A quote only opens a value
 * where a value can start, which is how a browser reads it too, so a stray quote in the
 * middle of an unquoted value does not swallow the rest of the page. */
{
char quote = 0;
boolean expectValue = FALSE;    /* just past an '=', the value has not started yet */
boolean inBareValue = FALSE;    /* inside an unquoted value, where a quote is just a character */
for (;  *s != 0;  ++s)
    {
    if (quote != 0)
        {
        if (*s == quote)
            quote = 0;
        }
    else if (isspace((unsigned char)*s))
        inBareValue = FALSE;
    else if (inBareValue)
        {
        if (*s == '>')
            return s;
        }
    else if (expectValue)
        {
        expectValue = FALSE;
        if (*s == '"' || *s == '\'')
            quote = *s;
        else if (*s == '>')
            return s;
        else
            inBareValue = TRUE;
        }
    else if (*s == '=')
        expectValue = TRUE;
    else if (*s == '>')
        return s;
    }
return NULL;
}

static boolean isNameChar(char c)
/* Is c part of an element or attribute name? */
{
return isalnum((unsigned char)c) || c == ':' || c == '_' || c == '-' || c == '.';
}

static boolean allNameChars(char *s)
/* Is every character in s one that belongs in a name?  A message we hand back names an
 * attribute the page wrote, and that text can end up in a terminal. */
{
for (;  *s != 0;  ++s)
    {
    if (!isNameChar(*s))
        return FALSE;
    }
return TRUE;
}

static char *tagName(char *s, char *name, int nameSize)
/* Copy the element name starting at s into name, lower cased.  Return the first character
 * after the name. */
{
int i = 0;
while (isNameChar(*s))
    {
    if (i < nameSize-1)
        name[i++] = *s;
    ++s;
    }
name[i] = 0;
tolowers(name);
return s;
}

static char *skipToClose(char *s, char *name, boolean rawText)
/* s points just after the opening tag of name.  Return the first character after the
 * matching closing tag, or NULL if there is no closing tag. */
{
int depth = 1;
int nameLen = strlen(name);
char *p = s;
while ((p = strchr(p, '<')) != NULL)
    {
    boolean closing = (p[1] == '/');
    char *q = p + (closing ? 2 : 1);
    if (strncasecmp(q, name, nameLen) == 0 && !isNameChar(q[nameLen]))
        {
        char *e = findTagEnd(q);
        if (e == NULL)
            return NULL;
        if (closing)
            {
            depth -= 1;
            if (depth == 0)
                return e+1;
            }
        else if (!rawText && e[-1] != '/')
            depth += 1;
        p = e+1;
        }
    else
        p += 1;
    }
return NULL;
}

static char *nextAttribute(char *s, char *tagEnd,
                           char **retName, int *retNameLen, char **retVal, int *retValLen)
/* Pick the next attribute out of the text between s and tagEnd.  Return the first character
 * after it, or NULL when there are no more.  A name length of zero means junk we skipped. */
{
while (s < tagEnd && (isspace((unsigned char)*s) || *s == '/'))
    ++s;
if (s >= tagEnd)
    return NULL;
char *nameStart = s;
while (s < tagEnd && !isspace((unsigned char)*s) && *s != '=' && *s != '/')
    ++s;
*retName = nameStart;
*retNameLen = s - nameStart;
*retVal = NULL;
*retValLen = 0;
if (*retNameLen == 0)
    return s+1;                 /* junk, but keep moving */
char *afterName = s;
while (s < tagEnd && isspace((unsigned char)*s))
    ++s;
if (s >= tagEnd || *s != '=')
    return afterName;
++s;
while (s < tagEnd && isspace((unsigned char)*s))
    ++s;
if (s < tagEnd && (*s == '"' || *s == '\''))
    {
    char quote = *s++;
    *retVal = s;
    while (s < tagEnd && *s != quote)
        ++s;
    *retValLen = s - *retVal;
    if (s < tagEnd)
        ++s;
    }
else
    {
    *retVal = s;
    while (s < tagEnd && !isspace((unsigned char)*s))
        ++s;
    *retValLen = s - *retVal;
    }
return s;
}

static void appendEscaped(struct dyString *dy, char *s)
/* Append s as an attribute value, hiding the characters that could end the attribute or
 * start a tag.  Ampersands are left alone so that entities the author wrote stay as
 * they are. */
{
for (;  *s != 0;  ++s)
    {
    switch (*s)
        {
        case '"':
            dyStringAppend(dy, "&quot;");
            break;
        case '<':
            dyStringAppend(dy, "&lt;");
            break;
        case '>':
            dyStringAppend(dy, "&gt;");
            break;
        default:
            dyStringAppendC(dy, *s);
            break;
        }
    }
}

static char *decodeNumericRefs(char *s)
/* Return a copy of s with numeric character references turned into the characters they
 * name, which is what a browser does before it looks for a scheme.  Only &#NN and &#xNN
 * are decoded, with or without the closing semicolon, because that is what a browser
 * accepts.  A named entity is left alone and the caller refuses the URL over it.  A
 * character above ASCII cannot be part of a scheme, so one stand-in character does for
 * all of them. */
{
struct dyString *dy = dyStringNew(strlen(s)+1);
char *p = s;
while (*p != 0)
    {
    if (p[0] == '&' && p[1] == '#')
        {
        char *digits = p+2;
        int base = 10;
        if (*digits == 'x' || *digits == 'X')
            {
            base = 16;
            digits += 1;
            }
        char *end = NULL;
        errno = 0;
        long value = strtol(digits, &end, base);
        if (end != digits)
            {
            if (*end == ';')
                end += 1;
            if (value > 0 && value < 128 && errno == 0)
                dyStringAppendC(dy, (char)value);
            else
                dyStringAppendC(dy, '~');
            p = end;
            continue;
            }
        }
    dyStringAppendC(dy, *p);
    p += 1;
    }
return dyStringCannibalize(&dy);
}

static char *urlScheme(char *val, boolean *retSuspect)
/* Return the scheme of val, lower cased and freshly allocated, or NULL if it has none.
 * Set retSuspect when the text in front of the path holds something that could hide a
 * scheme from us and still be one to a browser: a named entity, a backslash, or a control
 * character.  Reading the text this way, rather than copying every rule a browser has for
 * repairing a broken URL, is the point.  Matching those rules exactly is how a check like
 * this gets beaten. */
{
*retSuspect = FALSE;
char *decoded = decodeNumericRefs(val);
char *s = decoded;
while (*s != 0 && (unsigned char)*s <= ' ')
    ++s;
char *scheme = NULL;
char *p;
for (p = s;  *p != 0;  ++p)
    {
    if (*p == '/' || *p == '?' || *p == '#')
        break;                          /* a path, query or anchor starts, so no scheme */
    if (*p == '&' || *p == '\\' || (unsigned char)*p < ' ' || *p == 0x7f)
        {
        *retSuspect = TRUE;
        break;
        }
    if (*p == ':')
        {
        int len = p - s;
        char buf[33];
        if (len < 1 || len >= sizeof buf)
            {
            *retSuspect = TRUE;
            break;
            }
        memcpy(buf, s, len);
        buf[len] = 0;
        tolowers(buf);
        boolean plain = isalpha((unsigned char)buf[0]);
        char *c;
        for (c = buf;  plain && *c != 0;  ++c)
            {
            if (!isalnum((unsigned char)*c) && *c != '+' && *c != '.' && *c != '-')
                plain = FALSE;
            }
        if (plain)
            scheme = cloneString(buf);
        else
            *retSuspect = TRUE;
        break;
        }
    }
freeMem(decoded);
return scheme;
}

static boolean urlOk(char *val, struct sanitizer *san)
/* Is this a URL we are willing to print? */
{
boolean suspect = FALSE;
char *scheme = urlScheme(val, &suspect);
if (suspect)
    {
    noteRemoved(san, "removed a link that does not read as a plain web address");
    return FALSE;
    }
if (scheme == NULL)
    return TRUE;
boolean ok = (hashLookup(schemeHash, scheme) != NULL);
if (!ok)
    noteRemoved(san, "removed a link that used the %s: scheme", scheme);
freeMem(scheme);
return ok;
}

static boolean iframeSrcOk(char *src)
/* Does src point at one of the video hosts we allow in a frame? */
{
if (isEmpty(src))
    return FALSE;
boolean suspect = FALSE;
char *scheme = urlScheme(src, &suspect);
if (suspect)
    return FALSE;
if (scheme != NULL)
    {
    boolean https = sameString(scheme, "https");
    freeMem(scheme);
    if (!https)
        return FALSE;
    }
else if (!startsWith("//", skipLeadingSpaces(src)))
    return FALSE;
char *host = stringIn("//", src);
if (host == NULL)
    return FALSE;
host += 2;
int len = strcspn(host, "/?#:");
char hostName[256];
if (len >= sizeof hostName)
    return FALSE;
memcpy(hostName, host, len);
hostName[len] = 0;
tolowers(hostName);
return (hashLookup(videoHostHash, hostName) != NULL);
}

static void stripCssComments(char *s)
/* Blank out CSS comments in place. */
{
char *open;
while ((open = stringIn("/*", s)) != NULL)
    {
    char *close = stringIn("*/", open+2);
    char *end = (close == NULL ? open + strlen(open) : close+2);
    while (open < end)
        *open++ = ' ';
    s = end;
    }
}

static char *filterStyle(char *val, struct sanitizer *san)
/* Return the declarations of val that we allow, or NULL if none of them survive. */
{
char *dupe = cloneString(val);
stripCssComments(dupe);
struct dyString *out = dyStringNew(strlen(dupe)+1);
char *decl = dupe;
while (decl != NULL && *decl != 0)
    {
    char *next = strchr(decl, ';');
    if (next != NULL)
        *next++ = 0;
    char *colon = strchr(decl, ':');
    if (colon != NULL)
        {
        *colon = 0;
        char *prop = trimSpaces(decl);
        char *value = trimSpaces(colon+1);
        tolowers(prop);
        if (isNotEmpty(prop) && isNotEmpty(value))
            {
            char *lower = cloneString(value);
            tolowers(lower);
            if (hashLookup(stylePropHash, prop) == NULL)
                ;                       /* not a property we print, and nothing to explain */
            else if (stringIn("url(", lower) != NULL || stringIn("expression", lower) != NULL
                     || strchr(lower, '\\') != NULL)
                noteRemoved(san, "removed the value of the style property %s", prop);
            else
                dyStringPrintf(out, "%s:%s;", prop, value);
            freeMem(lower);
            }
        }
    decl = next;
    }
freeMem(dupe);
if (out->stringSize == 0)
    {
    dyStringFree(&out);
    return NULL;
    }
return dyStringCannibalize(&out);
}

static void writeAttributes(struct sanitizer *san, char *element, char *attrText, char *tagEnd)
/* Write the attributes of element that we allow, from the text between attrText and tagEnd. */
{
boolean isAnchor = sameString(element, "a");
boolean isFrame = sameString(element, "iframe");
boolean hasTarget = FALSE;
char *relValue = NULL;
char *s = attrText;
char *name, *val;
int nameLen, valLen;
while ((s = nextAttribute(s, tagEnd, &name, &nameLen, &val, &valLen)) != NULL)
    {
    if (nameLen == 0 || nameLen > 128)
        continue;
    char attr[129];
    memcpy(attr, name, nameLen);
    attr[nameLen] = 0;
    tolowers(attr);
    char key[256];
    safef(key, sizeof key, "%s.%s", element, attr);
    if (hashLookup(attrHash, key) == NULL)
        {
        safef(key, sizeof key, "*.%s", attr);
        if (hashLookup(attrHash, key) == NULL)
            {
            if (startsWith("on", attr) && allNameChars(attr))
                noteRemoved(san, "removed the attribute %s", attr);
            continue;
            }
        }
    char *value = cloneStringZ(val == NULL ? "" : val, valLen);
    if (sameString(attr, "style"))
        {
        char *style = filterStyle(value, san);
        if (style != NULL)
            {
            dyStringAppend(san->out, " style=\"");
            appendEscaped(san->out, style);
            dyStringAppendC(san->out, '"');
            freeMem(style);
            }
        }
    else if (sameString(attr, "href") || sameString(attr, "src"))
        {
        if (urlOk(value, san))
            {
            dyStringPrintf(san->out, " %s=\"", attr);
            appendEscaped(san->out, value);
            dyStringAppendC(san->out, '"');
            }
        }
    else if (isAnchor && sameString(attr, "rel"))
        {
        freez(&relValue);
        relValue = cloneString(value);
        }
    else
        {
        if (isAnchor && sameString(attr, "target"))
            hasTarget = TRUE;
        dyStringPrintf(san->out, " %s=\"", attr);
        appendEscaped(san->out, value);
        dyStringAppendC(san->out, '"');
        }
    freeMem(value);
    }
if (isAnchor && (hasTarget || relValue != NULL))
    {
    /* A link that opens a new window hands that window a handle back to ours unless we
     * say otherwise. */
    dyStringAppend(san->out, " rel=\"");
    if (relValue != NULL)
        {
        appendEscaped(san->out, relValue);
        dyStringAppendC(san->out, ' ');
        }
    dyStringAppend(san->out, "noopener noreferrer\"");
    }
if (isFrame)
    dyStringAppend(san->out, " sandbox=\"allow-scripts allow-same-origin allow-popups"
                             " allow-presentation\"");
freez(&relValue);
}

static char *attributeValue(char *attrText, char *tagEnd, char *wanted)
/* Return a copy of the value of the named attribute, or NULL if the tag has no such
 * attribute. */
{
char *s = attrText;
char *name, *val;
int nameLen, valLen;
while ((s = nextAttribute(s, tagEnd, &name, &nameLen, &val, &valLen)) != NULL)
    {
    if (nameLen > 0 && nameLen == strlen(wanted) && strncasecmp(name, wanted, nameLen) == 0)
        return cloneStringZ(val == NULL ? "" : val, valLen);
    }
return NULL;
}

static void closeThrough(struct sanitizer *san, char *name)
/* Close name, and anything opened inside it, if name is open at all. */
{
struct slName *el;
boolean found = FALSE;
for (el = san->openStack;  el != NULL;  el = el->next)
    {
    if (sameString(el->name, name))
        {
        found = TRUE;
        break;
        }
    }
if (!found)
    return;
while (san->openStack != NULL)
    {
    struct slName *top = slPopHead(&san->openStack);
    san->depth -= 1;
    dyStringPrintf(san->out, "</%s>", top->name);
    boolean done = sameString(top->name, name);
    freeMem(top);
    if (done)
        break;
    }
}

static void sanitizeOnePass(char *html, struct sanitizer *san)
/* Walk html, writing what we allow into san->out. */
{
char *s = html;
while (*s != 0)
    {
    char *lt = strchr(s, '<');
    if (lt == NULL)
        {
        dyStringAppend(san->out, s);
        break;
        }
    if (lt > s)
        dyStringAppendN(san->out, s, lt - s);
    s = lt;
    if (startsWith("<!--", s))
        {
        char *end = stringIn("-->", s+4);
        s = (end == NULL ? s + strlen(s) : end+3);
        continue;
        }
    if (s[1] == '!' || s[1] == '?')
        {
        char *end = strchr(s, '>');
        s = (end == NULL ? s + strlen(s) : end+1);
        continue;
        }
    boolean closing = (s[1] == '/');
    char *nameStart = s + (closing ? 2 : 1);
    if (!isalpha((unsigned char)*nameStart))
        {
        dyStringAppendC(san->out, '<');
        s += 1;
        continue;
        }
    char *tagEnd = findTagEnd(nameStart);
    if (tagEnd == NULL)
        break;                          /* tag with no end, drop what is left */
    char name[64];
    char *attrText = tagName(nameStart, name, sizeof name);
    s = tagEnd + 1;
    if (closing)
        {
        if (hashLookup(keepHash, name) != NULL && hashLookup(voidHash, name) == NULL)
            closeThrough(san, name);
        continue;
        }
    boolean isVoid = (hashLookup(voidHash, name) != NULL);
    boolean kill = (hashLookup(killHash, name) != NULL);
    boolean noted = FALSE;
    if (!kill && sameString(name, "iframe"))
        {
        char *src = attributeValue(attrText, tagEnd, "src");
        kill = !iframeSrcOk(src);
        if (kill)
            {
            noteRemoved(san, "removed an iframe, we only allow one that plays a video "
                             "from a site we know");
            noted = TRUE;
            }
        freeMem(src);
        }
    if (kill)
        {
        if (!isVoid && tagEnd[-1] != '/')
            {
            boolean rawText = (hashLookup(rawTextHash, name) != NULL);
            /* Once the search for a closing tag has run off the end of the input, every
             * later search for that same tag will too, and repeating it on a page built
             * of thousands of unclosed tags would cost us a pass each time. */
            char *afterClose = NULL;
            if (hashLookup(san->exhausted, name) == NULL)
                {
                afterClose = skipToClose(s, name, rawText);
                if (afterClose == NULL)
                    hashAdd(san->exhausted, name, NULL);
                }
            if (afterClose != NULL)
                s = afterClose;
            else if (rawText)
                s += strlen(s);         /* never closed, and its content is not for reading */
            }
        if (!noted && hashLookup(silentKillHash, name) == NULL)
            noteRemoved(san, "removed the %s element and everything inside it", name);
        continue;
        }
    if (hashLookup(keepHash, name) == NULL)
        continue;                       /* tag goes, text inside it stays */
    if (!isVoid && san->depth >= maxNestDepth)
        continue;
    dyStringPrintf(san->out, "<%s", name);
    writeAttributes(san, name, attrText, tagEnd);
    dyStringAppendC(san->out, '>');
    if (!isVoid)
        {
        /* A trailing slash does not close an element like this one, whatever the author
         * meant by it, so remember it as open.  Anything still open at the end is closed
         * for us, which stops a page ending up inside a hub's div. */
        slNameAddHead(&san->openStack, name);
        san->depth += 1;
        }
    }
while (san->openStack != NULL)
    {
    struct slName *top = slPopHead(&san->openStack);
    dyStringPrintf(san->out, "</%s>", top->name);
    freeMem(top);
    }
}

char *htmlSanitizeReport(char *html, struct slName **retRemoved)
/* Like htmlSanitize, and if retRemoved is not NULL also return a list of one-line messages
 * naming each kind of thing that was removed.  The list is NULL when nothing was removed. */
{
if (retRemoved != NULL)
    *retRemoved = NULL;
if (html == NULL)
    return NULL;
initTables();
struct sanitizer san;
ZeroVar(&san);
san.out = dyStringNew(strlen(html) + 128);
san.report = (retRemoved != NULL);
san.exhausted = hashNew(6);
if (retRemoved != NULL)
    san.seen = hashNew(0);
sanitizeOnePass(html, &san);
hashFree(&san.exhausted);
if (retRemoved != NULL)
    {
    slReverse(&san.removed);
    *retRemoved = san.removed;
    hashFree(&san.seen);
    }
return dyStringCannibalize(&san.out);
}

char *htmlSanitize(char *html)
/* Return a cloned copy of html holding only allowlisted elements, attributes and style
 * properties. */
{
return htmlSanitizeReport(html, NULL);
}

/* dotBracketTrack - overlay an RNA secondary structure, written as a dot-bracket
 * string in a named bigBed column, onto a type bigBed track.
 *
 * The string covers the spliced item: one character per base of the blocks, in
 * transcript orientation, so its length is the sum of the block sizes.  Spaces pad
 * bases with no prediction and draw nothing.  The characters are drawn over the item
 * whenever a base is wide enough to hold one; at wider views the item is left as the
 * plain bed that linkedFeaturesDrawAt already drew.
 *
 * This is the bigBed counterpart of rnaFoldTrack.c, which does the same thing from an
 * rnaSecStr table.  No code is shared: those items are struct rnaSecStr covering one
 * contiguous span, these are struct linkedFeatures with blocks.
 *
 * Bad data warns and falls back to drawing a plain bed rather than aborting.  Both load
 * paths in hgTracks.c do catch an errAbort out of loadItems, but the message it leaves
 * is whatever text happened to abort, the rest of the rows are lost with it, and
 * nothing catches an errAbort raised while drawing.  Reporting through networkErrMsg
 * keeps the message specific, keeps the other items, and works on either path.
 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "dystring.h"
#include "bigBed.h"
#include "hgTracks.h"
#include "dotBracketTrack.h"

static char *dotBracketOpens  = "([{<";
static char *dotBracketCloses = ")]}>";

static void dotBracketSwapBracket(char *c)
/* Swap one bracket character for its mate, leaving anything else alone. */
{
char *open = strchr(dotBracketOpens, *c);
char *close = strchr(dotBracketCloses, *c);
if (open != NULL)
    *c = dotBracketCloses[open - dotBracketOpens];
else if (close != NULL)
    *c = dotBracketOpens[close - dotBracketCloses];
}

static void dotBracketReverse(char *dotBracket)
/* Turn a string stored in transcript orientation into genomic left to right order.
 * Like reverseFold in rnautil.c, but also swaps the pseudoknot brackets [] {} <>,
 * which extended dot-bracket from RNAfold and friends uses. */
{
char *c;
reverseBytes(dotBracket, strlen(dotBracket));
for (c = dotBracket; *c != '\0'; c++)
    dotBracketSwapBracket(c);
}

#define DOT_BRACKET_MSG_MAX 900
/* Longest message to hand to bigDrawWarning, which paints it one sentence per line
 * through a 1024 byte safef and errAborts on a longer line.  Item and column names come
 * out of the bigBed with no length limit of their own, so bound the whole message. */

static void dotBracketError(struct track *tg, char *format, ...)
/* Declared ahead of its definition only to have the compiler check the call sites. */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

static void dotBracketError(struct track *tg, char *format, ...)
/* Report bad dot-bracket data and give up on this track.  networkErrMsg is how the rest
 * of hgTracks reports a broken bigData track: after the load, serial or threaded,
 * setSharedErrorsAcrossWindows swaps in the bigWarn handlers, which print the message
 * across the track.  The rest of the image, and the control to turn the track off, are
 * still there.  An errAbort here would not do: both load paths catch one, but the item
 * that raised it takes the rest of the rows down with it, the reader gets whatever text
 * aborted, and an errAbort raised while drawing is not caught at all. */
{
if (tg->networkErrMsg == NULL)
    {
    struct dyString *dy = dyStringNew(512);
    va_list args;
    va_start(args, format);
    dyStringVaPrintf(dy, format, args);
    va_end(args);
    if (dy->stringSize > DOT_BRACKET_MSG_MAX)
        dyStringResize(dy, DOT_BRACKET_MSG_MAX);
    tg->networkErrMsg = dyStringCannibalize(&dy);
    }
}

static void dotBracketErrorField(struct track *tg, struct bbiFile *bbi, char *fieldName,
                                int fieldIx)
/* Say that dotBracketField does not name a usable column, listing the ones it could.
 * Only the columns past the first four are listed: those are the only ones it could
 * name.  A file with very many columns runs past DOT_BRACKET_MSG_MAX and the list is
 * cut off there. */
{
struct dyString *names = dyStringNew(256);
struct slName *name;
int ix = 0;
for (name = bbFieldNames(bbi); name != NULL; name = name->next, ix++)
    {
    if (ix >= 4)
        dyStringPrintf(names, " %s", name->name);
    }
dotBracketError(tg, "track %s: %s names the column '%s', which %s.  Set %s to one of "
                "these columns:%s", tg->track, DOT_BRACKET_FIELD, fieldName,
                (fieldIx < 0 ? "the bigBed does not have"
                             : "is one of the first four columns, which this setting "
                               "cannot use"),
                DOT_BRACKET_FIELD, names->string);
dyStringFree(&names);
}

static boolean dotBracketParseColor(char *setting, unsigned char *r, unsigned char *g,
                                    unsigned char *b)
/* Read an r,g,b setting.  FALSE rather than errAbort when it is not three numbers in
 * range, so a mistyped colour is reported on the track like any other bad data instead
 * of taking the page down, which is what parseColor in trackDbCustom.c would do. */
{
char dupe[64], *words[4];
int i;
if (strlen(setting) >= sizeof dupe)
    return FALSE;
safecpy(dupe, sizeof dupe, setting);
if (chopString(dupe, ", \t", words, ArraySize(words)) != 3)
    return FALSE;
for (i = 0; i < 3; i++)
    {
    if (!isAllDigits(words[i]) || (atoi(words[i]) > 255))
        return FALSE;
    }
*r = atoi(words[0]);
*g = atoi(words[1]);
*b = atoi(words[2]);
return TRUE;
}

static int dotBracketRestIx(struct track *tg, struct bbiFile *bbi)
/* Index into bb->rest of the column named by the dotBracketField setting, or -1 when
 * there is no usable column.  The bigBed has to be open to work this out, and it is
 * closed again by the time items are drawn, so this is resolved once at load time. */
{
char *fieldName = trackDbSetting(tg->tdb, DOT_BRACKET_FIELD);
int fieldIx = bbFieldIndex(bbi, fieldName);
if (fieldIx < 4)
    {
    dotBracketErrorField(tg, bbi, fieldName, fieldIx);
    return -1;
    }
return fieldIx - 3;
}

static int dotBracketSplicedLength(struct linkedFeatures *lf)
/* Number of bases the dot-bracket string has to cover: the sum of the block sizes. */
{
struct simpleFeature *sf;
int length = 0;
for (sf = lf->components; sf != NULL; sf = sf->next)
    length += sf->end - sf->start;
return length;
}

static void dotBracketErrorLength(struct track *tg, struct linkedFeatures *lf,
                                 char *dotBracket, int splicedLength)
/* Say that the string does not line up with the blocks. */
{
dotBracketError(tg, "track %s, item %s at %s:%d-%d: the %s column holds %d characters "
                "but the item has %d bases in %d blocks.  The dot-bracket string covers "
                "the spliced item, so its length has to equal the sum of the block sizes "
                "(chromEnd-chromStart when there are no blocks).  Pad it with spaces "
                "where there is no prediction.",
                tg->track, lf->name, chromName, lf->start, lf->end, DOT_BRACKET_FIELD,
                (int)strlen(dotBracket), splicedLength, slCount(lf->components));
}

static char *dotBracketForLf(struct track *tg, struct linkedFeatures *lf)
/* Dot-bracket string for one item, in genomic left to right order, or NULL when the
 * row does not carry a usable one. */
{
char *dotBracket = restField((struct bigBedInterval *)lf->original, tg->customInt);
int splicedLength = dotBracketSplicedLength(lf);
if (dotBracket == NULL)
    {
    dotBracketError(tg, "track %s, item %s at %s:%d-%d: the %s column is empty.  "
                   "Every row needs a dot-bracket string; use spaces for bases with no "
                   "prediction.",
                   tg->track, lf->name, chromName, lf->start, lf->end, DOT_BRACKET_FIELD);
    return NULL;
    }
if (strlen(dotBracket) != splicedLength)
    {
    dotBracketErrorLength(tg, lf, dotBracket, splicedLength);
    freeMem(dotBracket);
    return NULL;
    }
if (lf->orientation < 0)
    dotBracketReverse(dotBracket);
return dotBracket;
}

static void dotBracketParseItems(struct track *tg)
/* Hang the genomic order dot-bracket string on each item.  Items merged by mergeSpan
 * are built without a bigBedInterval, so they have no string and draw as plain beds,
 * and so does any item whose string does not check out. */
{
struct linkedFeatures *lf;
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    if (lf->original != NULL)
        lf->extra = dotBracketForLf(tg, lf);
    }
}

static void dotBracketLoadItems(struct track *tg)
/* Load as a gapped bed, resolving the dot-bracket column while the bigBed is open. */
{
char *colorSetting = trackDbSetting(tg->tdb, DOT_BRACKET_COLOR);
unsigned char r, g, b;
if ((colorSetting != NULL) && !dotBracketParseColor(colorSetting, &r, &g, &b))
    dotBracketError(tg, "track %s: %s is '%s', which is not three numbers from 0 to 255.  "
                    "Write it the way the color setting is written, for example 0,0,255.",
                    tg->track, DOT_BRACKET_COLOR, colorSetting);
tg->customInt = dotBracketRestIx(tg, fetchBbiForTrack(tg));
loadGappedBed(tg);
if (tg->customInt >= 0)
    dotBracketParseItems(tg);
}

static int dotBracketExonCmp(const void *va, const void *vb)
/* Compare two blocks by start, for ascending genomic order. */
{
const struct simpleFeature *a = *((struct simpleFeature **)va);
const struct simpleFeature *b = *((struct simpleFeature **)vb);
return a->start - b->start;
}

static struct simpleFeature **dotBracketExonsAscending(struct linkedFeatures *lf, int *retCount)
/* Blocks of an item in ascending genomic order, which is the order the dot-bracket
 * string counts in.  lf->components comes out of linkedFeaturesBoundsAndGrays in
 * descending order, but sort rather than rely on that. */
{
int count = slCount(lf->components), i = 0;
struct simpleFeature **exons = needMem(count * sizeof(*exons)), *sf;
for (sf = lf->components; sf != NULL; sf = sf->next)
    exons[i++] = sf;
qsort(exons, count, sizeof(*exons), dotBracketExonCmp);
*retCount = count;
return exons;
}

static Color dotBracketColor(struct track *tg, struct hvGfx *hvg, Color itemColor)
/* Colour to draw the structure in: the dotBracketColor setting when the track gives one,
 * otherwise whatever stands out against the item. */
{
char *setting = trackDbSetting(tg->tdb, DOT_BRACKET_COLOR);
unsigned char r, g, b;
if ((setting != NULL) && dotBracketParseColor(setting, &r, &g, &b))
    return hvGfxFindColorIx(hvg, r, g, b);
return hvGfxContrastingColor(hvg, itemColor);
}

static void dotBracketDrawChar(struct hvGfx *hvg, int base, double scale, int xOff, int y,
                               int height, Color color, MgFont *font, char c)
/* Draw one dot-bracket character centered over one genomic base. */
{
char text[2];
int x1, x2;
if (scaledBoxToPixelCoords(base, base + 1, scale, xOff, &x1, &x2))
    {
    if (c == '.')
        {
        /* Draw the unpaired mark rather than setting a character.  A period is two
         * pixels on the baseline, and the middle dot that would replace it is UTF-8,
         * which only FreeType draws, and which ftWidth measures a byte at a time, so
         * hvGfxTextCentered would place it off centre.  A box has none of that. */
        int side = min(3, max(1, (x2 - x1) / 3));
        hvGfxBox(hvg, x1 + (x2 - x1 - side)/2, y + (height - side)/2, side, side, color);
        return;
        }
    text[0] = c;
    text[1] = '\0';
    if (hvg->rc)
        {
        /* Reverse complement display mirrors where a base is drawn but not what is
         * drawn there, so a bracket would end up pointing away from its partner. */
        dotBracketSwapBracket(text);
        }
    hvGfxTextCentered(hvg, x1, y, max(x2 - x1, 1), height, color, font, text);
    }
}

static void dotBracketDrawExonChars(struct hvGfx *hvg, struct simpleFeature *sf,
                                    char *dotBracket, double scale, int xOff, int y,
                                    int height, Color color, MgFont *font)
/* Draw the dot-bracket characters over one block, in a colour that shows up against the
 * item.  Only the part of the block on screen is walked, so the cost
 * goes with the window and not with the size of the item. */
{
int base, start = max(winStart, sf->start), end = min(winEnd, sf->end);
for (base = start; base < end; base++)
    {
    char c = dotBracket[base - sf->start];
    if (!isspace((unsigned char)c))
        dotBracketDrawChar(hvg, base, scale, xOff, y, height, color, font, c);
    }
}

static int dotBracketGlyphWidth(MgFont *font, char *dotBracket)
/* Width in pixels of the widest bracket this item actually uses.  Measuring every bracket
 * the notation allows would hold back the usual data, which has only ( and ), for the
 * width of a < it never contains: here those are 4 pixels against 8.  The unpaired mark
 * is not counted, it is drawn rather than set and is narrower than any of them. */
{
static char *brackets = "()[]{}<>";
char *b;
int width = 1;
for (b = brackets; *b != '\0'; b++)
    {
    if (strchr(dotBracket, *b) != NULL)
        width = max(width, mgFontCharWidth(font, *b));
    }
return width;
}

static boolean dotBracketShowing(struct track *tg, struct linkedFeatures *lf,
                                 enum trackVisibility vis)
/* Is this item's structure on?  That is a separate question from whether there is room
 * to draw it: an item whose structure is on but too small to read is drawn as a plain
 * box, never with the direction arrows, which look like brackets of their own. */
{
return (lf->extra != NULL) && (lf->components != NULL) && (vis != tvDense)
    && (tg->heightPer >= tl.fontHeight)
    && cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, DOT_BRACKET_SHOW_SUFFIX, TRUE);
}

static boolean dotBracketRoomForChars(struct linkedFeatures *lf, double scale, MgFont *font)
/* Is a base wide enough to hold a character?  Not zoomedToBaseLevel, which keeps the
 * width of an M for every base: a bracket is a third of that, so the structure stays
 * readable in a window several times wider.  A pixel of overlap between neighbours is
 * allowed on top of that, since a bracket is one thin stroke in the middle of its slot
 * and crowding it reads worse than it looks.
 *
 * The scale >= 2 test is redundant with the one after it, but dotBracketGlyphWidth walks
 * the string, and at a genome wide view that would run for every item to reach a no. */
{
return (scale >= 2) && (scale >= dotBracketGlyphWidth(font, lf->extra) - 1);
}

static void dotBracketDrawItemAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff,
                                 int y, double scale, MgFont *font, Color color,
                                 enum trackVisibility vis)
/* Draw an item the usual way, then overlay its dot-bracket structure. */
{
struct linkedFeatures *lf = item;
boolean showing = dotBracketShowing(tg, lf, vis);
boolean drawChars = showing && dotBracketRoomForChars(lf, scale, font);
boolean exonArrows = tg->exonArrows, exonArrowsAlways = tg->exonArrowsAlways;
struct simpleFeature **exons;
Color structColor;
int exonCount, i, offset = 0;

if (showing)
    {
    /* The characters go in the blocks, where the arrows would be, and the two together
     * are unreadable.  linkedFeaturesDrawAt drops the arrows for base level sequence for
     * the same reason; it cannot see this overlay, so do it here.  The arrows stay off
     * when the item is too small for the characters as well: a box of > marks where the
     * structure was reads as more structure, and the one is replaced by the other as the
     * reader zooms. */
    tg->exonArrows = FALSE;
    tg->exonArrowsAlways = FALSE;
    }
linkedFeaturesDrawAt(tg, item, hvg, xOff, y, scale, font, color, vis);
tg->exonArrows = exonArrows;
tg->exonArrowsAlways = exonArrowsAlways;

if (!drawChars)
    return;
structColor = dotBracketColor(tg, hvg, color);
exons = dotBracketExonsAscending(lf, &exonCount);
for (i = 0; i < exonCount; i++)
    {
    dotBracketDrawExonChars(hvg, exons[i], lf->extra + offset, scale, xOff, y,
                            tg->heightPer, structColor, font);
    offset += exons[i]->end - exons[i]->start;
    }
freez(&exons);
}

void dotBracketMethods(struct track *tg)
/* Set up a type bigBed track to overlay a dot-bracket string from a named column. */
{
if (trackDbSetting(tg->tdb, "quickLiftUrl") != NULL)
    return;  /* the lift recuts the blocks, so the string no longer lines up with them */
tg->loadItems = dotBracketLoadItems;
tg->drawItemAt = dotBracketDrawItemAt;
}

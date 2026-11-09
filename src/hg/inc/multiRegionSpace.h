/* multiRegionSpace - a synthetic coordinate space defined by a list of regions.
 *
 * A multi-region view is a list of regions of a source assembly laid end to end.
 * That layout is described entirely by one bigChain file: its target side is the
 * synthetic sequence and its query side is the source assembly, so the chain
 * carries the synthetic sequence names, the synthetic sizes, and the mapping
 * back to the source.  Nothing else is needed to describe the space, and in
 * particular no synthetic 2bit: sequence comes from the source 2bit through the
 * same chain.
 *
 * A space is named by the SHA1 of the assembly name and the region list, in the order
 * the regions are laid out, so any two states that produce the same layout share one
 * chain.  The chain lives in the trash under that name, which means the file needs no
 * table to be found:  the name is the hash and the hash is the name.
 *
 * Finding a space that way costs the region list, and in exon or gene view that is a
 * scan of the whole gene table, about a quarter of a second, spent to name a chain
 * that is usually already there.  So the region list is the SLOW way in, and there
 * are two faster ones ahead of it:
 *
 *   1. the cart, which remembers the chain file the last request used, together with
 *      the conditions it was built for.  Nothing is asked of hgcentral at all.
 *   2. the multiRegionSpaceKey table in hgcentral, which maps those conditions to the
 *      SHA1 of the region list they produce.
 *
 * The conditions are the cheap description of the view: the assembly, the sequence,
 * the mode, the gene table, its filters and the search matches it keeps, and when that
 * table was last loaded;  for a single transcript, the track, the item, its chrom and
 * whether only the coding exons are used.  Each is written as the region builder reads
 * it, not as the cart spells it, so one view is one row.  Two
 * requests with the same conditions produce the same regions, so the chain the first
 * one built is the chain the second one wants.  A miss anywhere along the way (no
 * cart value, no row, or a row whose chain the trash cleaner has taken) falls back
 * to building the region list, which is always correct and never wrong, only slow.
 *
 * refs #37788 */

#ifndef MULTIREGIONSPACE_H
#define MULTIREGIONSPACE_H

/* hg.conf: turns the feature on. */
#define MULTI_REGION_SPACES_CONF     "multiRegionSpaces"
#define MULTI_REGION_MAX_REGIONS_CONF "multiRegionSpaceMaxRegions"
#define DEFAULT_MULTI_REGION_MAX_REGIONS 500000

/* hg.conf: lets a test machine point at its own copy of the table. */
#define MULTI_REGION_SPACE_KEY_TABLE_CONF     "multiRegionSpaceKeyName"
#define DEFAULT_MULTI_REGION_SPACE_KEY_TABLE  "multiRegionSpaceKey"

/* The trash subdirectory the chains live in. */
#define MULTI_REGION_SPACE_TRASH_DIR  "mrSpace"

/* An sha1 written as hex, plus the terminating zero. */
#define MULTI_REGION_SHA_SIZE 41

struct multiRegionSpan
/* One region of the source assembly, in source coordinates.  A list of these,
 * in the order they are laid out, is the whole definition of a space.  Deliberately
 * smaller than struct bed: a genome-wide exon list is nearly 400,000 of them. */
    {
    struct multiRegionSpan *next;
    char *chrom;	/* Source sequence name.  Not owned by this struct. */
    int start;		/* Source start, half open. */
    int end;		/* Source end. */
    char strand;	/* '-' to lay the region down reverse-complemented, else forward.
			 * Only singleAltHaplo sets it, for an alt placed in reverse. */
    };

boolean multiRegionSpacesEnabled();
/* Is the synthetic multi-region coordinate space feature turned on here? */

int multiRegionSpaceMaxRegions();
/* The most regions a space is built for, hg.conf multiRegionSpaceMaxRegions.  A longer
 * list is drawn the older way, so a region list from a URL cannot start a builder of
 * any size. */

char *multiRegionSpaceKeyTable();
/* Return the name of the conditions-to-SHA1 table in hgcentral. */


/* ---- Timing ---- */

/* The space path does several things once a request (building the region list,
 * hashing it, asking hgcentral for the chain, and building the chain when it is not
 * there yet) and several more many times a request, out of the chain.  Which of
 * those the time went into is not guessable from the total, and the answers differ
 * by an order of magnitude between a hit and a miss.  These report it the way the
 * rest of hgTracks does, so the lines land in the same list in the same order. */

void multiRegionSpaceSetTiming(boolean on);
/* Report where the time goes inside these functions.  hgTracks turns this on when
 * measureTiming is on, and it is off everywhere else. */

void multiRegionSpaceReportTiming();
/* Print the totals for the calls that happen many times in one request:  sequence
 * reads, size lookups and sequence-name lists.  A line each for those would bury the
 * rest of the timings.  Does nothing unless timing is on. */

void multiRegionSpaceSha(char *db, struct multiRegionSpan *regionList,
                         char shaOut[MULTI_REGION_SHA_SIZE]);
/* Write the SHA1 of this assembly and this region list, as hex, into shaOut.
 * The list is hashed in the order given, because that is the order the regions are
 * laid out in:  the same regions in another order are another space.  An alt
 * haplotype shown in place is its chromosome's flank, the alt, and the rest of the
 * chromosome, and sorting that list put the alt after the end of the chromosome. */

char *multiRegionSpaceChainFile(char *db, struct multiRegionSpan *regionList,
                                char *conditions);
/* Return the bigChain file that defines the space for this region list, building
 * it if this is the first time it has been asked for.  Returns NULL ONLY when the
 * feature is off;  a space that cannot be built aborts and says why, rather than
 * answering NULL and letting the caller quietly draw something else.  Free the
 * result.
 *
 * conditions, when it is not NULL, is the cheap description of the view that
 * produced this region list.  It is recorded against the region list's SHA1 so the
 * next request with the same conditions can find this chain without building the
 * list again.  Pass NULL for a mode whose region list cannot be described without
 * reading the regions themselves;  that costs the shortcut, not correctness. */

char *multiRegionSpaceChainForConditions(char *conditions);
/* The chain file already built for these conditions, or NULL if there is none, or
 * none whose file is still in the trash.  Asks hgcentral and the file system and
 * nothing else:  no region list is built and no chain, which is the whole point.
 * Free the result. */

char *multiRegionSpaceChainForName(char *name);
/* The chain file named by a value that came out of the cart, or NULL if there is no
 * such chain.
 *
 * The cart holds the bare name of the file, not its path.  Two reasons, and both are
 * about the fact that a cart value is not ours:  a path lets a cart name any file on
 * the machine, and a cart value holding "../trash/..." is rewritten when a session is
 * saved:  sessionData.c moves the file it names into session storage and leaves the
 * chain's link file behind in the trash, which breaks the space it was trying to
 * preserve.  A bare name is neither of those things.  Free the result. */

char *multiRegionSpaceChainName(char *chainFile);
/* The name to put in the cart for this chain file, which is its base name.  Do not
 * free:  it points into chainFile. */

/* Reading a space back.  These are everything the browser needs to know about the
 * synthetic assembly, and they all come out of the chain: its target side is the
 * synthetic sequence, so the chain's own sequence list and sizes are the synthetic
 * ones, and its blocks say which source bases each synthetic base came from. */

struct slName *multiRegionSpaceChromNames(char *chainFile);
/* The synthetic sequence names.  Free with slFreeList. */

char *multiRegionSpaceSoleChrom(char *chainFile);
/* The name of the one synthetic sequence this space has, or NULL for an empty chain.
 * A space always has exactly one:  regions from a single source sequence lay into a
 * sequence named after it, and regions from several lay into one called "multi",
 * since a browser can only display one sequence at a time.  Do not free the result. */

int multiRegionSpaceChromSize(char *chainFile, char *chrom);
/* The size of one synthetic sequence, or -1 if there is no such sequence. */

struct chromInfo *multiRegionSpaceChromInfo(char *chainFile, char *chrom);
/* A chromInfo for one synthetic sequence, or NULL if there is no such sequence.
 * Its fileName is NULL:  there is no file holding this sequence, and anything that
 * opened one by name would be reading the source at synthetic coordinates. */

struct dnaSeq *multiRegionSpaceDna(char *chainFile, char *sourceTwoBit,
                                   char *chrom, int start, int end);
/* Read synthetic sequence out of the source assembly through the chain.
 *
 * sourceTwoBit is passed in rather than looked up from a database name on purpose.
 * In a space the source and the display assembly have the same name, and that name
 * means the display coordinates;  a source read that went looking for its own
 * sequence by that name would get the synthetic answer back.  refs #37788 */


/* ---- Drawing in a space ---- */

/* While a space is installed, an assembly name means the SYNTHETIC coordinates:
 * hChromSize(), hGetChromInfo(), hAllChromNames() and the hChromSeq() family all
 * answer from the chain.  That is what the rest of the browser wants, because the
 * name it holds is the name of what it is drawing.
 *
 * The source assembly therefore cannot be reached by its own name any more, and
 * nothing may try.  The source 2bit is resolved once, here, while the name still
 * means the source, and every later source read takes that path as an argument.
 * hNibForChrom() aborts for a db in a space rather than answering with a path that
 * would be read at synthetic coordinates. */

void multiRegionSpaceSetDisplay(char *db, char *chrom, char *chainFile);
/* Draw db in the synthetic space this chain describes.  Resolves and remembers the
 * source 2bit first, while the name still means the source, and then drops whatever
 * hdb has already cached for the name, since those answers are about the source.
 * chrom is a SOURCE sequence name, used only to resolve that file on a mirror that
 * keeps its sequence in chromInfo rather than in one 2bit. */

char *multiRegionSpaceForDb(char *db);
/* The chain describing the space db is being drawn in, or NULL if it is not in
 * one.  Cheap:  the usual answer is that no space is installed at all. */

char *multiRegionSpaceSourceTwoBit(char *db);
/* The source assembly's 2bit for a db in a space.  This is the only way to read
 * source sequence once a space is installed. */

#endif /* MULTIREGIONSPACE_H */

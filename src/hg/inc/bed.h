/* bed.h contains the interface to Browser Extensible Data (bed) files and tables.
 * The idea behind bed is that the first three fields are defined and required.
 * A total of 15 fields are defined, and the file can contain any number of these.
 * In addition after any number of defined fields there can be custom fields that
 * are not defined in the bed spec.
 *
 * Most of the code for this is actually in src/inc/basicBed.h.  Only the stuff that
 * depends on the database or other structures defined in src/hg/inc modules, is
 * done here. */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef BED_H
#define BED_H

#ifndef GENEPRED_H
#include "genePred.h"
#endif

#ifndef HASH_H
#include "hash.h"
#endif

#ifndef RANGETREE_H
#include "rangeTree.h"
#endif

#ifndef BASICBED_H
#include "basicBed.h"
#endif

struct genePred *bedToGenePred(struct bed *bed);
/* Convert a single bed to a genePred structure. */

struct bed *bedFromGenePred(struct genePred *genePred);
/* Convert a single genePred to a bed structure */

/* Constraints that can be placed on bed fields: */
enum charFilterType
    {
    cftIgnore = 0,
    cftSingleLiteral = 1,
    cftMultiLiteral = 2,
    };

enum stringFilterType
    {
    sftIgnore = 0,
    sftSingleLiteral = 1,
    sftMultiLiteral = 2,
    sftSingleRegexp = 3,
    sftMultiRegexp = 4,
    };

enum numericFilterType
    {
    nftIgnore = 0,
    nftLessThan = 1,
    nftLTE = 2,
    nftEqual = 3,
    nftNotEqual = 4,
    nftGTE = 5,
    nftGreaterThan = 6,
    nftInRange = 7,
    nftNotInRange = 8,
    };

struct bedFilter
    {
    enum stringFilterType chromFilter;
    char **chromVals;
    boolean chromInvert;
    enum numericFilterType chromStartFilter;
    int *chromStartVals;
    enum numericFilterType chromEndFilter;
    int *chromEndVals;
    enum stringFilterType nameFilter;
    char **nameVals;
    boolean nameInvert;
    enum numericFilterType scoreFilter;
    int *scoreVals;
    enum charFilterType strandFilter;
    char *strandVals;
    boolean strandInvert;
    enum numericFilterType thickStartFilter;
    int *thickStartVals;
    enum numericFilterType thickEndFilter;
    int *thickEndVals;
    enum numericFilterType blockCountFilter;
    int *blockCountVals;
    enum numericFilterType chromLengthFilter;
    int *chromLengthVals;
    enum numericFilterType thickLengthFilter;
    int *thickLengthVals;
    enum numericFilterType compareStartsFilter;
    enum numericFilterType compareEndsFilter;
    };

boolean bedFilterOne(struct bedFilter *bf, struct bed *bed);
/* Return TRUE if bed passes filter. */

struct bed *bedFilterListInRange(struct bed *bedListIn, struct bedFilter *bf,
				 char *chrom, int winStart, int winEnd);
/* Given a bed list, a position range, and a bedFilter which specifies
 * constraints on bed fields, return the list of bed items that meet
 * the constraints.  If chrom is NULL, position range is ignored. */

struct bed *bedFilterList(struct bed *bedListIn, struct bedFilter *bf);
/* Given a bed list and a bedFilter which specifies constraints on bed 
 * fields, return the list of bed items that meet the constraints. */

struct bed *bedFilterByNameHash(struct bed *bedList, struct hash *nameHash);
/* Given a bed list and a hash of names to keep, return the list of bed 
 * items whose name is in nameHash. */

struct bed *bedFilterByWildNames(struct bed *bedList, struct slName *wildNames);
/* Given a bed list and a list of names that may include wildcard characters,
 * return the list of bed items whose name matches at least one wildName. */

boolean bedFilterInt(int value, enum numericFilterType nft, int *filterValues);
/* Return TRUE if value passes the filter. */
/* This could probably be turned into a macro if performance is bad. */

boolean bedFilterLong(long long value, enum numericFilterType nft, long long *filterValues);
/* Return TRUE if value passes the filter. */
/* This could probably be turned into a macro if performance is bad. */

boolean bedFilterDouble(double value, enum numericFilterType nft, double *filterValues);
/* Return TRUE if value passes the filter. */
/* This could probably be turned into a macro if performance is bad. */

boolean bedFilterString(char *value, enum stringFilterType sft, char **filterValues, 
	boolean invert);
/* Return TRUE if value passes the filter. */

boolean bedFilterChar(char value, enum charFilterType cft,
			  char *filterValues, boolean invert);
/* Return TRUE if value passes the filter. */

struct hash *bedsIntoKeeperHash(struct bed *bedList);
/* Create a hash full of bin keepers (one for each chromosome or contig.
 * The binKeepers are full of beds. */

#endif /* BED_H */


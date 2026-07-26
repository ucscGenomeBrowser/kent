/* blatShare.h - reopen durable BLAT bigPsl results.  Shared by hgBlat (which rebuilds the results
 * table for a shared "?u=&s=" link) and hgc (which rebuilds one base-by-base alignment for a shared
 * alignment link), so both read the durable custom track through the same code and the same
 * server-file security check. */

/* Copyright (C) 2024 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef BLATSHARE_H
#define BLATSHARE_H

#include "psl.h"

struct cart;

struct psl *pslListFromBigPslFile(char *bbFileName);
/* Read every alignment out of a bigPsl bigBed file into a psl list, in query-display order.  The
 * two-char (query,target) strand pslFromBigPsl yields is collapsed to a single char for non-protein
 * queries, to match the classic/fresh result page. */

struct psl *pslFromBigPslFileMatch(char *bbFileName, char *chrom, int tStart, char *qName,
                                   char **retSeq, char **retCds);
/* Return the single alignment in a bigPsl bigBed matching chrom:tStart and qName, together with its
 * stored query sequence (retSeq) and CDS (retCds) when those out pointers are non-NULL, or NULL if
 * there is no such alignment.  Caller frees the returned psl (and any returned seq and cds). */

char *blatFindPinnedBigPsl(struct cart *cart);
/* Return a cloned path to the BLAT bigPsl bigBed that hgc's buildBigPsl pinned in the cart
 * (blatLastBigBed), or NULL if there is none or it is not a file the server itself placed under its
 * trash / durable session-data dir.  A shared session (?u=&s=) can carry an arbitrary value here, so
 * this only ever accepts a local file on the isValidBigDataUrl trash/session-data allow-list - never
 * a remote URL or an arbitrary local path. */

#endif /* BLATSHARE_H */

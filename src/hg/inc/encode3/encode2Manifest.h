/* encode2Manifest - manipulate a line of a manifest file of the type
 * used for importing data from encode2 into encode3 */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef ENCODE2MANIFEST_H
#define ENCODE2MANIFEST_H

#define ENCODE2MANIFEST_NUM_COLS 10

struct encode2Manifest
/* Information on one file */
    {
    struct encode2Manifest *next;  /* Next in singly linked list. */
    char *fileName;	/* Name of file with directory relative to manifest */
    char *format;	/* bam fastq etc */
    char *outputType;   /* aka view - alignment, transfrags, etc. */
    char *experiment;	/* wgEncodeXXXX */
    char *replicate;	/* 1 2 both n/a */
    char *enrichedIn;	/* promoter exon etc. */
    char *md5sum;	/* Hash of file contents or n/a */
    long long size;	/* File size. */
    long long modifiedTime; /* Time file last modified in seconds since birth of unix */
    char *validKey;     /* Pass key from validator. */
    };

struct encode2Manifest *encode2ManifestLoad(char **row);
/* Load a encode2Manifest from row fetched with select * from encode2Manifest
 * from database.  Dispose of this with encode2ManifestFree(). */

struct encode2Manifest *encode2ManifestLoadAll(char *fileName);
/* Load all encode2Manifests from file. */

struct encode2Manifest *encode2ManifestShortLoadAll(char *fileName);
/* Read a short (just first 6 columns) manifest file. */

void encode2ManifestTabOut(struct encode2Manifest *mi, FILE *f);
/* Write tab-separated version of encode2Manifest to f */

void encode2ManifestShortTabOut(struct encode2Manifest *mi, FILE *f);
/* Write tab-separated version of encode2Manifest to f */

void encode2ManifestPrintHeader(FILE *f);
/* Write out header line. */

#endif /* ENCODE2MANIFEST_H */

/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Sig.h - signatures that start various binary files. */
#ifndef SIG_H
#define SIG_H

#define aliSig 0xCDAB8245
/* Binary alignment file. */

#define alxSig 0xA1B1C1D3
/* Index into binary alignment file, sorted by start base offset. */

#define pgoSig 0x690
/* Index into GDF file, sorted by start base offset. Signature is 32 bit. */

#define cdoSig 0xCD01
/* Index into c2g text file, sorted by start base offset. 32 bit signature. */

#define xaoSig 0xA0B0C0D0
/* Index into xeno alignment, sorted by start base offset.  32 bit signature. */

#define glSig 0xF1E2D3C4
/* Binary gene file, sorted by chromosome and then starting offset. */

/* IX sig is int ixSig[4] = {0x693F8ED1, 0x7EDA1C32, 0x4BA58983, 0x277CB89C,};
 * These are made by snofMake, and are indexes sorted by name. */

/* XI - same as IX but on big-endian (or is it little-endian) archetectures. */

#define nt4Signature 0x12345678
/* Signature at the beginning of an nt4 file - 2 bit a nucleotide binary file. */

#define lm2Signature 0x12131416
/* Signature at the beginning of a lm2 file - a 2nd order markov model for nucleotides. */

#define oocSig 0x584155f2
/* Signature of file that contains over-represented oligomers for patSpace
 * algorithm. */

#define oocSigSwapped 0xf2554158
/* Signature of file that contains over-represented oligomers for patSpace
 * algorithm. */

#define fofSig 0x13410da8
/* Signature into fof type index file (that can index multiple external files). */

#define nibSig 0x6BE93D3A
/* Signature into nib file (4 bits per nucleotide DNA file) */

#define qacSig 0x32b67998
/* Signature of qac file (compressed quality file) */

#define caqSig 0x9879b632
/* Signature of byte-swapped qac file. */

#define twoBitSig 0x1A412743
/* Signature into 2bit file (2 bits per nucleotide DNA file) plus
 * information on N and masked bases. */

#define twoBitSwapSig 0x4327411A
/* Signature of byte-swapped two-bit file. */

#define chromGraphSig 0x4528421C
/* Signature of chromGraph binary data file */

#define chromGraphSwapSig 0x1C422845
/* Signature of byte-swapped chromGraph binary data file */

#define genomeRangeTreeSig     0xf7fb8104
/* Signature of genomeRangeTree binary data file */

#define genomeRangeTreeSwapSig 0x0481fbf7
/* Signature of genomeRangeTree binary data file */

#define bptSig 0x78CA8C91
/* Signature of generic b+ tree index file. */

#define bptSwapped 0x918CCA78
/* Signature of generic b+ tree index file. */

#define cirTreeSig 0x2468ACE0
/* Signature of a chromosome id r-tree index file. */

#define crTreeSig 0x2369ADE1
/* Signature of a chromosome r-tree index file. */

#define bigWigSig 0x888FFC26
/* Signature for a big wig file. */

#define bigBedSig 0x8789F2EB
/* Signature for a big bed file. */

#define udcBitmapSig 0x4187E2F6
/* Signature for a url data cache bitmap file. */

#endif /* SIG_H */



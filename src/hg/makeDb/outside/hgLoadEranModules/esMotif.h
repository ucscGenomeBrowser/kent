/* esm.h autoXml generated file */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef ESM_H
#define ESM_H

struct esmMotifs
    {
    struct esmMotifs *next;
    char *SeqFile;	/* Required */
    struct esmMotif *esmMotif;	/** Possibly empty list. **/
    };

void esmMotifsSave(struct esmMotifs *obj, int indent, FILE *f);
/* Save esmMotifs to file. */

struct esmMotifs *esmMotifsLoad(char *fileName);
/* Load esmMotifs from file. */

struct esmMotif
    {
    struct esmMotif *next;
    char *Consensus;	/* Required */
    char *Source;	/* Required */
    char *Name;	/* Required */
    char *Description;	/* Optional */
    struct esmWeights *esmWeights;	/** Single instance required. **/
    };

void esmMotifSave(struct esmMotif *obj, int indent, FILE *f);
/* Save esmMotif to file. */

struct esmMotif *esmMotifLoad(char *fileName);
/* Load esmMotif from file. */

struct esmWeights
    {
    struct esmWeights *next;
    double ZeroWeight;	/* Required */
    struct esmPosition *esmPosition;	/** Non-empty list required. **/
    };

void esmWeightsSave(struct esmWeights *obj, int indent, FILE *f);
/* Save esmWeights to file. */

struct esmWeights *esmWeightsLoad(char *fileName);
/* Load esmWeights from file. */

struct esmPosition
    {
    struct esmPosition *next;
    int Num;	/* Required */
    char *Weights;	/* Required */
    };

void esmPositionSave(struct esmPosition *obj, int indent, FILE *f);
/* Save esmPosition to file. */

struct esmPosition *esmPositionLoad(char *fileName);
/* Load esmPosition from file. */

#endif /* ESM_H */


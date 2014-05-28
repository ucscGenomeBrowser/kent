/* riken.c autoXml generated file to load RIKEN annotations. */

/* Copyright (C) 2002 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef RIKEN_H
#define RIKEN_H

struct rikenMaxmlClusters
    {
    struct rikenMaxmlClusters *next;
    struct rikenCluster *rikenCluster;	/** Possibly empty list. **/
    };

void rikenMaxmlClustersSave(struct rikenMaxmlClusters *obj, int indent, FILE *f);
/* Save rikenMaxmlClusters to file. */

struct rikenMaxmlClusters *rikenMaxmlClustersLoad(char *fileName);
/* Load rikenMaxmlClusters from file. */

struct rikenCluster
    {
    struct rikenCluster *next;
    char *id;	/* Required */
    struct rikenFantomid *rikenFantomid;	/** Single instance required. **/
    struct rikenRepresentativeSeqid *rikenRepresentativeSeqid;	/** Single instance required. **/
    struct rikenSequence *rikenSequence;	/** Possibly empty list. **/
    };

void rikenClusterSave(struct rikenCluster *obj, int indent, FILE *f);
/* Save rikenCluster to file. */

struct rikenCluster *rikenClusterLoad(char *fileName);
/* Load rikenCluster from file. */

struct rikenRepresentativeSeqid
    {
    struct rikenRepresentativeSeqid *next;
    char *text;
    };

void rikenRepresentativeSeqidSave(struct rikenRepresentativeSeqid *obj, int indent, FILE *f);
/* Save rikenRepresentativeSeqid to file. */

struct rikenRepresentativeSeqid *rikenRepresentativeSeqidLoad(char *fileName);
/* Load rikenRepresentativeSeqid from file. */

struct rikenMaxmlSequences
    {
    struct rikenMaxmlSequences *next;
    struct rikenSequence *rikenSequence;	/** Possibly empty list. **/
    };

void rikenMaxmlSequencesSave(struct rikenMaxmlSequences *obj, int indent, FILE *f);
/* Save rikenMaxmlSequences to file. */

struct rikenMaxmlSequences *rikenMaxmlSequencesLoad(char *fileName);
/* Load rikenMaxmlSequences from file. */

struct rikenSequence
    {
    struct rikenSequence *next;
    char *id;	/* Required */
    struct rikenAltid *rikenAltid;	/** Possibly empty list. **/
    struct rikenSeqid *rikenSeqid;	/** Optional (may be NULL). **/
    struct rikenFantomid *rikenFantomid;	/** Optional (may be NULL). **/
    struct rikenCloneid *rikenCloneid;	/** Optional (may be NULL). **/
    struct rikenRearrayid *rikenRearrayid;	/** Optional (may be NULL). **/
    struct rikenAccession *rikenAccession;	/** Optional (may be NULL). **/
    struct rikenAnnotator *rikenAnnotator;	/** Optional (may be NULL). **/
    struct rikenVersion *rikenVersion;	/** Optional (may be NULL). **/
    struct rikenModifiedTime *rikenModifiedTime;	/** Optional (may be NULL). **/
    struct rikenAnnotations *rikenAnnotations;	/** Optional (may be NULL). **/
    struct rikenComment *rikenComment;	/** Optional (may be NULL). **/
    };

void rikenSequenceSave(struct rikenSequence *obj, int indent, FILE *f);
/* Save rikenSequence to file. */

struct rikenSequence *rikenSequenceLoad(char *fileName);
/* Load rikenSequence from file. */

struct rikenAltid
    {
    struct rikenAltid *next;
    char *text;
    char *type;	/* Required */
    };

void rikenAltidSave(struct rikenAltid *obj, int indent, FILE *f);
/* Save rikenAltid to file. */

struct rikenAltid *rikenAltidLoad(char *fileName);
/* Load rikenAltid from file. */

struct rikenSeqid
    {
    struct rikenSeqid *next;
    char *text;
    };

void rikenSeqidSave(struct rikenSeqid *obj, int indent, FILE *f);
/* Save rikenSeqid to file. */

struct rikenSeqid *rikenSeqidLoad(char *fileName);
/* Load rikenSeqid from file. */

struct rikenCloneid
    {
    struct rikenCloneid *next;
    char *text;
    };

void rikenCloneidSave(struct rikenCloneid *obj, int indent, FILE *f);
/* Save rikenCloneid to file. */

struct rikenCloneid *rikenCloneidLoad(char *fileName);
/* Load rikenCloneid from file. */

struct rikenFantomid
    {
    struct rikenFantomid *next;
    char *text;
    };

void rikenFantomidSave(struct rikenFantomid *obj, int indent, FILE *f);
/* Save rikenFantomid to file. */

struct rikenFantomid *rikenFantomidLoad(char *fileName);
/* Load rikenFantomid from file. */

struct rikenRearrayid
    {
    struct rikenRearrayid *next;
    char *text;
    };

void rikenRearrayidSave(struct rikenRearrayid *obj, int indent, FILE *f);
/* Save rikenRearrayid to file. */

struct rikenRearrayid *rikenRearrayidLoad(char *fileName);
/* Load rikenRearrayid from file. */

struct rikenAccession
    {
    struct rikenAccession *next;
    char *text;
    };

void rikenAccessionSave(struct rikenAccession *obj, int indent, FILE *f);
/* Save rikenAccession to file. */

struct rikenAccession *rikenAccessionLoad(char *fileName);
/* Load rikenAccession from file. */

struct rikenAnnotator
    {
    struct rikenAnnotator *next;
    char *text;
    };

void rikenAnnotatorSave(struct rikenAnnotator *obj, int indent, FILE *f);
/* Save rikenAnnotator to file. */

struct rikenAnnotator *rikenAnnotatorLoad(char *fileName);
/* Load rikenAnnotator from file. */

struct rikenVersion
    {
    struct rikenVersion *next;
    char *text;
    };

void rikenVersionSave(struct rikenVersion *obj, int indent, FILE *f);
/* Save rikenVersion to file. */

struct rikenVersion *rikenVersionLoad(char *fileName);
/* Load rikenVersion from file. */

struct rikenModifiedTime
    {
    struct rikenModifiedTime *next;
    char *text;
    };

void rikenModifiedTimeSave(struct rikenModifiedTime *obj, int indent, FILE *f);
/* Save rikenModifiedTime to file. */

struct rikenModifiedTime *rikenModifiedTimeLoad(char *fileName);
/* Load rikenModifiedTime from file. */

struct rikenAnnotations
    {
    struct rikenAnnotations *next;
    struct rikenAnnotation *rikenAnnotation;	/** Possibly empty list. **/
    };

void rikenAnnotationsSave(struct rikenAnnotations *obj, int indent, FILE *f);
/* Save rikenAnnotations to file. */

struct rikenAnnotations *rikenAnnotationsLoad(char *fileName);
/* Load rikenAnnotations from file. */

struct rikenComment
    {
    struct rikenComment *next;
    char *text;
    };

void rikenCommentSave(struct rikenComment *obj, int indent, FILE *f);
/* Save rikenComment to file. */

struct rikenComment *rikenCommentLoad(char *fileName);
/* Load rikenComment from file. */

struct rikenAnnotation
    {
    struct rikenAnnotation *next;
    struct rikenQualifier *rikenQualifier;	/** Single instance required. **/
    struct rikenAnntext *rikenAnntext;	/** Optional (may be NULL). **/
    struct rikenDatasrc *rikenDatasrc;	/** Optional (may be NULL). **/
    struct rikenSrckey *rikenSrckey;	/** Optional (may be NULL). **/
    struct rikenEvidence *rikenEvidence;	/** Optional (may be NULL). **/
    };

void rikenAnnotationSave(struct rikenAnnotation *obj, int indent, FILE *f);
/* Save rikenAnnotation to file. */

struct rikenAnnotation *rikenAnnotationLoad(char *fileName);
/* Load rikenAnnotation from file. */

struct rikenQualifier
    {
    struct rikenQualifier *next;
    char *text;
    };

void rikenQualifierSave(struct rikenQualifier *obj, int indent, FILE *f);
/* Save rikenQualifier to file. */

struct rikenQualifier *rikenQualifierLoad(char *fileName);
/* Load rikenQualifier from file. */

struct rikenAnntext
    {
    struct rikenAnntext *next;
    char *text;
    };

void rikenAnntextSave(struct rikenAnntext *obj, int indent, FILE *f);
/* Save rikenAnntext to file. */

struct rikenAnntext *rikenAnntextLoad(char *fileName);
/* Load rikenAnntext from file. */

struct rikenDatasrc
    {
    struct rikenDatasrc *next;
    char *text;
    };

void rikenDatasrcSave(struct rikenDatasrc *obj, int indent, FILE *f);
/* Save rikenDatasrc to file. */

struct rikenDatasrc *rikenDatasrcLoad(char *fileName);
/* Load rikenDatasrc from file. */

struct rikenSrckey
    {
    struct rikenSrckey *next;
    char *text;
    };

void rikenSrckeySave(struct rikenSrckey *obj, int indent, FILE *f);
/* Save rikenSrckey to file. */

struct rikenSrckey *rikenSrckeyLoad(char *fileName);
/* Load rikenSrckey from file. */

struct rikenEvidence
    {
    struct rikenEvidence *next;
    char *text;
    };

void rikenEvidenceSave(struct rikenEvidence *obj, int indent, FILE *f);
/* Save rikenEvidence to file. */

struct rikenEvidence *rikenEvidenceLoad(char *fileName);
/* Load rikenEvidence from file. */

#endif /* RIKEN_H */


/* eapGraph - stuff to help traverse the graph defined by the eapRun, eapInput, and eapOutput
 * tables that define what files were used to produce what other files */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef EAPGRAPH_H
#define EAPGRAPH_H

struct eapGraph
/* This is a complete graph */
   {
   /* The basic structures for the graph itself*/
   struct eapRun *runList;	// All runs in memory ordered by ID
   struct eapInput *inputList;	// All inputs to all runs, ordered by ID
   struct eapOutput *outputList;  // All outputs of all runs, ordered by ID

   /* We'll be wanting to know about the files too though. */
   struct edwFile *fileList;
   struct edwValidFile *validList;

   /* Random access containers for our graph */
   struct hash *runByExperiment;  // Multivalued has of runs keyed by experiment
   struct rbTree *runById;	  // Way to fetch runs given ID
   struct longToList *inputByFile;  // Inputs keyed by fileID
   struct longToList *inputByRun;   // Inputs keyed by runID
   struct rbTree *outputByFile;   // Outputs keyed by fileId
   struct longToList *outputByRun;  // Outputs keyed by run

   /* Random access containers for files */
   struct rbTree *fileById;
   struct rbTree *validByFileId;
   struct hash *validByExperiment;
   };

struct eapGraph *eapGraphNew(struct sqlConnection *conn);
/* Return new eapGraph made by querying database */

void eapGraphFree(struct eapGraph **pEg);
/* Free up resources associated with graph */

/*** The graph search routines tend to return either file IDs or slRef eapInput or eapOut lists ***/

/* Routines to fetch out files given fileIds */

struct edwValidFile *eapGraphValidFromId(struct eapGraph *eg, unsigned fileId);
/* Return edwValidFile given  fileId.  Aborts if fileId is not found.  */

struct edwFile *eapGraphFileFromId(struct eapGraph *eg, unsigned fileId);
/* Return edwFile given ID.  Aborts if fileId not found */


/* Routines to fetch immediate parents */

unsigned eapGraphAnyParent(struct eapGraph *eg, unsigned fileId);
/* Return fileId of a parent chosen at convenience, zero if none */

unsigned eapGraphSingleParent(struct eapGraph *eg, unsigned fileId);
/* Return fileId of a parent, and make sure no more than one.  Returns zero if none. */

unsigned eapGraphOneSingleParent(struct eapGraph *eg, unsigned fileId);
/* Get file ID of parent gauranteeing that it exists and there is just one or aborting. */

struct slRef *eapGraphParentList(struct eapGraph *eg, unsigned fileId);
/* Return list of all parents, possibly NULL.  List is in form of slRefs with eapInput vals. 
 * Do not free this list, it is owned by graph. */


/* Routines to fetch inputs and outputs of run */

struct slRef *eapGraphRunInputs(struct eapGraph *eg, unsigned runId);
/* Fetch all inputs to this run.  Vals on slRef are eapInputs. 
 * Do not free this list, it is owned by graph. */

struct slRef *eapGraphRunOutputs(struct eapGraph *eg, unsigned runId);
/* Fetch all outputs to this run.  Vals on slRef are eapOutputs.   
 * Do not free this list, it is owned by graph. */


/* Routines to fetch more general ancestors */

unsigned eapGraphAnyAncestorOfFormat(struct eapGraph *eg, unsigned fileId, char *format);
/* Return first convenient ancestor of given format */

void eapGraphAncestorsOfFormat(struct eapGraph *eg, unsigned fileId, 
    char *format, int maxGenerations, struct slRef **retList);
/* Return list of all ancestors of format.  Set maxGenerations to -1 for any number of
 * generations back,   otherwise 1 will stop at parents, 2 at grandparents, etc. The vals
 * on the returned list are eapInputs. This returned value should be slFreeList()'d when
 * done. */

/* Routines to fetch immediate children */

unsigned eapGraphAnyChild(struct eapGraph *eg, unsigned fileId);
/* Return fileId of a child chosen at convenience, zero if none */

unsigned eapGraphSingleChild(struct eapGraph *eg, unsigned fileId);
/* Return fileId of a child, and make sure no more than one.  Returns zero if none. */

unsigned eapGraphOneSingleChild(struct eapGraph *eg, unsigned fileId);
/* Get file ID of child gauranteeing that it exists and there is just one or aborting. */

struct slRef *eapGraphChildList(struct eapGraph *eg, unsigned fileId);
/* Return list of all children, possibly NULL.  List is in form of slRefs with eapOutput vals. */


/* Routines to fetch more general descendants */

unsigned eapGraphAnyDescendantOfFormat(struct eapGraph *eg, unsigned fileId, char *format);
/* Return first convenient descendant of given format */

void eapGraphDescendantsOfFormat(struct eapGraph *eg, unsigned fileId, 
    char *format, int maxGenerations, struct slRef **retList);
/* Return list of all descendants of format.  Set maxGenerations to -1 for any number of
 * generations back,   otherwise 1 will stop at children, 2 at grandchildren, etc. The vals
 * on the returned list are eapOutputs. This returned value should be slFreeList()'d when
 * done. */

struct slRef *eapGraphAllAncestors(struct eapGraph *eg, unsigned fileId);
/* Return list of all ancestors.  Values of slRef are eapOutputs from which you can
 * harvest either ancestral fileIds or analysis runIds. */

struct slRef *eapGraphAllDescendants(struct eapGraph *eg, unsigned fileId);
/* Return list of all ancestors.  Values of slRef are eapInputs from which you can
 * harvest either ancestral fileIds or analysis runIds. */

#endif /* EAPGRAPH_H */


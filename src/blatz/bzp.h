/* bzp - blatz parameters.  Input settings structure for aligner.  Routine to
 * set options from command line.  A debugging/profiling utility function. */

#ifndef BZP_H
#define BZP_H

struct blatzIndex;

struct bzp
/* A structure with parameters for blatz.  These must all be 
 * initialized by calling program. */
    {
    int weight;	      /* Weight of seed. */
    boolean rna;	/* True if want to find introns. */
    int minScore;     /* Minimum score for final chain to be output. */
    int multiHits;	/* If non-zero takes two hits on diag to trigger MSP. */
    int minGapless;    /* Min score for MSP to trigger extension */
    int minChain;     /* Min score for chain to trigger extension. */
    int maxDrop;      /* Maximum amount score can drop before stopping. */
    int maxExtend;    /* Maximum size of gapped extension. */
    int maxBandGap;   /* Maximum gap allowed in banded extension. */
    int minExpand;    /* Min score for search in area with smaller seed. */
    int expandWindow;  /* Bases to do secondary more sensitive search in. */
    struct axtScoreScheme *ss; /* Matrix and affine gap info. */
    struct gapCalc *cheapGap;  /* Gap calculation info for first pass. */
    struct gapCalc *gapCalc;   /* Gap calculation info for final pass. */
    boolean bestChainOnly;  /* Only keep best chain (from MSP on) */
    char *out;		/* Output format.  Chain, axt, psl, etc. */
    char *mafQ;		/* Prefix for query side of maf output. */
    char *mafT;		/* Prefix for target side of maf output. */
    };

struct bzp *bzpDefault();
/* Return default parameters */

void bzpSetOptions(struct bzp *bzp);
/* Modify options from command line. */

void bzpClientOptionsHelp(struct bzp *bzp);
/* Explain options having to do with client side of alignments. */

void bzpServerOptionsHelp(struct bzp *bzp);
/* Explain options having to do with server side of alignments. */


#define BZP_SERVER_OPTIONS \
   {"weight", OPTION_INT}, 

#define BZP_CLIENT_OPTIONS \
   {"rna", OPTION_BOOLEAN}, \
   {"minScore", OPTION_INT}, \
   {"multiHits", OPTION_INT}, \
   {"minGapless", OPTION_INT}, \
   {"minChain", OPTION_INT}, \
   {"maxDrop", OPTION_INT}, \
   {"maxExtend", OPTION_INT}, \
   {"maxBandGap", OPTION_INT}, \
   {"expandWindow", OPTION_INT}, \
   {"minExpand", OPTION_INT}, \
   {"matrix", OPTION_STRING}, \
   {"gapCost", OPTION_STRING}, \
   {"out", OPTION_STRING}, \
   {"mafQ", OPTION_STRING}, \
   {"mafT", OPTION_STRING},

#define bzpDefaultPort 18273

int bzpVersion();
/* Return version number. */

void bzpTime(char *label, ...);
/* Print label and how long it's been since last call.  Call with 
 * a NULL label to initialize. */

extern boolean bzpTimeOn; /* If set to FALSE bzpTime's aren't printed. */

#endif /* BZP_H */

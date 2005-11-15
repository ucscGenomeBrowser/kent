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
    boolean bestScoreOnly;  /* Only output best scoring chain */
    int multiHits;	/* If non-zero takes two hits on diag to trigger MSP. */
    int transition;   /* If non-zero look allow one transition in seed. */
    int minGapless;    /* Min score for MSP to trigger extension */
    int minChain;     /* Min score for chain to trigger extension. */
    int maxDrop;      /* Maximum amount score can drop before stopping. */
    int maxExtend;    /* Maximum size of gapped extension. */
    int maxBandGap;   /* Maximum gap allowed in banded extension. */
    int minExpand;    /* Min score for search in area with smaller seed. */
    int expandWindow;  /* Bases to do secondary more sensitive search in. */
    int maxChainsToExplore;  /* Maximum number of chains to explore. */
    struct axtScoreScheme *ss; /* Matrix and affine gap info. */
    struct gapCalc *cheapGap;  /* Gap calculation info for first pass. */
    struct gapCalc *gapCalc;   /* Gap calculation info for final pass. */
    boolean unmask;	/* Unmask lower case sequence. */
    char *out;		/* Output format.  Chain, axt, psl, etc. */
    char *mafQ;		/* Prefix for query side of maf output. */
    char *mafT;		/* Prefix for target side of maf output. */
    /* LX BEG */
    int dynaLimitT;		/* Hit limit for dynamic masking. */
    int dynaLimitQ;		/* Hit limit for dynamic masking. */
    int dynaWordCoverage;	/* Word hit limit for dynamic masking. */
    char *dynaBedFileQ;		/* filename to use for dynamic mask output */
    /* LX END */
    boolean bestChainOnly;	/* Only keep best scoring chain (from MSP on). Not exported */
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
   {"bestScoreOnly", OPTION_BOOLEAN}, \
   {"multiHits", OPTION_INT}, \
   {"transition", OPTION_INT}, \
   {"minGapless", OPTION_INT}, \
   {"minChain", OPTION_INT}, \
   {"maxDrop", OPTION_INT}, \
   {"maxExtend", OPTION_INT}, \
   {"maxBandGap", OPTION_INT}, \
   {"expandWindow", OPTION_INT}, \
   {"minExpand", OPTION_INT}, \
   {"matrix", OPTION_STRING}, \
   {"gapCost", OPTION_STRING}, \
   {"unmask", OPTION_BOOLEAN}, \
   {"out", OPTION_STRING}, \
   {"mafQ", OPTION_STRING}, \
   {"mafT", OPTION_STRING}, \
   {"dynaLimitT", OPTION_INT}, \
   {"dynaLimitQ", OPTION_INT}, \
   {"dynaWordCoverage", OPTION_INT}, \
   {"dynaBedFileQ", OPTION_STRING},

#define bzpDefaultPort 18273

int bzpVersion();
/* Return version number. */

void bzpTime(char *label, ...);
/* Print label and how long it's been since last call.  Call with 
 * a NULL label to initialize. */

extern boolean bzpTimeOn; /* If set to FALSE bzpTime's aren't printed. */

#endif /* BZP_H */


struct element;

struct possibleEdge
{
    struct possibleEdge *next;
    int count;
    boolean doFlip;
    struct element *element;
};

struct adjacency
{
    struct possibleEdge *prev;
    struct possibleEdge *next;
 //   struct hash *prevHash;  
  //  struct hash *nextHash;
};

struct chromBreak
{
    struct chromBreak *next;
    struct element *element;
};

struct genome
{
    struct genome *next;
    struct chromBreak *breaks;
    struct hash *elementHash;
    struct phyloTree *node;
    char *name;
    struct element *elements;
};

struct distance
{
    struct distance *next;
    struct element *e1;
    struct element *e2;
    double distance;
    int used;
    int new;
};

struct eleDistance
{
    struct eleDistance *next;
    struct distance *distance;
};

struct element
{
    struct element *next;
    struct genome *genome;
    char *species;
    char *name;
    char *version;
    int count;
    int isFlipped;
    struct adjacency up;
    struct adjacency down[2];
    struct adjacency mix;
    struct adjacency calced;
    struct eleDistance *dist;
    //struct hash *nextHash;
    //struct hash *prevHash;
    int allocedEdges;
    int numEdges;
    struct element *parent;
    struct element **edges;
} ;

extern void setElementDist(struct element *e1, struct element *e2, double dist,
    struct distance **distanceList, struct hash **pDistHash, struct hash **pDistEleHash);
extern struct distance *readDistances(char *fileName, struct hash  *genomeHash,
    struct hash **pDistHash, struct hash **pDistEleHash);
extern struct genome *readGenomes(char *fileName);

extern struct element *eleAddEdge(struct element *parent, struct element *child);
/* add an edge to an element */
extern char *eleFullName(struct element *e, boolean doNeg);
extern char *eleName(struct element *e);
extern struct phyloTree *eleReadTree(char *fileName, boolean addStartStop);
extern void printElementTrees(struct phyloTree *node, int depth);
extern struct element *newElement(struct genome *g, char *name, char *version);
extern void outElementTrees(FILE *f, struct phyloTree *node);
extern struct genome *getGenome(char *file, char *name);
extern char *nextVersion();
extern char *nextGenome();
extern void removeXs(struct genome *list, char ch);
extern void removeAllStartStop(struct phyloTree *node);
extern void removeStartStop(struct genome *g);
extern int countLeaves(struct phyloTree *node);

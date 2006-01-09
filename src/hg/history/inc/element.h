
struct element
{
    struct element *next;
    struct genome *genome;
    char *species;
    char *name;
    char *version;
    int count;
    int isFlipped;
    int allocedEdges;
    int numEdges;
    struct element *parent;
    struct element **edges;
};

struct genome
{
    struct genome *next;
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


extern void setElementDist(struct element *e1, struct element *e2, double dist,
    struct distance **distanceList);
extern struct distance *readDistances(char *fileName, struct hash  *genomeHash);
extern struct genome *readGenomes(char *fileName);

extern struct element *eleAddEdge(struct element *parent, struct element *child);
/* add an edge to an element */
extern char *eleName(struct element *e);
extern struct phyloTree *eleReadTree(char *fileName);
extern void printElementTrees(struct phyloTree *node, int depth);

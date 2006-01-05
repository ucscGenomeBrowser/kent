
struct element
{
    struct element *next;
    struct genome *genome;
    char *species;
    char *name;
    char *version;
    int used;
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
};

extern struct distance *readDistances(char *fileName, struct hash  *genomeHash);
extern struct genome *readGenomes(char *fileName);

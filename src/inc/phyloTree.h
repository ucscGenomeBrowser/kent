/* phlyogenetic trees */

struct phyloName
{
    char *name;		/* name of this node */
    double length;	/* length of the branch to this node */
};

struct phyloTree
{
    struct phyloTree *next;		/* next in single linked list */
    struct phyloTree *parent;		/* the parent of this node */
    struct phyloTree *left;		/* the left child of this node */
    struct phyloTree *right;		/* the right child of this node */
    struct phyloName *ident;		/* the name and branch length */
    struct phyloTree *mark;		/* probably the favorite child */
};

extern struct phyloTree *phyloReadTree(struct lineFile *lf);
/* reads a phyloTree from lineFile (first line only) */

extern struct phyloTree *phyloParseString(char *string);
/* build a phyloTree from a string */

extern void phyloPrintTree( struct phyloTree *,FILE *f);
/* print a phyloTree to f */

extern char *phyloFindPath(struct phyloTree *tree, char *ref, char *cross);
/* find the shortest path from ref to cross (returns a list
 * of the node names separated by spaces */

extern char *phyloNodeNames(struct phyloTree *tree);
/* return list of all the node names separated by spaces */

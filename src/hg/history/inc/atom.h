
struct instance;

struct baseAtom
{
struct baseAtom *next;
int numBaseAtoms;
int *baseAtomNums;
};

struct atom
{
struct atom *next;
char *name;
int numInstances;
int length;
struct instance *instances;
int stringNum;
struct baseAtom *baseAtoms;
};

struct instance
{
struct instance *next; /* in atom */
int num; /* in atom */
char *species;
char *chrom;
int start;
int end;
char strand;
struct instance **seqPos; /* in sequence */
struct atom *atom;
};

struct sequence
{
struct sequence *next;
char *name;
int length;
struct instance **data;
char *chrom;
int chromStart;
int chromEnd;
};


struct atomString
{
struct atomString *next;
int num;
int length;
int count;
struct instance *instances;
struct baseAtom *baseAtoms;
};

struct atom *getAtoms(char *atomsName, struct hash *atomHash);

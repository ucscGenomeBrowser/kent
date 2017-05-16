
struct seqBuffer
{
char *species;
char *buffer;
char *position;
char lastTwo[2];
};

struct alignDetail
{
char *proName;
int exonNum;
int exonSize;
int exonCount;
//char *position;
//char strand;
int startFrame;
int endFrame;
int numSpecies;
struct seqBuffer *seqBuffers;
char *geneName;
};


typedef void (*columnFunc)( struct alignDetail *detail, int cNum, void *closure);
typedef void (*alignFunc)( struct alignDetail *detail, columnFunc cfunc, void *closure);


void allColumns( struct alignDetail *detail, columnFunc cfunc, void *closure);
void fullColumns( struct alignDetail *detail, columnFunc cfunc, void *closure);
void binColumns( struct alignDetail *detail, columnFunc cfunc, void *closure);

void parseAli(char *treeFilename, char *fastaFile, 
    alignFunc afunc, columnFunc cfunc, void *closure);

boolean isDashXorZ(char aa);

struct hash *getOrderHash(char *file);
struct slName *getOrderList(char *file);
struct aaMap
{
char *empty;
double *map;
};

struct aaMap *readAAMap(char *fileName);

char *getPosString(char *oldPos, char strand, int resNum, int sfn, int efn);

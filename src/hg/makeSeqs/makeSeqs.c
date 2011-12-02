/** makeSeqs.c - Makes *.seq files from the *.fa output of faSplit */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"


/** Method prototypes */
struct dyString *makeSeqList(char *seqdata, char *path);
void usage(char **argv);

void usage(char **argv)
{
   if(argv[1] == NULL || argv[2] == NULL) 
        {
        errAbort(
          "makeSeqs - Create *.seq files from *.fa files.\n"
          "usage:\n   makeSeqs filename.seqs fafilelist [path]\n"
           );
         }
}

struct dyString *makeSeqList(char *seqdata, char* path)
{
FILE *infile, *outfile;
char *line, *token;
struct dyString *seqlist = newDyString(128);
infile = mustOpen(seqdata, "r");
    if(path != NULL)
        dyStringAppend(seqlist, path);
dyStringAppend(seqlist, "seqlist");
outfile = mustOpen(seqlist->string, "w");
     while( (line = readLine(infile)) != NULL )
         {
         token = firstWordInLine(line);
             if(startsWith(">", token))
                 {
                 stripChar(token, '>');
                 fprintf(outfile, "%s.seq\n", token);
                 }
         freeMem(line);
         }
carefulClose(&infile);
carefulClose(&outfile);
return seqlist;
}

int main(int argc, char** argv)
{
FILE *fafile, *seqfile;
char *temp1, *temp2;
struct dyString *seqlist, *faname, *seqname, *path;
faname = newDyString(128);
seqname = newDyString(128);
path = newDyString(128);
usage(argv);
    if(argv[3] != NULL)
        {
        dyStringAppend(path, argv[3]);
            if(!endsWith(path->string, "/"))
                dyStringAppend(path, "/");
        }
seqlist = makeSeqList(argv[1], path->string);
fafile = mustOpen(argv[2], "r");
seqfile = mustOpen(seqlist->string, "r");
     while( (temp1 = readLine(fafile)) != NULL && 
            (temp2 = readLine(seqfile)) != NULL )
         {
         if(path->string != NULL)
             {
             dyStringPrintf(faname, "%s", path->string);
             dyStringPrintf(seqname, "%s", path->string);
             }
         dyStringPrintf(faname, "%s", temp1);
         dyStringPrintf(seqname, "%s",  temp2);
         rename(faname->string, seqname->string);
         freeMem(temp1);
         freeMem(temp2);  
         dyStringClear(faname);
         dyStringClear(seqname);
         }
carefulClose(&fafile);
freeDyString(&seqlist);
freeDyString(&faname);
freeDyString(&seqname);
return 0;
}



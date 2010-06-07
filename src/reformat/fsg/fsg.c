/* Filter Sanger Gff - extract genes. */

#include "common.h"
#include "hash.h"

char *inNames[] = {
    "CHROMOSOME_I.gff",
    "CHROMOSOME_II.gff",
    "CHROMOSOME_III.gff",
    "CHROMOSOME_IV.gff",
    "CHROMOSOME_V.gff",
    "CHROMOSOME_X.gff",
};

char *outNames[] = {
    "i.gff",
    "ii.gff",
    "iii.gff",
    "iv.gff",
    "v.gff",
    "x.gff",
};

struct gffRec
	{
	struct gffRec *next;
	char *line;
	boolean isSeq;
        int start;
	int end;
	};

struct groupLines
	{
	struct groupLines *next;
	char *name;
	struct gffRec *lines;
	};

int cmpRec(const void *va,  const void *vb)
{
struct gffRec **pa = (struct gffRec **)va, **pb = (struct gffRec **)vb;
struct gffRec *a = *pa, *b = *pb;
int dif;
dif = b->isSeq - a->isSeq;
if (dif == 0)
    dif = a->start - b->start;
if (dif == 0)
    dif = a->end - b->end;
return dif;
}

int main(int argc, char *argv[])
{
FILE *in;
FILE *out;
char line[1024];
char origLine[1024];
char *words[256];
int wordCount;
struct groupLines *gl;
struct gffRec *gr;
struct hashEl *hel;
struct hash *hash;
char *groupName;
int i;

if (argc != 2 || differentWord(argv[1], "now"))
    {
    errAbort("fsg - Format Sanger Gff.  Converts Sanger format GFF to GFF gene\n"
             "predictions clumped by gene\n"
             "usage:\n"
             "    fsg now\n"
             "This needs to be run from directory with CHROMOSOME_N.gff files.\n"
             "It will create n.gff files.");
    }

for (i=0; i<ArraySize(inNames); ++i)
    {
    struct groupLines *groupList = NULL;
    printf("Processing %s to %s\n", inNames[i], outNames[i]);
    in = mustOpen(inNames[i], "r");
    out = mustOpen(outNames[i], "w");
    hash = newHash(14);
    while (fgets(line, sizeof(line), in))
        {
        if (strncmp(line, "CHROMOSOME", 10) != 0)
            continue;
        strcpy(origLine, line);
        wordCount = chopLine(line, words);
        if (wordCount >= 10)
	    {
	    if ((sameString(words[1], "Coding") ||
	        sameString(words[1], "hand_built") ||
                sameString(words[1], "stl") ||
                sameString(words[1], "stl-preliminary") ||
	        sameString(words[1], "Genefinder"))
		    &&
	        strcmp(words[2], "coding_exon") != 0)
	        {
	        groupName = words[9];
	        if ((hel = hashLookup(hash, groupName)) == NULL)
		    {
		    AllocVar(gl);
		    hel = hashAdd(hash, groupName, gl);
		    gl->name = hel->name;
		    slAddHead(&groupList, gl);
		    }
	        gl = hel->val;
	        AllocVar(gr);
                if (sameString(words[2], "sequence"))
                    gr->isSeq = TRUE;
	        gr->line = cloneString(origLine);
	        gr->start = atoi(words[3]);
	        gr->end = atoi(words[4]);
	        slAddTail(&gl->lines, gr);
	        }
	    }
        }
    slReverse(&groupList);
    for (gl = groupList; gl != NULL; gl = gl->next)
        {
        slSort(&gl->lines, cmpRec);
        for (gr = gl->lines; gr != NULL; gr = gr->next)
	    {
	    fputs(gr->line, out);
	    }
        }
    fclose(in);
    fclose(out);
    freeHash(&hash);
    slFreeList(&groupList);
    }
return 0;
}

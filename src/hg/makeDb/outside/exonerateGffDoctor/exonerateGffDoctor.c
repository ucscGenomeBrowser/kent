/* Make the exonerate GFF format into a genePred */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dystring.h"
#include "options.h"
#include "linefile.h"

void usage() 
{
errAbort("exonerateGffDoctor - Make the exonerate GFF format readable by ldHgGene. .\n"
	 "usage:\n"
	 "   exonerateGffDoctor file.gff");
}

void fixGff(struct lineFile *lf) 
{
char *geneName = NULL;
char *fields[17], *line = NULL;
int groupNum = 1;
while (lineFileNext(lf,&line,NULL))
    {
    char *tmp = NULL;
    if ((line[0] == '#') || startsWith("Command", line) || startsWith("Hostname", line) || startsWith("--", line)
	|| stringIn("intron", line) || stringIn("splice3", line) || stringIn("splice5", line) || stringIn("similarity", line))
	continue;
    tmp = cloneString(line);
    chopLine(tmp, fields);
    if (sameString(fields[2], "gene"))
        {
	fields[2] = "transcript";
	if (geneName) 
	    freeMem(geneName);
	geneName = cloneString(fields[12]);
	groupNum++;
	}
    else if (sameString(fields[2], "exon"))
	fields[2] = "CDS";
    else
	continue;
    printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\tex%d.%s\n", fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7], groupNum, geneName);
    freeMem(tmp);
    }
}

int main(int argc, char *argv[])
/* The program */
{
struct lineFile *lf = lineFileOpen(argv[1], TRUE);
fixGff(lf);
lineFileClose(&lf);
return 0;
}

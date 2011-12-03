#include "common.h"
#include "synMap.h"
#include "binMap.h"


void usage()
{
errAbort("createBinMap - using an all against all synMap from Lior at\n"
	 "Berkeley or Sanja and Robert from UCSC create a mapping from\n"
	 "genomeA bits of size N to all the bits of genomeB that map to it.\n"
	 "The idea is to have a particular piece of a genome and then find\n"
	 "all the pieces of the other genome that align to it.\n"
	 "usage:\n\t"
	 "createBinMap <fileNameIn> <fileNameOut> <genome {A,B}> <reference size>\n");
}

int main(int argc, char *argv[])
{
errAbort("Not Done yet!\n");
if(argc != 5)
    usage();
return 0;
}

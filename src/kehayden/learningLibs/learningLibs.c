/* learningLibs - A program to help learn the kent libraries.
 * A basic script that reads in a text file and breaks
 * into word frequencies. */

#include "common.h"
/* includes basic utilities for script */
#include "linefile.h"
/* lib to help read text files */
#include "hash.h"
/* creating a hash table */
#include "options.h"
/* useful to parse commandline options */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "learningLibs  - A program to help learn the kent libraries.\n"
  "usage: reads in text file, breaks each line into collection of word frequencies \n"
  "    learningLibs textFile\n"
  "    example textFile path:  /cluster/home/kehayden/rpNH.txt\n"
  "options:\n"
  "   -t=textFile [argv0] ** currently (-t) is not working\n"
    );
}

static struct optionSpec options[] = {
  /*  Not sure what this is doing, but see something similar in lib/options.c */
   {NULL, 0},
   /* I think there needs to be some option for "-t" ?? */
};


void learningLibs(char *textFile)
/* learningLibs - A program to help learn the kent libraries.*/
{
  /*Open and Assign file to pointer:   similar to twoBit.c line 962 */
  struct lineFile *lf = lineFileOpen(textFile, TRUE); 

  /* initialize hash: similar to hash.h line 3 */
  /* struct hash *hash = hashNew(0); */
  #define COUNT 140
  #define INIT 1
  char *line;

  int i;
  int wordCount;
  int totalWords;
  totalWords=0;

  while (lineFileNextReal(lf, &line)) 
    {
      char *words[COUNT];  /*character array named 'words' and an unspecified number of elements. */
      /* initialize inside of block so that automatic storage duration is set recreate each loop */

      /* printf(line); */
      /* Chop line by white space. common.h line 893*/
      wordCount=chopLine(line, words);

      /* sum all words to generate frequency of each word in document */
      totalWords=totalWords + wordCount;
      /* totalWords summation is working:  1789 words obs */
      /* printf("%6d\n",totalWords);*/

      /* process each element of array words[] */
      for( i = 0; i < COUNT; i++ )
	{
	  printf("%4d\t%20s\n",i,words[i]);
	  /* store each word into a hash counter */
	  /* if exists in hash:  increment counter */
	  /* else inialize hash with count = 1 */
	  /*hashAdd(hash, words[i], INIT) ; */
	}

      
    }		
      lineFileClose(&lf);
}



int main(int argc, char *argv[])
/* Process command line. */
{

optionInit(&argc, argv, options);
if (argc != 2)
     usage();
     learningLibs(argv[1]);
     return 0;
}

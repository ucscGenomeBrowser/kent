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
#include "tokenizer.h"
/* useful to break string including punctuation */

boolean allCaps = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tokenizerLib  - A modification of learningLibs to incorporate the tokenizerLib.\n"
  "usage: reads in text file, breaks each line into collection of word frequencies **NEW: breaking on punctuation**\n"
  "    tokenizerLib textFile\n"
  "    example textFile path:  /cluster/home/kehayden/rpNH.txt\n"
  "options:\n"
  "   -verbose=N print out extra diagnostic information. 0 for silent, 1 default, 2 extra\n"
  "   -allCaps if set then will convert all words to upper case\n"
    );
}

static struct optionSpec options[] = {
  /*  Not sure what this is doing, but see something similar in lib/options.c */
   {"allCaps", OPTION_BOOLEAN},
   {NULL, 0},
   
};

struct wordTracker
/* Information to track a word. */
    {
    struct wordTracker *next;  /* next in list */
    char *word;   /* The string value of the word - not allocated here. */
int count;   /* Number of times a word is used. */
};

int wordTrackerCmpCount(const void *va, const void *vb)
/* Compare two word trackers by count, useful with sorting. */
{
  const struct wordTracker *a = *((struct wordTracker **)va);
  const struct wordTracker *b = *((struct wordTracker **)vb);
  return a->count - b->count;
}

int wordTrackerCmpWord(const void *va, const void *vb)
/* Compare two word trackers by word , useful with sorting. */
{
  const struct wordTracker *a = *((struct wordTracker **)va);
  const struct wordTracker *b = *((struct wordTracker **)vb);
  return strcmp(a->word, b->word);
}

void learningLibs(char *textFile)
/* learningLibs - A program to help learn the kent libraries.*/
{
  /*Open and Assign file to pointer:   similar to twoBit.c line 962 */
  struct lineFile *lf = lineFileOpen(textFile, TRUE); 

  /* tokenizer.h L29; and similar to lib/rqlParse.c*/
   struct tokenizer *tkz=tokenizerOnLineFile(lf);
  /* Create a new tokenizer on open lineFile. */

  /* initialize hash: similar to hash.h line 3 */
  struct hash *hash = hashNew(0); 
  #define COUNT 140
  #define INIT 1
  char *line;

  int i;
  int wordCount;
  int totalWords;
  struct wordTracker *wordList = NULL;
  totalWords=0;

  while (lineFileNextReal(lf, &line)) 
      {
	if (allCaps)
	  strUpper(line);
	
	char *words[COUNT];
	/*char *tmpWords[COUNT];*/
	/*character array named 'words' and an unspecified number of elements. */
	/* initialize inside of block so that automatic storage duration is set recreate each loop */
	
	/* printf(line); */
	/* Chop line by white space. common.h line 893*/
	wordCount=chopLine(line, words);
	/* add tokenizerLib here ?? or in loop below; rqlParse.c L290*/
	/* HERE?  char *words = tokenizerNext(tkz); */
	/*char *tokenizerNext(struct tokenizer *tkz);*/
    
	
	/* sum all words to generate frequency of each word in document */
	totalWords=totalWords + wordCount;
      /* totalWords summation is working:  1789 words obs */
      /* printf("%6d\n",totalWords);*/

      /* process each element of array words[] */
      for (i = 0; i < wordCount; i++)
	  {
	  verbose(2,"%4d\t%20s\n",i,words[i]);
	  char *word = tokenizerNext(tkz);
          /*char *word = words[i];*/
          struct wordTracker *tracker = hashFindVal(hash, word);
          if (!tracker)
	    {
	      AllocVar(tracker);
	      hashAddSaveName(hash, word, tracker, &tracker->word);
              slAddHead(&wordList, tracker);
	    }
          tracker->count += 1;
	  /* store each word into a hash counter */
	  /* if exists in hash:  increment counter */
	  /* else inialize hash with count = 1 */
	  /*hashAdd(hash, words[i], INIT) ; */
	  }
      
    }

  // Sort, loop through list and iterate.     
  slSort(&wordList, wordTrackerCmpCount);
  struct wordTracker *tracker;
  for (tracker = wordList; tracker != NULL; tracker = tracker->next)
    printf("%s %d\n", tracker->word, tracker->count);
		
      lineFileClose(&lf);
}



int main(int argc, char *argv[])
/* Process command line. */
{

optionInit(&argc, argv, options);
allCaps = optionExists("allCaps");
if (argc != 2)
     usage();
learningLibs(argv[1]);
return 0;
}

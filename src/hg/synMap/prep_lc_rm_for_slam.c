#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


#define REPCHAR 'N'
#define REPHEADER "   SW  perc perc perc  query     position in query    matching repeat       position in  repeat\nscore  div. del. ins.  sequence  begin  end (left)   repeat   class/family begin  end (left)  ID\n\n"

int convertFasta(char *inFileName);

int main(int argc,char *argv[]) {
  int i,fail;

  for(i=1,fail=0; i<argc; i++) {
    if(convertFasta(argv[i]) == EXIT_FAILURE) {
      fprintf(stderr,"convertFasta: problem processing %s\n",argv[i]);
      fail=1;
    }
  }
  return(fail);
}


int convertFasta(char *inFileName) {
  FILE *inFile,*outFile,*maskedFile;
  char *outFileName,*maskedFileName;
  char c;
  int done,headerStarted,headerDone,inRepeat,i,seqDone,repStart;

  /* make name of .out file */
  outFileName = (char *)malloc((10+strlen(inFileName))*sizeof(char));
  strcpy(outFileName,inFileName);
  strcat(outFileName,".out");

  /* make name of .masked file */
  maskedFileName = (char *)malloc((10+strlen(inFileName))*sizeof(char));
  strcpy(maskedFileName,inFileName);
  strcat(maskedFileName,".masked");

  if ( ((inFile = fopen(inFileName,"r")) == NULL) || ((outFile = fopen(outFileName,"w")) == NULL)|| ((maskedFile = fopen(maskedFileName,"w")) == NULL) )
    return(EXIT_FAILURE);
  fprintf(outFile,REPHEADER);

  /* Read to beginning of fasta header */
  headerStarted=0;
  while(!done & !headerStarted) {
    c = fgetc(inFile);
    if(c == EOF) {
      done = 1;
    } else {
      fputc(c,maskedFile);
      if(c == '>')
        headerStarted=1;
    }
  }

  for(done=0; !done; ) {
    headerDone=0;
    while(!done & !headerDone) {
      c = fgetc(inFile);
      if(c == EOF) {
        done = 1;
      } else {
        fputc(c,maskedFile);
        if(c == '\n')
          headerDone=1;
      }
    }
    for(i=0,inRepeat=0,seqDone=0; !done & !seqDone;) {
      c = fgetc(inFile);
      if(c == EOF) {
        done = 1;
      } else if(c == '>') {
        seqDone = 1;
      } else {
        if(isspace(c)) {
          fputc(c,maskedFile);
        } else {
          if(inRepeat) {
            if(isupper(c)) {
              inRepeat=0;
              fprintf(outFile,"    0   0.0  0.0  0.0     query  %5d %4d    (0) +  MIR      SINE/MIR         0    0    (0)   0\n",repStart+1,i);
              fputc(c,maskedFile);
            } else {
              fputc(REPCHAR,maskedFile);
            }
          } else {
            if(islower(c)) {
              inRepeat=1;
              repStart=i;
              fputc(REPCHAR,maskedFile);
            } else {
              fputc(c,maskedFile);
            }
          }
          i++;
        }
      }
    }
    if(inRepeat)
      fprintf(outFile,"    0   0.0  0.0  0.0     query  %5d %4d    (0) +  MIR      SINE/MIR         0    0    (0)   0\n",repStart+1,i);
  }
  
  fclose(inFile);
  fclose(outFile);
  fclose(maskedFile);
  free(outFileName);
  free(maskedFileName);

  return(EXIT_SUCCESS);
}

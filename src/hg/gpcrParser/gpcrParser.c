/** gpcrParser.c - Parses transmembrane cooridnates, outputs to xml file. */
#include "common.h"
#include "memalloc.h"

/** Method prototypes */
void catSeqFile(FILE* outfile, char* filename);
void getTransFromFile(FILE* infile, char *path);
FILE* createXMLFile(char* proteinID, char *path);
void populateXMLFile(FILE *outfile, int tmNumber, char *proteinID, char *path);
void addCoordinatesToXMLFile(FILE *outfile, int start, int end, char* mapTo, int count);
void finishXMLFile(FILE *outfile, char *proteinID, int seqLength);
void usage(char **argv);

void usage(char **argv)
{
   if(argv[1] == NULL || argv[2] == NULL) 
        {
        errAbort(
          "gpcrParser - Create xml files for gpcr snakeplots.\n"
          "usage:\n   gpcrParser TMHMMresult.htm path\n"
          "    The first parameter is the html output from www.cbs.dtu.dk\n"
          "    Path is the working directory holding the *.seq files needed to\n"
          "    create the xml files. The xml files will be created in the path location.\n"
           );
         }
}

void catSeqFile(FILE* outfile, char* filename)
/** Copies contents of one file to another file */
{
FILE* infile;
char* line = NULL;
infile = mustOpen(filename, "r");
     while( (line = readLine(infile))  != NULL )
     {
     fprintf(outfile, "\t\t%s\n", line);
     freeMem(line);
     }
carefulClose(&infile);
}

void finishXMLFile(FILE *outfile, char *proteinID, int seqLength)
/** Add the closing lines to the filed*/
{
fprintf(outfile, 
   "\t\t\n</secondary-structure>\n\n\t</protein>\n\n
    \t<diagram>\n\n\t\t<colors>\n\n
    \t\t\t<define color=\"white\" red=\"255\" green=\"255\" blue=\"255\"/>\n\n
    \t\t\t<define color=\"red\" red=\"255\" green=\"0\" blue=\"0\"/>\n\n
    \t\t\t<color-scheme name=\"GPCRDB\">\n\n
    \t\t\t\t<residue-background color=\"white\"/>\n\n\t\t\t</color-scheme>\n\n
    \t\t\t<residue-color position=\"20\" color=\"white\"/>\n\n
    \t\t\t<residue-color position=\"21\" color=\"red\"/>\n\n
    \t\t</colors>\n\n\t\t<elipses>\n\n
    \t\t\t<exclude residueRange=\"1-%d\" />\n\n
    \t\t\t<include residueRange=\"1-1\"/>\n\n\t\t</elipses>\n\n
    \t\t<output>\n\n\t\t\t<image format=\"GIF\" filename=\"%s.gif\"/>\n\n
    \t\t\t<page template=\"H:\\dev\\crover\\nrdb\\sample-template.xsl\" filename=\"gpcr-rbdg-out.xml\" diagram-name=\"gpcr\"/>\n\n
    \t\t\t<image-map filename=\"%s.map\"/>\n\n\t\t</output>\n\n
    \t</diagram>\n\n</rbde-diagram>", 
    seqLength-1, proteinID, proteinID);
}

void addCoordinatesToXMLFile(FILE *outfile, int start, int end, char* mapTo, int count)
/** Add each coordinate for the gpcr */
{
fprintf(outfile, "\t\t\t");
   if( sameString(mapTo, "TM") )
       {
       fprintf(outfile, 
           "<segment start=\"%d\" end=\"%d\" mapto=\"%s%d\" />\n", 
           start, end, mapTo, count);
       }
       else
       {
       fprintf( outfile, 
           "<segment start=\"%d\" end=\"%d\" mapto=\"%s\" />\n",
           start, end, mapTo );
       }
}

void populateXMLFile(FILE *outfile, int tmNumber, char *proteinID, char *path)
/** Adds the number of TM helices and sequence the the xml file */
{
char *filename = (char*)malloc(128*sizeof(char));
strcpy(filename,path);
        if(!endsWith(path, "/"))
           strcat(filename, "/");
strcat(filename, proteinID);
strcat(filename, ".seq"); 
fprintf(outfile, 
    "%d\" id-prefix=\"TM\" nterm-id=\"N-term\" cterm-id=\"C-term\" direction=\"down\" />\n\n
    \t</diagram-layout>\n\n
    \t<protein>\n\n\t\t<name>%s</name>\n\n\t\t<residue-codes>\n\n", 
    tmNumber, proteinID);
catSeqFile(outfile, filename);
fprintf(outfile, "\n\n\t\t</residue-codes>\n\n\t\t<secondary-structure>\n\n");
free(filename);
}

FILE* createXMLFile(char* proteinID, char *path)
{
FILE* outfile;
char *filename = (char*)malloc(128*sizeof(char));
    if(path != NULL)
        {
        strcpy(filename,path);
        strcat(filename, "/");
        }
strcat(filename,proteinID);
strcat(filename, ".xml");
outfile = mustOpen(filename, "w");
fprintf(outfile, 
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n\n
    <rbde-diagram xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"H:\\dev\\crover\\xml\\xsd\\residue-based-diagram.xsd\">\n\n
    \t<diagram-layout>\n\n
    \t\t<tm-bundle tm-number=\"");
free(filename);
filename = NULL;
return outfile;
}

void getTransFromFile(FILE* infile, char *path)
{
FILE* outfile = NULL;
int start, end, tmNumber, count, seqLength;
char *proteinID, *line, *token;
start = end = tmNumber = count = seqLength = 0;
while( ( line  = readLine(infile) ) != NULL )
    {  /*grab the protein ID after each <PRE> tag*/
    while( (token = nextWord(&line)) != NULL )
        {
   	    if( sameString(token,"<PRE>#"))
                {  /*create a new xml file*/
                token = nextWord(&line);
       	        proteinID = (char*)malloc(24*sizeof(char));
                strcpy(proteinID, token);
                outfile = createXMLFile(proteinID, path);
	        token = lastWordInLine(line);
                seqLength = atoi(token);
                }
            if( sameString(token,"predicted") )
                {   /*grab the number of transmembrane helices*/
                token = lastWordInLine(line); 
                tmNumber = atoi(token);
                populateXMLFile(outfile, tmNumber, proteinID, path);
                }
            if( sameString(token,"</PRE>") )
                {  /*close the xml file*/
	        carefulClose(&outfile);
  	        free(proteinID);
                count = 0;
                }
            if( sameString(token,"TMhelix") )
                {
                    if( (token = nextWord(&line)) != NULL )
                         {  /*get the start*/
                         start = atoi(token);
                              if( (token = nextWord(&line)) != NULL  )
                                  end = atoi(token);
                         }
                    if( count == 0)
                        addCoordinatesToXMLFile(outfile, 1, start-1, "N-term", count);
                    count++;
                    addCoordinatesToXMLFile(outfile, start, end, "TM", count);
                    if( count == tmNumber && outfile != NULL )
                        {
                        addCoordinatesToXMLFile(outfile,end+1,seqLength,"C-term",count);
                        finishXMLFile(outfile, proteinID, seqLength);
                        }
                }
        } /*end inner while*/
    }   /*end outer while*/
}

int main(int argc, char** argv)
{
FILE *infile;
usage(argv);
infile = mustOpen(argv[1], "r");
getTransFromFile(infile, argv[2]);
carefulClose(&infile);
return 0;
}



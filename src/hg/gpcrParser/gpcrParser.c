/** proteinParser.c - Parses transmembrane cooridnates, outputs to xml file. */
#include "common.h"
#include "memalloc.h"

/** Method prototypes */
void catSeqFile(FILE* outfile, char* filename);
void getTransFromFile(FILE* infile, char *path);
FILE* createXMLFile(char* proteinID, char *path);
void populateXMLFile(FILE *outfile, int tmNumber, char *proteinID, char *path);
void addCoordinatesToXMLFile(FILE *outfile, int start, int end, char* mapTo, int count);
void finishXMLFile(FILE *outfile, char *proteinID, int seqLength);

void catSeqFile(FILE* outfile, char* filename)
/** Copies contents of one file to another file */
{
FILE* infile;
char* line = NULL;
infile = mustOpen(filename, "r");
line = readLine(infile);
     while( line  != NULL )
     {
     fprintf(outfile, "\t\t%s\n", line);
     freeMem(line);
     line = readLine(infile);
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
    if(path != NULL)
        {
        strcpy(filename,path);
        strcat(filename, "/");
        }
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
   	    if( sameString(token,"<PRE>#") )
                {  /*create a new xml file*/
                token = nextWord(&line);
       	        proteinID = cloneString(token);
                outfile = createXMLFile(proteinID, path);
	        token = lastWordInLine(line);
                seqLength = atoi(token);
                }
            if( sameString(token,"predicted") )
                {   /*grab the number of transmembrane helices*/
                tmNumber = atoi(lastWordInLine(line));
                populateXMLFile(outfile, tmNumber, proteinID, path);
                }
            if( sameString(token,"</PRE>") )
                {  /*close the xml file*/
	        carefulClose(&outfile);
  	        freeMem(proteinID);
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
char * filename = (char*)malloc(128*sizeof(char));
    if(argv[1] == NULL)
        {
        errAbort(
          "gpcrParser - Create xml files for gpcr snakeplots.\n"
          "usage:\n   gpcrParser path\n"
          "    Where path is the working directory holding the THMMH output,\n"
          "    result.htm, and the *.seq files you wish to create xml files for.\n"
          "    The xml files will also be put in the path location.  If no parameter\n"
          "    is specified the files are expected to be in the same directory as\n"
          "    the executable."
           );
         }
    if( argv[1] != NULL )
        {
        strcpy(filename, argv[1]);
        strcat(argv[1], "/result.htm");
        }
    else
       strcpy(filename, "result.htm");
infile = mustOpen(filename, "r");
getTransFromFile(infile, argv[1]);
carefulClose(&infile);
return 0;
}



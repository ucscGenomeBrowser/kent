/* display multiple alignment using jalview */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"

void displayJalView(char *inUrl, char *type, char *optional)
{
//char *host = getenv("HTTP_HOST"); 
printf("<applet code=\"jalview.bin.JalviewLite\"\n");
printf(" width=\"140\"");
printf(" height=\"35\"");
printf(" archive=\"/pseudogene/jalviewApplet.jar\">\n");
printf("<param name=\"file\" value=\"%s\">\n", inUrl);
//printf("<param name=\"file\" value=\"http://%s/%s\">\n", host, inUrl);
printf("<param name=\"defaultColour\" value=\"%% Identity\">\n");
printf("<param name=\"showAnnotation\" value=\"false\">\n");
printf("<param name=\"wrap\" value=\"true\">\n");
printf("<param name=\"sortBy\" value=\"Pairwise Identity\">\n");
printf("<param name=\"type\" value=\"URL\">\n");
printf("<param name=\"format\" value=\"%s\">\n", type);
if (optional != NULL)
    printf("%s\n",optional);
#ifdef GROUP
printf("<param name=\"groups\" value=\"1\">");
printf("<param name=\"group1\" value=\"1-99:PID:false:false:false\">");
printf("jalview");
#endif
printf("</APPLET>\n");

#ifdef samplejalview
<applet code="jalview.bin.JalviewLite" width="140" height="35" archive="jalviewApplet.jar">
<param name="file" value="uniref50.fa">
<param name="defaultColour" value="Blosum62">
<param name="RGB"  value="F2F2FF">
<param name="linkLabel_1" value="SRS">
<param name="linkUrl_1" value="http://srs.ebi.ac.uk/srs7bin/cgi-bin/wgetz?-e+[uniprot-all:$SEQUENCE_ID$]+-vn+2">
<param name="linkLabel_2" value="Uniprot">
<param name="linkUrl_2" value="http://us.expasy.org/cgi-bin/niceprot.pl?$SEQUENCE_ID$">
</applet>
#endif
#ifdef OBJECT
printf("\n<OBJECT ");
printf(" classid=\"jalview.ButtonAlignApplet.class\"\n");
printf(" codetype=\"application/java\"\n");
printf(" codebase=\"pseudogene/\"\n");
printf(" code=\"jalview.ButtonAlignApplet.class\"\n");
printf(" width=\"100\"");
printf(" height=\"35\"");
printf(">\n");
printf("<param name=\"input\" value=\"http://%s/%s\">\n", host, inUrl);
printf("<param name=\"type\" value=\"URL\">\n");
printf("<param name=\"format\" value=\"%s\">\n", type);
printf("<PARAM name=fontsize value=\"14\">\n");
if (optional != NULL)
    printf("%s\n",optional);
printf("</OBJECT>\n");
#endif
}

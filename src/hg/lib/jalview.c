/* display multiple alignment using jalview */
#include "common.h"
#include "portable.h"

void displayJalView(char *inUrl, char *type, char *optional)
{
char *host = getenv("HTTP_HOST"); 
printf("<APPLET \n");
printf(" archive=\"/pseudogene/jalview.jar\"\n");
printf(" code=\"jalview.AlignApplet.class\"\n");
printf(" width=\"1\"");
printf(" height=\"1\"");
printf(">\n");
printf("<param name=\"input\" value=\"http://%s/%s\">\n", host, inUrl);
printf("<param name=\"type\" value=\"URL\">\n");
printf("<param name=\"format\" value=\"%s\">\n", type);
printf("<PARAM name=fontsize value=\"14\">\n");
if (optional != NULL)
    printf("%s\n",optional);
#ifdef GROUP
printf("<param name=\"groups\" value=\"1\">");
printf("<param name=\"group1\" value=\"1-99:PID:false:false:false\">");
printf("jalview");
#endif
printf("</APPLET>\n");

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

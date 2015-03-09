/* testSimpleD3 - Try and get some simple D3 things running inside of Genome Browser web framework.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;

void drawPrettyPieGraph(struct slPair *data, char *id)
{
printf("<script>\nvar pie = new d3pie(\"%s\", {\n\"header\": {", id);
printf("\"title\": { \"text\": \"Testing pretty pie graph function\",");
printf("\"fontSize\": 24,");
printf("\"font\": \"open sans\",},");
printf("\"subtitle\": { \"text\": \"Things should be pretty...\",");
printf("\"color\": \"#999999\",");
printf("\"fontSize\": 12,");
printf("\"font\": \"open sans\",},\"titleSubtitlePadding\":9 },\n");
printf("\"footer\": {\"color\": \"#999999\",");
printf("\"fontSize\": 12,");
printf("\"font\": \"open sans\",");
printf("\"location\": \"bottom-left\",},\n");
printf("\"size\": { \"canvasWidth\": 590},\n");
printf("\"data\": { \"sortOrder\": \"value-desc\", \"content\": [\n");
struct slPair *start = NULL;
for (start=data; start!=NULL; start=start->next)
{
char * temp = start->val;
printf("\t{\"label\": \"%s\",\n\t\"value\": %s,\n\t\"color\": \"#2484c1\"}", start->name, temp);
if (start->next!=NULL) printf(",\n");
}
printf("]},\n\"labels\": {");
printf("\"outer\":{\"pieDistance\":32},");
printf("\"inner\":{\"hideWhenLessThanPercentage\":10},");
printf("\"mainLabel\":{\"fontSize\":11},");
printf("\"percentage\":{\"color\":\"#ffffff\", \"decimalPlaces\": 0},");
printf("\"value\":{\"color\":\"#adadad\", \"fontSize\":11},");
printf("\"lines\":{\"enabled\":true},},\n");
printf("\"effects\":{\"pullOutSegmentOnClick\":{");
printf("\"effect\": \"linear\",");
printf("\"speed\": 400,");
printf("\"size\": 8}},\n");
printf("\"misc\":{\"gradient\":{");
printf("\"enabled\": true,");
printf("\"percentage\": 100}}});");
printf("</script>\n");
}

void drawPieGraph(struct slPair *data)
{
// Get D3
// Some D3 setup, chart size and format
printf("<script>\n");
printf("var width = 320,\n\t height = 200,\n\t radius = Math.min(width, height)/2;\n\n");
printf("var color = d3.scale.ordinal()\n\t.range([\"#98abc5\", \"#8a89ac\"]);\n\n");
printf("var arc = d3.svg.arc()\n\t.outerRadius(radius - 10)\n\t.innerRadius(0);\n\n");
printf("var pie = d3.layout.pie()\n\t.sort(null)\n\t.value(function(d) { return d.size; });\n\n");
printf("var svg = d3.select(\"body\").append(\"svg\")\n");
printf("\t.attr(\"width\", width)\n");
printf("\t.attr(\"height\", height)\n");
printf("\t.append(\"g\")\n");
printf("\t.attr(\"transform\", \"translate(\" + width / 2 + \",\" + height/2 + \")\");\n\n");
// Create a data file for D3 to read. All stanza's will have a comma at the end EXCEPT for the last stanza. 
printf("var data = [\n");
struct slPair *start = NULL;
for (start=data; start!=NULL; start=start->next)
{
char * temp = start->val;
printf("\t{\"label\": \"%s\", \"size\": %s}", start->name, temp);
if (start->next!=NULL) printf(",\n");
}
printf("\n]\n\n");
// Some more D3 administrative stuffs, put colors and text in 
printf("data.forEach(function(d) {\n\t d.size = +d.size; \n\t});\n");
printf("var g = svg.selectAll(\".arc\")\n\t.data(pie(data))\n\t.enter().append(\"g\")\n\t.attr(\"class\",\"arc\");\n\n");
printf("g.append(\"path\")\n\t.attr(\"d\", arc)\n\t.style(\"fill\", function(d) { return color(d.data.size); });\n\n");
printf("g.append(\"text\")\n\t");
printf(".attr(\"transform\", function(d) { return \"translate(\" + arc.centroid(d) + \")\"; })\n\t");
printf(".attr(\"dy\", \".25em\")\n\t.style(\"text-anchor\", \"middle\")\n\t.text(function(d) { return d.data.label; });\n\n");
printf("</script>\n");
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web plabel */
{
cart = theCart;
char *db = cartUsualString(cart, "db", hDefaultDb());
cartWebStart(cart, db, "Try and get some simple D3 things running inside of Genome Browser web framework.");
printf("<script src=\"//cdnjs.cloudflare.com/ajax/libs/d3/3.4.4/d3.min.js\"></script>");
printf("<script src=\"/js/d3pie.min.js\"></script>");
struct slPair *data= slPairNew("A","100");
slPairAdd(&data, "B", "10");
slPairAdd(&data, "C", "1");
slReverse(&data);
printf("<div id=\"pieChart1\">\n");
drawPrettyPieGraph(data, "pieChart1");
printf("</div>\n");
printf("<div id=\"pieChart2\">\n");
drawPrettyPieGraph(data, "pieChart2");
printf("</div>\n");
#ifdef SOON
drawPieGraph(data);
slPairAdd(&data, "D", "10");
drawPieGraph(data);
slPairAdd(&data, "E", "15");
drawPieGraph(data);
slPairAdd(&data, "F", "25");
drawPieGraph(data);
slPairAdd(&data, "G", "50");
drawPieGraph(data);
#endif /* SOON */
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}

/* tagStormToHtml - Convert tagStorm file to a hierarchical list with controls to expand and contract.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormToHtml - Convert tagStorm file to a hierarchical list with controls to expand and contract.\n"
  "usage:\n"
  "   tagStormToHtml in.tags out.html\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static void rTsWrite(struct tagStanza *list, FILE *f, int depth, char *liLabel)
/* Recursively write out list to file in html UL format */
{
repeatCharOut(f, '\t', depth);
fprintf(f, "<UL>\n");
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
	repeatCharOut(f, '\t', depth);
	fprintf(f, "%s%s %s\n", liLabel, pair->name, (char*)(pair->val));
	liLabel = "<LI>";
	}
    repeatCharOut(f, '\t', depth);
    fprintf(f, "<LI>\n");
    if (stanza->children != NULL)
	rTsWrite(stanza->children, f, depth+1, liLabel);
    }
repeatCharOut(f, '\t', depth);
fprintf(f, "</UL>\n");
}

void writeHtml(struct tagStorm *ts, FILE *f)
/* Write out html page and javascript.   Most of this function is just
 * stringified output */
{
int i, maxDepth = tagStormMaxDepth(ts);
int stanzaCount = tagStormCountStanzas(ts);
int tagCount = tagStormCountTags(ts);
int fieldCount = tagStormCountFields(ts);

fputs(
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\">\n"
"<HTML>\n"
"<HEAD>\n"
"    <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">\n"
, f);
fprintf(f, "    <TITLE>tagStorm %s</TITLE>\n", ts->fileName);
fputs(
"    <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/themes/default/style.min.css\" />\n"
"    <script src=\"https://cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js\"></script>\n"
"    <script src=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/jstree.min.js\"></script>\n"
"</HEAD>\n"
"<BODY>\n"
, f);
fprintf(f, "<H2>tagStorm %s</H2>\n", ts->fileName);
fputs(
"<div>\n"
"<B>levels:</B> \n"
, f);
for (i=1; i<=maxDepth; ++i)
    {
    fprintf(f, "<button id=\"open_%d\">%d</button>\n", i, i);
    }
fputs(
"<button id=\"open_all\">all</button> \n"
, f);
fprintf(f, "<B>stanzas:</B> %d ", stanzaCount);
fprintf(f, "<B>tags:</B> %d ", tagCount);
fprintf(f, "<B>fields:</B> %d ", fieldCount);
fputs(
"<BR><BR>\n"
"</div>\n"
"\n"
"<div id=\"js_tag_tree\">\n"
, f);
rTsWrite(ts->forest, f, 0, "<LI id=\"ts_root_1\">");
fputs(
"</div>\n"
"\n"
"<script>\n"
"\n"
"var tag_storm_tree = (function() {\n"
"    // Effectively global vars set by init\n"
"    var tree_div;	// Points to div we live in\n"
"    var tree;		// Points to our jsTree object\n"
"    var root_1;	\n"
"\n"
"    function open_n(node, depth) {\n"
"	var count = 0;\n"
"	while (node) {\n"
"	     if (tree.is_parent(node)) {\n"
"	         if (depth <= 1) {\n"
"		      tree.close_all(node);\n"
"		 } else {\n"
"		      open_n(tree.get_next_dom(node, false), depth-1);\n"
"		 }\n"
"	     }\n"
"	     node = tree.get_next_dom(node, true);\n"
"	}\n"
"    }\n"
"\n"
"    function init() {\n"
"        tree_div = $('#js_tag_tree');\n"
"        tree_div.jstree();\n"
"        tree = tree_div.jstree(true);\n"
"        tree.open_all();\n"
"        root_1 = $('#ts_root_1');\n"
"\n"
,f );
for (i=1; i<=maxDepth; ++i)
    {
    fprintf(f, 
    "	$('#open_%d').on('click', function () {\n"
    "	    tree.open_all();\n"
    "	    tag_storm_tree.open_n(root_1, %d);\n"
    "	});\n"
    "\n"
    , i, i);
    }
fputs(
"	$('#open_all').on('click', function () {\n"
"	    tree.open_all();\n"
"	});\n"
"\n"
"   }\n"
"\n"
"   return { init: init, open_n :open_n };\n"
"\n"
"}());\n"
"\n"
"\n"
"$(function () { \n"
"   tag_storm_tree.init();\n"
"});\n"
"\n"
"</script>\n"
"</BODY>\n"
"</HTML>\n"
"\n"
, f);
}

void tagStormToHtml(char *inTags, char *outHtml)
/* tagStormToUl - Turn a tag storm into a hierarchical HTML list.. */
{
struct tagStorm *ts = tagStormFromFile(inTags);
FILE *f = mustOpen(outHtml, "w");
writeHtml(ts, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tagStormToHtml(argv[1], argv[2]);
return 0;
}

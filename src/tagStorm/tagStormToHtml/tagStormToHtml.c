/* tagStormToHtml - Convert tagStorm file to a hierarchical list with controls to expand and contract.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmshell.h"
#include "tagStorm.h"

boolean gEmbed = FALSE;
char *gNonce= NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormToHtml - Convert tagStorm file to a hierarchical list with controls to expand and contract.\n"
  "usage:\n"
  "   tagStormToHtml in.tags out.html\n"
  "options:\n"
  "   -embed - don't write beginning and end of page, just controls and tree.\n"
  "            useful for making html to be embedded in another page.\n"
  "   -nonce=nonce-string\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"embed", OPTION_BOOLEAN},
   {"nonce", OPTION_STRING},
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
    repeatCharOut(f, '\t', depth);
    fprintf(f, "<LI>\n");
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
        repeatCharOut(f, '\t', depth);
        fprintf(f, "%s%s %s<BR>\n", liLabel, pair->name, (char*)(pair->val));
        liLabel = "";
        }
    repeatCharOut(f, '\t', depth);
    fprintf(f, "<BR>\n");
    if (stanza->children != NULL)
        {
        rTsWrite(stanza->children, f, depth+1, liLabel);
        }
    }
repeatCharOut(f, '\t', depth);
fprintf(f, "</UL>\n");
}

void writeHtml(struct tagStorm *ts, FILE *f)
/* Write out html page and javascript.   Most of this function is just
 * stringified output from a html/javascript page developed previously. */
{
int i, maxDepth = tagStormMaxDepth(ts);
int stanzaCount = tagStormCountStanzas(ts);
int tagCount = tagStormCountTags(ts);
int fieldCount = tagStormCountFields(ts);

if (!gEmbed)
    {
    fprintf(f,
    "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\">\n"
    "<HTML>\n"
    "<HEAD>\n"
    "%s"
    "    <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">\n"
    , getCspMetaHeader());
    fprintf(f, "    <TITLE>tagStorm %s</TITLE>\n", ts->fileName);
    fputs(
    "</HEAD>\n"
    "<BODY>\n"
    , f);
    fprintf(f, "<H2>tagStorm %s</H2>\n", ts->fileName);
    }
fputs(
"<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/themes/default/style.min.css\" />\n"
"<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js\"></script>\n"
"<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/jstree.min.js\"></script>\n"
"<style>.jstree-default .jstree-anchor { height: initial; } </style>\n"
"<div id=\"ts_controls\">\n"
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
"<BR>\n"
"</div>\n"
"\n"
"<div id=\"ts_tree\">\n"
, f);
rTsWrite(ts->forest, f, 0, "<LI id=\"ts_root\">");
fputs(
"</div>\n"
"\n"
, f);

if (gNonce != NULL)
    fprintf(f, "<script nonce='%s'>\n", gNonce);
else
    fprintf(f, "<script>\n");
fputs(
"\n"
"var tag_storm_tree = (function() {\n"
"    // Effectively global vars set by init\n"
"    var tree_div;        // Points to div we live in\n"
"    var tree;                // Points to our jsTree object\n"
"    var root_1;        \n"
"\n"
"    function open_r(node, depth) {\n"
"        var count = 0;\n"
"        while (node) {\n"
"             if (tree.is_parent(node)) {\n"
"                 if (depth <= 1) {\n"
"                      tree.close_all(node);\n"
"                 } else {\n"
"                      open_r(tree.get_next_dom(node, false), depth-1);\n"
"                 }\n"
"             }\n"
"             node = tree.get_next_dom(node, true);\n"
"        }\n"
"    }\n"
"\n"
"    function open_n(depth) {\n"
"        tree.open_all();\n"
"        open_r(root_1, depth);\n"
"    }\n"
"\n"
"    function init() {\n"
"       if (navigator.userAgent.match(/msie|trident/i)) {\n"
"           $('#ts_controls').hide()\n"
"       } else {\n"
"            $.jstree.defaults.core.themes.icons = false;\n"
"            $.jstree.defaults.core.themes.dots = false;\n"
"            tree_div = $('#ts_tree');\n"
"            tree_div.jstree();\n"
"            tree = tree_div.jstree(true);\n"
"            root_1 = $('#ts_root');\n"
"            open_n(2);\n"
"\n"
,f );

for (i=1; i<=maxDepth; ++i)
    {
    fprintf(f, 
    "            $('#open_%d').on('click', function () {\n"
    "                open_n(%d);\n"
    "            });\n"
    "\n"
    , i, i);
    }
fputs(
"            $('#open_all').on('click', function () {\n"
"                tree.open_all();\n"
"            });\n"
"        }\n"
"   }\n"
"\n"
"   return { init: init};\n"
"\n"
"}());\n"
"\n"
"\n"
"$(function () { \n"
"   tag_storm_tree.init();\n"
"});\n"
"\n"
"</script>\n"
, f);
if (!gEmbed)
    {
    fputs(
    "</BODY>\n"
    "</HTML>\n"
    "\n"
    , f);
    }
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
gEmbed = optionExists("embed");
gNonce = optionVal("nonce", NULL);
tagStormToHtml(argv[1], argv[2]);
return 0;
}

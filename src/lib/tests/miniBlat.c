#include "common.h"
#include "htmlPage.h"
#include "qa.h"
int main(int argc, char *argv[])
{
char *url, *dnaFileName;
struct htmlPage *rootPage, *page;
struct htmlForm *mainForm;
struct qaStatus *qs;
if (argc != 3) errAbort("usage: %s url file", argv[0]);
url = argv[1]; dnaFileName = argv[2];
qs = qaPageGet(url, &rootPage);
if (qs->errMessage) errAbort("%s",qs->errMessage);
if (qs->hardError) errAbort("hard error");
if ((mainForm = htmlFormGet(rootPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form");
htmlPageSetVar(rootPage, mainForm, "seqFile", dnaFileName);
qs = qaPageFromForm(rootPage, mainForm,
                "Submit", "submit", &page);
if (qs->errMessage) errAbort("%s",qs->errMessage);
if (qs->hardError) errAbort("hard error");
fprintf(stdout,"%s ok\n",page->fullText);
htmlPageFree(&page);
htmlPageFree(&rootPage);
return 0;
}

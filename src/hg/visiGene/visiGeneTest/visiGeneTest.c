/* visiGeneTest - Test out visiGene database interface routines. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "visiGene.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "visiGeneTest - Test out visiGene database interface routines\n"
  "usage:\n"
  "   visiGeneTest database\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void printPaths(struct sqlConnection *conn, int image)
/* Print out paths associated with image */
{
int imageWidth = 0, imageHeight=0;
printf("full: %s\n", visiGeneFullSizePath(conn, image));
visiGeneImageSize(conn, image, &imageWidth, &imageHeight);
printf("width: %d height: %d\n", imageWidth, imageHeight);
printf("thumb: %s\n", visiGeneThumbSizePath(conn, image));
}

void printAndFreeList(struct slName *list, char *label)
/* Print and free list. */
{
struct slName *el;
printf("%s:", label);
for (el = list; el != NULL; el = el->next)
    printf(" %s", el->name);
printf("\n");
slFreeList(&list);
}

void printAndFreeIntList(struct slInt *list, char *label)
/* Print and free list. */
{
struct slInt *el;
printf("%s (%d):", label, slCount(list));
for (el = list; el != NULL; el = el->next)
    printf(" %d", el->val);
printf("\n");
slFreeList(&list);
}

void visiGeneTest(char *database)
/* visiGeneTest - Test out visiGene database interface routines. */
{
struct sqlConnection *conn = sqlConnect(database);
int image = 17;

//for (image=1; image < 100000; image *= 10)
    {
    printf("image #%d\n", image);
    printPaths(conn, image);
    printf("organism: %s\n", visiGeneOrganism(conn, image));
    printf("age: %s\n", visiGeneStage(conn, image, FALSE));
    printf("stage: %s\n", visiGeneStage(conn, image, TRUE));
    printAndFreeList(visiGeneGeneName(conn, image), "gene");
    printAndFreeList(visiGeneRefSeq(conn, image), "RefSeq");
    printAndFreeList(visiGeneGenbank(conn, image), "Genbank");
    printAndFreeList(visiGeneUniProt(conn, image), "UniProt");
    printf("submitId: %s\n", visiGeneSubmitId(conn, image));
    printf("fixation: %s\n", visiGeneFixation(conn, image));
    printf("embedding: %s\n", visiGeneEmbedding(conn, image));
    printf("permeablization: %s\n", visiGenePermeablization(conn, image));
    printf("contributors: %s\n", visiGeneContributors(conn, image));
    printf("publication: %s\n", visiGenePublication(conn, image));
    printf("pubUrl: %s\n", visiGenePubUrl(conn, image));
    printf("setUrl: %s\n", visiGeneSetUrl(conn, image));
    printf("itemUrl: %s\n", visiGeneItemUrl(conn, image));
    printAndFreeIntList(visiGeneSelectRefSeq(conn, "NM_010469"), "NM_010469");
    printAndFreeIntList(visiGeneSelectLocusLink(conn, "15436"), "LL 15436");
    printAndFreeIntList(visiGeneSelectGenbank(conn, "J03770"), "J03770");
    printAndFreeIntList(visiGeneSelectUniProt(conn, "P12345"), "UP P12345");
    printf("copyright: %s\n", visiGeneCopyright(conn, image));

    printf("\n");
    }

sqlDisconnect(&conn);

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
visiGeneTest(argv[1]);
return 0;
}

/* gfWebLib - stuff shared by gfWebBlat and gfWebPcr - little web programs
 * that query gfServer in different ways. */

struct gfServerAt
/* A gfServer with an in-memory index. */
    {
    struct gfServerAt *next;
    char *host;		/* IP Address of machine running server. */
    char *port;		/* IP Port on host. */
    char *seqDir;	/* Where associated sequence lives. */
    char *name;		/* Name user sees. */
    };

struct gfServerAt *gfWebFindServer(struct gfServerAt *serverList, char *varName);
/* Find active server (that matches contents of CGI variable varName). */

struct gfWebConfig
/* A parsed out configuration file. */
    {
    char *company;	/* Company name if any. */
    char *background;	/* Web page background if any. */
    struct gfServerAt *serverList;  /* List of servers. */
    struct gfServerAt *transServerList;  /* List of translated servers. */
    char *tempDir;	/* Where to put temporary files. */
    };

struct gfWebConfig *gfWebConfigRead(char *fileName);
/* Read configuration file into globals. */

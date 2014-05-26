/* ggTypes - Type enumerations used by geneGraph and other modules. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef GGTYPES_H
#define GGTYPES_H

enum ggVertexType
 /* Classifies a vertex.  */
    {
    ggSoftStart,  /* 0 First vertex - exact position unknown. */
    ggHardStart,  /* 1 Start of a middle exon, position known. */      
    ggSoftEnd,    /* 2 Last vertex - exact position unknown. */
    ggHardEnd,    /* 3 End of a middle exon, position known. */
    ggUnused,     /* 4 Vertex no longer part of graph. */
	/* These next two are only used by spliceWalk, which is no
	 * longer maintained. */
    ggRangeEnd,   /* 5 An end that could happen anywhere in a range. */ 
    ggRangeStart  /* 6 An start that could happen anywhere in a range. */
    };

char *ggVertexTypeAsString(enum ggVertexType type);
/* Return string corresponding to vertex type. */

enum ggVertexType ggVertexTypeFromString(char *s);
/* Return string corresponding to vertex type. */

enum ggEdgeType
/* classifies an edge */
{
    ggExon,      /* Exon, not necessarily coding */
    ggIntron,    /* Intron, spliced out */
        /* This last one is not used lately. */
    ggSJ,        /* Splice Junction */
};

char *ggEdgeTypeAsString(enum ggVertexType type);
/* Return string corresponding to edge type. */

enum ggVertexType ggEdgeTypeFromString(char *s);
/* Return string corresponding to edge type. */

#endif /* GGTYPES_H */


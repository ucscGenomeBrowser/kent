/* encode2Meta - Create meta files.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encode/encodeExp.h"
#include "encode3/encode2Manifest.h"
#include "mdb.h"

char *metaDbs[] = {"hg19", "mm9"};
char *organisms[] = {"human", "mouse"};
char *metaTable = "metaDb";
char *expDb = "hgFixed";
char *expTable = "encodeExp";

/* Command line variables */
boolean withParent = FALSE;
boolean maniFields = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2Meta - Create meta.txt file. This is a hierarchical .ra file with heirarchy defined\n"
  "by indentation.  You might think of it as a meta tag tree.  It contains the contents of\n"
  "the hg19 and mm9 metaDb tables and the hgFixed.encodeExp table.\n"
  "usage:\n"
  "   encode2Meta database manifest.tab meta.txt\n"
  "options:\n"
  "   -withParent - if set put a parent tag in each stanza in addition to indentation\n"
  "   -maniFields - includes some fileds normally suppressed because they are also in manifest\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"withParent", OPTION_BOOLEAN},
   {"maniFields", OPTION_BOOLEAN},
   {NULL, 0},
};

struct metaNode
/* A node in a metadata tree */
    {
    struct metaNode *next;	/* Next sibling. */
    struct metaNode *children;  /* Children if any */
    struct metaNode *parent;	/* Parent if any */
    char *name;	/* Node's unique symbolic name. */
    struct mdbVar *vars;    /* Variables used if any */
    };

char *mdbVarLookup(struct mdbVar *list, char *var)
/* Return value associated with given var if var is on list, else NULL */
{
struct mdbVar *v;
for (v = list; v != NULL; v = v->next)
    {
    if (sameString(v->var, var))
        return v->val;
    }
return NULL;
}

char *metaLocalVal(struct metaNode *node, char *var)
/* Look up value, not going up to parents. */
{
return mdbVarLookup(node->vars, var);
}

char *metaVal(struct metaNode *node, char *var)
/* Return value of given var, or none if variable isn't defined.
 * Looks first in self, and then in parents. */
{
char *val;
while (node != NULL)
    {
    if ((val = metaLocalVal(node, var)) != NULL)
        return val;
    node = node->parent;
    }
return NULL;
}

struct mdbVar *mdbVarNew(char *var, char *val)
/* Return a new mdbVar. */
{
struct mdbVar *v;
AllocVar(v);
v->var = cloneString(var);
v->val = cloneString(val);
return v;
}

void metaNodeAddVar(struct metaNode *node, char *var, char *val)
/* Add var to node - but only if it is not already present at same value at a higher level */
{
if (val == NULL)
    return;
if (node->parent != NULL && sameOk(metaVal(node->parent, var), val))
    return;   /* Already in parent, we are fine. */
if (metaLocalVal(node, var))
    errAbort("Redefining %s.%s\n", node->name, var);
struct mdbVar *v = mdbVarNew(var, val);
slAddHead(&node->vars, v);
}

void metaNodeAddVarVals(struct metaNode *node, char *varVals)
/* Add string of var=vals to node */
{
if (varVals == NULL)
    return;
struct slPair *pair, *pairList = slPairListFromString(varVals, FALSE);
for (pair = pairList; pair != NULL; pair = pair->next)
    metaNodeAddVar(node, pair->name, pair->val);
}

struct metaNode *metaNodeNew(char *name)
/* Make new but empty and unconnected node. */
{
struct metaNode *meta;
AllocVar(meta);
meta->name = cloneString(name);
return meta;
}

struct metaNode *metaTreeNew(char *name)
/* Make largely empty root node. */
{
return metaNodeNew(name);
}

struct mdbObj *getMdbList(char *database)
/* Get list of metaDb objects for a database. */
{
struct sqlConnection *conn = sqlConnect(database);
struct mdbObj *list = mdbObjsQueryAll(conn, metaTable);
sqlDisconnect(&conn);
return list;
}

struct metaNode *wrapNodeAroundExp(struct encodeExp *exp)
/* Wrap a metaNode around exp,  and return it. */
{
struct metaNode *node = metaNodeNew(exp->accession);
metaNodeAddVar(node, "organism", exp->organism);
metaNodeAddVar(node, "lab", exp->lab);
metaNodeAddVar(node, "dataType", exp->dataType);
metaNodeAddVar(node, "cellType", exp->cellType);
metaNodeAddVar(node, "updateTime", exp->updateTime);
metaNodeAddVarVals(node, exp->expVars);
return node;
}

void metaTreeWrite(int level, int minLevel, int maxLevel, boolean isFile,
    char *parent, struct metaNode *node, struct hash *suppress, FILE *f)
/* Write out self and children to file recursively. */
{
if (level >= minLevel && level < maxLevel)
    {
    int indent = (level-minLevel)*3;
    spaceOut(f, indent);
    fprintf(f, "meta %s\n", node->name);
    if (withParent && parent != NULL)
	{
	spaceOut(f, indent);
	fprintf(f, "parent %s\n", parent);
	}
    struct mdbVar *v;
    for (v = node->vars; v != NULL; v = v->next)
	{
	if (!hashLookup(suppress, v->var))
	    {
	    spaceOut(f, indent);
	    fprintf(f, "%s %s\n", v->var, v->val);
	    }
	}
    fprintf(f, "\n");
    }
struct metaNode *child;
for (child = node->children; child != NULL; child = child->next)
    metaTreeWrite(level+1, minLevel, maxLevel, isFile, node->name, child, suppress, f);
}

boolean mdbVarRemove(struct mdbVar **pList, char *var)
/* Find given variable in list and remove it. Returns TRUE if it
 * actually removed it,  FALSE if it never found it. */
{
struct mdbVar **ln = pList;
struct mdbVar *v;
for (v = *pList; v != NULL; v = v->next)
    {
    if (sameString(v->var, var))
        {
	*ln = v->next;
	return TRUE;
	}
    ln = &v->next;
    }
return FALSE;
}


void hoistOne(struct metaNode *node, char *var, char *val)
/* We've already determined that var exists and has same value in all children.
 * What we do here is add it to ourselves and remove it from children. */
{
if (mdbVarLookup(node->vars, var))
    mdbVarRemove(&node->vars, var);
metaNodeAddVar(node, var, val);
struct metaNode *child;
for (child = node->children; child != NULL; child = child->next)
    {
    mdbVarRemove(&child->vars, var);
    }
}

struct slName *varsInAnyNode(struct metaNode *nodeList)
/* Return list of variables that are used in any node in list. */
{
struct hash *varHash = hashNew(6);
struct slName *var, *varList = NULL;
struct metaNode *node;
for (node = nodeList; node != NULL; node = node->next)
    {
    struct mdbVar *v;
    for (v = node->vars; v != NULL; v = v->next)
        {
	if (!hashLookup(varHash, v->var))
	    {
	    var = slNameAddHead(&varList, v->var);
	    hashAdd(varHash, var->name, var);
	    }
	}
    }
hashFree(&varHash);
return varList;
}

char *allSameVal(char *var, struct metaNode *nodeList)
/* Return value of variable if it exists and is the same in each node on list */
{
char *val = NULL;
struct metaNode *node;
for (node = nodeList; node != NULL; node = node->next)
    {
    char *oneVal = mdbVarLookup(node->vars, var);
    if (oneVal == NULL)
        return NULL;
    if (val == NULL)
        val = oneVal;
    else
        {
	if (!sameString(oneVal, val))
	    return NULL;
	}
    }
return val;
}

char *allSameValWithDataMostWithData(char *var, struct metaNode *nodeList, double minProportion)
/* Return variable if all nodes that have it have it set to same value, and
 * most (at least minProportion) have it. */
{
char *val = NULL;
struct metaNode *node;
int nodeCount = 0, dataCount = 0;
for (node = nodeList; node != NULL; node = node->next)
    {
    ++nodeCount;
    char *oneVal = mdbVarLookup(node->vars, var);
    if (oneVal != NULL)
	{
	++dataCount;
	if (val == NULL)
	    val = oneVal;
	else
	    {
	    if (!sameString(oneVal, val))
		return NULL;
	    }
	}
    }
int minDataNeeded = round(nodeCount * minProportion);
if (dataCount < minDataNeeded)
    return NULL;
return val;
}


void metaTreeHoist(struct metaNode *node, struct hash *closeEnoughTags)
/* Move variables that are the same in all children up to parent. */
{
/* Do depth first recursion, but get early return if we're a leaf. */
struct metaNode *child;
if (node->children == NULL)
    return;
for (child = node->children; child != NULL; child = child->next)
    metaTreeHoist(child, closeEnoughTags);

/* Build up list of variables used in any child. */
struct slName *var, *varList = varsInAnyNode(node->children);

/* Go through list and figure out ones that are same in all children. */
for (var = varList; var != NULL; var = var->next)
    {
    char *val;
    double *closeEnough = hashFindVal(closeEnoughTags, var->name);
    if (closeEnough)
        val = allSameValWithDataMostWithData(var->name, node->children, *closeEnough);
    else
	val = allSameVal(var->name, node->children);
    if (val != NULL)
        {
	if (!sameString(var->name, "fileName"))
	    hoistOne(node, var->name, val);
	}
    }
}

double *cloneDouble(double x)
/* Return clone of double in dynamic memory */
{
return CloneVar(&x);
}

struct hash *makeCloseEnoughTags()
/* Make double pointer valued hash keyed by tags that only need to be
 * present in most children to be hoisted. */
{
struct hash *closeEnoughTags = hashNew(5);
hashAdd(closeEnoughTags, "organism", cloneDouble(0.8));
hashAdd(closeEnoughTags, "lab", cloneDouble(0.8));
hashAdd(closeEnoughTags, "age", cloneDouble(0.8));
hashAdd(closeEnoughTags, "grant", cloneDouble(0.8));
hashAdd(closeEnoughTags, "dateSubmitted", cloneDouble(0.8));
hashAdd(closeEnoughTags, "dateUnrestricted", cloneDouble(0.8));
hashAdd(closeEnoughTags, "softwareVersion", cloneDouble(0.8));
hashAdd(closeEnoughTags, "control", cloneDouble(0.9));
hashAdd(closeEnoughTags, "geoSampleAccession", cloneDouble(0.7));
return closeEnoughTags;
}

struct hash *makeSuppress()
/* Make a hash full of fields to suppress. */
{
struct hash *suppress = hashNew(4);
hashAdd(suppress, "objType", NULL);   // Inherent in hierarchy or ignored
hashAdd(suppress, "subId", NULL);     // Submission ID not worth carrying forward
hashAdd(suppress, "tableName", NULL);	// We aren't interested in tables, just files
hashAdd(suppress, "project", NULL);   // Always wgEncode
hashAdd(suppress, "expId", NULL);     // Redundant with dccAccession
hashAdd(suppress, "cell", NULL);      // Completely redundant with cellType - I checked
hashAdd(suppress, "sex", NULL);       // This should be implied in cellType
if (!maniFields)
    {
    hashAdd(suppress, "dccAccession", NULL);  // Redundant with meta object name
    hashAdd(suppress, "composite", NULL); // Inherent in hierarchy now
    hashAdd(suppress, "view", NULL);      // This is in maniest
    hashAdd(suppress, "replicate", NULL); // This is in manifest
    hashAdd(suppress, "md5sum", NULL);    // Also in manifest
    }
return suppress;
}

boolean originalData(char *symbol)
/* Return TRUE if it's not just a repackaging. */
{
return (symbol != NULL && !startsWith("wgEncodeAwg", symbol) && !startsWith("wgEncodeReg", symbol));
}

int metaNodeCmp(const void *va, const void *vb)
// Compare metaNode to sort on var name, case-insensitive.
{
const struct metaNode *a = *((struct metaNode **)va);
const struct metaNode *b = *((struct metaNode **)vb);
return strcasecmp(a->name, b->name);
}

void metaTreeSortChildrenSortTags(struct metaNode *node)
/* Reverse child list recursively and sort tags list. */
{
slSort(&node->vars, mdbVarCmp);
slSort(&node->children,  metaNodeCmp);
struct metaNode *child;
for (child = node->children; child !=NULL; child = child->next)
    metaTreeSortChildrenSortTags(child);
}

void encode2Meta(char *database, char *manifestIn, char *outMetaRa)
/* encode2Meta - Create meta files.. */
{
int dbIx = stringArrayIx(database, metaDbs, ArraySize(metaDbs));
if (dbIx < 0)
    errAbort("Unrecognized database %s", database);

/* Create a three level meta.ra format file based on hgFixed.encodeExp
 * and database.metaDb tables. The levels are composite, experiment, file */
struct metaNode *metaTree = metaTreeNew("encode2");

/* Load up the manifest. */
struct encode2Manifest *mi, *miList = encode2ManifestShortLoadAll(manifestIn);
struct hash *miHash = hashNew(18);
for (mi = miList; mi != NULL; mi = mi->next)
    hashAdd(miHash, mi->fileName, mi);
verbose(1, "%d files in %s\n", miHash->elCount, manifestIn);

/* Load up encodeExp info. */
struct sqlConnection *expConn = sqlConnect(expDb);
struct encodeExp *expList = encodeExpLoadByQuery(expConn, NOSQLINJ "select * from encodeExp");
sqlDisconnect(&expConn);
verbose(1, "%d experiments in encodeExp\n", slCount(expList));

struct hash *compositeHash = hashNew(0);

/* Go through each  organism database in turn. */
int i;
for (i=0; i<ArraySize(metaDbs); ++i)
    {
    char *db = metaDbs[i];
    if (!sameString(database, db))
        continue;

    verbose(1, "exploring %s\n", db);
    struct mdbObj *mdb, *mdbList = getMdbList(db);
    verbose(1, "%d meta objects in %s\n", slCount(mdbList), db);

    /* Get info on all composites. */
    for (mdb = mdbList; mdb != NULL; mdb = mdb->next)
        {
	char *objType = mdbVarLookup(mdb->vars, "objType");
	if (objType != NULL && sameString(objType, "composite"))
	    {
	    char compositeName[256];
	    safef(compositeName, sizeof(compositeName), "%s", mdb->obj);
	    struct metaNode *compositeNode = metaNodeNew(compositeName);
	    slAddHead(&metaTree->children, compositeNode);
	    compositeNode->parent = metaTree;
	    struct mdbVar *v;
	    for (v=mdb->vars; v != NULL; v = v->next)
	        {
		metaNodeAddVar(compositeNode, v->var, v->val);
		}
	    metaNodeAddVar(compositeNode, "assembly", db);
	    hashAdd(compositeHash, mdb->obj, compositeNode);
	    }
	}

    /* Make up one more for experiments with no composite. */
    char *noCompositeName = "wgEncodeZz";
    struct metaNode *noCompositeNode = metaNodeNew(noCompositeName);
    slAddHead(&metaTree->children, noCompositeNode);
    noCompositeNode->parent = metaTree;
    hashAdd(compositeHash, noCompositeName, noCompositeNode);


    /* Now go through objects trying to tie experiments to composites. */ 
    struct hash *expToComposite = hashNew(16);
    for (mdb = mdbList; mdb != NULL; mdb = mdb->next)
        {
	char *composite = mdbVarLookup(mdb->vars, "composite");
	if (originalData(composite))
	    {
	    char *dccAccession = mdbVarLookup(mdb->vars, "dccAccession");
	    if (dccAccession != NULL)
	        {
		char *oldComposite = hashFindVal(expToComposite, dccAccession);
		if (oldComposite != NULL)
		    {
		    if (!sameString(oldComposite, composite))
		        verbose(2, "%s maps to %s ignoring mapping to %s", dccAccession, oldComposite, composite);
		    }
		else
		    {
		    hashAdd(expToComposite, dccAccession, composite);
		    }
		}
	    }
	}
    /* Now get info on all experiments in this organism. */
    struct hash *expHash = hashNew(0);
    struct encodeExp *exp;
    for (exp = expList; exp != NULL; exp = exp->next)
        {
	if (sameString(exp->organism, organisms[i]))
	    {
	    if (exp->accession != NULL)
		{
		char *composite = hashFindVal(expToComposite,  exp->accession);
		struct metaNode *compositeNode;
		if (composite != NULL)
		    {
		    compositeNode = hashMustFindVal(compositeHash, composite);
		    }
		else
		    {
		    compositeNode = noCompositeNode;
		    }
		struct metaNode *expNode = wrapNodeAroundExp(exp);
		hashAdd(expHash, expNode->name, expNode);
		slAddHead(&compositeNode->children, expNode);
		expNode->parent = compositeNode;
		}
	    }
	}

    for (mdb = mdbList; mdb != NULL; mdb = mdb->next)
	{
	char *fileName = NULL, *dccAccession = NULL;
	char *objType = mdbVarLookup(mdb->vars, "objType");
	if (objType != NULL && sameString(objType, "composite"))
	    continue;
	dccAccession = mdbVarLookup(mdb->vars, "dccAccession");
	if (dccAccession == NULL)
	    continue;
	char *composite = hashFindVal(expToComposite,  dccAccession);
	if (composite == NULL)
	    errAbort("Can't find composite for %s", mdb->obj);
	struct mdbVar *v;
	for (v = mdb->vars; v != NULL; v = v->next)
	    {
	    char *var = v->var, *val = v->val;
	    if (sameString("fileName", var))
		{
		fileName = val;
		char path[PATH_LEN];
		char *comma = strchr(fileName, ',');
		if (comma != NULL)
		     *comma = 0;	/* Cut off comma separated list. */
		safef(path, sizeof(path), "%s/%s/%s", db, 
		    composite, fileName);  /* Add database path */
		fileName = val = v->val = cloneString(path);
		}
	    }
	if (fileName != NULL)
	    {
	    if (hashLookup(miHash, fileName))
		{
		struct metaNode *expNode = hashFindVal(expHash, dccAccession);
		if (expNode != NULL)
		    {
		    struct metaNode *fileNode = metaNodeNew(mdb->obj);
		    slAddHead(&expNode->children, fileNode);
		    fileNode->parent = expNode;
		    struct mdbVar *v;
		    for (v=mdb->vars; v != NULL; v = v->next)
			{
			metaNodeAddVar(fileNode, v->var, v->val);
			}
		    }
		}
	    }
	}
#ifdef SOON
#endif /* SOON */
    }

struct hash *suppress = makeSuppress();
struct hash *closeEnoughTags = makeCloseEnoughTags();

metaTreeHoist(metaTree, closeEnoughTags);
metaTreeSortChildrenSortTags(metaTree);
FILE *f = mustOpen(outMetaRa, "w");
struct metaNode *node;
for (node = metaTree->children; node != NULL; node = node->next)
    metaTreeWrite(0, 0, BIGNUM, FALSE, NULL, node, suppress, f);
carefulClose(&f);

/* Write warning about tags in highest parent. */
struct mdbVar *v;
for (v = metaTree->vars; v != NULL; v = v->next)
    verbose(1, "Omitting universal %s %s\n", v->var, v->val);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
withParent = optionExists("withParent");
maniFields = optionExists("maniFields");
encode2Meta(argv[1], argv[2], argv[3]);
return 0;
}

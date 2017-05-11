table ggLink
" hgcGeneGraph links between genes and enough info to draw a graph"
    (
    string gene1 index;       "Gene pair, symbol1 (first in alphabet)"
    string gene2 index;       "Gene pair, symbol2 (second in alphabet)"
    string linkTypes;   "comma-sep list of pwy (=pathway),ppi (=ppi-db), text. If ppi or pwy: low if low-throughput assay"
    int pairCount;    "text mining document count in gene1->gene2 direction "
    int oppCount;    "text mining document count in gene2->gene1 direction "
    int docCount;     " text mining document count for both directions "
    string dbList;     "|-separated list of databases that have curated this gene pair"
    int minResCount;     "minimum number of interactions assigned to any paper supporting this interaction. If this value is lower than X, the interaction is considered low-throughput-supported."
    lstring snippet;     "if linkType contains 'text': selected sentence from text-mined results"
    lstring context;     "select document contexts (e.g. diseases or pathways)"
    )

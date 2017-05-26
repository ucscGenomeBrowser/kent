table ggEvent
" hgcGeneGraph links between genes and enough info to draw a graph"
    (
    string gene1 index;       "Gene pair, symbol1 (first in alphabet)"
    string gene2;       "Gene pair, symbol2 (second in alphabet)"
    string linkTypes;   "comma-sep list of pwy (=pathway),ppi (=ppi-db), text. If ppi or pwy: low if low-throughput assay"
    string pairCount;    "strengh in gene1->gene2 direction, usually number of documents"
    string oppCount;    "strengh in gene2->gene1 direction, usually number of documents"
    string snippet;     "if linkType 'text': selected sentence from text-mined results"
    )

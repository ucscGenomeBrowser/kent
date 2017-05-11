table ggDoc
" hgcGeneGraph: meta information of a document, author, year, title, etc"
    (
    string docId index;       "identifier of document, if integer assumed to be a PMID"
    lstring authors;   "semicolon-separated list of authors"
    string year;   "year of publication"
    string journal;   "name of journal"
    string printIssn;   "ISSN of journal"
    lstring title;   "title of article"
    lstring abstract;   "abstract of article"
    lstring context;   "MESH keywords of article, |-separated"
    int resCount;   "number of pairs associated to this article in curated databases"
    )

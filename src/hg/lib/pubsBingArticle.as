table pubsBingArticle
" information about a website, some fields are always empty (like pmid) to stay compatible with pubsArticle, fields like authors and title are extracted from meta tags of the html (Dublin core, etc)"
    (
    bigint articleId;     "internal article ID, created during download"
    string extId;         "publisher ID e.g. PMCxxxx or doi or sciencedirect ID"
    bigint pmid;          "PubmedID if available"
    string doi;           "DOI if available"
    string source;        "data source, e.g. elsevier or pmcftp"
    string citation;      "source journal citation"
    int year;             "year of publication or 0 if not defined"
    string title;         "article title"
    string authors;       "author list for this article"
    string firstAuthor;   "first author family name"
    string abstract;      "article abstract"
    string url;           "url to fulltext of article"
    string dbs;           "list of DBs with matches to this article"
    )

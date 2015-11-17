table cdwDataset
"A dataset is a collection of files, usually associated to a paper"
    (
    string name unique;  "Short name of this dataset, one word, no spaces"
    string label;  "short title of the dataset, a few words"
    lstring description;  "Description of dataset, can be a complete html paragraph."
    string pmid;  "Pubmed ID of abstract"
    string pmcid;  "PubmedCentral ID of paper full text"
    )

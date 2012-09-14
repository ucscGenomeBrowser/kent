table cvDb_antibody
"Information on an antibody including where to get it and what it targets"
    (
    uint id; "Unique unsigned integer identifier for this item"
    string term; "A relatively short label, no more than a few words"
    string tag; "A short human and machine readable symbol with just alphanumeric characters."
    string deprecated; "If non-empty, the reason why this entry is obsolete."
    lstring description; "Short description of antibody itself."
    string target; "Molecular target of antibody."
    string vendorName; "Name of vendor selling reagent."
    string vendorId; "Catalog number of other way of identifying reagent."
    string orderUrl; "Web page to order regent."
    string lab; "Scientific lab producing data."
    string validation; "How antibody was validated to be specific for target."
    string label; "A relatively short label, no more than a few words"
    string lots; "The specific lots of reagent used."
    )

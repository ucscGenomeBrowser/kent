# if you are seeing this file as text in a web browser, you need to configure
# chimerax as a helper application.

# Python code for chimerax files to display proteins with SNPs annotated.
# Due scoping bug in exec coding from chimerax files (1.3 alpha release and
# earlier), can only have one function and must import into function's local
# namespace.

def displayPdb(pdbSpec, snps):
    """pdbSpec can be a pdbId or a URL of a PDB format file.
    snps is list of (snpId, chain, snpPos, [isPrimary])"""
    from chimera import runCommand
    from chimera.selection import OSLSelection
    runCommand("close all")
    if pdbSpec.find("://") >= 0:
        runCommand("open %s" % pdbSpec)
    else:
        runCommand("open pdb:%s" % pdbSpec)

    # hide all models/sub-models, NMR will only show first model
    runCommand("select #*:*.*")
    runCommand("~display sel")
    runCommand("~select")

    # get model spec to use, testing for sub-models, as is the case with NMR
    sel = OSLSelection("#0.1")
    if len(sel.atoms()) > 0:
        modelSpec = "#0.1"
    else:
        modelSpec = "#0"
    caSpec = "%s:.*@CA" % modelSpec

    # display ribbons
    runCommand("ribbon %s" % caSpec)
    runCommand("ribbackbone %s" % caSpec)
    runCommand("focus %s" % caSpec)

    for snp in snps:
        (snpId, chain, snpPos) = snp[0:3]
        if ((len(snp) > 3) and snp[3]):
            color = "red" 
        else:
            color = "gold"
        spec = "%s:%s.%s@CA" % (modelSpec, snpPos, chain)
        runCommand("color %s %s" % (color, spec))
        runCommand("setattr r label \"%s\" %s" % (snpId, spec))

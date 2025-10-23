#!/cluster/software/bin/python3.6

import xml.etree.ElementTree as ET
import argparse
import re
import time
import sys


class Gene():
    """
    The class Gene() is used for recording the Ensembl gene id, chromosome, start and end chromosomal positions.
    """
    def __init__(self, geneInfo):
        """
        Gene constructor.

        :param geneInfo: (list) a reformatted line taken directly from hg19.ensGene.gtf (refer to the method getGenesInfo()
            for more information).

        Attributes:
            id: (str) Ensembl geneID, found in position 9 of geneInfo list.
            chr: (str) Chromosome, found in position 0.
            start: (str) Chromosomal start position, found in position 3.
            end: (str) Chromosomal end position, found in position 4.
            strand: (str) Strand (either + or -) found in position 6.

        """
        self.id = geneInfo[9]
        self.chr = geneInfo[0]
        self.start = geneInfo[3]
        self.end = geneInfo[4]
        self.strand = geneInfo[6]

    def printGene(self):
        """
        Prints the attributes of a Gene object.
        """
        print([self.id, self.chr, self.start, self.end, self.strand])


def getEnsGeneInfo():
    """
    Retrieves all transcript information according to
    Ensembl gene_id (hg19 and hg38) to be stored in a dictionary, ensemblInfo.
    Iterates through each transcript line for each gene and creates a Gene object
    using the transcript with the greatest chromosomal length. The created Gene
    object stores the gene ID, chromosome, chromosomal positions, and strand.  This
    object is then stored as a value in the ensemblInfo dictionary, with the gene
    ID as the dictionary key.

    NOTE: the gene_id ('ENSG...') is used to identify genes because these are the IDs included in the Orphadata files.

    EnsemblInfo format:
        { geneID: <Gene object containing gene information> }

    :return: EnsemblInfo (dict)
    """

    # Downloaded from here: http://hgdownload.soe.ucsc.edu/goldenPath/hg19/bigZips/genes/
    # NOTE: we need to download ensembl info
    hg19ensFile = open("../hg19.ensGene.gtf", "r", encoding='utf-8')
    hg38ensFile = open("../hg38.ensGene.gtf", "r", encoding='utf-8')

    # Capture all ensembl transcript information into a dictionary ensemblInfo
    hg19ensemblInfo = dict()
    hg38ensemblInfo = dict()

    # hg19 chromosomal coordinates
    for line in hg19ensFile:
        geneInfo = [x.replace('"', "").replace(';', "") for x in line.strip().split()]  # reformat, convert to list
        geneID = geneInfo[9]  # grab geneID

        if geneInfo[2] == 'transcript':
            if geneID not in hg19ensemblInfo.keys():  # check new gene
                gene = Gene(geneInfo)  
                hg19ensemblInfo[geneID] = gene  # store the Gene object
            else:  
                # Grab the stored coordinates
                oldStart = int(hg19ensemblInfo[geneID].start)
                oldEnd = int(hg19ensemblInfo[geneID].end) 

                # compare the coordinates
                if abs(int(geneInfo[4]) - int(geneInfo[3])) > abs(oldEnd - oldStart):
                    hg19ensemblInfo[geneID].start = geneInfo[3]
                    hg19ensemblInfo[geneID].end = geneInfo[4]
    
    # hg38 chromosomal coorddinates
    for line in hg38ensFile:
        geneInfo = [x.replace('"', "").replace(';', "") for x in line.strip().split()]  # reformat, convert to list
        geneID = geneInfo[9]  # grab geneID

        if geneInfo[2] == 'transcript':
            if geneID not in hg38ensemblInfo.keys():  # check new gene
                gene = Gene(geneInfo)  
                hg38ensemblInfo[geneID] = gene  # store the Gene object
            else:  
                # Grab the stored coordinates
                oldStart = int(hg38ensemblInfo[geneID].start)
                oldEnd = int(hg38ensemblInfo[geneID].end) 

                # compare the coordinates
                if abs(int(geneInfo[4]) - int(geneInfo[3])) > abs(oldEnd - oldStart):
                    hg38ensemblInfo[geneID].start = geneInfo[3]
                    hg38ensemblInfo[geneID].end = geneInfo[4]
    
    # Close the files
    hg19ensFile.close()
    hg38ensFile.close()

    
    return hg19ensemblInfo, hg38ensemblInfo


def parseRareDiseases(disorderDict, ensemblDict):
    """
    Retrieves all disorders and their associated gene information from the Orphadata GENES ASSOCIATED WITH RARE DISEASES
    file (en_product6.xml). Begins by iterating through the .xml files to find all disorder names, then adds all
    disorder information to the gene values in ensemblInfo. The data formatted in .xml file is as follows:

    All disorders are contained within the <DisorderList> tag. Each disorder is separated by the <Disorder id="..."> tag
    which is not relevant for this script. Instead, the <Name> tag is used to store the disorder information.

    Within each disorder, there is the <DisorderGeneAssociationList count="X"> tag that includes all genes and gene
    information associated with the disorder, where X is the total number of associated genes. Each gene is separated by
    the <DisorderGeneAssociation> tag, which stores all of a specific gene's information within various tags. Of these
    tags, this script captures the following tag information and stores it in the ensemblInfo dictionary:

        - <Name> : full name of the gene
        - <Symbol> : abbreviated symbol of the gene
        - <Synonym> : abbreviated symbol synonyms
        - <GeneType> : the type of gene (either gene with protein product, locus, or non-coding RNA)
        - all <Source> and <Reference> tags that contain 'Ensembl', 'HGNC', 'OMIM' (found within the tag
          <ExternalReferenceList>)
            -- NOTE: the Ensembl gene ID contained here is used to grab chromosomal information from the hg19 Ensembl
            transcript information. If there is no Ensembl gene ID available, then it will not be included in the BED
            file downstream.
        - <SourceOfValidation> that contain 'PMID' : includes PMID codes
        - <Source> : references includng Ensembl, HGNC, OMIM (found in the <ExternalReferenceList> tag)
        - <DisorderGeneAssociationType> : gene-disease relationship
        - <DisorderGeneAssociationStatus> : validation status (can be either validated or not validated)
        - <GeneLocus> : chromosomal location of gene

    More information regarding the tags can be found in Free access products description file for the GENES ASSOCIATED
    WITH RARE DISEASES data on Orphadata.

    :return: updated dictionary containing all disorders and associated genes
    """
    #sys.stdout.write('Finding disorders and associated genes/gene information...')
    # Open .xml file using ElementTree
    tree = ET.parse('en_product6.xml')
    root = tree.getroot()
    assnValues = dict()
    assnStatusVals = dict()

    # Iterate through all disorders
    for disorder in root[1].findall("Disorder"):
        diseaseName = disorder.find('Name').text
        id = disorder.attrib
        orphaCode = disorder.find('OrphaCode').text
        if diseaseName not in disorderDict.keys():
            disorderDict[diseaseName] = dict()
            disorderDict[diseaseName].update(id)
            disorderDict[diseaseName]["orphaCode"] = orphaCode

        # Link
        link = disorder.find('ExpertLink').text

        # Disorder Type
        disorderType = disorder.find('DisorderType').find('Name').text

        geneList = disorder.find('DisorderGeneAssociationList')
        # print(geneInfo.text)
        # disorderDict[diseaseName].update(geneInfo.attrib)

        disorderDict[diseaseName]["genes"] = []

        # Find Gene Associations:
        for association in geneList:
            # One gene per association
            gene = association.find("Gene")
            geneDict = dict()
            geneDict["symbol"] = gene.find("Symbol").text

            # Source Validation

            srcValidation = association.find("SourceOfValidation").text
            geneDict['pmid'] = []
            if srcValidation is not None:
                srcValidation = re.split('_', srcValidation)
                for src in srcValidation:
                    if 'PMID' in src:
                        id = src.replace('[PMID]', '')
                        geneDict['pmid'].append(id)

            # Gene Name
            geneDict["name"] = association.find("Gene").find("Name").text

            # Gene Synonyms
            synonyms = []
            for syn in gene.find('SynonymList').iter('Synonym'):
                synonyms.append(syn.text)
            geneDict['synonyms'] = synonyms

            # Gene Type
            geneType = gene.find("GeneType").find("Name").text
            geneDict['type'] = geneType

            # References - Ensembl, OMIM, HGNC
            refList = gene.find('ExternalReferenceList')
            geneDict['externalRefList'] = []

            for ref in refList.iter("ExternalReference"):  # iterate through reference list
                refName = ref.find("Source").text
                refID = ref.find("Reference").text

                # Find Ensembl reference
                if ref.find("Source").text.lower() == "ensembl":
                    ensemblId = ref.find("Reference").text
                    geneDict['ensembl'] = str(ensemblId)

                    # Grab the Ensembl reference link
                    geneDict[
                        'ensemblLink'] = 'https://ensembl.org/Homo_sapiens/Gene/Summary?db=core;g=' + ensemblId

                    # Capture ensembl info
                    if ensemblId in ensemblDict.keys():
                        geneDict['chr'] = ensemblDict[ensemblId].chr
                        geneDict['start'] = ensemblDict[ensemblId].start
                        geneDict['end'] = ensemblDict[ensemblId].end
                        geneDict['strand'] = ensemblDict[ensemblId].strand

                # Find OMIM reference
                if refName.lower() == 'omim':
                    geneDict['OMIM'] = str(refID)

                # Find HGNC reference
                if refName.lower() == 'hgnc':
                    geneDict['HGNC'] = str(refID)

            # Gene-Disorder Association Type
            assnType = association.find("DisorderGeneAssociationType").find('Name').text
            geneDict['associationType'] = assnType
            if assnType not in assnValues.keys():
                assnValues[assnType] = 1
            else:
                assnValues[assnType] += 1

            # Gene-Disorder Association Status
            assnStatus = association.find("DisorderGeneAssociationStatus").find("Name").text
            geneDict['assnStatus'] = assnStatus
            if assnStatus not in assnStatusVals.keys():
                assnStatusVals[assnStatus] = 1
            else:
                assnStatusVals[assnStatus] += 1

            # Gene Locus
            locusList = gene.find("LocusList")
            geneLocus = []
            for locus in locusList.iter("GeneLocus"):
                geneLocus.append(locus.text)
            geneDict['geneLocus'] = geneLocus

            # Add gene to disorder
            disorderDict[diseaseName]["genes"].append(geneDict)

        # Add attributes to dictionary
        disorderDict[diseaseName]["expertLink"] = link
        disorderDict[diseaseName]["type"] = disorderType

    #sys.stdout.write('Finished finding disorders and genes/gene information.')
    return disorderDict


def parsePhenotypesRareDiseases(disorderDict):
    """
    Stores all phenotypes associated with rare diseases and stores them in the dictionary storing all disorders.
    Phenotypes are divided into four categories: 'very frequent phenotypes', 'frequent phenotypes', 'occasional
    phenotypes', and 'rare phenotypes'. HPO IDs and terms are also recorded for each phenotype.

    The following tags from en_product4.xml are used in this method:

        - <Name> : the disorder name (found under the <Disorder> tag)
        - <Source> : PMID sources
        - <ValidationStatus> : whether the phenotype-disorder relationships are validated
        - <HPOID> : ID assigned by HPO to a given phenotype
        - <HPOTerm> : preferred name of HPO phenotype
        - <HPOFrequency> : estimated frequency of occurrence for a given phenotype in a given clinical entity
        - <DiagnosticCriteria> : indicates if the given phenotype is a pathognomonic sign or a diagnostic criterion in a
          given clinical entity

    Updates these phenotypes for all disorders.

    :return: updated dictionary containing phenotypes associated with disorders
    """

    #sys.stdout.write('Finding phenotypes associated with Rare Diseases...')

    # Open .xml files with ElementTree
    tree = ET.parse('en_product4.xml')
    root = tree.getroot()

    # Iterate through all disorder and associated HPO phenotypes
    for item in root[1].findall("HPODisorderSetStatus"):
        disorder = item.find("Disorder")
        diseaseName = disorder.find('Name').text
        if diseaseName not in disorderDict.keys():
            disorderDict[diseaseName] = dict()

        # Source(s)
        sources = item.find("Source").text

        # Validation status & date
        disorderDict[diseaseName]["valid"] = item.find("ValidationStatus").text
        disorderDict[diseaseName]["validDate"] = item.find("ValidationDate").text
        try:
            disorderDict[diseaseName]["sources"] = sources.split("_")
        except AttributeError:
            disorderDict[diseaseName]["sources"] = sources

        # very frequent phenotypes
        disorderDict[diseaseName]["very frequent phenotypes"] = []

        # frequent phenotype(s)
        disorderDict[diseaseName]["frequent phenotypes"] = []

        # occasional phenotype(s)
        disorderDict[diseaseName]["occasional phenotypes"] = []

        # rare phenotype(s)
        disorderDict[diseaseName]["rare phenotypes"] = []

        for association in disorder.find("HPODisorderAssociationList"):

            # HPO id
            hpoId = association.find("HPO").find("HPOId").text

            # HPO Name
            hpoName = association.find("HPO").find("HPOTerm").text

            # HPO Frequency
            hpoFrequency = association.find("HPOFrequency").find("Name").text

            # Diagnostic Criteria
            try:
                hpoDiagnostic = association.find("DiagnosticCriteria").text.strip()
            except AttributeError:
                hpoDiagnostic = association.find("DiagnosticCriteria").text

            # Tuple format to create <a> tags in details page
            x = (hpoName, hpoId)

            # categorize frequencies
            if hpoFrequency == 'very frequent (99-80%)':
                disorderDict[diseaseName]["very frequent phenotypes"].append(x)
            elif hpoFrequency == 'Frequent (79-30%)':
                disorderDict[diseaseName]["frequent phenotypes"].append(x)
            elif hpoFrequency == 'Occasional (29-5%)':
                disorderDict[diseaseName]["occasional phenotypes"].append(x)
            elif 'Very rare ' in hpoFrequency:
                disorderDict[diseaseName]["rare phenotypes"].append(x)

    return disorderDict


def parseRareDiseaseEpi(disorderDict):
    """
    Stores all prevalance(s) for disorders and updates the disorderDict using the epidemiological data from Orphadata.

    The following .xml tags from en_product9_prev.xml are used in this method:

        - <Name> (within <Disorder> tag): disorder name
        - <PrevalenceType> : the name of the prevalence type (can be "Point prevalence",
            "birth prevalence", "lifelong prevalence", "incidence", "cases/families")
        - <PrevalenceQualification> : prevalence qualification name (can be either "Value and Class",
            "Only class", "Case" or "Family")
        - <Source> : source(s) of information of a given prevalence type for disorder
        - <PrevalenceClass> : estimated prevalence of disorder
        - <ValMoy> : mean value of a given prevalence type
        - <PrevalenceGeographic> : geographic area of a given prevalence type
        - <PrevalenceValidationStatus> : can be either: "Validated" or "Not yet validated"

    More information regarding these tags and other tags used in this file can be found via the Orphadata Free Access
    Products file.

    """

    # Open .xml files with ElementTree
    tree = ET.parse('en_product9_prev.xml')
    root = tree.getroot()

    # Iterate through disorders
    for item in root.find("DisorderList"):
        name = item.find("Name").text

        if name not in disorderDict.keys():  # add disorder if not already recorded
            disorderDict[name] = dict()

        # Grab prevalence(s)
        prevalences = dict()

        for prev in item.find("PrevalenceList"):
            try:
                prevType = prev.find("PrevalenceType").find("Name").text
                prevalences[prevType] = dict()
            except AttributeError:
                break

            try:
                prevQual = prev.find("PrevalenceQualification").find("Name").text
                prevalences[prevType]["qualification"] = prevQual
            except AttributeError:
                pass

            try:
                prevSrc = prev.find("Source").text.split('_')
                prevalences[prevType]["source(s)"] = prevSrc
            except AttributeError:
                pass

            try:
                prevClass = prev.find("PrevalenceClass").find("Name").text
                prevalences[prevType]["class"] = prevClass
            except AttributeError:
                pass

            try:
                valMoy = prev.find("ValMoy").text
                prevalences[prevType]["valmoy"] = valMoy
            except AttributeError:
                pass

            try:
                prevGeo = prev.find("PrevalenceGeographic").find("Name").text
                prevalences[prevType]["geographic"] = prevGeo
            except AttributeError:
                pass

            try:
                prevValidStatus = prev.find("PrevalenceValidationStatus").find("Name").text
                prevalences[prevType]["validation"] = prevValidStatus
            except AttributeError:
                pass

        # Add the prevalence info to the disorder
        disorderDict[name]["prevalence(s)"] = prevalences

    #sys.stdout.write('Finished finding epidemiological data.')


def naturalHistory(disorderDict):
    """
    Parses through disorders listed in en_product1.xml to retrieve disorder
    attributes relating to natural history. The disorder dictionary is then updated
    with this information.

    The following .xml tags from en_product9_ages.xml is used in this method:

        - <Name> (found within <Disorder> tag) : name of the disorder
        - <AverageAgeOfOnset> : class based on estimated average age of
          disorder onset; found within the <AverageAgeOfOnsetList> tag
        - <AverageAgeOfDeath> : classes based on the estimated average age at
          death for a given disorder; found within the <AverageAgeOfDeathList>
          tag
        - <TypeOfInheritance> : type(s) of inheritance associated with a given
          disorder; found within the <TypeOfInheritanceList> tag

    More information regarding these tags and other tags used in this file can
    be found via the Orphadata Free Access Products file.
    """

    #sys.stdout.write('Finding ages of onset, death, and inheritance types...')
    # Open .xml file using ElementTree
    tree = ET.parse('en_product9_ages.xml')
    root = tree.getroot()

    inheritanceValues = dict()
    onsetValues = dict()

    # Iterate through disorders
    for item in root.find("DisorderList"):
        name = item.find("Name").text

        # Add disorders if not yet recorded
        if name not in disorderDict.keys():
            disorderDict[name] = dict()

        # Average Age of Onset List
        disorderDict[name]["onsetList"] = []
        for onset in item.find("AverageAgeOfOnsetList"):
            onsetType = onset.find("Name").text
            disorderDict[name]["onsetList"].append(onsetType)
            if onsetType not in onsetValues.keys():
                onsetValues[onsetType] = 1
            else:
                onsetValues[onsetType] += 1

        # Average Age of Death List
        disorderDict[name]["deathList"] = []
        # removed the following as AverageAgeOfDeath is not in XML anymore, Max, Dec 12 2022
        #for ageDeath in item.find("AverageAgeOfDeathList"):
        #    disorderDict[name]["deathList"].append(ageDeath.find("Name").text)

        # Inheritance List
        disorderDict[name]["inheritance"] = []
        for inheritType in item.find("TypeOfInheritanceList"):
            inheritance = inheritType.find("Name").text
            disorderDict[name]["inheritance"].append(inheritance)
            if inheritance not in inheritanceValues.keys():
                inheritanceValues[inheritance] = 1
            else:
                inheritanceValues[inheritance] += 1

    #sys.stdout.write('Finished finding onset/death/inheritance information.')
    return disorderDict


def createBed(disorderDict, assembly):
    """
    Creates the final BED file using all disorders/genes and other information. 

    This method begins by iterating through each disorder in disorderDict. For
    each gene associated with a given disorder, the method creates one entry in the
    BED file that includes the disorder name, chromosomal positions, and other
    aggregated information from the previous methods.  For example, given a
    disorder "Disorder1" with three genes associated with the disorder, "GeneA",
    "GeneB", and "GeneC", resulting BED file will appear as follows:

        <GeneA chr>   <GeneA start>  <GeneA end> <Disorder1 ID> ... <Disorder1 Name> ... <disorder info>
        <GeneB chr>   <GeneB start>  <GeneB end> <Disorder1 ID> ... <Disorder1 Name> ... <disorder info>
        <GeneC chr>   <GeneC start>  <GeneC end> <Disorder1 ID> ... <Disorder1 Name> ... <disorder info>

    NOTE: disorders with no genes associated are skipped and not included in the BED file.

    The BED file fields for the Orphadata information are included below:

        1. chrom : name of chromosome in which gene exists (required)
        2. chromStart : start position of gene (required)
        3. chromEnd : end position of gene (required)
        4. name : name of the BED line; the OrphaCode for each disorder is used here
        5. score : score between 0 and 1000; not relevant to this track and is by default 0
        6. strand : defines the strand, either "+", "-", or "." for no strand
        7. thickStart : start position where gene is drawn thickly (same as field 2)
        8. thickEnd : end position where gene is drawn thickly (same as field 3)
        9. itemRgb : color of the item
        10. disorderName : name of the disorder
        11. geneSymbol : HGNC-approved gene symbol
        12. geneName : gene name
        13. ensemblID : identifier used within Ensembl database
        14. geneType : the type of gene
        15. geneLocus : gene chromosomal location
        16. assnStatus : whether disorder-gene association is validated or not
        17. assnType : type of gene-disease relationship
        18. pmid : PMID reference(s)
        19. orphaCode : unique numerical identifier internal to Orphanet
        20. omim : OMIM reference
        21. hgnc : HGNC reference
        22. inheritance : type(s) of inheritance associated with the disorder
        23. onsetList : list of ages of onset associated with the disorder
        24. deathList : list of average ages of death associated with the disorder
        25. prevalences : types of prevalence associated with disorder
        26. verFreqPhen : very frequent phenotype(s)
        27. freqPhen : frequent phenotype(s)
        28. occasPhen : occasional phenotype(s)
        29. rarePhen : rare phenotype(s)
    """

    # Create new BED file
    # NOTE: encoding for utf-8 is required since several disorders contain special characters that are not included in
    # ascii encoding.
    f = open('orphadata.'+assembly+'.bed', "w", encoding='utf-8')

    # Iterate through disorders
    for disease in disorderDict.keys():
        # Only include disorders with genes associated
        if 'genes' in disorderDict[disease]:
            # Iterate through genes
            for gene in disorderDict[disease]['genes']:
                row = []  # stores the info for each BED line
                # Only include if chromosome is included in gene information
                if 'chr' in gene.keys():
                    # Fields: 1 chr, 2 start, 3 end
                    row.append(str(gene['chr']))
                    row.append(str(gene['start']))
                    row.append(str(gene['end']))
                    # Field 4: name of BED line
                    row.append(disorderDict[disease]['orphaCode'])
                    # Field 5: no score, place 0
                    row.append('0')
                    # Field 6: strand
                    if 'strand' in gene.keys():
                        row.append(str(gene["strand"]))
                    else:  # if no strand is included
                        row.append('.')
                    # Fields 7, 8: Thick start and end (same as fields 2 and 3)
                    row.append(str(gene['start']))
                    row.append(str(gene['end']))
                    # Field 9:  itemRgb
                    row.append('0,146,156')
                    # Field 10: Disorder name
                    row.append(disease)
                    # Field 11: geneSymbol (visible in description, mouseover)
                    row.append(str(gene['symbol']))
                    # Field 12: geneName (visible in description)
                    row.append(str(gene['name']))
                    # Field 13: ensemblID
                    row.append(gene['ensembl'])
                    # Field 14: geneType (visible in description)
                    row.append(str(gene['type']))
                    # Field 15: geneLocus (visible in description)
                    row.append(' '.join(x for x in gene['geneLocus']))
                    # Field 16: association status
                    row.append(str(gene['assnStatus']))
                    # Field 17: association type
                    row.append(str(gene['associationType']))
                    # Field 18: PMID(s)
                    row.append(str(','.join(x for x in gene['pmid'])))
                    # Field 19: Orphacode
                    row.append(disorderDict[disease]['orphaCode'])
                    # Field 20: OMIM ID
                    if 'OMIM' in gene.keys():
                        row.append(gene['OMIM'])
                    else:
                        row.append('')
                    # Field 21: HGNC ID
                    if 'HGNC' in gene.keys():
                        row.append(gene['HGNC'])
                    else:
                        row.append('')
                    # Field 22: Inheritance (visible in description, mouseover)
                    if 'inheritance' not in disorderDict[disease].keys():
                        row.append('')
                    else:
                        row.append(', '.join(x for x in disorderDict[disease]['inheritance']))
                    # Field 23: Onset List
                    if 'onsetList' not in disorderDict[disease].keys():
                        row.append('')
                    else:
                        row.append(', '.join(x for x in disorderDict[disease]['onsetList']))
                    # Field 24: Death List
                    if 'deathList' not in disorderDict[disease].keys():
                        row.append('')
                    else:
                        row.append(', '.join(x for x in disorderDict[disease]['deathList']))
                    # Field 25: prevalence(s)
                    if 'prevalence(s)' not in disorderDict[disease].keys():
                        row.append('')
                    else:
                        row.append(', '.join(str(x) for x in disorderDict[disease]['prevalence(s)']))
                    # NOTE: The following fields featuring phenotype(s) are formatted according to the extraTabelFields format (refer to bigBed documentation).
                    # Field 26: very frequent phenotype(s)
                    if 'very frequent phenotypes' in disorderDict[disease].keys() and len(
                            disorderDict[disease]['very frequent phenotypes']) > 0:
                        row.append(', '.join('<a href="https://hpo.jax.org/app/browse/term/'+x[1]+'" target="_blank">'+x[0]+'</a>' for x in disorderDict[disease]['very frequent phenotypes']))
                    else:
                        row.append('')
                    # Field 27: frequent phenotype(s)
                    if 'frequent phenotypes' in disorderDict[disease].keys() and len(
                            disorderDict[disease]['frequent phenotypes']) > 0:
                        row.append(', '.join('<a href="https://hpo.jax.org/app/browse/term/'+x[1]+'" target="_blank">'+x[0]+'</a>' for x in disorderDict[disease]['frequent phenotypes']))
                    else:
                        row.append('')
                    # Field 28: occasional phenotype(s)
                    if 'occasional phenotypes' in disorderDict[disease].keys() and len(
                            disorderDict[disease]['occasional phenotypes']) > 0:
                        row.append(', '.join('<a href="https://hpo.jax.org/app/browse/term/'+x[1]+'" target="_blank">'+x[0]+'</a>' for x in disorderDict[disease]['occasional phenotypes']))
                    else:
                        row.append('')
                    # Field 29: rare phenotype(s)
                    if 'rare phenotypes' in disorderDict[disease].keys() and len(
                            disorderDict[disease]['rare phenotypes']) > 0:
                        row.append(', '.join('<a href="https://hpo.jax.org/app/browse/term/'+x[1]+'" target="_blank">'+x[0]+'</a>' for x in disorderDict[disease]['rare phenotypes']))
                    else:
                        row.append('')

                    # Write line to BED file
                    f.write('\t'.join(x for x in row))

                    # add new line
                    f.write('\n')
    f.close()


def main():
    """
    Calls all methods required for downloading Orphadata files and creating BED file.
    """

    # Grab timestamp
    parser = argparse.ArgumentParser(description='Downloads most recent Orphadata files and generates a BED file for Orphadata track hub.')
    #parser.add_argument("-t", "--timestamp", help="The timestamp at which script is called.", required=True)
    args = parser.parse_args()
    #timestamp = args.timestamp
    
    startTime = time.time()

    # Create Ensembl dictionaries for hg19 and hg38
    hg19ensemblDict, hg38ensemblDict = getEnsGeneInfo()

    # Create dictonary to store disorders
    hg19disorderDict = dict()
    hg38disorderDict = dict()

    # 1. Parse Rare Diseases
    parseRareDiseases(hg19disorderDict, hg19ensemblDict)
    parseRareDiseases(hg38disorderDict, hg38ensemblDict)

    # 2. Parse Phenotypes of Rare Diseases
    parsePhenotypesRareDiseases(hg19disorderDict)
    parsePhenotypesRareDiseases(hg38disorderDict)

    # 3. Parse Epidemology Prevalences
    parseRareDiseaseEpi(hg19disorderDict)
    parseRareDiseaseEpi(hg38disorderDict)

    # 4. Natural History (ages)
    naturalHistory(hg19disorderDict)
    naturalHistory(hg38disorderDict)

    # 5. Create BED files
    createBed(hg19disorderDict, assembly='hg19')
    createBed(hg38disorderDict, assembly='hg38')

    #print("Time elapsed: ", time.time() - startTime)
    

if __name__ == "__main__":
    main()

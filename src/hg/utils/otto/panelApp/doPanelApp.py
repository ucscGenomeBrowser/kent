#!/hive/data/outside/otto/panelApp/venv/bin/python3

from datetime import date
import pandas as pd 
import gzip, logging, re, sys, json, time, requests, shutil, os, subprocess

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

def getArchDir(db):
    " return hgwdev archive directory given db "
    dateStr = date.today().strftime("%Y-%m-%d")
    archDir = "/usr/local/apache/htdocs-hgdownload/goldenPath/archive/%s/panelApp/%s" % (db, dateStr)
    if not os.path.isdir(archDir):
        os.makedirs(archDir)
    return archDir

def writeBb(hg19Table, hg38Table, subTrack):
    " sort the pandas tables, write to BED and convert "
    for db in ["hg19", "hg38"]:
        archDir = getArchDir(db)

        bedFname = "current/%s/%s.bed" % (db, subTrack)
        bbFname = "current/%s/%s.bb.tmp" % (db, subTrack)

        if db=="hg19":
            pdTable = hg19Table
        else:
            pdTable = hg38Table

        # for cnvs, one of the arguments can be None
        if pdTable is None:
            continue

        pdTable.sort_values(by=['chrom','chromStart'], ascending = (True, True), inplace=True)
        pdTable = pdTable.applymap(lambda x: x.replace('\t', ' ') if isinstance(x, str) else x)
        pdTable.to_csv(bedFname, sep='\t', index=False, header=None)

        asFname = subTrack+".as"

        # -extraIndex=geneName 
        cmd = "bedToBigBed -tab -as=%s -type=bed9+26 %s /hive/data/genomes/%s/chrom.sizes %s" % (asFname, bedFname, db, bbFname)
        assert(os.system(cmd)==0)

        # put a copy into the archive
        archBbFname = archDir+"/%s.bb" % subTrack
        shutil.copyfile(bbFname, archBbFname)

def updateGbdbSymlinks(country):
    " update the symlinks in /gbdb. Not really necessary but kept this code just in case. "
    if country == "Australia":
        subtracks = ["genesAus", "tandRepAus", "cnvAus"]
    else:
        subtracks = ["genes", "tandRep", "cnv"]
    for db in ["hg19", "hg38"]:
        archDir = getArchDir(db)
        for subTrack in subtracks:
            if db=="hg19" and "cnv" in subTrack:
                continue # no cnv on hg19
            cmd = "ln -sf `pwd`/current/%s/%s.bb /gbdb/%s/panelApp/%s.bb" % (db, subTrack, db, subTrack)
            assert(os.system(cmd)==0)

def checkIfFilesTooDifferent(oldFname,newFname):
    # Exit if the difference is more than 10%

    oldItemCount = bash('bigBedInfo '+oldFname+' | grep "itemCount"')
    oldItemCount = int(oldItemCount.rstrip().split("itemCount: ")[1].replace(",",""))
    
    newItemCount = bash('bigBedInfo '+newFname+' | grep "itemCount"')
    newItemCount = int(newItemCount.rstrip().split("itemCount: ")[1].replace(",",""))

    if abs(newItemCount - oldItemCount) > 0.1 * max(newItemCount, oldItemCount):
        sys.exit(f"Difference between itemCounts greater than 10%: {newItemCount}, {oldItemCount}")
    else:
        print(oldFname+" vs. new count: "+str(oldItemCount)+" - "+str(newItemCount))

def flipFiles(country):
    " rename the .tmp files to the final filenames "
    if country == "Australia":
        subtracks = ["genesAus", "tandRepAus", "cnvAus"]
    else:
        subtracks = ["genes", "tandRep", "cnv"]
    for db in ["hg19", "hg38"]:
        archDir = getArchDir(db)
        for subTrack in subtracks:
            if db=="hg19" and "cnv" in subTrack:
                # no cnvs for hg19 yet
                continue
            oldFname = "current/%s/%s.bb.tmp" % (db, subTrack) #New data just generated
            newFname = "current/%s/%s.bb" % (db, subTrack) #Existing .bb

            #Check if files are more than 10% different
            checkIfFilesTooDifferent(newFname,oldFname)
            
            os.replace(oldFname, newFname)

def getAllPages(url, results=[]):
    " recursively download all pages. Stack should be big enough "
    try:
        myResponse = requests.get(url)
        if (myResponse.ok):
            jData = json.loads(myResponse.content.decode())
            # If jData is empty create, else append
            if "error" in jData.keys() or not "results" in jData.keys():
                raise Exception("Error in keys when downloading %s" % url)

            if "count" in jData and not "page" in url:
                print("API says that there are %d results for url %s" % (jData["count"], url))
            results.extend(jData["results"])

            if "next" in jData and jData["next"] is not None: # need to get next URL
                return getAllPages(jData["next"], results)
        else:
            raise Exception("Error in object when downloading %s" % url)
    except:
        raise Exception("HTTP Error when downloading %s" % url)
    return results

def downloadCnvs(url):
    Error = True
    continuous_count=0
    res = getAllPages(url, results=[])

    num_gene_data = len(res)
    print("Got %d CNVs" % num_gene_data)
    count = 0
    continuous_count = 0
    hg19_dict = dict()
    hg38_dict = dict()

    for geneCount, cnvData in enumerate(res):
        temp_attribute_dictionary = dict()
        string_dict_key = 'gene_{}'.format(geneCount)
        
        chromo = cnvData['chromosome']
        chromosome = 'chr' + chromo

        start_coordinates = cnvData['grch38_coordinates'][0]
        end_coordinates = cnvData['grch38_coordinates'][1]

        score = '0'
        strand = '.'
        thickStart = start_coordinates
        thickEnd = end_coordinates
        blockCount = '1'
        blockSizes = int(end_coordinates) - int(start_coordinates)
        blockStarts = 0
        
        confidence_level = cnvData['confidence_level']

        rgb_dict = {'0' : '100,100,100', '3': '0,255,0', '2': '255,191,0', '1':'255,0,0'}
        itemRgb = rgb_dict[confidence_level]
        
        entity_name = cnvData['entity_name']
        entity_type = cnvData['entity_type']
        evidence = ' '.join(cnvData['evidence'])

        haploinsufficiency_score = cnvData.get('haploinsufficiency_score')
        if not haploinsufficiency_score:
            haploinsufficiency_score = ''

        moi = cnvData.get('mode_of_inheritance')
        if not moi:
            moi = ''

        disease_group = cnvData['panel'].get('disease_group')
        if not disease_group:
            disease_group = ''

        disease_sub_group = cnvData['panel'].get('disease_sub_group')
        if not disease_sub_group:
            disease_sub_group = ''

        # idd = Panel ID
        idd = cnvData['panel'].get('id')
        if not idd:
            idd = ''

        panel_name = cnvData['panel'].get('name')
        if not panel_name:
            panel_name = ''
        
        relevant_disorders = ' '.join(cnvData['panel'].get('relevant_disorders', []))
        if not relevant_disorders:
            relevant_disorders = ''

        status = cnvData['panel'].get('status')
        if not status:
            status = ''
        
        '''
        types = cnvData['panel'].get['types')
        if not types:
            types = ''
        else:
            types = str(types).replace("{","").replace("}", "").replace("'", "")
            types = types[1:-1]
        '''

        types = cnvData['panel']['types'][0].get('name')

        version = cnvData['panel']['version']
        if 'genomicsengland' in url:
            if float(version) < 0.99:
                continue
            if not version:
                version = ''
        else:
            if not version:
                version = ''

        penetrance = cnvData.get('penetrance')
        if not penetrance:
            penetrance = ''

        phenotypes = ' '.join(cnvData.get('phenotypes', []))
        if not phenotypes:
            phenotypes = ''

        publications = ' '.join(cnvData['publications'])
        if not publications:
            publications = ''
        
        #required_overlap_percentage = cnvData['required_overlap_percentage']
        tags = cnvData['tags']
        if not tags:
            tags = ''
    
        triplosensitivity_score = cnvData.get('triplosensitivity_score')
        if not triplosensitivity_score:
            triplosensitivity_score = ''
    
        type_of_variants = None
        if "type_of_variants" in cnvData:
            type_of_variants = cnvData['type_of_variants']
        if not type_of_variants:
            type_of_variants = ''

        verbose_name = cnvData.get('verbose_name')
        if not verbose_name:
            verbose_name = ''    

        # Mouse Over Field
        mouseOverField = ""
        try:
            mof = 'Gene: ' +  entity_name + ';' + ' Panel: ' + name + ';' + ' MOI: ' + moi + ';' + ' Phenotypes: ' + phenotypes + ';' + ' Confidence: ' + confidence_level + ';'    
            mouseOverField = mof
        except:
            mouseOverField = ''        

        # name
        name = '{} ({})'.format(entity_name, panel_name)
        
        hg38_dict[string_dict_key] = [chromosome, start_coordinates, end_coordinates, name, score, strand, 
                            thickStart, thickEnd, itemRgb, blockCount, blockSizes, blockStarts, confidence_level, 
                            panel_name, idd, entity_name, entity_type, evidence, haploinsufficiency_score, moi, disease_group, 
                            disease_sub_group, relevant_disorders, status, types, version, penetrance, phenotypes, 
                            publications, triplosensitivity_score, type_of_variants, verbose_name, mouseOverField]
        
        #-------------------------------------------------------------------------------
    
        continuous_count = continuous_count + 1
        #count = count + 1

    # Removes new lines
    for key, item in hg38_dict.items():
        strip_list = list()
        for i in item:
            try:
                strip_list.append(i.replace('\t', ' ').strip().strip("\n").strip("\r"))
            except:
                strip_list.append(i)
        hg38_dict[key] = strip_list

    pd_38_table = pd.DataFrame.from_dict(hg38_dict)
    pd_38_table = pd_38_table.T
    pd_38_table.columns = ['chrom', 'chromStart', 'End', 'name', 'Score', 'strand', 'thickStart', 'thickEnd', 
                            'itemRgb', 'blockCount', 'blockSizes', 'blockStarts', 'Confidence Level', 
                            'Panel Name', 'Panel ID', 'Entity Name', 'Entity Type', 'Evidence', 'ClinGen Haploinsufficiency Score', 
                            'Mode of Inheritance', 'Disease Group', 'Disease Sub Group', 'Relevant Disorders', 
                            'Status', 'Types', 'Version Created', 'Penetrance', 'Phenotypes', 'Publications', 
                            'CinGen Triplosensitivity Score', 'Type of Variants', 'Verbose Name', 'Mouse Over Field']
    #pd_38_table = pd_38_table.sort_values(by=['chromosome', 'Start'], ascending = (True, True))
    #pd_38_table.to_csv('hg38_region_noheader_sorted.tsv', sep='\t', index=False, header=None) 
    #pd_38_table.to_csv('hg38_region_header_sorted.tsv', sep='\t', index=False) 
    return pd_38_table

def downloadTandReps(url):
    Error = True
    continuous_count=0
    res = getAllPages(url, results=[])

    num_gene_data = len(res)
    print("Got %d tandem repeats from API" % num_gene_data)

    count = 0
    continuous_count = 0
    hg19_dict = dict()
    hg38_dict = dict()

    while count != num_gene_data:
        #if count == 10:
        #    break
        string_dict_key = 'gene_{}'.format(continuous_count)
        
        temp_attribute_dictionary = dict()
        chromosome = res[count]['chromosome']    
        chromosome = 'chr{}'.format(chromosome)
        confidence_level = res[count]['confidence_level']
        entity_name = res[count]['entity_name']
        entity_type = res[count]['entity_type']
        evidence = ' '.join(res[count]['evidence'])
        
        gene_data = res[count]['gene_data']
        if gene_data:
            alias = ' '.join(gene_data.get('alias', []))
            biotype = gene_data['biotype']
        else:
            alias = ""
            biotype = ""
        try:
            ensembl_id_37 = gene_data['ensembl_genes']['GRch37']['82']['ensembl_id']
        except:
            ensembl_id_37 = "None"
        try:        
            ensembl_id_38 = gene_data['ensembl_genes']['GRch38']['90']['ensembl_id']
        except:
            ensembl_id_38 = "None"        
        
        gene_name = gene_data['gene_name']
        #gene_symbol = gene_data['gene_symbol']
        #hgnc_date_symbol_changed = gene_data['hgnc_date_symbol_changed']
        hgnc_id = gene_data['hgnc_id']
        hgnc_symbol = gene_data['hgnc_symbol']
        if str(gene_data['omim_gene']) == "None":
            omim_gene = "None"
        else:
            omim_gene = ' '.join(gene_data['omim_gene'])
        grch37_coordinates = res[count]['grch37_coordinates']
        if grch37_coordinates == None:
            coordinates = gene_data['ensembl_genes']['GRch37']['82']['location'] 
            location = coordinates.split(':')
            grch37_coordinates = location[1].split('-')
        chromStart_19 = int(grch37_coordinates[0])
        chromEnd_19 = int(grch37_coordinates[1])

        # hg38
        grch38_coordinates = res[count]['grch38_coordinates']
        if grch38_coordinates == None:
            coordinates = gene_data['ensembl_genes']['GRch38']['90']['location'] 
            location = coordinates.split(':')
            grch38_coordinates = location[1].split('-')

        mode_of_inheritance = res[count]['mode_of_inheritance']
        normal_repeats = res[count]['normal_repeats']
        chromStart_38 = int(grch38_coordinates[0])
        chromEnd_38 = int(grch38_coordinates[1])
        
        panel = res[count]['panel']
        disease_group = panel['disease_group']
        disease_sub_group = panel['disease_sub_group']
        hash_id = panel['hash_id']
        idd = panel['id']
        panel_name = panel['name']
        relevant_disorders = ' '.join(panel['relevant_disorders'])
        #relevant_disorders = relevant_disorders[:240]
        
        stats = panel['stats']
        number_of_gene = stats['number_of_genes']
        number_of_regions = stats['number_of_regions']
        number_of_strs = stats['number_of_strs']

        status = panel['status']
        #description = panel['types'][0]['description'][:240]
        description = panel['types'][0]['description']
        version = panel['version']
        version_created = panel['version_created']
        pathogenic_repeats = res[count]['pathogenic_repeats']
        penetrance = res[count]['penetrance']
        phenotypes = ' '.join(res[count]['phenotypes'])
        phenotypes_no_num = ''.join([i for i in phenotypes if not i.isdigit()])
        publications = ' '.join(res[count]['publications'])
        repeated_sequence = res[count]['repeated_sequence']
        tags = ' '.join(res[count]['tags'])

        # Check to see if panel_name is not empty
        if panel_name:
            try:
                panel_name = panel_name.split(' - ')
                panel_name = panel_name[0]
                name = '{} ({})'.format(hgnc_symbol, panel_name)
            except:
                name = '{} ({})'.format(hgnc_symbol, panel_name)
        else:
            name = hgnc_symbol
        score = 0
        strand = '.'
        thickStart_19 = chromStart_19
        thickEnd_19 = chromEnd_19
        thickStart_38 = chromStart_38
        thickEnd_38 = chromEnd_38

        rgb_dict = {'3': '0,255,0', '2': '255,191,0', '1':'255,0,0'}
        # If the confidence level is set to 0, set to 1
        if confidence_level == '0':
            confidence_level = '1'
        rgb = rgb_dict[confidence_level]
        rgb = rgb.strip('"')
        blockCount = 1
        
        # Cases where coordinates are reads as string data types instead of ints
        try:
            blockSizes_19 = chromEnd_19 - chromStart_19
        except:
            blockSizes_19 = int(chromEnd_19) - int(chromStart_19)

        try:
            blockSizes_38 = chromEnd_38 - chromStart_38
        except:
            blockSizes_39 = int(chromEnd_38) - int(chromStart_38)

        blockStarts = 0
        geneSymbol = hgnc_symbol

        #-------------------------------------------------------------------------------

        temp19_list = [chromosome, chromStart_19, chromEnd_19, name, score, strand,
        thickStart_19, thickEnd_19, rgb, blockCount, blockSizes_19, chromStart_19, geneSymbol, confidence_level, 
        entity_type, evidence, alias, ensembl_id_37, gene_name, hgnc_id, hgnc_symbol, omim_gene, 
        mode_of_inheritance, normal_repeats, disease_group, disease_sub_group, idd, panel_name, 
        relevant_disorders, number_of_gene, number_of_regions, number_of_strs, description, 
        version, version_created, pathogenic_repeats, penetrance, phenotypes, 
        publications, repeated_sequence]
        
        try: 
            temp19_list = [i.replace('\t', ' ').strip().strip("\n").strip("\r") for i in temp19_list]
        except:
            hg19_dict[string_dict_key] = temp19_list

        #-------------------------------------------------------------------------------

        temp38_list = [chromosome, chromStart_38, chromEnd_38, name, score, strand,
        thickStart_38, thickEnd_38, rgb, blockCount, blockSizes_38, chromStart_38, geneSymbol, confidence_level, 
        entity_type, evidence, alias, ensembl_id_38, gene_name, hgnc_id, hgnc_symbol, omim_gene, 
        mode_of_inheritance, normal_repeats, disease_group, disease_sub_group, idd, panel_name, 
        relevant_disorders, number_of_gene, number_of_regions, number_of_strs, description, 
        version, version_created, pathogenic_repeats, penetrance, phenotypes, 
        publications, repeated_sequence]
        
        try: 
            temp38_list = [i.replace('\t', ' ').strip().strip("\n").strip("\r") for i in temp38_list]
        except:
            hg38_dict[string_dict_key] = temp38_list

        #-------------------------------------------------------------------------------
    
        continuous_count = continuous_count + 1
        count = count + 1

    pd_19_table = pd.DataFrame.from_dict(hg19_dict)
    pd_19_table = pd_19_table.T
    pd_19_table.columns = ['chrom', 'chromStart', 'chromEnd', 'name', 'score', 'strand',
        'thickStart', 'thickEnd', 'rgb', 'blockCount', 'blockSizes', 'blockStarts', 'geneSymbol', 
        'confidence_level', 'entity_type', 'evidence', 'alias', 'ensembl_id_37', 'gene_name', 
        'hgnc_id', 'geneSymbol', 'omim_gene', 'mode_of_inheritance', 'normal_repeats', 'disease_group', 
        'disease_sub_group', 'idd', 'panel_name', 'relevant_disorders', 'number_of_gene', 'number_of_regions', 
        'number_of_strs', 'description', 'version', 'version_created', 'pathogenic_repeats', 'penetrance', 
        'phenotypes', 'publications', 'repeated_sequence']
    #pd_19_table = pd_19_table.sort_values(by=['chrom','chromStart'], ascending = (True, True))
    #pd_19_table.to_csv('str_hg19.bed', sep='\t', index=False, header=None)

    pd_38_table = pd.DataFrame.from_dict(hg38_dict)
    pd_38_table = pd_38_table.T
    pd_38_table.columns = ['chrom', 'chromStart', 'chromEnd', 'name', 'score', 'strand',
        'thickStart', 'thickEnd', 'rgb', 'blockCount', 'blockSizes', 'blockStarts', 'geneSymbol', 
        'confidence_level', 'entity_type', 'evidence', 'alias', 'ensembl_id_37', 'gene_name', 
        'hgnc_id', 'geneSymbol', 'omim_gene', 'mode_of_inheritance', 'normal_repeats', 'disease_group', 
        'disease_sub_group', 'idd', 'panel_name', 'relevant_disorders', 'number_of_gene', 'number_of_regions', 
        'number_of_strs', 'description', 'version', 'version_created', 'pathogenic_repeats', 'penetrance', 
        'phenotypes', 'publications', 'repeated_sequence']
    #pd_38_table = pd_38_table.sort_values(by=['chrom','chromStart'], ascending = (True, True))
    #pd_38_table.to_csv('hg38_str_noheader_sorted.tsv', sep='\t', index=False, header=None)

    return pd_19_table, pd_38_table

def getPanelIds(url):
    #logging.basicConfig(level=logging.DEBUG)
    logging.basicConfig(level=logging.INFO)
    logging.getLogger("urllib3").propagate = False

    logging.info("Downloading panel IDs")
    panelIds = []

    gotError = False
    while not gotError:
        logging.debug("Getting %s" % url)
        myResponse = requests.get(url)

        jsonData = myResponse.content
        #jsonFh.write(jsonData)
        #jsonFh.write("\n".encode())
        data = json.loads(jsonData.decode())

        for res in data["results"]:
            panelIds.append(res["id"])

        logging.debug("Total Panel IDs downloaded:  %s" % len(panelIds))
        url = data["next"]
        if url is None:
            break

    return panelIds

def downloadPanels(url):
    panelIds = getPanelIds(url)
    panelInfos = {}

    for panelId in panelIds:
        if 'england' in url:
            panelUrl = "https://panelapp.genomicsengland.co.uk/api/v1/panels/%d?format=json" % panelId
        elif 'aus' in url:
            panelUrl = "https://panelapp-aus.org/api/v1/panels/%d/?format=json" % panelId
        logging.debug("Getting %s" % panelUrl)
        resp = requests.get(panelUrl)
        res  = resp.json()

        panelInfos[panelId] = res

    return panelInfos

def getGeneSymbols(url):
    try:
        panelInfos = downloadPanels(url)
    except requests.exceptions.JSONDecodeError:
        time.sleep(30)
        panelInfos = downloadPanels(url)

    syms = set()
    for panelInfo in panelInfos.values():
        for gene in panelInfo["genes"]:
            sym = gene["gene_data"]["gene_symbol"]
            syms.add(sym)
            assert(sym!="")
    logging.info("Got %d gene symbols" % len(syms))
    return list(syms)

def getGenesLocations(jsonFh,url,onlyPanels):
    hg19_dict = dict()
    hg38_dict = dict()
    repeat19 = list()
    repeat38 = list()
    continuous_count = 0
    genes_missing_info = list()
    genes_no_location = list()

    syms = getGeneSymbols(url)

    for sym in syms:
        if 'england' in url:
            entityUrl = "https://panelapp.genomicsengland.co.uk/api/v1/genes/?entity_name={}&format=json".format(sym)
        elif 'aus' in url:
            entityUrl = "https://panelapp-aus.org/api/v1/genes/?entity_name={}&format=json".format(sym)

        count = 0
        while True:
            try:
                myResponse = requests.get(entityUrl)
                if myResponse.ok:
                    break
                else:
                    logging.error("Some error on %s, retrying after 1 minute (trial %d)" % (entityUrl, count))
            except Exception:
                logging.error("HTTP error on %s, retrying after 1 minute (trial %d)" % (entityUrl, count))

            time.sleep(60)    # Wait 1 minute before trying again
            count += 1        # Count the number of tries before failing
            if count > 10:    # Quit afer 10 failed attempts
                assert False, "Cannot get URL after 10 attempts"

        jsonData = myResponse.content
        #jData = myResponse.json()
        jData = json.loads(jsonData.decode())

        jsonFh.write(jsonData)
        jsonFh.write("\n".encode())

        res = jData['results']
        
        #filter by onlyPanels early, if specified
        if onlyPanels is not None:
            res = [entry for entry in res if entry.get('panel', {}).get('id') in onlyPanels]

        num_gene_variant = len(res)
        count = 0
        while count != num_gene_variant:
            temp_attribute_dictionary = dict()
            string_dict_key = 'gene_{}'.format(continuous_count)

            gene_range_37 = None
            gene_range_38 = None

            try:
                ensembl_genes_GRch37_82_location = res[count]['gene_data']['ensembl_genes']['GRch37']['82']['location']
                location_37 = ensembl_genes_GRch37_82_location.split(':')
                chromo_37 = 'chr'+location_37[0]
                gene_range_37 = location_37[1].split('-')
                # on hg19, we have added a chrMT sequence later.
            except:
                genes_missing_info.append(res[count]['gene_data']['gene_symbol']+"/hg19")

            try:
                ensembl_genes_GRch38_90_location = res[count]['gene_data']['ensembl_genes']['GRch38']['90']['location']
                location_38 = ensembl_genes_GRch38_90_location.split(':')
                chromo_38 = 'chr'+location_38[0]
                # Change mitochondrial chromosomal suffix from MT -> M for hg38 only
                if chromo_38 == "chrMT":
                    chromo_38 = "chrM"

                gene_range_38 = location_38[1].split('-')
            except:
                genes_missing_info.append(res[count]['gene_data']['gene_symbol']+"/hg38")

            if gene_range_37 is None and gene_range_38 is None:
                #print("gene without location on any assembly: %s" % res[count])
                genes_no_location.append(res[count]['gene_data'])
                count+=1
                continue

            score = '0'
            strand = '.'
            blockCount = '1'
            blockStarts = '0'

            #-----------------------------------------------------------------------------------------------------------

            gene_data_list = ['gene_name', 'hgnc_symbol', 'hgnc_id']
            for attribute in gene_data_list:
                try:
                    temp_attribute_dictionary[attribute] = res[count]['gene_data'][attribute]
                except:
                    temp_attribute_dictionary[attribute] = ''

            try:
                temp_attribute_dictionary['omim_gene'] = ' '.join(res[count]['gene_data']['omim_gene'])
            except:
                temp_attribute_dictionary['omim_gene'] = ''

            try:
                temp_attribute_dictionary['gene_symbol'] = res[count]['gene_data']['gene_symbol']
            except:
                temp_attribute_dictionary['gene_symbol'] = ''

            #-----------------------------------------------------------------------------------------------------------
            # Need to split HGNC ID
            try:
                hgnc = res[count]['gene_data']['hgnc_id']
                temp_attribute_dictionary['hgnc_id'] = hgnc.split(':')[1]
            except:
                temp_attribute_dictionary['hgnc_id'] = ''

            #-----------------------------------------------------------------------------------------------------------
            # Capitalize(title) gene name

            try:
                gene_name = res[count]['gene_data']['gene_name'].title()
                temp_attribute_dictionary['gene_name'] = gene_name
            except:
                temp_attribute_dictionary['gene_name'] = ''
            #-----------------------------------------------------------------------------------------------------------
            # Biotype change protein_coding to Protein Coding

            try:
                biotype = res[count]['gene_data']['biotype']

                if biotype == 'protein_coding':
                    biotype = 'Protein Coding'
                
                temp_attribute_dictionary['biotype'] = biotype
                if biotype == None:
                    temp_attribute_dictionary['biotype'] = ''
            except:
                temp_attribute_dictionary['biotype'] = ''

            #-----------------------------------------------------------------------------------------------------------    

            try:
                ensembl_genes_GRch37_82_ensembl_id = res[count]['gene_data']['ensembl_genes']['GRch37']['82']['ensembl_id']
                ensembl_genes_GRch38_90_ensembl_id = res[count]['gene_data']['ensembl_genes']['GRch38']['90']['ensembl_id']

            except:
                ensembl_genes_GRch37_82_ensembl_id = ''

            #-----------------------------------------------------------------------------------------------------------

            gene_type_list = ['confidence_level', 'phenotypes', 'mode_of_inheritance', 'tags']

            for attribute in gene_type_list:
                try:
                    x = res[count][attribute]
                    if not x:
                        temp_attribute_dictionary[attribute] = ''
                    else:
                        pre = ' '.join(res[count][attribute])
                        temp_attribute_dictionary[attribute] = pre.replace('\t', ' ')
                except:
                    temp_attribute_dictionary[attribute] = ''

            #-----------------------------------------------------------------------------------------------------------
            # Cannot exceed 255 characters
            # Cannot have tabs
            try:
                x = res[count]['phenotypes']
                y = ' '.join(res[count]['phenotypes'])

                if not x:
                    temp_attribute_dictionary['phenotypes'] = ''
                else:
                    temp_attribute_dictionary['phenotypes'] = y.replace("\t", " ")
            except:
                temp_attribute_dictionary['phenotypes'] = ''
            #-----------------------------------------------------------------------------------------------------------
            # Evidence cannot exceed 255 characters
            try:
                x = res[count]['evidence']
                y = ' '.join(res[count]['evidence'])
                if not x:
                    temp_attribute_dictionary['evidence'] = ''
                else:
                    temp_attribute_dictionary['evidence'] = y
            except:
                temp_attribute_dictionary['evidence'] = ''

            #-----------------------------------------------------------------------------------------------------------
            tags = ' '.join(res[count]['tags']).title()
            try:
                if not tags:
                    temp_attribute_dictionary['tags'] = ''
                else:
                    temp_attribute_dictionary['tags'] = tags
            except:
                temp_attribute_dictionary['tags'] = ''

            #-----------------------------------------------------------------------------------------------------------
            # Mode of Inheritance (fix format)
            MOI = ' '.join(res[count]['mode_of_inheritance']).replace("  ", "???").replace(" ", "").replace("???", " ")
            try:
                if not MOI:
                    temp_attribute_dictionary['mode_of_inheritance'] = ''
                else:
                    temp_attribute_dictionary['mode_of_inheritance'] = MOI
            except:
                temp_attribute_dictionary['mode_of_inheritance'] = ''

            #-----------------------------------------------------------------------------------------------------------
            # For values with spaces 
            gene_type_list = ['entity_name', 'penetrance']

            for attribute in gene_type_list:
                try:
                    temp_attribute_dictionary[attribute] = ' '.join(res[count][attribute]).replace(" ", "")
                except:
                    temp_attribute_dictionary[attribute] = ''
            #-----------------------------------------------------------------------------------------------------------
            # For values with spaces and need to be capitalized
            attribute = 'entity_type'

            try:
                temp_attribute_dictionary[attribute] = ' '.join(res[count][attribute]).replace(" ", "").capitalize()
            except:
                temp_attribute_dictionary[attribute] = ''
            #-----------------------------------------------------------------------------------------------------------
            attribute = 'mode_of_pathogenicity'

            try:
                mode = ' '.join(res[count][attribute]).replace("  ", "???").replace(" ", "").replace("???", " ")
                if mode[0] == 'L' or mode[0] == 'l':
                    temp_attribute_dictionary[attribute] = 'Loss-of-function variants'
                elif mode[0] == 'G' or mode[0] == 'g':
                    temp_attribute_dictionary[attribute] = 'Gain-of-function'
                elif mode[0] == 'O' or mode[0] == 'o':
                    temp_attribute_dictionary[attribute] = 'Other'
                else:
                    temp_attribute_dictionary[attribute] = mode
            except:
                temp_attribute_dictionary[attribute] = ''

            #-----------------------------------------------------------------------------------------------------------

            panel_list = ['id','name', 'disease_group', 'disease_sub_group', 'status', 'version_created']

            for attribute in panel_list:
                try:
                    x = res[count]['panel'][attribute]
                    if not x:
                        temp_attribute_dictionary[attribute] = ''
                    else:
                        temp_attribute_dictionary[attribute] = x
                except:
                    temp_attribute_dictionary[attribute] = ''

            #-----------------------------------------------------------------------------------------------------------
            
            version_num = 0.0
            try:
                version_num = float(res[count]['panel']['version'])
                temp_attribute_dictionary['version'] = version_num
            except:
                temp_attribute_dictionary['version'] = ''

            #-----------------------------------------------------------------------------------------------------------
            
            try:
                x = res[count]['panel']['relevant_disorders']
                y = ' '.join(res[count]['panel']['relevant_disorders'])
                if not x:
                    temp_attribute_dictionary['relevant_disorders'] = ''
                else:
                    temp_attribute_dictionary['relevant_disorders'] = y
            except:
                temp_attribute_dictionary['relevant_disorders'] = ''
            
            #-----------------------------------------------------------------------------------------------------------
            # minimal effort to clean up the publication field, which is a mess of free form text
            pubs = res[count]['publications']
            newPubs = []
            for pub in pubs:
                pub = pub.replace("\n", "")
                # replace commas with html commas as unfortunately I use commasin the browser to split fields
                pub = pub.replace(",", "&comma;")
                # translate unicode chars to something the genome browser can display
                pub = pub.encode('ascii', 'xmlcharrefreplace').decode("ascii")
                if re.match("^[0-9]+$", pub):
                    #pubUrls.append("https://pubmed.ncbi.nlm.nih.gov/"+pub+"|PMID"+pub)
                    pub = "PMID"+pub

                newPubs.append(pub)

            temp_attribute_dictionary['publications'] = ", ".join(newPubs)

            #-----------------------------------------------------------------------------------------------------------
            # MouseOverField
            try:
                mof = 'Gene: ' +  temp_attribute_dictionary['gene_symbol'] + ';' + ' Panel: ' + temp_attribute_dictionary['name'] + ';' + ' MOI: ' + MOI + ';' + ' Phenotypes: ' + temp_attribute_dictionary['phenotypes'] + ';' + ' Confidence: ' + temp_attribute_dictionary['confidence_level'] + ';'
                temp_attribute_dictionary['mouseOverField'] = mof
            except:
                temp_attribute_dictionary['mouseOverField'] = ''
            
            #-----------------------------------------------------------------------------------------------------------
            # Column 4
            temp_attribute_dictionary['label'] = temp_attribute_dictionary['gene_symbol'] + ' (' + temp_attribute_dictionary['name'] + ')'
            #-----------------------------------------------------------------------------------------------------------

            #-----------------------------------------------------------------------------------------------------------
            rgb_dict = {'3': '0,255,0', '2': '255,191,0', '1':'255,0,0'}

            # If the confidence level is set to 0, set to 1
            if temp_attribute_dictionary['confidence_level'] == '0':
                temp_attribute_dictionary['confidence_level'] = '1'

            rgb = rgb_dict[temp_attribute_dictionary['confidence_level']]
            rgb = rgb.strip('"')

            '''
            Replace all tab in value with spaces and removes new lines
            '''
            for key, item in temp_attribute_dictionary.items():
                try:
                    if isinstance(item, int):
                        pass
                    elif isinstance(item, float):
                        pass
                    else:
                        temp_attribute_dictionary[key] = item.replace('\t', ' ').strip().strip("\n").strip("\r")
                except:
                    pass

            if temp_attribute_dictionary['label'] not in repeat19 and gene_range_37 is not None:    # Removes Repeats
                repeat19.append(temp_attribute_dictionary['label'])
                blockSizes = int(gene_range_37[1]) - int(gene_range_37[0])
                hg19_dict[string_dict_key] = [chromo_37, int(gene_range_37[0]), gene_range_37[1], temp_attribute_dictionary['label'], 
                                        score, strand, gene_range_37[0], gene_range_37[1], rgb, blockCount, blockSizes, blockStarts, 
                                        temp_attribute_dictionary['gene_symbol'], temp_attribute_dictionary['biotype'], temp_attribute_dictionary['hgnc_id'], 
                                        temp_attribute_dictionary['gene_name'], temp_attribute_dictionary['omim_gene'], ensembl_genes_GRch38_90_ensembl_id,
                                        temp_attribute_dictionary['entity_type'], temp_attribute_dictionary['entity_name'], temp_attribute_dictionary['confidence_level'],    
                                        temp_attribute_dictionary['penetrance'], temp_attribute_dictionary['mode_of_pathogenicity'], temp_attribute_dictionary['publications'], 
                                        temp_attribute_dictionary['evidence'], temp_attribute_dictionary['phenotypes'], temp_attribute_dictionary['mode_of_inheritance'], 
                                        temp_attribute_dictionary['tags'], temp_attribute_dictionary['id'], temp_attribute_dictionary['name'],
                                        temp_attribute_dictionary['disease_group'], temp_attribute_dictionary['disease_sub_group'], temp_attribute_dictionary['status'], 
                                        temp_attribute_dictionary['version'], temp_attribute_dictionary['version_created'], temp_attribute_dictionary['relevant_disorders'], temp_attribute_dictionary['mouseOverField']]
            
            if temp_attribute_dictionary['label'] not in repeat38 and gene_range_38 is not None:    # Remove Repeats
                repeat38.append(temp_attribute_dictionary['label'])
                blockSizes = int(gene_range_38[1]) - int(gene_range_38[0])
                hg38_dict[string_dict_key] = [chromo_38, int(gene_range_38[0]), gene_range_38[1], temp_attribute_dictionary['label'], 
                                        score, strand, gene_range_38[0], gene_range_38[1], rgb, blockCount, blockSizes, blockStarts, 
                                        temp_attribute_dictionary['gene_symbol'], temp_attribute_dictionary['biotype'], temp_attribute_dictionary['hgnc_id'], 
                                        temp_attribute_dictionary['gene_name'], temp_attribute_dictionary['omim_gene'], ensembl_genes_GRch38_90_ensembl_id,
                                        temp_attribute_dictionary['entity_type'], temp_attribute_dictionary['entity_name'], temp_attribute_dictionary['confidence_level'],    
                                        temp_attribute_dictionary['penetrance'], temp_attribute_dictionary['mode_of_pathogenicity'], temp_attribute_dictionary['publications'], 
                                        temp_attribute_dictionary['evidence'], temp_attribute_dictionary['phenotypes'], temp_attribute_dictionary['mode_of_inheritance'], 
                                        temp_attribute_dictionary['tags'], temp_attribute_dictionary['id'], temp_attribute_dictionary['name'],
                                        temp_attribute_dictionary['disease_group'], temp_attribute_dictionary['disease_sub_group'], temp_attribute_dictionary['status'], 
                                        temp_attribute_dictionary['version'], temp_attribute_dictionary['version_created'], temp_attribute_dictionary['relevant_disorders'], temp_attribute_dictionary['mouseOverField']]
                    
            
            count = count + 1
            continuous_count = continuous_count + 1

    print('Genes with missing coordinates in one assembly (written to missing_genes.txt):')
    print(genes_missing_info)

    print('Genes with missing coordinates in both assemblies (written to missing_genes.txt):')
    missSyms = []
    for miss in genes_no_location:
        missSyms.append(miss["gene_symbol"])
    print(",".join(missSyms))

    missOfh = open("missing_genes.txt", "w")
    missOfh.write("* Not found in one assembly:\n")
    missOfh.write("\n".join(genes_missing_info))
    missOfh.write("* No location at all:\n")
    for miss in genes_no_location:
        missOfh.write("\t"+str(miss))
        missOfh.write("\n")
    missOfh.close()

    return(hg19_dict, hg38_dict)

def downloadGenes(url, onlyPanels=None):
    jsonFh = gzip.open("currentJson/genes.json.gz", "w")

    hg19_dict, hg38_dict = getGenesLocations(jsonFh,url, onlyPanels)

    jsonFh.close()
    
    pd_19_table = pd.DataFrame.from_dict(hg19_dict)
    pd_38_table = pd.DataFrame.from_dict(hg38_dict)
    pd_19_table = pd_19_table.T
    pd_38_table = pd_38_table.T
    pd_19_table.columns = ["chrom", "chromStart", 
        "chromEnd", "name", "score", "strand", "thickStart", "thickEnd", "itemRgb",
        "blockCount", "blockSizes", "blockStarts", "Gene Symbol", "Biotype", "HGNC ID",
        "Gene Name", "OMIM Gene", "Ensembl Genes", "Entity Type", "Entity Name", "Confidence Level",
        "Penetranace", "Mode of Pathogenicity", "Publications", "Evidence", "Phenotypes", 
        "Mode of Inheritance", "Tags", "Panel ID", "Panel Name", "Disease Group", "Disease Subgroup", 
        "Status", "Panel Version", "Version Created", "Relevant Disorders", "MouseOverField"]
    pd_38_table.columns = ["chrom", "chromStart", 
        "chromEnd", "name", "score", "strand", "thickStart", "thickEnd", "itemRgb",
        "blockCount", "blockSizes", "blockStarts", "Gene Symbol", "Biotype", "HGNC ID",
        "Gene Name", "OMIM Gene", "Ensembl Genes", "Entity Type", "Entity Name", "Confidence Level",
        "Penetranace", "Mode of Pathogenicity", "Publications", "Evidence", "Phenotypes", 
        "Mode of Inheritance", "Tags", "Panel ID", "Panel Name", "Disease Group", "Disease Subgroup", 
        "Status", "Panel Version", "Version Created", "Relevant Disorders", "MouseOverField"]

    return pd_19_table, pd_38_table


def main():
    " create the 2 x three BED files and convert each to bigBed and update the archive "

    # the script uses relative pathnames, so make sure we're always in the right directory
    os.chdir("/hive/data/outside/otto/panelApp")
    
    # First update panelApp England
    # Gene panels track
    hg19Bed, hg38Bed = downloadGenes("https://panelapp.genomicsengland.co.uk/api/v1/panels/?format=json")
    writeBb(hg19Bed, hg38Bed, "genes")
    # STRs track
    hg19Bed, hg38Bed = downloadTandReps("https://panelapp.genomicsengland.co.uk/api/v1/strs/?format=json")
    writeBb(hg19Bed, hg38Bed, "tandRep")
    # CNV track
    hg38Bed = downloadCnvs('https://panelapp.genomicsengland.co.uk/api/v1/regions/?format=json')
    # no hg19 CNV data yet from PanelApp - still true as of 5/20/2025
    writeBb(None, hg38Bed, "cnv")

    flipFiles('England')
    updateGbdbSymlinks('England')

    # Now update panelApp Australia
    # Genes track
    hg19Bed, hg38Bed = downloadGenes("https://panelapp-aus.org/api/v1/panels/?format=json")
    writeBb(hg19Bed, hg38Bed, "genesAus")
    # STRs track
    hg19Bed, hg38Bed = downloadTandReps("https://panelapp-aus.org/api/v1/strs/?format=json")
    writeBb(hg19Bed, hg38Bed, "tandRepAus")
    # CNVs track
    hg38Bed = downloadCnvs('https://panelapp-aus.org/api/v1/regions/?format=json')
    # no hg19 CNV data yet from PanelApp - still true as of 5/20/2025
    writeBb(None, hg38Bed, "cnvAus")

    flipFiles('Australia')
    updateGbdbSymlinks('Australia')

    print("PanelApp otto update: OK")

main()

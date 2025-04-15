import os
import requests
import time
import json 
import pandas as pd 
import sys 
import argparse
import re
import gzip
import logging

'''
download panelApp data via its API (somewhat slow)
'''

# originally from /cluster/home/bnguy/trackhub/panel/bigBedConversion/final_version/panel_app.py
# Written by a project student, Beagan, in 2020/2021, fixed up by Max

# set to True for debugging
onlyOne = False

def getPanelIds():
    #logging.basicConfig(level=logging.DEBUG)
    logging.basicConfig(level=logging.INFO)
    logging.getLogger("urllib3").propagate = False

    logging.info("Downloading panel IDs")
    panelIds = []

    gotError = False
    url = "https://panelapp.genomicsengland.co.uk/api/v1/panels/?format=json"
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
        if onlyOne:
            break

    return panelIds

def downloadPanels():
    panelIds = getPanelIds()
    panelInfos = {}

    for panelId in panelIds:
        url = "https://panelapp.genomicsengland.co.uk/api/v1/panels/%d?format=json" % panelId
        logging.debug("Getting %s" % url)
        resp = requests.get(url)
        res  = resp.json()

        panelInfos[panelId] = res
        if onlyOne:
            break

    return panelInfos

def getGeneSymbols():
    try:
        panelInfos = downloadPanels()
    except requests.exceptions.JSONDecodeError:
        time.sleep(30)
        panelInfos = downloadPanels()

    syms = set()
    for panelInfo in panelInfos.values():
        for gene in panelInfo["genes"]:
            sym = gene["gene_data"]["gene_symbol"]
            syms.add(sym)
            assert(sym!="")
    logging.info("Got %d gene symbols" % len(syms))
    return list(syms)

def getGenesLocations(jsonFh):
    hg19_dict = dict()
    hg38_dict = dict()
    repeat19 = list()
    repeat38 = list()
    continuous_count = 0
    genes_missing_info = list()
    genes_no_location = list()

    syms = getGeneSymbols()

    for sym in syms:
        url = "https://panelapp.genomicsengland.co.uk/api/v1/genes/?entity_name={}&format=json".format(sym)

        count = 0
        while True:
            try:
                myResponse = requests.get(url)
                if myResponse.ok:
                    break
                else:
                    logging.error("Some error on %s, retrying after 1 minute (trial %d)" % (url, count))
            except Exception:
                logging.error("HTTP error on %s, retrying after 1 minute (trial %d)" % (url, count))

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
                        temp_attribute_dictionary[attribute] = res[count]['panel'][attribute]
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

            # Version Threshold = 0.99
            max_num = float(0.99)
            
            if version_num > max_num: 
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

def downloadGenes():
    jsonFh = gzip.open("currentJson/genes.json.gz", "w")

    hg19_dict, hg38_dict = getGenesLocations(jsonFh)

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
    
    #pd_19_table.to_csv('hg19_header.tsv', sep='\t', index=False)
    #pd_38_table.to_csv('hg38_header.tsv', sep='\t', index=False)

    #pd_19_table.to_csv('hg19_noheadertem.tsv', sep='\t', index=False, header=None) 
    #pd_38_table.to_csv('hg38_noheader.tsv', sep='\t', index=False, header=None) 

    #/usr/local/apache/htdocs-hgdownload/goldenPath/archive/hg38/panelApp/

#if __name__ == "__main__":
    #main()



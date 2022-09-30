#!/usr/bin/env python3
import os
import requests
import json 
import pandas as pd 
import sys 
import argparse
import re
from datetime import date

'''
download panelApp data via its API (somewhat slow) and convert to two bigBed files into the byDay/ directory.
Then create symlinks to them.
'''

# originally from /cluster/home/bnguy/trackhub/panel/bigBedConversion/final_version/panel_app.py
# Written by a project student, Beagan, in 2020/2021

def getGenesLocations():
    page_count = 1
    Error = True
    hg19_dict = dict()
    hg38_dict = dict()
    repeat19 = list()
    repeat38 = list()
    continuous_count = 0
    genes_missing_info = list()
    while Error: 
        url = "https://panelapp.genomicsengland.co.uk/api/v1/genes/?format=json&page={}".format(page_count)
        myResponse = requests.get(url)

        if (myResponse.ok):
            jsonData = myResponse.content.decode()
            jData = json.loads(jsonData)

            if "error" in jData.keys():
                raise Exception("{} page count is missing.".format(page_count))
            
            res = jData['results']
            num_gene_variant = len(res)
            count = 0
            while count != num_gene_variant:
                temp_attribute_dictionary = dict()
                string_dict_key = 'gene_{}'.format(continuous_count)

                try:
                    ensembl_genes_GRch37_82_location = res[count]['gene_data']['ensembl_genes']['GRch37']['82']['location']
                except:
                    print(count)
                    genes_missing_info.append(res[count]['gene_data']['gene_symbol']+"/hg19")
                    count = count + 1

                try:
                    ensembl_genes_GRch38_90_location = res[count]['gene_data']['ensembl_genes']['GRch38']['90']['location']
                except:
                    print(count)
                    genes_missing_info.append(res[count]['gene_data']['gene_symbol']+"/hg38")
                    count = count + 1

                location_37 = ensembl_genes_GRch37_82_location.split(':')
                chromo_37 = 'chr'+location_37[0]
                gene_range_37 = location_37[1].split('-')
        
                location_38 = ensembl_genes_GRch38_90_location.split(':')

                # Change mitochondrial chromosomal suffix from MT -> M, fetchrom recognize only chrM
                chr_num = location_38[0]

                if chr_num == "MT":
                    chr_num = "M"
                chromo_38 = 'chr'+chr_num

                gene_range_38 = location_38[1].split('-')

                score = '0'
                strand = '.'
                blockCount = '1'
                blockSizes = int(gene_range_37[1]) - int(gene_range_37[0])
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
                    print(res[count])

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
                # Add comma separated to list of pub id

                publications = ' '.join(res[count]['publications'])

                if not publications:
                    temp_attribute_dictionary['publications'] = ''
                else:
                    if re.match("^[0-9 ]+$", publications):
                        temp_attribute_dictionary['publications'] = publications.replace(' ', ', ')
                    else:
                        temp_attribute_dictionary['publications'] = publications

                # Remove new lines
                temp_attribute_dictionary['publications'] = temp_attribute_dictionary['publications'].replace("\n", "")

                # make everything a URL, as we have not only PMIDs in here
                # convert numbers to Pubmed URLs
                pubs = temp_attribute_dictionary['publications'].split(", ")
                pubUrls = []
                for pub in pubs:
                    if re.match("^[0-9 ]+$", pub):
                        pubUrls.append("https://pubmed.ncbi.nlm.nih.gov/"+pub)
                    else:
                        pubUrls.append(pub)

                temp_attribute_dictionary['publications'] = ", ".join(pubUrls)

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
                    if temp_attribute_dictionary['label'] not in repeat19:    # Removes Repeats
                        repeat19.append(temp_attribute_dictionary['label'])
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
                    
                    if temp_attribute_dictionary['label'] not in repeat38:    # Remove Repeats
                        repeat38.append(temp_attribute_dictionary['label'])
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
    
        else:
            Error = False        # End of all pages

        page_count = page_count + 1
        print(page_count)
    print('Genes with missing coordinates (written to missing_genes.txt):')
    print(genes_missing_info)
    open("missing_genes.txt", "w").write("\n".join(genes_missing_info))
    return(hg19_dict, hg38_dict)

def main():
    hg19_dict, hg38_dict = getGenesLocations()
    
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
    
    #pd_19_table.to_csv('hg19_header.tsv', sep='\t', index=False)
    #pd_38_table.to_csv('hg38_header.tsv', sep='\t', index=False)

    #pd_19_table.to_csv('hg19_noheadertem.tsv', sep='\t', index=False, header=None) 
    #pd_38_table.to_csv('hg38_noheader.tsv', sep='\t', index=False, header=None) 

    outDir = date.today().strftime("byDay/%Y-%m-%d")
    os.makedirs(outDir)

    outFnames = {}
    outFnames["19"] = outDir+'/hg19_sorted_noheader.tsv'
    outFnames["38"] = outDir+'/hg38_sorted_noheader.tsv'

    outBbs = {}
    outBbs["19"] = outDir+"/panelapp_hg19.bb"
    outBbs["38"] = outDir+"/panelapp_hg38.bb"

    ''' Sort '''
    pd_19_table = pd_19_table.sort_values(by=['chrom','chromStart'], ascending = (True, True))
    pd_19_table.to_csv(outFnames["19"], sep='\t', index=False, header=None) 
    
    pd_38_table = pd_38_table.sort_values(by=['chrom','chromStart'], ascending = (True, True))
    pd_38_table.to_csv(outFnames["38"], sep='\t', index=False, header=None) 

    for db in ["19", "38"]:
        cmd = "bedToBigBed -tab -as=panelapp.as -type=bed9+26 -extraIndex=geneName %s /hive/data/genomes/hg%s/chrom.sizes %s" % (outFnames[db], db, outBbs[db])
        assert(os.system(cmd)==0)

    # make sure that we never end up with a only one updated bb file
    #for db in ["19", "38"]:
        #os.rename("%s.tmp" % outBbs[db], outBbs[db])

    # update the symlinks
    for db in ["19", "38"]:
        cmd = "ln -s %s /gbdb/hg%s/panelApp/genesPanel.bb" % (outBbs[db], db)
        assert(os.system(cmd)==0)

    print("PanelApp otto update: OP")

if __name__ == "__main__":
    main()



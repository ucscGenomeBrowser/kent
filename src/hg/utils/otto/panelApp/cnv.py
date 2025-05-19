import requests
import json 
import pandas as pd 
import sys 
import argparse
import re

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

def downloadCnvs():
    Error = True
    continuous_count=0
    url = "https://panelapp.genomicsengland.co.uk/api/v1/regions/?format=json"
    res = getAllPages(url)

    num_gene_data = len(res)
    print("Got %d CNVs" % num_gene_data)
    count = 0
    continuous_count = 0
    hg19_dict = dict()
    hg38_dict = dict()

    while count != num_gene_data:
        temp_attribute_dictionary = dict()
        string_dict_key = 'gene_{}'.format(continuous_count)
        
        chromo = res[count]['chromosome']
        chromosome = 'chr' + chromo

        start_coordinates = res[count]['grch38_coordinates'][0]
        end_coordinates = res[count]['grch38_coordinates'][1]

        score = '0'
        strand = '.'
        thickStart = start_coordinates
        thickEnd = end_coordinates
        blockCount = '1'
        blockSizes = int(end_coordinates) - int(start_coordinates)
        blockStarts = 0
        
        confidence_level = res[count]['confidence_level']

        rgb_dict = {'0' : '100,100,100', '3': '0,255,0', '2': '255,191,0', '1':'255,0,0'}
        itemRgb = rgb_dict[confidence_level]
        
        entity_name = res[count]['entity_name']
        entity_type = res[count]['entity_type']
        evidence = ' '.join(res[count]['evidence'])

        haploinsufficiency_score = res[count].get('haploinsufficiency_score')
        if not haploinsufficiency_score:
            haploinsufficiency_score = ''

        moi = res[count].get('mode_of_inheritance')
        if not moi:
            moi = ''

        disease_group = res[count]['panel'].get('disease_group')
        if not disease_group:
            disease_group = ''

        disease_sub_group = res[count]['panel'].get('disease_sub_group')
        if not disease_sub_group:
            disease_sub_group = ''

        # idd = Panel ID
        idd = res[count]['panel'].get('id')
        if not idd:
            idd = ''

        panel_name = res[count]['panel'].get('name')
        if not panel_name:
            panel_name = ''
        
        relevant_disorders = ' '.join(res[count]['panel'].get('relevant_disorders', []))
        if not relevant_disorders:
            relevant_disorders = ''

        status = res[count]['panel'].get('status')
        if not status:
            status = ''
        
        '''
        types = res[count]['panel'].get['types')
        if not types:
            types = ''
        else:
            types = str(types).replace("{","").replace("}", "").replace("'", "")
            types = types[1:-1]
        '''

        types = res[count]['panel']['types'][0].get('name')

        version = res[count]['panel']['version']
        if float(version) < 0.99:
            continue
        if not version:
            version = ''

        penetrance = res[count].get('penetrance')
        if not penetrance:
            penetrance = ''

        phenotypes = ' '.join(res[count].get('phenotypes', []))
        if not phenotypes:
            phenotypes = ''

        publications = ' '.join(res[count]['publications'])
        if not publications:
            publications = ''
        
        #required_overlap_percentage = res[count]['required_overlap_percentage']
        tags = res[count]['tags']
        if not tags:
            tags = ''
    
        triplosensitivity_score = res[count].get('triplosensitivity_score')
        if not triplosensitivity_score:
            triplosensitivity_score = ''
    
        type_of_variants = None
        if "type_of_variants" in res[count]:
            type_of_variants = res[count]['type_of_variants']
        if not type_of_variants:
            type_of_variants = ''

        verbose_name = res[count].get('verbose_name')
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
        count = count + 1

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

#print(len(downloadCnvs()))

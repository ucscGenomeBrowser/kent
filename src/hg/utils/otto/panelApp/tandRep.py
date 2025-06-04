import requests
import json 
import pandas as pd 
import sys 
import argparse
import re
import cnv

from cnv import getAllPages

def downloadTandReps():
    Error = True
    continuous_count=0
    url = "https://panelapp.genomicsengland.co.uk/api/v1/strs/?format=json"
    res = getAllPages(url)

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
        alias = ' '.join(gene_data['alias'])
        biotype = gene_data['biotype']
        ensembl_id_37 = gene_data['ensembl_genes']['GRch37']['82']['ensembl_id']
        #ensembl_location_37 = gene_data['ensembl_genes']['GRch37']['82']['location']
        ensembl_id_38 = gene_data['ensembl_genes']['GRch38']['90']['ensembl_id']
        #ensembl_location_38 = gene_data['ensembl_genes']['GRch38']['90']['location']
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

#if __name__ == "__main__":
    #main()

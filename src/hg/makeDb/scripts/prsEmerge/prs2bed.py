# Written by Zia Truong, committed by Max
import pathlib, gzip, csv

#I put the PRS SNP files in a directory called "data"
#iterate through all the filenames in the directory
for filename in map(str, pathlib.Path("data").iterdir()):
    disease = filename.split("_PRS")[0][5:]

    #BED file to write to
    bed_file = open("output/"+disease+".bed", 'w')

    #the PRS file is gzipped, read it as a text file
    prs_file = gzip.open(filename, 'rt')

    #skip header line
    next(prs_file)

    for line in csv.reader(prs_file, delimiter='\t'):
        #compute field values
        effect_allele, weight = line[1], line[2]

        #id contains the chromosome, start position
        id_info = line[0].split(':')

        #make start positions zero-based
        chr, start_pos = id_info[0], int(id_info[1])-1

        #get the reference allele (the allele in the id that isn't the effect allele)
        ref_allele = id_info[2] if effect_allele != id_info[2] else id_info[3]

        #compute 1-based end position
        end_pos = start_pos + len(ref_allele)

        #chr  start  end  id  score  strand  thick_start  thick_end  color  effect_allele  weight
        bed_file.write("\t".join(("chr"+chr, str(start_pos), str(end_pos), line[0], '0', '.', str(start_pos), str(start_pos), "0,0,0", effect_allele, weight))+"\n")

    prs_file.close()
    bed_file.close()

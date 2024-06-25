import sys, csv, gzip
import subprocess

allFiles = {'hg19MaskedIndel': '/hive/data/outside/spliceAi/scoresFromIllumina/spliceai_scores.masked.indel.hg19.vcf.gz',\
           'hg38MaskedIndel': '/hive/data/outside/spliceAi/scoresFromIllumina/spliceai_scores.masked.indel.hg38.vcf.gz',\
           'hg19MaskedSnv': '/hive/data/outside/spliceAi/scoresFromIllumina/spliceai_scores.masked.snv.hg19.vcf.gz',\
           'hg38MaskedSnv': '/hive/data/outside/spliceAi/scoresFromIllumina/spliceai_scores.masked.snv.hg38.vcf.gz',\
           'hg19RawIndel': '/hive/data/outside/spliceAi/scoresFromIllumina/spliceai_scores.raw.indel.hg19.vcf.gz',\
           'hg38RawIndel': '/hive/data/outside/spliceAi/scoresFromIllumina/spliceai_scores.raw.indel.hg38.vcf.gz',\
           'hg19RawSnv': '/hive/data/outside/spliceAi/scoresFromIllumina/spliceai_scores.raw.snv.hg19.vcf.gz',\
           'hg38RawSnv': '/hive/data/outside/spliceAi/scoresFromIllumina/spliceai_scores.raw.snv.hg38.vcf.gz'}

def processAndMakeBedFile(dbsAndMasking,filePath):
    with gzip.open(filePath, 'rt') as f:
        with open('/hive/data/outside/spliceAi/'+dbsAndMasking+'.bed', 'w', newline='', encoding='utf-8') as outfile1:
            AIwriter = csv.writer(outfile1, delimiter='\t')
            atypes = {'acceptor_gain' : '255,0,0', 
              'acceptor_loss' : '255,128,0', 
              'donor_gain' : '0,0,255', 
              'donor_loss' : '212,0,255'}
            for line in f:
                if line.startswith('#'):
                    continue
                [chrom, pos, id, ref, alt, qual, filter, info] = line.strip().split('\t')
                startpos = int(pos) -1
                # match scores with positions
                name = info.split('|')[1]
                scores = [float(s) for s in info.split('|')[2:6]]
                positions = [int(s) for s in info.split('|')[6:10]]
                # Iterate over the zipped data
                for atype, score, position in zip(atypes.keys(), scores, positions):
                    # Check if the score is greater than or equal to 0.02
                    if score >= 0.02:
                        # make clear if position is upstream or downstream
                        if position > 0:
                            position = '+' + str(position)
                  #      print(f"Type: {atype}, Score: {score}, Position: {position}")
                        AIwriter.writerow(['chr'+chrom, startpos, startpos+1, ref+'>'+alt, 0, '+', startpos, startpos, atypes[atype], score, atype, position, name])

def bash(cmd):
    """Run the cmd in bash subprocess"""
    try:
        rawBashOutput = subprocess.run(cmd, check=True, shell=True,\
                                       stdout=subprocess.PIPE, universal_newlines=True, stderr=subprocess.STDOUT)
        bashStdoutt = rawBashOutput.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(e.cmd, e.returncode, e.output))
    return(bashStdoutt)

for file in allFiles:
    processAndMakeBedFile(file,allFiles[file])
    
for track in allFiles:
    trackName = track
    originalFileUrl = allFiles[track]
    bedFile = "/hive/data/outside/spliceAi/"+trackName+".bed"
    if "hg19" in trackName:
        dbs = "hg19"
    else:
        dbs = "hg38"
    if "Masked" in trackName:
        if "Snv" in trackName:
            BBname = "spliceAIsnvsMasked"
        else:
            BBname = "spliceAIindelsMasked"
    else:
        if "Snv" in trackName:
            BBname = "spliceAIsnvs"
        else:
            BBname = "spliceAIindels"
    bash("bedToBigBed -type=bed9+4 -tab -as=/hive/data/outside/spliceAi/spliceAI.as "+bedFile+" \
    /hive/data/genomes/"+dbs+"/chrom.sizes /hive/data/outside/spliceAi/"+dbs+"/"+BBname+".bb")

    bash("ln -sf /hive/data/outside/spliceAi/"+dbs+"/"+BBname+".bb /gbdb/"+dbs+"/bbi/"+BBname+".bb")

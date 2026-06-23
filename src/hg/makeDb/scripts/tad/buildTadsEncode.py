#!/usr/bin/env python3
# Rebuild tadsEncode as a faceted bigBed composite across ALL 117 single-biosample
# ENCODE contact-domain datasets (replaces the old 14-subtrack subGroup composite).
# refs #21599
#
# ENCODE's contact-domain files are heterogeneous: the canonical product is the
# 16-column Juicer/Arrowhead "blocks" bedpe (chr1 x1 x2 chr2 y1 y2 name score
# strand1 strand2 color cornerScore uVarScore lVarScore upSign loSign), but the
# pool also contains 12/11-col bedpe variants, 24-col loop files (different anchors,
# no "chr" prefix), and 5-col hg19-lifted bed. We therefore tier the files and use
# the best available per biosample:
#   tier 16 : 16-col bedpe (full Arrowhead scores)            -> 111 biosamples
#   tier 12 : 12-col bedpe (cornerScore only; others 0)       -> A549
#   tier 11 : 11-col bedpe (no scores; all 0)
#   tier  5 : hg19-lifted bed5 (no scores; all 0)             -> 5 biosamples
# Loop files (anchors differ, x!=y) and non-hg38 chroms are dropped by the parser.
#
# Per biosample: pick one representative experiment (best file tier; preferred_default;
# assay priority intact>in situ>dilution; then total size). Pool that experiment's
# files of the chosen tier (union; drop a domain whose endpoints both fall within one
# 5 kb bin of a higher-cornerScore domain). Write bigBed 4+5 (tadDomainEncode).
# Conversion verified vs the retired tadsEncodeGM12878.bb (scores taken verbatim;
# duplicate resolution keeps the higher cornerScore).

import json, os, re, gzip, subprocess, sys
from collections import defaultdict, Counter

# Assembly-aware: `python3 buildTadsEncode.py [hg38|mm10]` (default hg38, the original human build).
# mm10 is the native mouse build; mm39 is produced by lifting the mm10 bigBeds (see liftEncodeMouse.sh).
ASM = sys.argv[1] if len(sys.argv) > 1 else "hg38"
_HUMAN = "/hive/users/lrnassar/claude/RM21599/encode"
_MOUSE = "/hive/data/outside/tad/encode/mouse"
CFG = {
 "hg38": dict(src=_HUMAN+"/contact_domains",
              manif="/hive/data/outside/tad/encode/source/_cd_select.json",
              meta="/hive/data/outside/tad/encode/source/encode_meta_all.json",
              pert="/hive/data/outside/tad/encode/source/encode_perturbed.json",
              chroms="/hive/data/genomes/hg38/chrom.sizes", out="/hive/data/outside/tad/encode/build/hg38",
              species="human",
              default_on={"GM12878","K562","HepG2","HCT116","IMR-90","A549","HL-60/S4",
                "heart left ventricle","heart right ventricle","dorsolateral prefrontal cortex",
                "ovary","pancreas","transverse colon","motor neuron"}),
 "mm10": dict(src=_MOUSE+"/contact_domains", manif=_MOUSE+"/source/_cd_select.json",
              meta=_MOUSE+"/source/encode_meta_all.json", pert=_MOUSE+"/source/encode_perturbed.json",
              chroms="/hive/data/genomes/mm10/chrom.sizes", out="/hive/data/outside/tad/encode/build/mm10",
              species="mouse",
              default_on={"mouse embryonic stem cell","CH12.LX","heart","left cerebral cortex"}),
}
C = CFG[ASM]
SRC=C["src"]; MANIF=C["manif"]; META=C["meta"]; PERT=C["pert"]; CHROMS=C["chroms"]
SPECIES=C["species"]; DEFAULT_ON=C["default_on"]
COLORS = "/hive/data/outside/tad/organ_colors.json"   # TAD-owned (symlinked into /gbdb/<asm>/bbi/tad/)
ASFILE = "/hive/data/outside/tad/tadDomainEncode.as"
GBDB   = "/gbdb/%s/bbi/tad" % ASM
OUT    = C["out"]
BBDIR  = os.path.join(OUT, "tadsEncode")
TSV    = os.path.join(OUT, "tadsEncode_metadata.tsv")
RA     = os.path.join(OUT, "tadsEncode.ra")
BIN    = 5000

ASSAY_PRIORITY = {"intact Hi-C":0, "in situ Hi-C":1, "dilution Hi-C":2}

SLIM2KEY = {
 'bodily fluid':'Blood','blood':'Blood','epithelium':'Epithelium',
 'heart':'Heart','pericardium':'Heart','lung':'Lung',
 'vasculature':'Blood vessel','blood vessel':'Blood vessel',
 'arterial blood vessel':'Blood vessel','vein':'Blood vessel',
 'colon':'Colon','large intestine':'Large intestine','intestine':'Large intestine',
 'brain':'Brain','spinal cord':'Spinal cord','nerve':'Nerve','eye':'Eye',
 'musculature of body':'Muscle','skin of body':'Skin','limb':'Limb',
 'mammary gland':'Breast','breast':'Breast',
 'embryo':'Embryo','extraembryonic component':'Placenta','placenta':'Placenta',
 'kidney':'Kidney','liver':'Liver','pancreas':'Pancreas','esophagus':'Esophagus','stomach':'Stomach',
 'connective tissue':'Connective tissue','bone marrow':'Bone marrow','bone element':'Bone',
 'prostate gland':'Prostate','uterus':'Uterus','vagina':'Vagina','ovary':'Ovary','testis':'Testis',
 'adrenal gland':'Adrenal gland','thyroid gland':'Thyroid','spleen':'Spleen',
 'immune organ':'Lymphoid tissue','lymph node':'Lymphoid tissue',
 'exocrine gland':'Epithelium','endocrine gland':'Epithelium','gonad':'Epithelium',
}
PRIORITY = ['Heart','Lung','Brain','Liver','Kidney','Pancreas','Prostate','Breast','Ovary','Testis',
 'Uterus','Vagina','Cervix','Stomach','Esophagus','Colon','Large intestine','Small intestine',
 'Spleen','Thyroid','Adrenal gland','Eye','Nerve','Spinal cord','Skin','Blood vessel','Muscle',
 'Bone marrow','Bone','Connective tissue','Placenta','Limb','Embryo','Lymphoid tissue',
 'Blood','Epithelium']
PRANK = {k:i for i,k in enumerate(PRIORITY)}
ORGAN_OVERRIDE = {'GM11168':'Blood','Calu3':'Lung','Ramos':'Blood',
                  'activated CD8-positive, alpha-beta T cell':'Blood',
                  # mouse biosamples with empty organ_slims (else fall back to Epithelium):
                  'CH12F3':'Blood','v-Abl transformed pro-B cells':'Blood',
                  'mouse trophoblast stem cell':'Placenta'}
CLASS_LABEL = {'tissue':'Tissue','cell line':'Cell line','primary cell':'Primary cell',
               'in vitro differentiated cells':'In vitro differentiated'}

def pick_organ(slims, term):
    keys=[SLIM2KEY[s] for s in slims if s in SLIM2KEY]
    return ORGAN_OVERRIDE.get(term,'Epithelium') if not keys else min(keys,key=lambda k:PRANK.get(k,999))

def clean(s):
    s="" if s is None else str(s); return re.sub(r"\s+"," ",s.replace("\t"," ")).strip()
def scap(s):
    """Sentence-case: capitalize the first letter only if the first word is all-lowercase
    (so acronyms/proper tokens like hTERT, GM12878, CD4+, Hi-C are preserved)."""
    if not s: return s
    first=s.split(" ",1)[0]
    if first[:1].isalpha() and first==first.lower(): return s[:1].upper()+s[1:]
    return s
def hex2rgb(h):
    h=h.lstrip("#"); return "%d,%d,%d"%(int(h[0:2],16),int(h[2:4],16),int(h[4:6],16))

# ASCII-safe abbreviations for long ENCODE biosample names, applied before truncation
ENC_ABBR=[(r'CD4-positive','CD4+'),(r'CD8-positive','CD8+'),(r'CD34-positive','CD34+'),
          (r'CD14-positive','CD14+'),(r'thymus-derived ',''),(r', alpha-beta',''),
          (r' myocardium',''),(r'-positive','+'),(r'-negative','-')]
STOP={"the","of","a","an","in","on","to","and","for","with","at","by"}
def shortlab(name, maxlen=22, abbr=None):
    """Concise, sensical shortLabel: abbreviate, truncate only at whole-word boundaries
    (never mid-word), drop a trailing connective word/punctuation."""
    s=name.replace("_"," ")
    for pat,rep in (abbr or []): s=re.sub(pat,rep,s)
    s=re.sub(r"\s+"," ",s).strip()
    if " of " in s.lower():        # "A of B" -> organ-first "B A" (avoids dangling-preposition cuts)
        i=s.lower().find(" of "); head,tail=s[:i].strip(),s[i+4:].strip()
        if head and tail:
            if head[:1].isupper() and head[1:2].islower(): head=head[0].lower()+head[1:]
            s=tail+" "+head
    if len(s)>maxlen:
        out=""
        for t in s.split(" "):
            if len((out+" "+t).strip())<=maxlen: out=(out+" "+t).strip()
            else: break
        s=out if out else s[:maxlen]
    toks=s.split(" ")
    while len(toks)>1 and toks[-1].lower().strip(",;-") in STOP: toks.pop()
    return " ".join(toks).rstrip(" ,;-")

CHROMSZ={l.split('\t')[0]: int(l.split('\t')[1]) for l in open(CHROMS)}

def file_tier(acc, fmt):
    """First domain row's column count -> tier; 0 if not usable."""
    p=os.path.join(SRC, acc+('.bedpe.gz' if fmt=='bedpe' else '.bed.gz'))
    if not os.path.exists(p): return 0,None
    with gzip.open(p,'rt') as fh:
        for line in fh:
            if line.startswith('#'): continue
            c=line.rstrip('\n').split('\t')
            if len(c)>1 and c[1]=='x1': continue
            n=len(c)
            if fmt=='bed' and n>=5: return 5,p
            # exact column counts only: 16-col Arrowhead blocks (full scores), 12/11-col
            # variants. 24-col files are loops (different anchors) -> tier 0, excluded.
            if fmt=='bedpe' and n==16: return 16,p
            if fmt=='bedpe' and n==12: return 12,p
            if fmt=='bedpe' and n==11: return 11,p
            return 0,p
    return 0,p

def parse_domains(path, fmt, tier, name):
    """Yield bed4+5 rows: domain intervals only, chrom-normalized, scores per tier."""
    out=[]
    with gzip.open(path,'rt') as fh:
        for line in fh:
            if line.startswith('#'): continue
            c=line.rstrip('\n').split('\t')
            if len(c)<3 or c[1]=='x1': continue
            ch=c[0] if c[0].startswith('chr') else 'chr'+c[0]
            if ch not in CHROMSZ: continue
            if fmt=='bedpe':
                if len(c)<6: continue
                # domain row: same chrom both anchors, same interval (x==y); else it's a loop
                if c[1]!=c[4] or c[2]!=c[5]: continue
                s,e=c[1],c[2]
                if tier==16: sc=[c[11],c[12],c[13],c[14],c[15]]
                elif tier==12: sc=[c[11],"0","0","0","0"]
                else: sc=["0","0","0","0","0"]
            else:  # bed5
                s,e=c[1],c[2]; sc=["0","0","0","0","0"]
            si,ei=int(s),int(e)
            if si<0 or ei>CHROMSZ[ch] or si>=ei: continue   # drop out-of-bounds/degenerate
            out.append([ch,si,ei,name]+sc+[float(sc[0])])
    return out

def main():
    os.makedirs(BBDIR, exist_ok=True)
    recs=json.load(open(MANIF)); recs=recs if isinstance(recs,list) else recs.get("@graph",recs)
    meta=json.load(open(META)); colors=json.load(open(COLORS))["Organ"]
    pert=json.load(open(PERT))   # experiment accession -> {perturbed, summary}

    bybs=defaultdict(lambda: defaultdict(list))
    for r in recs:
        bo=r.get("biosample_ontology")
        if not isinstance(bo,dict): continue
        if "/aggregate" in str(r.get("dataset","")): continue
        bybs[bo["term_name"]][r["dataset"]].append(r)

    # annotate each file with its tier + path (cache)
    for term,exps in bybs.items():
        for ds,fs in exps.items():
            for f in fs:
                f["_tier"],f["_path"]=file_tier(f["accession"],f["file_format"])

    def exp_rank(ds, efiles):
        besttier=max((f["_tier"] for f in efiles), default=0)
        encsr=ds.strip("/").split("/")[-1]
        perturbed=1 if pert.get(encsr,{}).get("perturbed") else 0  # prefer unperturbed
        has_pref=any(f.get("preferred_default") for f in efiles)
        assay=min((ASSAY_PRIORITY.get(f.get("assay_title"),9) for f in efiles), default=9)
        size=sum(f.get("file_size",0) for f in efiles if f["_tier"]==besttier)
        return (-besttier, perturbed, 0 if has_pref else 1, assay, -size)

    subtracks=[]; fail=[]
    for term,exps in bybs.items():
        exp,efiles=min(exps.items(), key=lambda kv:(exp_rank(kv[0],kv[1]), kv[0]))
        encsr=exp.strip("/").split("/")[-1]
        tier=max(f["_tier"] for f in efiles)
        if tier==0: fail.append((term,"no usable file")); continue
        use=[f for f in efiles if f["_tier"]==tier]
        fmt=use[0]["file_format"]
        assay=scap(sorted(use,key=lambda f:ASSAY_PRIORITY.get(f.get("assay_title"),9))[0].get("assay_title"))
        dterm=scap(term)   # sentence-cased display name (term kept original for logic/lookups)

        rows=[]
        for f in use:
            rows+=parse_domains(f["_path"], fmt, tier, dterm)
        if not rows: fail.append((term,"no domain rows")); continue

        kept=[]; bychrom=defaultdict(list)
        for r in rows: bychrom[r[0]].append(r)
        for ch,rs in bychrom.items():
            rs.sort(key=lambda r:-r[-1]); acc=[]
            for r in rs:
                if any(abs(r[1]-k[1])<=BIN and abs(r[2]-k[2])<=BIN for k in acc): continue
                acc.append(r)
            kept.extend(acc)

        symbol=re.sub("[^A-Za-z0-9]","",term)
        m=meta.get(encsr,{})
        organ=pick_organ(m.get("organ_slims",[]),term)
        if organ not in colors: organ="Epithelium"
        rgb=hex2rgb(colors[organ])
        btype=CLASS_LABEL.get(m.get("classification",""),"Unknown")
        ls=m.get("life_stage",[]); life="Unknown" if not ls else (ls[0].capitalize() if len(ls)==1 else "Mixed")
        lifted=(tier==5)
        calls="Lifted from hg19" if lifted else "Arrowhead (%s)"%ASM
        summary=clean(m.get("summary")) or term

        bb=os.path.join(BBDIR,symbol+".bb"); tmp=os.path.join(BBDIR,symbol+".bed")
        kept.sort(key=lambda r:(r[0],r[1],r[2]))
        with open(tmp,"w") as o:
            for r in kept: o.write("\t".join([r[0],str(r[1]),str(r[2]),r[3],r[4],r[5],r[6],r[7],r[8]])+"\n")
        rc=subprocess.run(["bedToBigBed","-type=bed4+5","-tab","-as="+ASFILE,tmp,CHROMS,bb],
                          stderr=subprocess.PIPE,text=True)
        os.remove(tmp)
        if rc.returncode!=0: fail.append((term,rc.stderr.strip())); continue

        subtracks.append({"symbol":symbol,"term":term,"dterm":dterm,"organ":organ,"rgb":rgb,"encsr":encsr,
            "assay":assay,"btype":btype,"life":life,"calls":calls,
            "summary":summary,"lifted":lifted,"tier":tier,"n":len(kept),
            "on":("on" if term in DEFAULT_ON else "off")})

    if fail:
        print("FAILURES (%d):"%len(fail),file=sys.stderr)
        for t,e in fail: print("  ",t,e,file=sys.stderr)
    print("built %d / %d biosamples"%(len(subtracks),len(bybs)))
    print("default-on:",sum(1 for s in subtracks if s["on"]=="on"))
    print("tiers:",Counter(s["tier"] for s in subtracks))

    # primaryKey = the representative ENCODE experiment accession (linked via subtrackUrls,
    # like the wgEncodeReg4 reference). _Biosample (readable name) and _Description (the full
    # ENCODE biosample summary) are shown in the metadata table but excluded from faceting.
    cols=["Accession","_Biosample","Organ","Biosample_type","Assay","Life_stage","Calls","_Description"]
    with open(TSV,"w") as fh:
        fh.write("\t".join(cols)+"\n")
        for s in sorted(subtracks,key=lambda s:s["encsr"]):
            fh.write("\t".join([s["encsr"],s["dterm"],s["organ"],s["btype"],s["assay"],
                                s["life"],s["calls"],s["summary"]])+"\n")

    with open(RA,"w") as fh:
        fh.write(
"""track tadsEncode
parent tads
priority 2
compositeTrack faceted
shortLabel ENCODE TADs
longLabel ENCODE contact domains (TADs) across %d %s biosamples (Arrowhead/Hi-C)
type bigBed 4 + 5
group regulation
visibility pack
metaDataUrl %s/tadsEncode_metadata.tsv
primaryKey Accession
colorSettingsUrl %s/organ_colors.json
subtrackUrls Accession=https://www.encodeproject.org/experiments/$$/
maxCheckboxes 50
html tadsEncode

""" % (len(subtracks), SPECIES, GBDB, GBDB))
        for s in sorted(subtracks,key=lambda s:(s["on"]!="on", s["term"].lower())):
            short=scap(shortlab(s["term"],22,ENC_ABBR))
            long="ENCODE TADs in %s (%s)"%(s["dterm"],s["encsr"])
            mouse=("Biosample: %s | TAD lifted from hg19 (no Arrowhead score)"%s["dterm"] if s["lifted"]
                   else "Biosample: %s | Arrowhead corner score: $cornerScore"%s["dterm"])
            fh.write(
"""    track tadsEncode_%s
    parent tadsEncode %s
    shortLabel %s
    longLabel %s
    type bigBed 4 + 5
    bigDataUrl %s/tadsEncode/%s.bb
    color %s
    visibility dense
    mouseOver %s

"""%(s["encsr"],s["on"],short,long,GBDB,s["symbol"],s["rgb"],mouse))
    print("wrote %d subtracks -> %s"%(len(subtracks),RA))

if __name__=="__main__":
    main()

#!/usr/bin/env python3
"""
Human Methylation Atlas Track Setup Script
==========================================

This script sets up the Human Methylation Atlas tracks for the UCSC Genome Browser.
It performs two main tasks:
1. Creates symlinks from source data files to /gbdb/{assembly}/dnaMethylationAtlas/
2. Converts bigBed4 summary files to bigBed9 with cell-type-specific RGB colors

Usage:
    python3 humanMethylationAtlas_setup.py [--assembly hg19|hg38|both] [--dry-run]

Requirements:
    - bigBedToBed and bedToBigBed must be in PATH
    - Write access to /gbdb/{assembly}/dnaMethylationAtlas/
    - Write access to source data directories for bigBed9 files

Author: UCSC Genome Browser Group
Date: February 2025
"""

import os
import subprocess
import argparse
import sys
from pathlib import Path

# =============================================================================
# CONFIGURATION
# =============================================================================

# Assembly-specific paths
ASSEMBLIES = {
    'hg38': {
        'source_dir': '/hive/data/outside/humanMethylationAtlas/HumanMethylationAtlasHg38/hg38',
        'gbdb_dir': '/gbdb/hg38/dnaMethylationAtlas',
        'chrom_sizes': '/hive/data/genomes/hg38/chrom.sizes',
    },
    'hg19': {
        'source_dir': '/hive/data/outside/humanMethylationAtlas/HumanMethylationAtlasHg19/hg19',
        'gbdb_dir': '/gbdb/hg19/dnaMethylationAtlas',
        'chrom_sizes': '/hive/data/genomes/hg19/chrom.sizes',
    }
}

# BigBed summary files to convert to bigBed9 with colors
BIGBED_FILES = {
    'Dataset1.bigbed': 'hmaSummaryUnmethylated.bb',  # All unmethylated regions
    'Dataset2.bigbed': 'hmaSummaryPutEnhancers.bb',  # Putative enhancers
    'U250.bigbed': 'hmaSummaryU250.bb',              # Top 250 markers per cell type
}

# Cell type to RGB color mapping (from HTML description page)
# These colors match the signal tracks for visual consistency
CELL_TYPE_COLORS = {
    # Single cell types
    'Adipocytes': '210,180,140',
    'Bladder-Ep': '186,85,211',
    'Blood-B': '255,165,0',
    'Blood-Granul': '255,99,71',
    'Blood-Mono+Macro': '244,164,96',
    'Blood-NK': '255,127,80',
    'Blood-T': '255,140,0',
    'Bone-Osteob': '188,143,143',
    'Breast-Basal-Ep': '219,112,147',
    'Breast-Luminal-Ep': '255,182,193',
    'Colon-Ep': '34,139,34',
    'Colon-Fibro': '85,107,47',
    'Dermal-Fibro': '245,222,179',
    'Endothel': '255,105,180',
    'Epid-Kerat': '222,184,135',
    'Eryth-prog': '205,51,51',
    'Fallopian-Ep': '218,112,214',
    'Gallbladder': '107,142,35',
    'Gastric-Ep': '60,179,113',
    'Head-Neck-Ep': '0,206,209',
    'Heart-Cardio': '220,20,60',
    'Heart-Fibro': '178,34,34',
    'Kidney-Ep': '255,140,105',
    'Liver-Hep': '139,69,19',
    'Lung-Ep-Alveo': '135,206,250',
    'Lung-Ep-Bron': '100,149,237',
    'Neuron': '138,43,226',
    'Oligodend': '148,103,189',
    'Ovary-Ep': '221,160,221',
    'Pancreas-Acinar': '189,183,107',
    'Pancreas-Alpha': '218,165,32',
    'Pancreas-Beta': '255,215,0',
    'Pancreas-Delta': '240,230,140',
    'Pancreas-Duct': '238,232,170',
    'Prostate-Ep': '153,50,204',
    'Skeletal-Musc': '139,0,0',
    'Small-Int-Ep': '46,139,87',
    'Smooth-Musc': '205,92,92',
    'Thyroid-Ep': '72,61,139',

    # Compound cell types (use first cell type's color)
    'Breast-Basal-Ep:Breast-Luminal-Ep': '219,112,147',
    'Colon-Ep:Gastric-Ep:Small-Int-Ep': '34,139,34',
    'Colon-Ep:Small-Int-Ep': '34,139,34',
    'Colon-Fibro:Heart-Fibro': '85,107,47',
    'Fallopian-Ep:Ovary-Ep': '218,112,214',
    'Gastric-Ep:Small-Int-Ep': '60,179,113',
    'Heart-Cardio:Heart-Fibro': '220,20,60',
    'Heart-Cardio:Skeletal-Musc:Smooth-Musc': '220,20,60',
    'Lung-Ep-Alveo:Lung-Ep-Bron': '135,206,250',
    'Neuron:Oligodend': '138,43,226',
    'Pancreas-Alpha:Pancreas-Beta:Pancreas-Delta': '218,165,32',
    'Skeletal-Musc:Smooth-Musc': '139,0,0',
}

# Symlink mappings: source filename -> symlink name
# This maps the original filenames from the data provider to our standardized track IDs
SYMLINK_MAPPINGS = [
    # Neurons
    ('Neuron.bigwig', 'neuronMerged.bw'),
    ('Cerebellum-Neuron-Z000000TB.bigwig', 'cerebNeuron0TB.bw'),
    ('Cortex-Neuron-Z000000TD.bigwig', 'cortexNeuron0TD.bw'),
    ('Cortex-Neuron-Z000000TF.bigwig', 'cortexNeuron0TF.bw'),
    ('Cortex-Neuron-Z0000042F.bigwig', 'cortexNeuron42F.bw'),
    ('Cortex-Neuron-Z0000042H.bigwig', 'cortexNeuron42H.bw'),
    ('Cortex-Neuron-Z0000042J.bigwig', 'cortexNeuron42J.bw'),
    ('Cortex-Neuron-Z0000042K.bigwig', 'cortexNeuron42K.bw'),
    ('Cortex-Neuron-Z0000042M.bigwig', 'cortexNeuron42M.bw'),
    ('Cortex-Neuron-Z0000042P.bigwig', 'cortexNeuron42P.bw'),
    ('Neuron-Z000000TH.bigwig', 'neuron0TH.bw'),

    # Oligodendrocytes
    ('Oligodend.bigwig', 'oligodendMerged.bw'),
    ('Oligodendrocytes-Z000000TK.bigwig', 'oligo0TK.bw'),
    ('Oligodendrocytes-Z0000042E.bigwig', 'oligo42E.bw'),
    ('Oligodendrocytes-Z0000042L.bigwig', 'oligo42L.bw'),
    ('Oligodendrocytes-Z0000042N.bigwig', 'oligo42N.bw'),

    # Heart Cardiomyocytes
    ('Heart-Cardio.bigwig', 'heartCardioMerged.bw'),
    ('Heart-Cardiomyocyte-Z0000044G.bigwig', 'heartCardio44G.bw'),
    ('Heart-Cardiomyocyte-Z0000044K.bigwig', 'heartCardio44K.bw'),
    ('Heart-Cardiomyocyte-Z0000044Q.bigwig', 'heartCardio44Q.bw'),
    ('Heart-Cardiomyocyte-Z0000044R.bigwig', 'heartCardio44R.bw'),

    # Smooth Muscle
    ('Smooth-Musc.bigwig', 'smoothMuscMerged.bw'),
    ('Aorta-Smooth-Muscle-Z0000041U.bigwig', 'aortaSmMusc41U.bw'),
    ('Bladder-Smooth-Muscle-Z0000041Z.bigwig', 'bladderSmMusc41Z.bw'),
    ('Coronary-Artery-Smooth-Muscle-Z00000420.bigwig', 'coronArtSmMusc420.bw'),
    ('Lung-Bronchus-Smooth-Muscle-Z00000421.bigwig', 'lungBronSmMusc421.bw'),
    ('Prostate-Smooth-Muscle-Z0000041Y.bigwig', 'prostSmMusc41Y.bw'),

    # Heart Fibroblasts
    ('Heart-Fibro.bigwig', 'heartFibroMerged.bw'),
    ('Heart-Fibroblasts-Z0000041V.bigwig', 'heartFibro41V.bw'),
    ('Heart-Fibroblasts-Z0000041W.bigwig', 'heartFibro41W.bw'),
    ('Heart-Fibroblasts-Z0000041X.bigwig', 'heartFibro41X.bw'),
    ('Heart-Fibroblasts-Z0000043R.bigwig', 'heartFibro43R.bw'),

    # Skeletal Muscle
    ('Skeletal-Musc.bigwig', 'skelMuscMerged.bw'),
    ('Skeletal-Muscle-Z00000427.bigwig', 'skelMusc427.bw'),
    ('Skeletal-Muscle-Z00000429.bigwig', 'skelMusc429.bw'),

    # Adipocytes
    ('Adipocytes.bigwig', 'adipocytesMerged.bw'),
    ('Adipocytes-Z000000T5.bigwig', 'adipo0T5.bw'),
    ('Adipocytes-Z000000T7.bigwig', 'adipo0T7.bw'),
    ('Adipocytes-Z000000T9.bigwig', 'adipo0T9.bw'),

    # Endothelial
    ('Endothel.bigwig', 'endothelMerged.bw'),
    ('Aorta-Endothel-Z00000422.bigwig', 'aortaEndoth422.bw'),
    ('Aorta-Endothel-Z0000043G.bigwig', 'aortaEndoth43G.bw'),
    ('Kidney-Glomerular-Endothel-Z000000Q5.bigwig', 'kidneyGlomEndoth0Q5.bw'),
    ('Kidney-Glomerular-Endothel-Z00000443.bigwig', 'kidneyGlomEndoth443.bw'),
    ('Kidney-Glomerular-Endothel-Z0000045J.bigwig', 'kidneyGlomEndoth45J.bw'),
    ('Kidney-Tubular-Endothel-Z000000PX.bigwig', 'kidneyTubEndoth0PX.bw'),
    ('Kidney-Tubular-Endothel-Z000000Q3.bigwig', 'kidneyTubEndoth0Q3.bw'),
    ('Kidney-Tubular-Endothel-Z0000042R.bigwig', 'kidneyTubEndoth42R.bw'),
    ('Liver-Endothelium-Z000000RB.bigwig', 'liverEndothium0RB.bw'),
    ('Lung-Alveolar-Endothel-Z000000Q1.bigwig', 'lungAlveoEndoth0Q1.bw'),
    ('Lung-Alveolar-Endothel-Z000000QK.bigwig', 'lungAlveoEndoth0QK.bw'),
    ('Lung-Alveolar-Endothel-Z0000045H.bigwig', 'lungAlveoEndoth45H.bw'),
    ('Pancreas-Endothel-Z0000042D.bigwig', 'pancEndoth42D.bw'),
    ('Pancreas-Endothel-Z0000042X.bigwig', 'pancEndoth42X.bw'),
    ('Pancreas-Endothel-Z00000430.bigwig', 'pancEndoth430.bw'),
    ('Pancreas-Islet-Endothel-Z0000042Y.bigwig', 'pancIsletEndoth42Y.bw'),
    ('Saphenous-Vein-Endothel-Z000000RM.bigwig', 'saphVeinEndoth0RM.bw'),
    ('Saphenous-Vein-Endothel-Z000000S7.bigwig', 'saphVeinEndoth0S7.bw'),
    ('Saphenous-Vein-Endothel-Z000000SB.bigwig', 'saphVeinEndoth0SB.bw'),

    # Blood T Cells
    ('Blood-T.bigwig', 'bloodTMerged.bw'),
    ('Blood-T-CD3-Z000000TV.bigwig', 'bloodTCd30TV.bw'),
    ('Blood-T-CD3-Z000000UP.bigwig', 'bloodTCd30UP.bw'),
    ('Blood-T-CD4-Z000000TT.bigwig', 'bloodTCd40TT.bw'),
    ('Blood-T-CD4-Z000000U7.bigwig', 'bloodTCd40U7.bw'),
    ('Blood-T-CD4-Z000000UM.bigwig', 'bloodTCd40UM.bw'),
    ('Blood-T-CD8-Z000000TR.bigwig', 'bloodTCd80TR.bw'),
    ('Blood-T-CD8-Z000000U5.bigwig', 'bloodTCd80U5.bw'),
    ('Blood-T-CD8-Z000000UK.bigwig', 'bloodTCd80UK.bw'),
    ('Blood-T-CenMem-CD4-Z00000417.bigwig', 'bloodTCenmemCd4417.bw'),
    ('Blood-T-CenMem-CD4-Z0000041D.bigwig', 'bloodTCenmemCd441D.bw'),
    ('Blood-T-CenMem-CD4-Z0000041N.bigwig', 'bloodTCenmemCd441N.bw'),
    ('Blood-T-Eff-CD8-Z00000419.bigwig', 'bloodTEffCd8419.bw'),
    ('Blood-T-Eff-CD8-Z0000041F.bigwig', 'bloodTEffCd841F.bw'),
    ('Blood-T-Eff-CD8-Z0000041Q.bigwig', 'bloodTEffCd841Q.bw'),
    ('Blood-T-EffMem-CD4-Z00000416.bigwig', 'bloodTEffmemCd4416.bw'),
    ('Blood-T-EffMem-CD4-Z0000041C.bigwig', 'bloodTEffmemCd441C.bw'),
    ('Blood-T-EffMem-CD4-Z0000041M.bigwig', 'bloodTEffmemCd441M.bw'),
    ('Blood-T-EffMem-CD8-Z0000041A.bigwig', 'bloodTEffmemCd841A.bw'),
    ('Blood-T-EffMem-CD8-Z0000041G.bigwig', 'bloodTEffmemCd841G.bw'),
    ('Blood-T-Naive-CD4-Z0000041E.bigwig', 'bloodTNaiveCd441E.bw'),
    ('Blood-T-Naive-CD8-Z0000041B.bigwig', 'bloodTNaiveCd841B.bw'),
    ('Blood-T-Naive-CD8-Z0000041H.bigwig', 'bloodTNaiveCd841H.bw'),

    # Blood B Cells
    ('Blood-B.bigwig', 'bloodBMerged.bw'),
    ('Blood-B-Mem-Z0000041J.bigwig', 'bloodBMem41J.bw'),
    ('Blood-B-Mem-Z0000041K.bigwig', 'bloodBMem41K.bw'),
    ('Blood-B-Z000000TX.bigwig', 'bloodB0TX.bw'),
    ('Blood-B-Z000000UB.bigwig', 'bloodB0UB.bw'),
    ('Blood-B-Z000000UR.bigwig', 'bloodB0UR.bw'),

    # Blood NK Cells
    ('Blood-NK.bigwig', 'bloodNkMerged.bw'),
    ('Blood-NK-Z000000TM.bigwig', 'bloodNk0TM.bw'),
    ('Blood-NK-Z000000U1.bigwig', 'bloodNk0U1.bw'),
    ('Blood-NK-Z000000UF.bigwig', 'bloodNk0UF.bw'),

    # Blood Monocytes/Macrophages
    ('Blood-Mono+Macro.bigwig', 'bloodMonoMacroMerged.bw'),
    ('Blood-Monocytes-Z000000TP.bigwig', 'bloodMono0TP.bw'),
    ('Blood-Monocytes-Z000000U3.bigwig', 'bloodMono0U3.bw'),
    ('Blood-Monocytes-Z000000UH.bigwig', 'bloodMono0UH.bw'),
    ('Colon-Macrophages-Z00000444.bigwig', 'colonMacro444.bw'),
    ('Colon-Macrophages-Z00000446.bigwig', 'colonMacro446.bw'),
    ('Liver-Macrophages-Z0000043P.bigwig', 'liverMacro43P.bw'),
    ('Lung-Alveolar-Macrophages-Z00000448.bigwig', 'lungAlveoMacro448.bw'),
    ('Lung-Alveolar-Macrophages-Z0000044C.bigwig', 'lungAlveoMacro44C.bw'),
    ('Lung-Interstitial-Macrophages-Z00000447.bigwig', 'lungInterMacro447.bw'),
    ('Lung-Interstitial-Macrophages-Z0000044D.bigwig', 'lungInterMacro44D.bw'),
    ('Lung-Interstitial-Macrophages-Z0000044E.bigwig', 'lungInterMacro44E.bw'),

    # Blood Granulocytes
    ('Blood-Granul.bigwig', 'bloodGranulMerged.bw'),
    ('Blood-Granulocytes-Z000000TZ.bigwig', 'bloodGran0TZ.bw'),
    ('Blood-Granulocytes-Z000000UD.bigwig', 'bloodGran0UD.bw'),
    ('Blood-Granulocytes-Z000000UT.bigwig', 'bloodGran0UT.bw'),

    # Erythrocyte Progenitors
    ('Eryth-prog.bigwig', 'erythProgMerged.bw'),
    ('Bone_marrow-Erythrocyte_progenitors-Z000000RF.bigwig', 'boneMarrowErythProg0RF.bw'),
    ('Bone_marrow-Erythrocyte_progenitors-Z000000RH.bigwig', 'boneMarrowErythProg0RH.bw'),
    ('Bone_marrow-Erythrocyte_progenitors-Z000000RK.bigwig', 'boneMarrowErythProg0RK.bw'),

    # Head/Neck Epithelium
    ('Head-Neck-Ep.bigwig', 'headNeckEpMerged.bw'),
    ('Esophagus-Epithelial-Z000000PZ.bigwig', 'esophEp0PZ.bw'),
    ('Esophagus-Epithelial-Z00000426.bigwig', 'esophEp426.bw'),
    ('Larynx-Epithelial-Z000000QB.bigwig', 'larynxEp0QB.bw'),
    ('Pharynx-Epithelial-Z0000044A.bigwig', 'pharynxEp44A.bw'),
    ('Tongue-Epithelial-Z000000QV.bigwig', 'tongueEp0QV.bw'),
    ('Tongue-Epithelial-Z00000449.bigwig', 'tongueEp449.bw'),
    ('Tongue-Epithelial-Z0000044F.bigwig', 'tongueEp44F.bw'),
    ('Tongue_base-Epithelial-Z0000044B.bigwig', 'tongueBaseEp44B.bw'),
    ('Tonsil-Palatine-Epithelial-Z000000QF.bigwig', 'tonsilPalatineEp0QF.bw'),
    ('Tonsil-Palatine-Epithelial-Z000000RP.bigwig', 'tonsilPalatineEp0RP.bw'),
    ('Tonsil-Palatine-Epithelial-Z000000RR.bigwig', 'tonsilPalatineEp0RR.bw'),
    ('Tonsil-Pharyngeal-Epithelial-Z000000Q9.bigwig', 'tonsilPharyngealEp0Q9.bw'),
    ('Tonsil-Pharyngeal-Epithelial-Z000000S1.bigwig', 'tonsilPharyngealEp0S1.bw'),

    # Lung Epithelium
    ('Lung-Ep-Bron.bigwig', 'lungBronEpMerged.bw'),
    ('Lung-Bronchus-Epithelial-Z000000QD.bigwig', 'lungBronEp0QD.bw'),
    ('Lung-Bronchus-Epithelial-Z000000RZ.bigwig', 'lungBronEp0RZ.bw'),
    ('Lung-Bronchus-Epithelial-Z000000S5.bigwig', 'lungBronEp0S5.bw'),
    ('Lung-Ep-Alveo.bigwig', 'lungAlveoEpMerged.bw'),
    ('Lung-Alveolar-Epithelial-Z000000T1.bigwig', 'lungAlveoEp0T1.bw'),
    ('Lung-Alveolar-Epithelial-Z000000VC.bigwig', 'lungAlveoEp0VC.bw'),
    ('Lung-Alveolar-Epithelial-Z000000VE.bigwig', 'lungAlveoEp0VE.bw'),
    ('Lung-Pleura-Z0000042B.bigwig', 'lungPleura42B.bw'),

    # Breast Epithelium
    ('Breast-Basal-Ep.bigwig', 'breastBasalEpMerged.bw'),
    ('Breast-Basal-Epithelial-Z000000V6.bigwig', 'breastBasalEp0V6.bw'),
    ('Breast-Basal-Epithelial-Z000000VG.bigwig', 'breastBasalEp0VG.bw'),
    ('Breast-Basal-Epithelial-Z000000VL.bigwig', 'breastBasalEp0VL.bw'),
    ('Breast-Basal-Epithelial-Z0000043E.bigwig', 'breastBasalEp43E.bw'),
    ('Breast-Luminal-Ep.bigwig', 'breastLuminalEpMerged.bw'),
    ('Breast-Luminal-Epithelial-Z000000V2.bigwig', 'breastLumEp0V2.bw'),
    ('Breast-Luminal-Epithelial-Z000000VJ.bigwig', 'breastLumEp0VJ.bw'),
    ('Breast-Luminal-Epithelial-Z000000VN.bigwig', 'breastLumEp0VN.bw'),

    # Pancreas
    ('Pancreas-Alpha.bigwig', 'pancAlphaMerged.bw'),
    ('Pancreas-Alpha-Z00000453.bigwig', 'pancAlpha453.bw'),
    ('Pancreas-Alpha-Z00000456.bigwig', 'pancAlpha456.bw'),
    ('Pancreas-Alpha-Z00000459.bigwig', 'pancAlpha459.bw'),
    ('Pancreas-Beta.bigwig', 'pancBetaMerged.bw'),
    ('Pancreas-Beta-Z00000452.bigwig', 'pancBeta452.bw'),
    ('Pancreas-Beta-Z00000455.bigwig', 'pancBeta455.bw'),
    ('Pancreas-Beta-Z00000458.bigwig', 'pancBeta458.bw'),
    ('Pancreas-Delta.bigwig', 'pancDeltaMerged.bw'),
    ('Pancreas-Delta-Z00000451.bigwig', 'pancDelta451.bw'),
    ('Pancreas-Delta-Z00000454.bigwig', 'pancDelta454.bw'),
    ('Pancreas-Delta-Z00000457.bigwig', 'pancDelta457.bw'),
    ('Pancreas-Acinar.bigwig', 'pancAcinarMerged.bw'),
    ('Pancreas-Acinar-Z000000QX.bigwig', 'pancAcinar0QX.bw'),
    ('Pancreas-Acinar-Z0000043W.bigwig', 'pancAcinar43W.bw'),
    ('Pancreas-Acinar-Z0000043X.bigwig', 'pancAcinar43X.bw'),
    ('Pancreas-Acinar-Z0000043Y.bigwig', 'pancAcinar43Y.bw'),
    ('Pancreas-Duct.bigwig', 'pancDuctMerged.bw'),
    ('Pancreas-Duct-Z000000QZ.bigwig', 'pancDuct0QZ.bw'),
    ('Pancreas-Duct-Z0000043T.bigwig', 'pancDuct43T.bw'),
    ('Pancreas-Duct-Z0000043U.bigwig', 'pancDuct43U.bw'),
    ('Pancreas-Duct-Z0000043V.bigwig', 'pancDuct43V.bw'),

    # Liver
    ('Liver-Hep.bigwig', 'liverHepMerged.bw'),
    ('Liver-Hepatocytes-Z000000R3.bigwig', 'liverHep0R3.bw'),
    ('Liver-Hepatocytes-Z000000T3.bigwig', 'liverHep0T3.bw'),
    ('Liver-Hepatocytes-Z00000431.bigwig', 'liverHep431.bw'),
    ('Liver-Hepatocytes-Z0000043Q.bigwig', 'liverHep43Q.bw'),
    ('Liver-Hepatocytes-Z0000044H.bigwig', 'liverHep44H.bw'),
    ('Liver-Hepatocytes-Z0000044M.bigwig', 'liverHep44M.bw'),

    # Kidney
    ('Kidney-Ep.bigwig', 'kidneyEpMerged.bw'),
    ('Kidney-Glomerular-Epithelial-Z0000045K.bigwig', 'kidneyGlomEp45K.bw'),
    ('Kidney-Glomerular-Epithelial-Z0000045L.bigwig', 'kidneyGlomEp45L.bw'),
    ('Kidney-Glomerular-Podocytes-Z0000042W.bigwig', 'kidneyGlomPodo42W.bw'),
    ('Kidney-Glomerular-Podocytes-Z00000441.bigwig', 'kidneyGlomPodo441.bw'),
    ('Kidney-Glomerular-Podocytes-Z00000442.bigwig', 'kidneyGlomPodo442.bw'),
    ('Kidney-Tubular-Epithelial-Z000000QH.bigwig', 'kidneyTubEp0QH.bw'),
    ('Kidney-Tubular-Epithelial-Z0000043Z.bigwig', 'kidneyTubEp43Z.bw'),
    ('Kidney-Tubular-Epithelial-Z00000440.bigwig', 'kidneyTubEp440.bw'),

    # Gastric
    ('Gastric-Ep.bigwig', 'gastricEpMerged.bw'),
    ('Gastric-antrum-Endocrine-Z00000437.bigwig', 'gastAntEndo437.bw'),
    ('Gastric-antrum-Endocrine-Z00000438.bigwig', 'gastAntEndo438.bw'),
    ('Gastric-antrum-Epithelial-Z000000SF.bigwig', 'gastAntEp0SF.bw'),
    ('Gastric-antrum-Epithelial-Z000000SP.bigwig', 'gastAntEp0SP.bw'),
    ('Gastric-antrum-Epithelial-Z000000SR.bigwig', 'gastAntEp0SR.bw'),
    ('Gastric-body-Epithelial-Z000000SD.bigwig', 'gastBodyEp0SD.bw'),
    ('Gastric-body-Epithelial-Z000000SM.bigwig', 'gastBodyEp0SM.bw'),
    ('Gastric-body-Epithelial-Z000000ST.bigwig', 'gastBodyEp0ST.bw'),
    ('Gastric-fundus-Epithelial-Z000000RX.bigwig', 'gastFundEp0RX.bw'),
    ('Gastric-fundus-Epithelial-Z000000SK.bigwig', 'gastFundEp0SK.bw'),
    ('Gastric-fundus-Epithelial-Z000000SV.bigwig', 'gastFundEp0SV.bw'),

    # Small Intestine
    ('Small-Int-Ep.bigwig', 'smallIntEpMerged.bw'),
    ('Small-int-Endocrine-Z00000436.bigwig', 'smIntEndo436.bw'),
    ('Small-int-Epithelial-Z000000RT.bigwig', 'smIntEp0RT.bw'),
    ('Small-int-Epithelial-Z000000UW.bigwig', 'smIntEp0UW.bw'),
    ('Small-int-Epithelial-Z000000UY.bigwig', 'smIntEp0UY.bw'),
    ('Small-int-Epithelial-Z0000042V.bigwig', 'smIntEp42V.bw'),

    # Colon
    ('Colon-Ep.bigwig', 'colonEpMerged.bw'),
    ('Colon-Left-Endocrine-Z0000044J.bigwig', 'colonLeftEndo44J.bw'),
    ('Colon-Left-Endocrine-Z0000044T.bigwig', 'colonLeftEndo44T.bw'),
    ('Colon-Left-Epithelial-Z000000VA.bigwig', 'colonLeftEp0VA.bw'),
    ('Colon-Left-Epithelial-Z0000043B.bigwig', 'colonLeftEp43B.bw'),
    ('Colon-Left-Epithelial-Z0000043C.bigwig', 'colonLeftEp43C.bw'),
    ('Colon-Right-Endocrine-Z0000044S.bigwig', 'colonRightEndo44S.bw'),
    ('Colon-Right-Epithelial-Z000000V0.bigwig', 'colonRightEp0V0.bw'),
    ('Colon-Right-Epithelial-Z000000V8.bigwig', 'colonRightEp0V8.bw'),

    # Bladder
    ('Bladder-Ep.bigwig', 'bladderEpMerged.bw'),
    ('Bladder-Epithelial-Z000000QM.bigwig', 'bladderEp0QM.bw'),
    ('Bladder-Epithelial-Z000000QP.bigwig', 'bladderEp0QP.bw'),
    ('Bladder-Epithelial-Z0000043F.bigwig', 'bladderEp43F.bw'),
    ('Bladder-Epithelial-Z0000044L.bigwig', 'bladderEp44L.bw'),
    ('Bladder-Epithelial-Z00000450.bigwig', 'bladderEp450.bw'),

    # Prostate
    ('Prostate-Ep.bigwig', 'prostateEpMerged.bw'),
    ('Prostate-Epithelial-Z000000RV.bigwig', 'prostEp0RV.bw'),
    ('Prostate-Epithelial-Z000000S3.bigwig', 'prostEp0S3.bw'),
    ('Prostate-Epithelial-Z0000045F.bigwig', 'prostEp45F.bw'),
    ('Prostate-Epithelial-Z0000045G.bigwig', 'prostEp45G.bw'),

    # Female Reproductive
    ('Fallopian-Ep.bigwig', 'fallopianEpMerged.bw'),
    ('Fallopian-Epithelial-Z000000Q7.bigwig', 'fallopEp0Q7.bw'),
    ('Fallopian-Epithelial-Z000000S9.bigwig', 'fallopEp0S9.bw'),
    ('Fallopian-Epithelial-Z000000UV.bigwig', 'fallopEp0UV.bw'),
    ('Ovary-Ep.bigwig', 'ovaryEpMerged.bw'),
    ('Endometrium-Epithelial-Z00000434.bigwig', 'endomEp434.bw'),
    ('Endometrium-Epithelial-Z00000435.bigwig', 'endomEp435.bw'),
    ('Endometrium-Epithelial-Z0000043S.bigwig', 'endomEp43S.bw'),
    ('Ovary-Epithelial-Z000000QT.bigwig', 'ovaryEp0QT.bw'),

    # Skin
    ('Dermal-Fibro.bigwig', 'dermalFibroMerged.bw'),
    ('Dermal-Fibroblasts-Z00000423.bigwig', 'dermFibro423.bw'),
    ('Epid-Kerat.bigwig', 'epidKeratMerged.bw'),
    ('Epidermal-Keratinocytes-Z00000424.bigwig', 'epidKerat424.bw'),

    # Other
    ('Gallbladder.bigwig', 'gallbladderMerged.bw'),
    ('Gallbladder-Epithelial-Z00000432.bigwig', 'gallbladEp432.bw'),
    ('Colon-Fibro.bigwig', 'colonFibroMerged.bw'),
    ('Colon-Fibroblasts-Z0000042A.bigwig', 'colonFibro42A.bw'),
    ('Colon-Fibroblasts-Z0000042C.bigwig', 'colonFibro42C.bw'),
    ('Thyroid-Ep.bigwig', 'thyroidEpMerged.bw'),
    ('Thyroid-Epithelial-Z0000042S.bigwig', 'thyroidEp42S.bw'),
    ('Thyroid-Epithelial-Z0000042T.bigwig', 'thyroidEp42T.bw'),
    ('Thyroid-Epithelial-Z0000042U.bigwig', 'thyroidEp42U.bw'),
    ('Bone-Osteob.bigwig', 'boneOsteobMerged.bw'),
    ('Bone-Osteoblasts-Z0000042Z.bigwig', 'boneOsteob42Z.bw'),
]


# =============================================================================
# FUNCTIONS
# =============================================================================

def run_command(cmd, dry_run=False):
    """Execute a shell command."""
    if dry_run:
        print(f"  [DRY-RUN] {cmd}")
        return True
    try:
        subprocess.run(cmd, shell=True, check=True, capture_output=True, text=True)
        return True
    except subprocess.CalledProcessError as e:
        print(f"  ERROR: {e.stderr}")
        return False


def create_symlinks(assembly, dry_run=False):
    """Create symlinks for bigWig signal tracks."""
    config = ASSEMBLIES[assembly]
    source_dir = config['source_dir']
    gbdb_dir = config['gbdb_dir']

    print(f"\n{'='*60}")
    print(f"Creating symlinks for {assembly}")
    print(f"{'='*60}")
    print(f"Source: {source_dir}")
    print(f"Target: {gbdb_dir}")

    # Ensure target directory exists
    if not dry_run:
        os.makedirs(gbdb_dir, exist_ok=True)

    created = 0
    skipped = 0
    errors = 0

    for source_file, link_name in SYMLINK_MAPPINGS:
        source_path = os.path.join(source_dir, source_file)
        link_path = os.path.join(gbdb_dir, link_name)

        # Check if source exists
        if not os.path.exists(source_path):
            print(f"  WARNING: Source file not found: {source_file}")
            errors += 1
            continue

        # Create symlink
        if os.path.exists(link_path) or os.path.islink(link_path):
            if not dry_run:
                os.remove(link_path)

        cmd = f"ln -sf {source_path} {link_path}"
        if run_command(cmd, dry_run):
            created += 1
        else:
            errors += 1

    print(f"\nSymlinks: {created} created, {skipped} skipped, {errors} errors")
    return errors == 0


def convert_bed4_to_bed9(input_file, output_file, dry_run=False):
    """Convert a BED4 file to BED9 with RGB colors based on cell type."""
    print(f"  Converting: {os.path.basename(input_file)}")

    if dry_run:
        print(f"    [DRY-RUN] Would convert {input_file} to {output_file}")
        return None, 0

    try:
        # Read BED4, add BED9 columns
        with subprocess.Popen(
            ['bigBedToBed', input_file, '/dev/stdout'],
            stdout=subprocess.PIPE,
            text=True
        ) as proc:
            lines = []
            unknown_types = set()

            for line in proc.stdout:
                fields = line.strip().split('\t')
                if len(fields) < 4:
                    continue

                chrom, start, end, name = fields[0], fields[1], fields[2], fields[3]

                # Get color for cell type
                if name in CELL_TYPE_COLORS:
                    rgb = CELL_TYPE_COLORS[name]
                else:
                    rgb = '128,128,128'  # Gray for unknown
                    unknown_types.add(name)

                # BED9: chrom, start, end, name, score, strand, thickStart, thickEnd, itemRgb
                bed9_line = f"{chrom}\t{start}\t{end}\t{name}\t0\t.\t{start}\t{end}\t{rgb}"
                lines.append((chrom, int(start), bed9_line))

            if unknown_types:
                print(f"    WARNING: Unknown cell types: {unknown_types}")

        # Sort by chrom, start
        lines.sort(key=lambda x: (x[0], x[1]))

        # Write sorted BED9
        temp_bed = output_file + '.tmp.bed'
        with open(temp_bed, 'w') as f:
            for _, _, line in lines:
                f.write(line + '\n')

        return temp_bed, len(lines)

    except Exception as e:
        print(f"    ERROR: {e}")
        return None, 0


def create_bigbed9(assembly, dry_run=False):
    """Convert bigBed4 files to bigBed9 with colors."""
    config = ASSEMBLIES[assembly]
    source_dir = config['source_dir']
    gbdb_dir = config['gbdb_dir']
    chrom_sizes = config['chrom_sizes']

    print(f"\n{'='*60}")
    print(f"Converting bigBed files for {assembly}")
    print(f"{'='*60}")

    for source_name, link_name in BIGBED_FILES.items():
        source_path = os.path.join(source_dir, source_name)
        output_name = source_name.replace('.bigbed', '.bed9.bigbed')
        output_path = os.path.join(source_dir, output_name)
        link_path = os.path.join(gbdb_dir, link_name)

        print(f"\nProcessing: {source_name}")

        if not os.path.exists(source_path):
            print(f"  ERROR: Source file not found: {source_path}")
            continue

        # Convert to BED9
        temp_bed, count = convert_bed4_to_bed9(source_path, output_path, dry_run)

        if dry_run:
            continue

        if temp_bed is None:
            continue

        print(f"    Converted {count} records")

        # Create bigBed9
        cmd = f"bedToBigBed -type=bed9 {temp_bed} {chrom_sizes} {output_path}"
        print(f"    Creating bigBed9...")
        if run_command(cmd):
            print(f"    Created: {output_path}")

            # Update symlink
            if os.path.exists(link_path) or os.path.islink(link_path):
                os.remove(link_path)
            os.symlink(output_path, link_path)
            print(f"    Symlink: {link_path}")

        # Cleanup temp file
        if os.path.exists(temp_bed):
            os.remove(temp_bed)

    return True


def main():
    parser = argparse.ArgumentParser(
        description='Set up Human Methylation Atlas tracks for UCSC Genome Browser',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument(
        '--assembly',
        choices=['hg19', 'hg38', 'both'],
        default='both',
        help='Which assembly to process (default: both)'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would be done without making changes'
    )
    parser.add_argument(
        '--symlinks-only',
        action='store_true',
        help='Only create symlinks, skip bigBed conversion'
    )
    parser.add_argument(
        '--bigbed-only',
        action='store_true',
        help='Only convert bigBed files, skip symlinks'
    )

    args = parser.parse_args()

    if args.dry_run:
        print("\n*** DRY RUN MODE - No changes will be made ***\n")

    assemblies = ['hg19', 'hg38'] if args.assembly == 'both' else [args.assembly]

    for assembly in assemblies:
        if not args.bigbed_only:
            create_symlinks(assembly, args.dry_run)

        if not args.symlinks_only:
            create_bigbed9(assembly, args.dry_run)

    print("\n" + "="*60)
    print("Done!")
    print("="*60)


if __name__ == '__main__':
    main()

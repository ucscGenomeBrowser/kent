#!/bin/csh

genePredToGtf             hg16 refFlat refFlatDefault.out
genePredToGtf -order      hg16 refFlat refFlatOrder.out
genePredToGtf -order -utr hg16 refFlat refFlatOrderUtr.out
genePredToGtf        -utr hg16 refFlat refFlatUtr.out

genePredToGtf             hg16 refGene refGeneDefault.out
genePredToGtf -order      hg16 refGene refGeneOrder.out
genePredToGtf -order -utr hg16 refGene refGeneOrderUtr.out
genePredToGtf        -utr hg16 refGene refGeneUtr.out

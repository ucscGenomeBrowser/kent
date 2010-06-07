package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.util.*;

/** 
 * This container holds the rows from the knownGene table. 
 */
public class KnownGene {

  // data
  public String name;
  public String chrom;
  public int txStart;
  public int txEnd;
  public int cdsStart;
  public int cdsEnd;
  public int exonCount;
  public String proteinID;
  public String alignID;

  // constructors

 /**
  * Constructor holds only KnownGene names
  *
  * @param nameVar      Gene name
  */
  public KnownGene(String nameVar) {
    name = nameVar;
  }


 /**
  * Constructor holds Known Gene name and chrom location
  *
  * @param nameVar      Gene name
  * @param chromVar     Chromosome number: chrN
  */
  public KnownGene(String nameVar, String chromVar) {
    name = nameVar;
    chrom = chromVar;
  }


 /**
  * Constructor holds Known Gene name, chrom location and proteinID
  *
  * @param nameVar      Gene name
  * @param chromVar     Chromosome number: chrN
  * @param proteinIdVar Protein ID for this gene
  */
  public KnownGene(String nameVar, String chromVar, String proteinIdVar) {
    name = nameVar;
    chrom = chromVar;
    proteinID = proteinIdVar;
  }


 /**
  * Constructor holds 4 variables -- name and location info
  *
  * @param nameVar      Gene name
  * @param chromVar     Chromosome number: chrN
  * @param txStartVar   Transcrition start
  * @param txEndVar     Transcrition end
  */
  public KnownGene(String nameVar, String chromVar, int txStartVar, int txEndVar) {

    name = nameVar;
    chrom = chromVar;
    txStart = txStartVar;
    txEnd = txEndVar;
  }

 /**
  * Constructor holds 5 variables -- name, location (3 variables) and proteinID.
  *    Used in proteome browser.
  *
  * @param nameVar      Gene name
  * @param chromVar     Chromosome number: chrN
  * @param txStartVar   Transcrition start
  * @param txEndVar     Transcrition end
  * @param proteinIdVar Protein ID for this gene
  */
  public KnownGene(String nameVar, String chromVar, int txStartVar, int txEndVar, 
            String proteinIdVar) {

    name = nameVar;
    chrom = chromVar;
    txStart = txStartVar;
    txEnd = txEndVar;
    proteinID = proteinIdVar;
  }

 /**
  * Checks validity of some parameters of Known Gene object 
  *
  * @return  False if chrom is null or txStart is negative
  */
  public boolean validate() {
    if (chrom == null) return false;
    if (txStart <= 0) return false;
    return true;
  }
}

package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.util.*;

/* This container will hold the rows from the liftOverTestCase table */

public class EncodeIndels {

  // data
  public int bin;
  public String chrom;
  public int chromStart;
  public int chromEnd;
  public String name;
  public int score;
  public String strand;
  public int thickStart;
  public int thickEnd;
  public int reserved;
  public String traceName;
  public String traceId;
  public int tracePos;
  public String traceStrand;
  public String variant;
  public String reference;

  // constructors
  public EncodeIndels(int binVar, String chromVar, 
            int chromStartVar, int chromEndVar,
	    String nameVar, int scoreVar,
	    String strandVar, 
	    int thickStartVar, int thickEndVar,
	    int reservedVar,
	    String traceNameVar, String traceIdVar,
	    int tracePosVar, String traceStrandVar,
	    String variantVar, String referenceVar) {

      bin = binVar;
      chrom = chromVar;
      chromStart = chromStartVar; 
      chromEnd = chromEndVar;
      name = nameVar; 
      score = scoreVar;
      strand = strandVar; 
      thickStart = thickStartVar; 
      thickEnd = thickEndVar;
      reserved = reservedVar;
      traceName = traceNameVar; 
      traceId = traceIdVar;
      tracePosVar = tracePosVar;
      traceStrand = traceStrandVar;
      variant = variantVar; 
      reference = referenceVar;
  }

}

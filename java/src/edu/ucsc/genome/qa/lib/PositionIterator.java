package edu.ucsc.genome.qa.lib;
import java.util.ArrayList;

/**
 *  This container keeps track of positions to check
 */
public class PositionIterator {

  // constants
  static final String MODE_ALL = "all";
  static final String MODE_FIRST = "first";
  static final String MODE_MIDDLE = "middle";
  static final String MODE_END = "end";

  // data
  ArrayList chromList;
  int index;
  int max;
  String mode;
  String defaultPos;

  // constructors
 /**
  *  Constructor for stepping through all chromosomes.
  *  
  *  @param  chroms    List of possible chromosome names for this assembly.
  */
  public PositionIterator(ArrayList chroms) {

    this.chromList = chroms;
    this.index = 0;
    this.max = chromList.size();
    // this.mode = "all";  v changed this line -- kuhn v
    this.mode = MODE_ALL;
    this.defaultPos = "";
  }

 /**
  *  Constructor sets default position.
  *  
  *  @param  defaultPos  The default position for current assembly.
  */
  public PositionIterator(String defaultPos) {

    this.chromList = null;
    this.index = 0;
    this.max = 1;
    this.mode = "default";
    this.defaultPos = defaultPos;
  }

 /**
  *  Gets position on next chromosome.  Steps through each chromosome.
  *    Gets a beginning position (unless default).  
  *    Set at 1 - 10K at the moment.
  *  
  *  @return  The position in chrN:nnnnnn-nnnnnn format
  */
  public Position getNextPosition() {
    // if (!hasNext()) assert;
    Position mypos;

    if (mode.equals("default")) {
      mypos = new Position(defaultPos);
    } else {
      System.out.println("Reading from chromList");
      ChromInfo mychrom = (ChromInfo) chromList.get(index);
      String chrom = mychrom.chrom;
      System.out.println("chrom = " + chrom);
      mypos = new Position(chrom, 1, 10000);
    }
    this.index++;
    return mypos;
  }

  // check that index doesn't exceed maximum
 /**
  *  Checks that index doesn't exceed maximum.
  *  
  *  @return  True if position is within chromosome.
  */
  public boolean hasNext() {
    if (this.index < this.max) return true;
    return false;
  }
}

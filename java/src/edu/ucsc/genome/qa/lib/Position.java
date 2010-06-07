package edu.ucsc.genome.qa.lib;
/**
 *  This container holds a position.  Format will be chrN:nnnnn-nnnnn.
 */
public class Position {

  // data
  public String chromName;
  public int startPos;
  public int endPos;
  public String stringVal;
  public String format;

  // constructors
 /**
  *  Constructor creates Position object.
  *
  *  @param  newChromName  The chromosome name, e.g., chrN
  *  @param  newStartPos   
  *  @param  newEndPos
  */
  public Position(String newChromName, int newStartPos, int newEndPos) {

    this.chromName = newChromName;
    this.startPos = newStartPos;
    this.endPos = newEndPos;
    format = "full";
  }

 /**
  *  Constructor creates a Position object from preformed string.
  *  @param  posString   Preformed position coordinates.
  */
  public Position(String posString) {
    stringVal = posString;
    format = "string";
  }
}

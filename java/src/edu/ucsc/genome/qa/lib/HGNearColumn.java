package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.util.*;

/** 
 * This container holds the columns defined for hgNear
 */
public class HGNearColumn {

  // data
  String name;
  String shortLabel;
  String longLabel;
  int priority;
  String visibility;
  String type;
  String search;
  String searchLabel;
  String itemUrl;
  String protKey;
  String queryFull;
  String queryOne;
  String invQueryOne;
  String colWidth;
  String goaIdColumn;
  String noKeys;

  // constructors

 /**
  * Constructor 
  *
  * @param nameVar      Gene name
  */
  public HGNearColumn(String nameVar) {
    name = nameVar;
  }
}

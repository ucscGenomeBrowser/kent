import java.io.*;

/* Container for chrom, start, end */

class Coords {

  // data
  String chrom;
  int start;
  int end;

  // constructors
  Coords(String chromVar, int startVar, int endVar) {

    chrom = chromVar;
    start = startVar;
    end = endVar;
  }

  Coords(String filename) {
    FileReader fin;
    BufferedReader br;
    String thisline = "";
    int pos = 0;
    Integer myint;

    try {
      fin = new FileReader(filename);
      br = new BufferedReader(fin);
      // only need to read the first line
      thisline = br.readLine();
      fin.close();
    } catch (Exception e) {
      System.out.println(e.getMessage());
      chrom = "chr1";
      start = 1;
      end = 1;
      return;
    }
    pos = thisline.indexOf("\t");
    chrom = thisline.substring(0, pos);
    thisline = thisline.substring(pos+1);
    pos = thisline.indexOf("\t");
    String startstr = thisline.substring(0, pos);
    myint = new Integer(startstr);
    start = myint.intValue();
    thisline = thisline.substring(pos+1);
    myint = new Integer(thisline);
    end = myint.intValue();
  }

  boolean match(String newChrom, int newStart, int newEnd) {
    if (!chrom.equals(newChrom)) return (false);
    if (start != newStart) return (false);
    if (end != newEnd) return (false);
    return (true);
  }

  void print() {
    System.out.println(chrom + ":" + start + "-" + end);
  }

}

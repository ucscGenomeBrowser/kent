package edu.ucsc.genome.qa.lib;
import java.util.ArrayList;

/**
 *   This container holds track control values.
 *   But what are the values ??
 */
public class TrackControl {

  // data
  public ArrayList tracks;
  public ArrayList values;

  // constructors
 /**
  *   Constructor creates object with tracks and values.
  */
  public TrackControl(ArrayList newTracks, ArrayList newValues) {

    this.tracks = newTracks;
    this.values = newValues;
  }
}

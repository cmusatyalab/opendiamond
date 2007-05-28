package edu.cmu.cs.diamond.opendiamond;

public interface SearchEventListener {
    void searchStarted(SearchEvent e);
    
    void searchStopped(SearchEvent e);
}

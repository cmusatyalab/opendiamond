package edu.cmu.cs.diamond.opendiamond;

import java.util.EventObject;

public class SearchEvent extends EventObject {
    public SearchEvent(Search source) {
        super(source);
    }

    public Search getSearch() {
        return (Search) source;
    }
}

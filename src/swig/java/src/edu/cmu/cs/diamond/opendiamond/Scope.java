package edu.cmu.cs.diamond.opendiamond;

import edu.cmu.cs.diamond.opendiamond.glue.groupidArray;

public abstract class Scope {

    protected final groupidArray gids;
    protected final int gidsSize;

    protected Scope(groupidArray gids, int gidsSize) {
        this.gids = gids;
        this.gidsSize = gidsSize;
    }
    
    groupidArray getGids() {
        return gids;
    }

    int getGidsSize() {
        return gidsSize;
    }

}

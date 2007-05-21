package edu.cmu.cs.diamond.opendiamond;

import edu.cmu.cs.diamond.opendiamond.glue.groupidArray;

public class NameScope extends Scope {
    private final String name;

    NameScope(String name, groupidArray gids, int size) {
        super(gids, size);
        this.name = name;
    }
    
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder("scope: " + name + " [");
        for (int i = 0; i < gidsSize; i++) {
            sb.append(" " + gids.getitem(i));
        }
        sb.append(" ]");
        return sb.toString();
    }

    @Override
    public String getName() {
        return name;
    }
}

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
        StringBuilder sb = new StringBuilder("scope: " + name + " (gids: "
                + getGidsSize() + ")");
        return sb.toString();
    }

    @Override
    public String getName() {
        return name;
    }
}

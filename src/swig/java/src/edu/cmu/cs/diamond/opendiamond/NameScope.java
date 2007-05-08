package edu.cmu.cs.diamond.opendiamond;

public class NameScope extends Scope {
    private final String name;
    
    public NameScope(String name) {
        this.name = name;
    }
    
    @Override
    public String toString() {
        return "scope: " + name;
    }
}

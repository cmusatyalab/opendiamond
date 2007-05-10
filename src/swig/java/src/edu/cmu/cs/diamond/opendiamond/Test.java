package edu.cmu.cs.diamond.opendiamond;

import java.util.List;

public class Test {

    /**
     * @param args
     */
    public static void main(String[] args) {
        // get scopes
        List<Scope> scopes = ScopeSource.getPredefinedScopeList();
        for (Scope scope : scopes) {
            System.out.println(scope);
        }

        // use first scope
        Scope s = scopes.get(0);
        
    }
}

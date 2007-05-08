package edu.cmu.cs.diamond.opendiamond;

import java.util.List;

public class Test {

    /**
     * @param args
     */
    public static void main(String[] args) {
        List<Scope> scopes = ScopeSource.getPredefinedScopeList();
        for (Scope scope : scopes) {
            System.out.println(scope);
        }
    }

}

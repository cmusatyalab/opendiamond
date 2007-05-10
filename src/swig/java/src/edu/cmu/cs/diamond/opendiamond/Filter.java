package edu.cmu.cs.diamond.opendiamond;

public class Filter {
    final private FilterCode code;

    final private String[] dependencies;

    final private String[] arguments;

    final private String evalFunction;

    final private String finiFunction;

    final private String initFunction;

    final private int merit;

    final private String name;

    final private int threshold;

    public Filter(String name, FilterCode code, String evalFunction,
            String initFunction, String finiFunction, int threshold,
            String dependencies[], String arguments[], int merit) {

        // TODO check for valid characters as in filter_spec.l
        this.name = name;
        this.code = code;
        this.evalFunction = evalFunction;
        this.initFunction = initFunction;
        this.finiFunction = finiFunction;
        this.threshold = threshold;
        this.merit = merit;

        this.dependencies = new String[dependencies.length];
        System.arraycopy(dependencies, 0, this.dependencies, 0,
                dependencies.length);

        this.arguments = new String[arguments.length];
        System.arraycopy(arguments, 0, this.arguments, 0, arguments.length);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();

        sb.append("FILTER " + name + "\n");
        sb.append("THRESHOLD " + threshold + "\n");
        sb.append("MERIT " + merit + "\n");
        sb.append("EVAL_FUNCTION " + evalFunction + "\n");
        sb.append("INIT_FUNCTION " + initFunction + "\n");
        sb.append("FINI_FUNCTION " + finiFunction + "\n");

        for (String arg : arguments) {
            sb.append("ARG " + arg + "\n");
        }
        for (String req : dependencies) {
            sb.append("REQUIRES " + req + "\n");
        }

        return sb.toString();
    }
}

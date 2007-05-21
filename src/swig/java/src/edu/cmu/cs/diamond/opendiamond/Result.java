package edu.cmu.cs.diamond.opendiamond;

import java.util.List;

public abstract class Result {
    public abstract byte[] getData();

    public abstract byte[] getValue(String key);

    public abstract List<String> getKeys();
}
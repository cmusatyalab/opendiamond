package edu.cmu.cs.diamond.opendiamond;

public class Util {
    static int extractInt(byte[] tmp) {
        return (tmp[3] & 0xFF) << 24 | (tmp[2] & 0xFF) << 16
                | (tmp[1] & 0xFF) << 8 | (tmp[0] & 0xFF);
    }
}

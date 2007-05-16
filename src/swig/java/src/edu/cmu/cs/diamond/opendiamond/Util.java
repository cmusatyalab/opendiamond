package edu.cmu.cs.diamond.opendiamond;

public class Util {
    // XXX endian specific
    
    static int extractInt(byte[] value) {
        return (value[3] & 0xFF) << 24 | (value[2] & 0xFF) << 16
                | (value[1] & 0xFF) << 8 | (value[0] & 0xFF);
    }

    public static long extractLong(byte[] value) {
        return (value[7] & 0xFF) << 56 | (value[6] & 0xFF) << 48
                | (value[5] & 0xFF) << 40 | (value[4] & 0xFF) << 32
                | (value[3] & 0xFF) << 24 | (value[2] & 0xFF) << 16
                | (value[1] & 0xFF) << 8 | (value[0] & 0xFF);
    }
}

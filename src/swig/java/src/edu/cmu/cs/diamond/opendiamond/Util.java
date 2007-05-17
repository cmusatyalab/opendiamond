package edu.cmu.cs.diamond.opendiamond;

public class Util {
    private Util() {
    }

    // XXX endian specific
    public static int extractInt(byte[] value) {
        return (value[3] & 0xFF) << 24 | (value[2] & 0xFF) << 16
                | (value[1] & 0xFF) << 8 | (value[0] & 0xFF);
    }

    public static long extractLong(byte[] value) {
        return ((long) (value[7] & 0xFF) << 56)
                | ((long) (value[6] & 0xFF) << 48)
                | ((long) (value[5] & 0xFF) << 40)
                | ((long) (value[4] & 0xFF) << 32)
                | ((long) (value[3] & 0xFF) << 24)
                | ((long) (value[2] & 0xFF) << 16)
                | ((long) (value[1] & 0xFF) << 8) | (value[0] & 0xFF);
    }

    public static double extractDouble(byte[] value) {
        return Double.longBitsToDouble(extractLong(value));
    }
}

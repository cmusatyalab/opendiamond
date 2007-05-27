package edu.cmu.cs.diamond.opendiamond;

import java.net.InetAddress;

public class ServerStatistics {
    final private InetAddress address;

    final private int totalObjects;

    final private int processedObjects;

    final private int droppedObjects;

    public ServerStatistics(InetAddress address, int totalObjects,
            int processedObjects, int droppedObjects) {
        this.address = address;
        this.totalObjects = totalObjects;
        this.processedObjects = processedObjects;
        this.droppedObjects = droppedObjects;
    }

    public InetAddress getAddress() {
        return address;
    }

    public int getDroppedObjects() {
        return droppedObjects;
    }

    public int getProcessedObjects() {
        return processedObjects;
    }

    public int getTotalObjects() {
        return totalObjects;
    }

    @Override
    public String toString() {
        String name = address.getCanonicalHostName();
        return name + ": " + totalObjects + " total, " + processedObjects
                + " processed, " + droppedObjects + " dropped";
    }
}

# simple makefile for now

CC = gcc

all: libOpenDiamond.so src/edu/cmu/cs/diamond/opendiamond/glue/OpenDiamond.java

opendiamond_wrap.c src/edu/cmu/cs/diamond/opendiamond/glue/OpenDiamond.java: opendiamond.i
	swig -Wall -java $$(pkg-config opendiamond --cflags-only-I) -package edu.cmu.cs.diamond.opendiamond.glue -outdir src/edu/cmu/cs/diamond/opendiamond/glue $<


libOpenDiamond.so: opendiamond_wrap.c
	$(CC) -fno-strict-aliasing -m32 -shared $$(pkg-config opendiamond --cflags --libs) -g -O2 -Wall -o $@ $<

clean:
	$(RM) libOpenDiamond.so opendiamond_wrap.c src/edu/cmu/cs/diamond/opendiamond/glue/*.java *~




.PHONY: all clean

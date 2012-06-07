#!/bin/sh

# For DTD validation
# xml2rfc rfc.xml -d rfc2629.dtd --html --raw

xml2rfc diamond-protocol.xml -n --html --raw

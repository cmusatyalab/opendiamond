#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2018 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

SCOPELIST_XSL_BODY = """\
<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns="http://www.w3.org/1999/xhtml">
<xsl:output method="xml" indent="yes" encoding="UTF-8"/>
<xsl:template match="/objectlist">
    <html>
    <head><title>OpenDiamond Scope List</title></head>
    <body>
    <xsl:for-each select="object">
    <img height="64" width="64">
    <xsl:attribute name="src"><xsl:value-of select="@src"/></xsl:attribute>
    </img>
    </xsl:for-each>
    </body>
    </html>
</xsl:template>
</xsl:stylesheet>
"""

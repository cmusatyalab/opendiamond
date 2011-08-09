<?xml version="1.0" encoding="UTF-8"?>

<!--
The OpenDiamond Platform for Interactive Search

Copyright (c) 2011 Carnegie Mellon University
All rights reserved.

This software is distributed under the terms of the Eclipse Public License,
Version 1.0 which can be found in the file named LICENSE.  ANY USE,
REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES RECIPIENT'S
ACCEPTANCE OF THIS AGREEMENT
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:jxb="http://java.sun.com/xml/ns/jaxb"
    version="1.0">
  <xsl:template match="/">
    <html>
      <head>
        <title>XML Schema for OpenDiamond Bundles</title>
        <style type="text/css">
          a {
            color: #00a;
            text-decoration: none;
          }

          h1 {
            text-align: center;
            font-size: x-large;
          }

          .box {
            margin: 0em 0em 0.5em 1em;
            padding: 0.2em 0.6em;
          }

          .box h2 {
            font-size: large;
            text-align: center;
          }

          .box p {
            margin: 0.5em 0em;
          }

          .legend {
            float: right;
            background-color: #ffb;
          }

          .outer-instructions {
            text-align: center;
          }

          .instructions {
            display: inline-block;
            text-align: left;
            background-color: #ddf;
          }

          .treelink {
            color: #ddd;
            font-size: 85%;
          }

          .treelink:hover {
            color: #777;
          }

          .expand-children {
            margin-left: 1.2em;
          }

          .collapse-children {
            margin-left: 0.6em;
          }

          .tt {
            font-family: monospace;
          }

          .required {
            font-weight: bold;
          }

          .default {
            color: #888;
          }

          .type {
            font-style: italic;
            font-size: 85%;
            color: #080;
          }

          .indent {
            padding-left: 2em;
          }

          .section {
            margin-bottom: 0.5em;
          }
        </style>
        <script type="text/javascript"
            src="http://ajax.googleapis.com/ajax/libs/jquery/1.6.2/jquery.min.js">
        </script>
        <script type="text/javascript">
          $(function() {
            $(".expand").click(function(ev) {
              $(this).parent().next().slideToggle("fast");
              ev.preventDefault();
            });
            $(".collapse-children").click(function(ev) {
              var contents = $(this).parent().next();
              contents.hide();
              contents.find(".expand-contents").hide();
              ev.preventDefault();
            });
            $(".expand-children").click(function(ev) {
              var contents = $(this).parent().next();
              contents.find(".expand-contents").show();
              contents.show();
              ev.preventDefault();
            });
            $(".expand-contents").hide();
          });
        </script>
      </head>
      <body>
        <h1>XML Schema for OpenDiamond Bundles</h1>
        <div class="box legend">
          <h2>Legend</h2>

          <p>XML element and attribute names are shown
          <span class="tt">monospaced</span>.</p>

          <p>XML elements are shown in
          <span class="tt">&lt;angle-brackets&gt;</span>.</p>

          <p>Required elements and attributes are shown in
          <span class="tt required">bold</span>.</p>

          <p>Elements that can be repeated are shown with
          <span class="tt">&lt;ellipses&gt;...</span></p>

          <p>Default values for attributes, when provided, are shown in
          <span class="tt default">gray</span>.</p>

          <p>Data types for attribute values are shown in
          <span class="type">[green]</span>.</p>
        </div>
        <div class="outer-instructions">
          <div class="box instructions">
            <p>Click an element name to expand or collapse the element.</p>
          </div>
        </div>
        The root element must be one of:
        <div class="indent">
          <xsl:apply-templates select="xsd:schema/xsd:element"/>
        </div>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="xsd:element">
    <xsl:variable name="namespace" select="/xsd:schema/@targetNamespace"/>
    <xsl:variable name="type" select="@type"/>
    <xsl:variable name="docs">
      <!-- xjc does not allow javadoc on root elements -->
      <xsl:value-of select="xsd:annotation/xsd:documentation"/>
      <!-- xjc ignores javadoc on type definitions -->
      <xsl:value-of
          select="//xsd:complexType[@name=$type]/xsd:annotation/xsd:documentation"/>
      <!-- The preferred form of documentation -->
      <xsl:value-of
          select="xsd:annotation/xsd:appinfo/jxb:property/jxb:javadoc"/>
    </xsl:variable>
    <xsl:variable name="isRoot" select="not(ancestor::xsd:complexType)"/>
    <!-- Don't claim choice members or root elements are "required" -->
    <xsl:variable name="required"
        select="not(@minOccurs = 0) and not(name(..) = 'xsd:choice')
        and not($isRoot)"/>
    <xsl:variable name="repeated" select="@maxOccurs = 'unbounded'"/>
    <xsl:variable name="attributes">
      <xsl:if test="$isRoot">
        <!-- Add dummy attribute for xmlns -->
        <span class="tt required">
          xmlns
        </span>
        <span class="type">
          [string]
        </span>
        <div class="indent">
          Must be set to "<span class="tt"><a>
            <xsl:attribute name="href">
              <xsl:value-of select="$namespace"/>
            </xsl:attribute>
            <xsl:value-of select="$namespace"/>
          </a></span>".
        </div>
      </xsl:if>
      <xsl:apply-templates select="//xsd:complexType[@name=$type]">
        <xsl:with-param name="mode">attributes</xsl:with-param>
      </xsl:apply-templates>
    </xsl:variable>
    <xsl:variable name="elements">
      <xsl:apply-templates select="//xsd:complexType[@name=$type]">
        <xsl:with-param name="mode">elements</xsl:with-param>
      </xsl:apply-templates>
    </xsl:variable>
    <div>
      <div>
        <a class="tt expand" href="#">
          <xsl:if test="$required">
            <xsl:attribute name="class">tt expand required</xsl:attribute>
          </xsl:if>
          &lt;<xsl:value-of select="@name"/>&gt;<xsl:if
              test="$repeated">...</xsl:if>
        </a>
        <a class="treelink expand-children" href="#">[Expand all]</a>
        <a class="treelink collapse-children" href="#">[Collapse all]</a>
      </div>
      <div class="expand-contents indent">
        <xsl:if test="normalize-space($docs)">
          <div class="section">
            <xsl:copy-of select="$docs"/>
          </div>
        </xsl:if>
        <xsl:if test="normalize-space($attributes)">
          <div class="section">
            <xsl:copy-of select="$attributes"/>
          </div>
        </xsl:if>
        <xsl:if test="normalize-space($elements)">
          <div class="section">
            <xsl:copy-of select="$elements"/>
          </div>
        </xsl:if>
      </div>
    </div>
  </xsl:template>

  <xsl:template match="xsd:complexType">
    <xsl:param name="mode"/>
    <!-- Case: this is a subclass -->
    <xsl:apply-templates select="xsd:complexContent/xsd:extension">
      <xsl:with-param name="mode">
        <xsl:value-of select="$mode"/>
      </xsl:with-param>
    </xsl:apply-templates>
    <!-- Case: this is not a subclass -->
    <xsl:call-template name="typeContents">
      <xsl:with-param name="mode">
        <xsl:value-of select="$mode"/>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="xsd:extension">
    <xsl:param name="mode"/>
    <xsl:variable name="base" select="@base"/>
    <!-- Recurse into base class -->
    <xsl:apply-templates select="//xsd:complexType[@name=$base]">
      <xsl:with-param name="mode">
        <xsl:value-of select="$mode"/>
      </xsl:with-param>
    </xsl:apply-templates>
    <!-- Examine additional members -->
    <xsl:call-template name="typeContents">
      <xsl:with-param name="mode">
        <xsl:value-of select="$mode"/>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>

  <xsl:template name="typeContents">
    <xsl:param name="mode"/>
    <xsl:if test="$mode = 'attributes'">
      <xsl:apply-templates select="xsd:attribute"/>
    </xsl:if>
    <xsl:if test="$mode = 'elements'">
      <xsl:apply-templates select="xsd:all"/>
      <xsl:apply-templates select="xsd:choice"/>
      <xsl:apply-templates select="xsd:sequence"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="xsd:all">
    <!-- We don't document that child elements can be in any order because
         it would clutter the page -->
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="xsd:choice">
    <div>
      <xsl:choose>
        <xsl:when test="@minOccurs = 0 and @maxOccurs = 'unbounded'">
          Optionally one or more of:
        </xsl:when>
        <xsl:when test="@minOccurs = 0">
          Optionally one of:
        </xsl:when>
        <xsl:when test="@maxOccurs = 'unbounded'">
          One or more of:
        </xsl:when>
        <xsl:otherwise>
          Choice of:
        </xsl:otherwise>
      </xsl:choose>
      <div class="indent">
        <xsl:apply-templates/>
      </div>
    </div>
  </xsl:template>

  <xsl:template match="xsd:sequence">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="xsd:attribute">
    <span class="tt">
      <xsl:if test="@use = 'required'">
        <xsl:attribute name="class">tt required</xsl:attribute>
      </xsl:if>
      <xsl:value-of select="@name"/>
      <xsl:if test="@default">
        <span class="default">=&quot;<xsl:value-of
            select="@default"/>&quot;</span>
      </xsl:if>
    </span>
    <span class="type">
      [<xsl:apply-templates select="@type"/>]
    </span>
    <div class="indent">
      <xsl:value-of
          select="xsd:annotation/xsd:appinfo/jxb:property/jxb:javadoc"/>
    </div>
  </xsl:template>

  <xsl:template match="xsd:attribute/@type">
    <xsl:variable name="type" select="."/>
    <xsl:variable name="pattern"
        select="//xsd:simpleType[@name=$type]//xsd:pattern/@value"/>
    <xsl:choose>
      <xsl:when test="$pattern">
        <span class="tt"><xsl:value-of select="$pattern"/></span>
      </xsl:when>
      <xsl:when test="$type = 'xsd:boolean'">boolean</xsl:when>
      <xsl:when test="$type = 'xsd:double'">double</xsl:when>
      <xsl:when test="$type = 'xsd:int'">integer</xsl:when>
      <xsl:when test="$type = 'xsd:string'">string</xsl:when>
      <xsl:otherwise><xsl:value-of select="$type"/></xsl:otherwise>
    </xsl:choose>
  </xsl:template>
</xsl:stylesheet>

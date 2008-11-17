<?xml version="1.0" encoding="ISO-8859-1"?>
<!--
   - $Id$
   -
   - This is a filter to copy individual sections from innreport's HTML
   - files.  Actually only news.notice.* files are changed, index.html
   - is not modified.
   -
   - USAGE:
   -   xsltproc -novalid innreport-filter.xslt input.html
   -
   - (Note that option novalid is prefixed by two dashes.)
   -
   - Output is written to standard output, so you need to redirect that.
   -
   - Script written by Alexander Bartolich, 2008.
   -->
<xsl:stylesheet version="1.0"
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xhtml="http://www.w3.org/1999/xhtml"
   xmlns="http://www.w3.org/1999/xhtml">

<xsl:output method="xml" indent="no"/>
<xsl:strip-space elements="*"/>

<!--
   - This is a list of section IDs.  Only IDs quoted with + are copied.
   - Most nnrpd_XXX invade user's privacy.
   -->
<xsl:variable name="select_id" select="'
   +cnfsstat+
   +innd_connect+
   +innd_control+
   +innd_his+
   +innd_incoming_vol+
   +innd_misc_stat+
   +innd_perl+
   +innd_timer+
   +innfeed_connect+
   +innfeed_shrunk+
   +innfeed_timer+
   +innfeed_volume+
   +inn_flow+
   +inn_unwanted+
   +inn_unwanted_dist+
   +inn_unwanted_group+
   +inn_unwanted_unapp+
   -nnrpd_auth-
   +nnrpd_curious+
   -nnrpd_dom_groups-
   +nnrpd_group+
   -nnrpd_groups-
   +nnrpd_hierarchy+
   +nnrpd_no_permission+
   -nnrpd_resource-
   -nnrpd_timeout-
   +nnrpd_timer+
   -nnrpd_unrecognized-
   -nnrpd_unrecognized2-
   +nocem+
   +prog_type+
   -unrecognize-
'"/>

<!--
   - This is a list of section classes.  Only classes quoted with + are
   - copied.
   -->
<xsl:variable name="select_class" select="'
   +ir-feedTotals+
   +ir-pageFooter+
   +ir-pageTitle+
'"/>

<!--
   - This low-priority rule copies any attribute and any node.
   -->
<xsl:template match="@*|node()"
   ><xsl:copy
     ><xsl:apply-templates select="@*|node()"
   /></xsl:copy
 ></xsl:template>

<!--
   - This template matches the list items in the table of contents.
   - Copy items only if the target ID is found in variable select_id.
   - Function substrings cuts off the leading # in attribute href.
   -->
<xsl:template match="/xhtml:html/xhtml:body/xhtml:ul/xhtml:li"
   ><xsl:if test="contains($select_id,
     concat('+', substring(xhtml:a/@href, 2), '+')
   )"><xsl:copy
       ><xsl:apply-templates select="@*|node()"
     /></xsl:copy
   ></xsl:if
 ></xsl:template>

<!--
   - This template matches report sections.  Copy items if the ID is
   - found in variable select_id or the class is found in variable
   - select_class.
   -->
<xsl:template match="/xhtml:html/xhtml:body/xhtml:div"
   ><xsl:if test="
     contains($select_id, concat('+', @id, '+')) or
     contains($select_class, concat('+', @class, '+'))
   "><xsl:copy
       ><xsl:apply-templates select="@*|node()"
     /></xsl:copy
   ></xsl:if
 ></xsl:template>

<!--
   - Graph sections have no ID and are not part of the report section.
   - Instead we check whether the ID of the preceding report section
   - is found in variable select_id.
   -->
<xsl:template match="/xhtml:html/xhtml:body/xhtml:div[@class = 'ir-reportGraph']"
   ><xsl:variable name="prec" select="preceding-sibling::*"
   /><xsl:variable name="prec-id" select="$prec[count($prec)]/@id"
   /><xsl:if test="contains($select_id, concat('+', $prec-id, '+'))"
   ><xsl:copy
     ><xsl:apply-templates select="@*|node()"
     /></xsl:copy
   ></xsl:if
 ></xsl:template>

</xsl:stylesheet>

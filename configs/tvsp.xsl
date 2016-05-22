<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/TR/xhtml1/strict"
                xmlns:exsl="http://exslt.org/common"
                xmlns:date="http://exslt.org/dates-and-times"
                xmlns:str="http://exslt.org/strings">


<xsl:variable name="mapping" select="document('tvsp-category.xml')/mapping" />
<xsl:variable name="categorieMapping" select="$mapping/categories/category/contains" />

<xsl:template match="/">

<events>

    <xsl:for-each select="//Event">

      <xsl:if test="number(id) = id">

	<event>

<!-- bis eventid im Backend erweitert
	<xsl:attribute name="id"><xsl:value-of select="id"/></xsl:attribute>
-->
	<xsl:attribute name="id"><xsl:value-of select="number(substring(id,4))"/></xsl:attribute>

	<starttime><xsl:value-of select="timestart"/></starttime>
	<duration><xsl:value-of select="substring-after(lengthNetAndGross,'/') * 60"/></duration>
	<title><xsl:value-of select="title"/></title>

        <xsl:choose>
            <xsl:when test="string-length(episodeTitle)>0">
                <shorttext><xsl:value-of select="episodeTitle"/></shorttext>
            </xsl:when>
            <xsl:when test="string-length(subline)>0">
                <shorttext><xsl:value-of select="subline"/></shorttext>
            </xsl:when>
            <xsl:when test="string-length(originalTitle)>0">
                <shorttext><xsl:value-of select="originalTitle"/></shorttext>
            </xsl:when>
        </xsl:choose>


        <xsl:choose>
            <xsl:when test="sart_id = 'SP'">
                <category>Spielfilm</category>
            </xsl:when>
            <xsl:when test="sart_id = 'SE'">
                <category>Serie</category>
            </xsl:when>
            <xsl:when test="sart_id = 'KIN'">
                <category>Kinder</category>
            </xsl:when>
            <xsl:when test="sart_id = 'SPO'">
                <category>Sport</category>
            </xsl:when>
            <xsl:otherwise>
                <xsl:call-template name="mapping">
                <xsl:with-param name="str" select="genre" />
                </xsl:call-template>
            </xsl:otherwise>
        </xsl:choose>


        <xsl:if test="string-length(genre)">
            <xsl:choose>
                <xsl:when test="substring(genre,string-length(genre)-5) = 'sserie' and
                not(contains('aeiou',substring(genre,string-length(genre)-6,1)))">
                    <genre><xsl:value-of select="substring(genre,1,string-length(genre)-6)"/></genre>
                </xsl:when>
                <xsl:when test="substring(genre,string-length(genre)-4) = 'serie'">
                    <genre><xsl:value-of select="substring(genre,1,string-length(genre)-5)"/></genre>
                </xsl:when>
                <xsl:when test="substring(genre,string-length(genre)-5) = '-Serie'">
                    <genre><xsl:value-of select="substring(genre,1,string-length(genre)-6)"/></genre>
                </xsl:when>
                <xsl:when test="substring(genre,string-length(genre)-5) = 'sreihe' and
                    not(contains('aeiou',substring(genre,string-length(genre)-6,1)))">
                        <genre><xsl:value-of select="substring(genre,1,string-length(genre)-6)"/></genre>
                </xsl:when>
                <xsl:when test="substring(genre,string-length(genre)-4) = 'reihe'">
                    <genre><xsl:value-of select="substring(genre,1,string-length(genre)-5)"/></genre>
                </xsl:when>
                <xsl:otherwise>
                    <genre><xsl:value-of select="genre"/></genre>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:if>


	<xsl:if test="string-length(year)"><year><xsl:value-of select="year"/></year></xsl:if>
	<xsl:if test="string-length(fsk)"><parentalrating><xsl:value-of select="fsk"/></parentalrating></xsl:if>
	<xsl:if test="string-length(director)"><director><xsl:value-of select="director"/></director></xsl:if>
	<xsl:if test="string-length(anchorman)"><moderator><xsl:value-of select="anchorman"/></moderator></xsl:if>
	<xsl:if test="string-length(actors)"><actor><xsl:value-of select="actors"/></actor></xsl:if>
	<xsl:if test="string-length(studio_guests)"><guest><xsl:value-of select="studio_guests"/></guest></xsl:if>
	<xsl:if test="string-length(authorComment)"><commentator><xsl:value-of select="authorComment"/></commentator></xsl:if>

	<xsl:if test="string-length(text)"><longdescription><xsl:value-of select="text"/></longdescription></xsl:if>
	<xsl:if test="string-length(conclusion)"><shortreview><xsl:value-of select="conclusion"/></shortreview></xsl:if>
	<xsl:if test="string-length(currentTopics)"><topic><xsl:value-of select="str:replace(currentTopics,'|','/')"/></topic></xsl:if>
	<xsl:if test="string-length(preview)"><shortdescription><xsl:value-of select="preview"/></shortdescription></xsl:if>

        <xsl:choose>
	    <xsl:when test="commentBroadcast = 'DIE BESTEN FILME ALLER ZEITEN'"><tipp>GoldTipp</tipp></xsl:when>
	    <xsl:when test="isTopTip = 1"><tipp>TagesTipp</tipp></xsl:when>
	    <xsl:when test="isTipOfTheDay = 1"><tipp>TopTipp</tipp></xsl:when>
        </xsl:choose>

        <xsl:choose>
            <xsl:when test="commentBroadcast = 'DIE BESTEN FILME ALLER ZEITEN'">
                <txtrating>Einer der besten Filme aller Zeiten</txtrating>
            </xsl:when>
            <xsl:when test="thumbIdNumeric = 3">
                <txtrating>Sehr empfehlenswert</txtrating>
            </xsl:when>
            <xsl:when test="thumbIdNumeric = 2">
                <txtrating>Empfehlenswert</txtrating>
            </xsl:when>
            <xsl:when test="thumbIdNumeric = 1">
                <txtrating>Eher durchschnittlich</txtrating>
            </xsl:when>
            <xsl:when test="thumbIdNumeric = 0">
                <txtrating>Eher uninteressant</txtrating>
            </xsl:when>
        </xsl:choose>

        <xsl:choose>
            <xsl:when test="commentBroadcast = 'DIE BESTEN FILME ALLER ZEITEN'"><numrating>5</numrating></xsl:when>
            <xsl:when test="string-length(thumbIdNumeric)"><numrating><xsl:value-of select="thumbIdNumeric +1"/></numrating></xsl:when>
        </xsl:choose>

        <xsl:variable name="BEWERTUNG">
            <xsl:if test="ratingHumor>0"><xsl:text> / Spaß </xsl:text><xsl:value-of select="substring('**********', 1, ratingHumor)"/></xsl:if>
            <xsl:if test="ratingAction>0"><xsl:text> / Action </xsl:text><xsl:value-of select="substring('**********', 1, ratingAction)"/></xsl:if>
            <xsl:if test="ratingErotic>0"><xsl:text> / Erotik </xsl:text><xsl:value-of select="substring('**********', 1, ratingErotic)"/></xsl:if>
            <xsl:if test="ratingSuspense>0"><xsl:text> / Spannung </xsl:text><xsl:value-of select="substring('**********', 1, ratingSuspense)"/></xsl:if>
            <xsl:if test="ratingDemanding>0"><xsl:text> / Anspruch </xsl:text><xsl:value-of select="substring('**********', 1, ratingDemanding)"/></xsl:if>
        </xsl:variable>
        <xsl:if test="string-length($BEWERTUNG)"><rating><xsl:value-of select="$BEWERTUNG"/></rating></xsl:if>


        <xsl:variable name="AUDIO">
            <xsl:if test="isStereo=1"><xsl:text> Stereo</xsl:text></xsl:if>
        </xsl:variable>
        <xsl:if test="string-length($AUDIO)"><audio><xsl:value-of select="normalize-space($AUDIO)"/></audio></xsl:if>

        <xsl:variable name="FLAGS">
            <xsl:if test="isHDTV=1"><xsl:text> [HDTV]</xsl:text></xsl:if>
            <xsl:if test="isLive=1"><xsl:text> [Live]</xsl:text></xsl:if>
            <xsl:if test="isNew=1"><xsl:text> [Neu]</xsl:text></xsl:if>
        </xsl:variable>
	<xsl:if test="string-length($FLAGS)"><flags><xsl:value-of select="normalize-space($FLAGS)"/></flags></xsl:if>


        <xsl:variable name="BILDER">
            <xsl:for-each select="images/image">
		<xsl:call-template name="GetLastSegment">
    		    <xsl:with-param name="value" select="substring-before(.,'.jpg')" />
    		    <xsl:with-param name="separator" select="'/'" />
    		</xsl:call-template>
        	<xsl:text> </xsl:text>
            </xsl:for-each>
        </xsl:variable>

        <xsl:if test="string-length(normalize-space($BILDER))">
            <images><xsl:value-of select="translate(normalize-space($BILDER),' ',',')"/></images>
            <imagetype>jpg</imagetype>
        </xsl:if>

<!--
-->

	</event>
      </xsl:if>


    </xsl:for-each>

</events>

</xsl:template>

<xsl:template name="GetLastSegment">
    <xsl:param name="value" />
    <xsl:param name="separator" select="'.'" />

    <xsl:choose>
      <xsl:when test="contains($value, $separator)">
        <xsl:call-template name="GetLastSegment">
          <xsl:with-param name="value" select="substring-after($value, $separator)" />
          <xsl:with-param name="separator" select="$separator" />
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$value" />
      </xsl:otherwise>
    </xsl:choose>
 </xsl:template>

<xsl:template name="mapping">
    <xsl:param name="str" select="." />
   <xsl:variable name="value" select="translate($str,'ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖU', 'abcdefghijklmnopqrstuvwxyzäöü')" />
   <xsl:variable name="map" select="$categorieMapping[contains($value, .)]/../@name" />....
    <xsl:choose>
        <xsl:when test="string-length($map)">
          <category><xsl:value-of select="$map"/></category>
       </xsl:when>
        <xsl:otherwise>
          <category><xsl:text>Sonstige</xsl:text></category>
       </xsl:otherwise>
    </xsl:choose>
</xsl:template>

</xsl:stylesheet>

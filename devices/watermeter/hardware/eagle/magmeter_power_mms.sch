<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<eagle version="7.0.0">
<drawing>
<settings>
<setting alwaysvectorfont="no"/>
<setting verticaltext="up"/>
</settings>
<grid distance="0.1" unitdist="inch" unit="inch" style="lines" multiple="1" display="no" altdistance="0.01" altunitdist="inch" altunit="inch"/>
<layers>
<layer number="91" name="Nets" color="2" fill="1" visible="yes" active="yes"/>
<layer number="92" name="Busses" color="1" fill="1" visible="yes" active="yes"/>
<layer number="93" name="Pins" color="2" fill="1" visible="no" active="yes"/>
<layer number="94" name="Symbols" color="4" fill="1" visible="yes" active="yes"/>
<layer number="95" name="Names" color="7" fill="1" visible="yes" active="yes"/>
<layer number="96" name="Values" color="7" fill="1" visible="yes" active="yes"/>
<layer number="97" name="Info" color="7" fill="1" visible="yes" active="yes"/>
<layer number="98" name="Guide" color="6" fill="1" visible="yes" active="yes"/>
</layers>
<schematic xreflabel="%F%N/%S.%C%R" xrefpart="/%S.%C%R">
<description>AgSys Mag Meter Power Board MM-S (24V, 500Hz) - Rev 0.1</description>
<libraries>
</libraries>
<attributes>
<attribute name="TITLE" value="Mag Meter Power Board MM-S"/>
<attribute name="REVISION" value="0.1"/>
<attribute name="AUTHOR" value="AgSys"/>
<attribute name="VARIANT" value="MM-S (24V, 500Hz, 1.5-2 inch)"/>
</attributes>
<variantdefs>
</variantdefs>
<classes>
<class number="0" name="default" width="0.2032" drill="0.3048">
</class>
<class number="1" name="power" width="0.8128" drill="0.3048">
</class>
</classes>
<parts>
<!-- Power Input -->
<part name="J2" library="con-phoenix" deviceset="MKDSN1,5/2-5,08" device="" value="24V_IN"/>
<part name="C1" library="rcl" deviceset="CPOL-EU" device="E5-10.5" value="100uF/35V"/>
<part name="C2" library="rcl" deviceset="C-EU" device="C0805" value="100nF"/>

<!-- MOSFET Driver -->
<part name="Q1" library="transistor-power" deviceset="IRLB8721" device="" value="IRLB8721PBF"/>
<part name="R2" library="rcl" deviceset="R-EU_" device="R0402" value="10k"/>
<part name="R3" library="rcl" deviceset="R-EU_" device="R0402" value="100R"/>

<!-- Current Sense -->
<part name="R1" library="rcl" deviceset="R-EU_" device="R2512" value="0.1R/1W"/>

<!-- Flyback Diode -->
<part name="D1" library="diode" deviceset="SCHOTTKY-DIODE" device="SMA" value="SS34"/>

<!-- TVS Protection (Optional) -->
<part name="D2" library="diode" deviceset="SMBJ" device="" value="SMBJ28A"/>

<!-- Tier ID Voltage Divider -->
<part name="R4" library="rcl" deviceset="R-EU_" device="R0402" value="10k"/>
<part name="R5" library="rcl" deviceset="R-EU_" device="R0402" value="30k"/>

<!-- Connectors -->
<part name="J1" library="con-headers" deviceset="PINHD-1X8" device="" value="MAIN_CONN"/>
<part name="J3" library="con-phoenix" deviceset="MKDSN1,5/2-5,08" device="" value="COIL_OUT"/>

<!-- Ground Symbol -->
<part name="GND1" library="supply1" deviceset="GND" device=""/>
<part name="GND2" library="supply1" deviceset="GND" device=""/>
<part name="GND3" library="supply1" deviceset="GND" device=""/>
<part name="GND4" library="supply1" deviceset="GND" device=""/>

<!-- Power Symbol -->
<part name="+24V1" library="supply1" deviceset="+24V" device=""/>
<part name="+24V2" library="supply1" deviceset="+24V" device=""/>
</parts>
<sheets>
<sheet>
<description>Power Board MM-S</description>
<plain>
<text x="10.16" y="170.18" size="2.54" layer="97">POWER INPUT</text>
<text x="10.16" y="119.38" size="2.54" layer="97">MOSFET COIL DRIVER</text>
<text x="127" y="119.38" size="2.54" layer="97">TIER ID</text>
<wire x1="5.08" y1="165.1" x2="200.66" y2="165.1" width="0.1524" layer="97" style="shortdash"/>
<wire x1="5.08" y1="114.3" x2="200.66" y2="114.3" width="0.1524" layer="97" style="shortdash"/>
<wire x1="121.92" y1="114.3" x2="121.92" y2="5.08" width="0.1524" layer="97" style="shortdash"/>
<text x="10.16" y="12.7" size="1.778" layer="97">NOTES:</text>
<text x="10.16" y="10.16" size="1.778" layer="97">1. D2 (TVS) is optional - populate based on field testing</text>
<text x="10.16" y="7.62" size="1.778" layer="97">2. R1 should be low-inductance type (Kelvin sense)</text>
<text x="10.16" y="5.08" size="1.778" layer="97">3. Place D1 as close to Q1 drain as possible</text>
<text x="127" y="170.18" size="2.54" layer="97">MM-S: 24V, 500Hz</text>
<text x="127" y="165.1" size="1.778" layer="97">Pipe size: 1.5" - 2"</text>
<text x="127" y="162.56" size="1.778" layer="97">Coil current: ~2.5A peak</text>
</plain>
<instances>
<!-- Power Input Section -->
<instance part="J2" gate="G$1" x="20.32" y="149.86"/>
<instance part="C1" gate="G$1" x="40.64" y="142.24"/>
<instance part="C2" gate="G$1" x="50.8" y="142.24"/>
<instance part="D2" gate="G$1" x="60.96" y="142.24" rot="R90"/>
<instance part="+24V1" gate="G$1" x="71.12" y="157.48"/>
<instance part="GND1" gate="G$1" x="40.64" y="129.54"/>

<!-- MOSFET Driver Section -->
<instance part="Q1" gate="G$1" x="63.5" y="78.74"/>
<instance part="R2" gate="G$1" x="50.8" y="73.66" rot="R90"/>
<instance part="R3" gate="G$1" x="40.64" y="81.28"/>
<instance part="R1" gate="G$1" x="66.04" y="55.88" rot="R90"/>
<instance part="D1" gate="G$1" x="78.74" y="93.98" rot="R90"/>
<instance part="+24V2" gate="G$1" x="78.74" y="109.22"/>
<instance part="GND2" gate="G$1" x="66.04" y="43.18"/>

<!-- Coil Output -->
<instance part="J3" gate="G$1" x="101.6" y="88.9"/>

<!-- Tier ID Section -->
<instance part="R4" gate="G$1" x="147.32" y="93.98" rot="R90"/>
<instance part="R5" gate="G$1" x="147.32" y="73.66" rot="R90"/>
<instance part="GND3" gate="G$1" x="147.32" y="60.96"/>

<!-- Main Board Connector -->
<instance part="J1" gate="G$1" x="175.26" y="83.82"/>
<instance part="GND4" gate="G$1" x="162.56" y="68.58"/>
</instances>
<busses>
</busses>
<nets>
<!-- +24V Power Rail -->
<net name="+24V" class="1">
<segment>
<pinref part="J2" gate="G$1" pin="1"/>
<wire x1="22.86" y1="152.4" x2="40.64" y2="152.4" width="0.1524" layer="91"/>
<pinref part="C1" gate="G$1" pin="+"/>
<wire x1="40.64" y1="144.78" x2="40.64" y2="152.4" width="0.1524" layer="91"/>
<wire x1="40.64" y1="152.4" x2="50.8" y2="152.4" width="0.1524" layer="91"/>
<junction x="40.64" y="152.4"/>
<pinref part="C2" gate="G$1" pin="1"/>
<wire x1="50.8" y1="144.78" x2="50.8" y2="152.4" width="0.1524" layer="91"/>
<wire x1="50.8" y1="152.4" x2="60.96" y2="152.4" width="0.1524" layer="91"/>
<junction x="50.8" y="152.4"/>
<pinref part="D2" gate="G$1" pin="C"/>
<wire x1="60.96" y1="144.78" x2="60.96" y2="152.4" width="0.1524" layer="91"/>
<wire x1="60.96" y1="152.4" x2="71.12" y2="152.4" width="0.1524" layer="91"/>
<junction x="60.96" y="152.4"/>
<pinref part="+24V1" gate="G$1" pin="+24V"/>
<wire x1="71.12" y1="154.94" x2="71.12" y2="152.4" width="0.1524" layer="91"/>
<label x="30.48" y="152.4" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="D1" gate="G$1" pin="C"/>
<wire x1="78.74" y1="96.52" x2="78.74" y2="104.14" width="0.1524" layer="91"/>
<pinref part="+24V2" gate="G$1" pin="+24V"/>
<wire x1="78.74" y1="104.14" x2="78.74" y2="106.68" width="0.1524" layer="91"/>
</segment>
</net>
<!-- GND -->
<net name="GND" class="1">
<segment>
<pinref part="J2" gate="G$1" pin="2"/>
<wire x1="22.86" y1="149.86" x2="30.48" y2="149.86" width="0.1524" layer="91"/>
<wire x1="30.48" y1="149.86" x2="30.48" y2="134.62" width="0.1524" layer="91"/>
<pinref part="C1" gate="G$1" pin="-"/>
<wire x1="30.48" y1="134.62" x2="40.64" y2="134.62" width="0.1524" layer="91"/>
<wire x1="40.64" y1="137.16" x2="40.64" y2="134.62" width="0.1524" layer="91"/>
<pinref part="C2" gate="G$1" pin="2"/>
<wire x1="40.64" y1="134.62" x2="50.8" y2="134.62" width="0.1524" layer="91"/>
<wire x1="50.8" y1="137.16" x2="50.8" y2="134.62" width="0.1524" layer="91"/>
<junction x="40.64" y="134.62"/>
<pinref part="D2" gate="G$1" pin="A"/>
<wire x1="50.8" y1="134.62" x2="60.96" y2="134.62" width="0.1524" layer="91"/>
<wire x1="60.96" y1="139.7" x2="60.96" y2="134.62" width="0.1524" layer="91"/>
<junction x="50.8" y="134.62"/>
<pinref part="GND1" gate="G$1" pin="GND"/>
<wire x1="40.64" y1="132.08" x2="40.64" y2="134.62" width="0.1524" layer="91"/>
</segment>
<segment>
<pinref part="R1" gate="G$1" pin="1"/>
<wire x1="66.04" y1="50.8" x2="66.04" y2="48.26" width="0.1524" layer="91"/>
<pinref part="GND2" gate="G$1" pin="GND"/>
<wire x1="66.04" y1="48.26" x2="66.04" y2="45.72" width="0.1524" layer="91"/>
<pinref part="R2" gate="G$1" pin="1"/>
<wire x1="50.8" y1="68.58" x2="50.8" y2="48.26" width="0.1524" layer="91"/>
<wire x1="50.8" y1="48.26" x2="66.04" y2="48.26" width="0.1524" layer="91"/>
<junction x="66.04" y="48.26"/>
</segment>
<segment>
<pinref part="R5" gate="G$1" pin="1"/>
<wire x1="147.32" y1="68.58" x2="147.32" y2="66.04" width="0.1524" layer="91"/>
<pinref part="GND3" gate="G$1" pin="GND"/>
<wire x1="147.32" y1="66.04" x2="147.32" y2="63.5" width="0.1524" layer="91"/>
</segment>
<segment>
<pinref part="J1" gate="G$1" pin="2"/>
<wire x1="177.8" y1="88.9" x2="162.56" y2="88.9" width="0.1524" layer="91"/>
<wire x1="162.56" y1="88.9" x2="162.56" y2="71.12" width="0.1524" layer="91"/>
<pinref part="GND4" gate="G$1" pin="GND"/>
<label x="165.1" y="88.9" size="1.778" layer="95"/>
</segment>
</net>
<!-- COIL_GATE from Main Board -->
<net name="COIL_GATE" class="0">
<segment>
<pinref part="J1" gate="G$1" pin="5"/>
<wire x1="177.8" y1="81.28" x2="160.02" y2="81.28" width="0.1524" layer="91"/>
<label x="160.02" y="81.28" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="R3" gate="G$1" pin="1"/>
<wire x1="35.56" y1="81.28" x2="25.4" y2="81.28" width="0.1524" layer="91"/>
<label x="25.4" y="81.28" size="1.778" layer="95"/>
</segment>
</net>
<!-- MOSFET Gate -->
<net name="GATE" class="0">
<segment>
<pinref part="R3" gate="G$1" pin="2"/>
<wire x1="45.72" y1="81.28" x2="50.8" y2="81.28" width="0.1524" layer="91"/>
<pinref part="R2" gate="G$1" pin="2"/>
<wire x1="50.8" y1="78.74" x2="50.8" y2="81.28" width="0.1524" layer="91"/>
<pinref part="Q1" gate="G$1" pin="G"/>
<wire x1="50.8" y1="81.28" x2="60.96" y2="81.28" width="0.1524" layer="91"/>
<junction x="50.8" y="81.28"/>
</segment>
</net>
<!-- MOSFET Drain (Coil-) -->
<net name="COIL_SW" class="1">
<segment>
<pinref part="Q1" gate="G$1" pin="D"/>
<wire x1="66.04" y1="83.82" x2="66.04" y2="88.9" width="0.1524" layer="91"/>
<pinref part="D1" gate="G$1" pin="A"/>
<wire x1="66.04" y1="88.9" x2="78.74" y2="88.9" width="0.1524" layer="91"/>
<wire x1="78.74" y1="88.9" x2="78.74" y2="91.44" width="0.1524" layer="91"/>
<pinref part="J3" gate="G$1" pin="2"/>
<wire x1="78.74" y1="88.9" x2="99.06" y2="88.9" width="0.1524" layer="91"/>
<junction x="78.74" y="88.9"/>
<label x="68.58" y="88.9" size="1.778" layer="95"/>
</segment>
</net>
<!-- MOSFET Source (Current Sense) -->
<net name="I_SENSE_INT" class="0">
<segment>
<pinref part="Q1" gate="G$1" pin="S"/>
<wire x1="66.04" y1="73.66" x2="66.04" y2="68.58" width="0.1524" layer="91"/>
<pinref part="R1" gate="G$1" pin="2"/>
<wire x1="66.04" y1="68.58" x2="66.04" y2="60.96" width="0.1524" layer="91"/>
<wire x1="66.04" y1="68.58" x2="86.36" y2="68.58" width="0.1524" layer="91"/>
<junction x="66.04" y="68.58"/>
<label x="71.12" y="68.58" size="1.778" layer="95"/>
</segment>
</net>
<!-- I_SENSE to Main Board -->
<net name="I_SENSE" class="0">
<segment>
<pinref part="J1" gate="G$1" pin="6"/>
<wire x1="177.8" y1="78.74" x2="160.02" y2="78.74" width="0.1524" layer="91"/>
<label x="160.02" y="78.74" size="1.778" layer="95"/>
</segment>
<segment>
<wire x1="86.36" y1="68.58" x2="109.22" y2="68.58" width="0.1524" layer="91"/>
<label x="99.06" y="68.58" size="1.778" layer="95"/>
</segment>
</net>
<!-- COIL+ Output -->
<net name="COIL+" class="1">
<segment>
<pinref part="J3" gate="G$1" pin="1"/>
<wire x1="99.06" y1="91.44" x2="91.44" y2="91.44" width="0.1524" layer="91"/>
<wire x1="91.44" y1="91.44" x2="91.44" y2="104.14" width="0.1524" layer="91"/>
<wire x1="91.44" y1="104.14" x2="78.74" y2="104.14" width="0.1524" layer="91"/>
<label x="83.82" y="91.44" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="J1" gate="G$1" pin="7"/>
<wire x1="177.8" y1="76.2" x2="160.02" y2="76.2" width="0.1524" layer="91"/>
<label x="160.02" y="76.2" size="1.778" layer="95"/>
</segment>
</net>
<!-- COIL- Output (same as GND) -->
<net name="COIL-" class="1">
<segment>
<pinref part="J1" gate="G$1" pin="8"/>
<wire x1="177.8" y1="73.66" x2="160.02" y2="73.66" width="0.1524" layer="91"/>
<label x="160.02" y="73.66" size="1.778" layer="95"/>
</segment>
</net>
<!-- VIN to Main Board -->
<net name="VIN" class="1">
<segment>
<pinref part="J1" gate="G$1" pin="1"/>
<wire x1="177.8" y1="91.44" x2="160.02" y2="91.44" width="0.1524" layer="91"/>
<label x="160.02" y="91.44" size="1.778" layer="95"/>
</segment>
</net>
<!-- +3V3 from Main Board (for Tier ID) -->
<net name="+3V3" class="0">
<segment>
<pinref part="J1" gate="G$1" pin="3"/>
<wire x1="177.8" y1="86.36" x2="160.02" y2="86.36" width="0.1524" layer="91"/>
<wire x1="160.02" y1="86.36" x2="147.32" y2="86.36" width="0.1524" layer="91"/>
<wire x1="147.32" y1="86.36" x2="147.32" y2="101.6" width="0.1524" layer="91"/>
<pinref part="R4" gate="G$1" pin="2"/>
<wire x1="147.32" y1="99.06" x2="147.32" y2="101.6" width="0.1524" layer="91"/>
<label x="152.4" y="86.36" size="1.778" layer="95"/>
</segment>
</net>
<!-- TIER_ID Voltage Divider Output -->
<net name="TIER_ID" class="0">
<segment>
<pinref part="R4" gate="G$1" pin="1"/>
<wire x1="147.32" y1="88.9" x2="147.32" y2="83.82" width="0.1524" layer="91"/>
<pinref part="R5" gate="G$1" pin="2"/>
<wire x1="147.32" y1="83.82" x2="147.32" y2="78.74" width="0.1524" layer="91"/>
<wire x1="147.32" y1="83.82" x2="160.02" y2="83.82" width="0.1524" layer="91"/>
<junction x="147.32" y="83.82"/>
<pinref part="J1" gate="G$1" pin="4"/>
<wire x1="160.02" y1="83.82" x2="177.8" y2="83.82" width="0.1524" layer="91"/>
<label x="152.4" y="83.82" size="1.778" layer="95"/>
</segment>
</net>
</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>

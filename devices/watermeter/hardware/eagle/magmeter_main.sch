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
<description>AgSys Mag Meter Main Board - Rev 0.1</description>
<libraries>
<library name="magmeter">
<description>AgSys Mag Meter Custom Parts</description>
</library>
</libraries>
<attributes>
<attribute name="TITLE" value="Mag Meter Main Board"/>
<attribute name="REVISION" value="0.1"/>
<attribute name="AUTHOR" value="AgSys"/>
</attributes>
<variantdefs>
</variantdefs>
<classes>
<class number="0" name="default" width="0.2032" drill="0.3048">
</class>
<class number="1" name="power" width="0.4064" drill="0.3048">
</class>
<class number="2" name="analog" width="0.254" drill="0.3048">
</class>
</classes>
<parts>
<!-- Power Section -->
<part name="U7" library="magmeter" deviceset="AP2112K-5.0" device="" value="AP2112K-5.0"/>
<part name="U8" library="magmeter" deviceset="AP2112K-3.3" device="" value="AP2112K-3.3"/>
<part name="C1" library="rcl" deviceset="C-EU" device="C0805" value="10uF"/>
<part name="C2" library="rcl" deviceset="C-EU" device="C0805" value="10uF"/>
<part name="C3" library="rcl" deviceset="C-EU" device="C0805" value="10uF"/>
<part name="C4" library="rcl" deviceset="C-EU" device="C0805" value="10uF"/>

<!-- Analog Front-End -->
<part name="U2" library="magmeter" deviceset="ADS131M02" device="" value="ADS131M02"/>
<part name="U3" library="magmeter" deviceset="THS4551" device="" value="THS4551"/>
<part name="U4" library="magmeter" deviceset="ADA4522-2" device="" value="ADA4522-2"/>

<!-- ADC Decoupling -->
<part name="C13" library="rcl" deviceset="C-EU" device="C0402" value="1uF"/>
<part name="C14" library="rcl" deviceset="C-EU" device="C0402" value="100nF"/>
<part name="C15" library="rcl" deviceset="C-EU" device="C0402" value="1uF"/>
<part name="C16" library="rcl" deviceset="C-EU" device="C0402" value="100nF"/>
<part name="C17" library="rcl" deviceset="C-EU" device="C0402" value="100nF"/>

<!-- THS4551 Decoupling -->
<part name="C10" library="rcl" deviceset="C-EU" device="C0402" value="100nF"/>
<part name="C11" library="rcl" deviceset="C-EU" device="C0805" value="10uF"/>

<!-- ADA4522 Decoupling -->
<part name="C12" library="rcl" deviceset="C-EU" device="C0402" value="100nF"/>

<!-- RC Filter -->
<part name="R10" library="rcl" deviceset="R-EU_" device="R0402" value="47R"/>
<part name="R11" library="rcl" deviceset="R-EU_" device="R0402" value="47R"/>
<part name="C20" library="rcl" deviceset="C-EU" device="C0402" value="100pF"/>
<part name="C21" library="rcl" deviceset="C-EU" device="C0402" value="100pF"/>

<!-- THS4551 Gain Resistors -->
<part name="R20" library="rcl" deviceset="R-EU_" device="R0402" value="1k"/>
<part name="R21" library="rcl" deviceset="R-EU_" device="R0402" value="10k"/>
<part name="R22" library="rcl" deviceset="R-EU_" device="R0402" value="10k"/>
<part name="R23" library="rcl" deviceset="R-EU_" device="R0402" value="10k"/>
<part name="R24" library="rcl" deviceset="R-EU_" device="R0402" value="10k"/>

<!-- Ground Symbols -->
<part name="AGND1" library="supply1" deviceset="AGND" device=""/>
<part name="AGND2" library="supply1" deviceset="AGND" device=""/>
<part name="AGND3" library="supply1" deviceset="AGND" device=""/>
<part name="DGND1" library="supply1" deviceset="DGND" device=""/>
<part name="DGND2" library="supply1" deviceset="DGND" device=""/>

<!-- Power Symbols -->
<part name="+5V1" library="supply1" deviceset="+5V" device=""/>
<part name="+3V3_1" library="supply1" deviceset="+3V3" device=""/>

<!-- Connectors -->
<part name="J1" library="con-headers" deviceset="PINHD-1X8" device="" value="PWR_CONN"/>
<part name="J2" library="con-headers" deviceset="PINHD-1X4" device="" value="ELECTRODE"/>
<part name="J4" library="con-headers" deviceset="PINHD-2X5" device="" value="SWD"/>
</parts>
<sheets>
<sheet>
<description>Power and Analog Front-End</description>
<plain>
<text x="10.16" y="180.34" size="2.54" layer="97">POWER SECTION</text>
<text x="10.16" y="127" size="2.54" layer="97">ANALOG FRONT-END</text>
<text x="10.16" y="50.8" size="2.54" layer="97">ADC SECTION</text>
<wire x1="5.08" y1="175.26" x2="200.66" y2="175.26" width="0.1524" layer="97" style="shortdash"/>
<wire x1="5.08" y1="121.92" x2="200.66" y2="121.92" width="0.1524" layer="97" style="shortdash"/>
<wire x1="5.08" y1="45.72" x2="200.66" y2="45.72" width="0.1524" layer="97" style="shortdash"/>
<text x="127" y="180.34" size="2.54" layer="97">NOTES:</text>
<text x="127" y="175.26" size="1.778" layer="97">1. All AGND and DGND connect at star ground point only</text>
<text x="127" y="172.72" size="1.778" layer="97">2. Place decoupling caps as close to IC pins as possible</text>
<text x="127" y="170.18" size="1.778" layer="97">3. Guard ring surrounds analog input traces</text>
<text x="127" y="167.64" size="1.778" layer="97">4. C20, C21 must be C0G/NP0 type</text>
</plain>
<instances>
<!-- 5V LDO -->
<instance part="U7" gate="G$1" x="50.8" y="160.02"/>
<instance part="C1" gate="G$1" x="30.48" y="154.94"/>
<instance part="C2" gate="G$1" x="71.12" y="154.94"/>

<!-- 3.3V LDO -->
<instance part="U8" gate="G$1" x="50.8" y="137.16"/>
<instance part="C3" gate="G$1" x="30.48" y="132.08"/>
<instance part="C4" gate="G$1" x="71.12" y="132.08"/>

<!-- THS4551 Diff Amp -->
<instance part="U3" gate="G$1" x="76.2" y="101.6"/>
<instance part="C10" gate="G$1" x="58.42" y="114.3"/>
<instance part="C11" gate="G$1" x="53.34" y="114.3"/>

<!-- Gain Resistors -->
<instance part="R23" gate="G$1" x="45.72" y="106.68" rot="R180"/>
<instance part="R24" gate="G$1" x="45.72" y="96.52" rot="R180"/>
<instance part="R20" gate="G$1" x="76.2" y="88.9" rot="R90"/>
<instance part="R21" gate="G$1" x="91.44" y="111.76" rot="R180"/>
<instance part="R22" gate="G$1" x="91.44" y="91.44" rot="R180"/>

<!-- ADA4522 Guard Driver -->
<instance part="U4" gate="A" x="35.56" y="76.2"/>
<instance part="U4" gate="B" x="35.56" y="55.88"/>
<instance part="C12" gate="G$1" x="20.32" y="66.04"/>

<!-- RC Filter -->
<instance part="R10" gate="G$1" x="109.22" y="106.68"/>
<instance part="R11" gate="G$1" x="109.22" y="96.52"/>
<instance part="C20" gate="G$1" x="119.38" y="101.6"/>
<instance part="C21" gate="G$1" x="119.38" y="91.44"/>

<!-- ADS131M02 ADC -->
<instance part="U2" gate="G$1" x="154.94" y="76.2"/>

<!-- ADC Decoupling -->
<instance part="C13" gate="G$1" x="127" y="88.9"/>
<instance part="C14" gate="G$1" x="132.08" y="88.9"/>
<instance part="C15" gate="G$1" x="180.34" y="88.9"/>
<instance part="C16" gate="G$1" x="185.42" y="88.9"/>
<instance part="C17" gate="G$1" x="190.5" y="81.28"/>

<!-- Ground Symbols -->
<instance part="AGND1" gate="G$1" x="76.2" y="78.74"/>
<instance part="AGND2" gate="G$1" x="127" y="78.74"/>
<instance part="AGND3" gate="G$1" x="119.38" y="83.82"/>
<instance part="DGND1" gate="G$1" x="180.34" y="78.74"/>
<instance part="DGND2" gate="G$1" x="50.8" y="124.46"/>

<!-- Power Symbols -->
<instance part="+5V1" gate="G$1" x="76.2" y="121.92"/>
<instance part="+3V3_1" gate="G$1" x="71.12" y="144.78"/>

<!-- Connectors -->
<instance part="J1" gate="G$1" x="10.16" y="160.02"/>
<instance part="J2" gate="G$1" x="10.16" y="96.52"/>
<instance part="J4" gate="G$1" x="180.34" y="35.56"/>
</instances>
<busses>
</busses>
<nets>
<!-- VIN Power -->
<net name="VIN" class="1">
<segment>
<pinref part="J1" gate="G$1" pin="1"/>
<wire x1="12.7" y1="167.64" x2="20.32" y2="167.64" width="0.1524" layer="91"/>
<wire x1="20.32" y1="167.64" x2="20.32" y2="165.1" width="0.1524" layer="91"/>
<pinref part="U7" gate="G$1" pin="VIN"/>
<wire x1="20.32" y1="165.1" x2="38.1" y2="165.1" width="0.1524" layer="91"/>
<pinref part="C1" gate="G$1" pin="1"/>
<wire x1="30.48" y1="157.48" x2="30.48" y2="165.1" width="0.1524" layer="91"/>
<junction x="30.48" y="165.1"/>
<label x="22.86" y="167.64" size="1.778" layer="95"/>
</segment>
</net>
<!-- +5V Rail -->
<net name="+5V" class="1">
<segment>
<pinref part="U7" gate="G$1" pin="VOUT"/>
<wire x1="63.5" y1="165.1" x2="71.12" y2="165.1" width="0.1524" layer="91"/>
<pinref part="C2" gate="G$1" pin="1"/>
<wire x1="71.12" y1="157.48" x2="71.12" y2="165.1" width="0.1524" layer="91"/>
<wire x1="71.12" y1="165.1" x2="78.74" y2="165.1" width="0.1524" layer="91"/>
<junction x="71.12" y="165.1"/>
<label x="73.66" y="165.1" size="1.778" layer="95"/>
</segment>
<segment>
<pinref part="U3" gate="G$1" pin="VS+"/>
<pinref part="+5V1" gate="G$1" pin="+5V"/>
<wire x1="76.2" y1="116.84" x2="76.2" y2="119.38" width="0.1524" layer="91"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="AVDD"/>
<wire x1="137.16" y1="91.44" x2="127" y2="91.44" width="0.1524" layer="91"/>
<pinref part="C13" gate="G$1" pin="1"/>
<pinref part="C14" gate="G$1" pin="1"/>
<wire x1="132.08" y1="91.44" x2="127" y2="91.44" width="0.1524" layer="91"/>
<junction x="127" y="91.44"/>
</segment>
</net>
<!-- +3.3V Rail -->
<net name="+3V3" class="1">
<segment>
<pinref part="U8" gate="G$1" pin="VOUT"/>
<wire x1="63.5" y1="142.24" x2="71.12" y2="142.24" width="0.1524" layer="91"/>
<pinref part="C4" gate="G$1" pin="1"/>
<wire x1="71.12" y1="134.62" x2="71.12" y2="142.24" width="0.1524" layer="91"/>
<pinref part="+3V3_1" gate="G$1" pin="+3V3"/>
<junction x="71.12" y="142.24"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="DVDD"/>
<wire x1="172.72" y1="91.44" x2="180.34" y2="91.44" width="0.1524" layer="91"/>
<pinref part="C15" gate="G$1" pin="1"/>
<pinref part="C16" gate="G$1" pin="1"/>
<wire x1="185.42" y1="91.44" x2="180.34" y2="91.44" width="0.1524" layer="91"/>
<junction x="180.34" y="91.44"/>
</segment>
</net>
<!-- AGND -->
<net name="AGND" class="0">
<segment>
<pinref part="U3" gate="G$1" pin="VS-"/>
<pinref part="AGND1" gate="G$1" pin="AGND"/>
<wire x1="76.2" y1="86.36" x2="76.2" y2="81.28" width="0.1524" layer="91"/>
</segment>
<segment>
<pinref part="U2" gate="G$1" pin="AGND"/>
<wire x1="137.16" y1="88.9" x2="127" y2="88.9" width="0.1524" layer="91"/>
<pinref part="AGND2" gate="G$1" pin="AGND"/>
<wire x1="127" y1="81.28" x2="127" y2="83.82" width="0.1524" layer="91"/>
<pinref part="C13" gate="G$1" pin="2"/>
<pinref part="C14" gate="G$1" pin="2"/>
<wire x1="127" y1="83.82" x2="127" y2="88.9" width="0.1524" layer="91"/>
<wire x1="132.08" y1="83.82" x2="127" y2="83.82" width="0.1524" layer="91"/>
<junction x="127" y="83.82"/>
</segment>
<segment>
<pinref part="C20" gate="G$1" pin="2"/>
<pinref part="C21" gate="G$1" pin="2"/>
<wire x1="119.38" y1="96.52" x2="119.38" y2="86.36" width="0.1524" layer="91"/>
<pinref part="AGND3" gate="G$1" pin="AGND"/>
<junction x="119.38" y="86.36"/>
</segment>
</net>
<!-- DGND -->
<net name="DGND" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="DGND"/>
<wire x1="172.72" y1="88.9" x2="180.34" y2="88.9" width="0.1524" layer="91"/>
<pinref part="DGND1" gate="G$1" pin="DGND"/>
<wire x1="180.34" y1="81.28" x2="180.34" y2="83.82" width="0.1524" layer="91"/>
<pinref part="C15" gate="G$1" pin="2"/>
<pinref part="C16" gate="G$1" pin="2"/>
<wire x1="180.34" y1="83.82" x2="180.34" y2="88.9" width="0.1524" layer="91"/>
<wire x1="185.42" y1="83.82" x2="180.34" y2="83.82" width="0.1524" layer="91"/>
<junction x="180.34" y="83.82"/>
<pinref part="C17" gate="G$1" pin="2"/>
<wire x1="190.5" y1="76.2" x2="190.5" y2="73.66" width="0.1524" layer="91"/>
<wire x1="190.5" y1="73.66" x2="180.34" y2="73.66" width="0.1524" layer="91"/>
<wire x1="180.34" y1="73.66" x2="180.34" y2="81.28" width="0.1524" layer="91"/>
<junction x="180.34" y="81.28"/>
</segment>
<segment>
<pinref part="U8" gate="G$1" pin="GND"/>
<pinref part="DGND2" gate="G$1" pin="DGND"/>
<wire x1="50.8" y1="124.46" x2="50.8" y2="127" width="0.1524" layer="91"/>
</segment>
</net>
<!-- Electrode Inputs -->
<net name="ELEC_P" class="2">
<segment>
<pinref part="J2" gate="G$1" pin="1"/>
<wire x1="12.7" y1="101.6" x2="25.4" y2="101.6" width="0.1524" layer="91"/>
<wire x1="25.4" y1="101.6" x2="25.4" y2="81.28" width="0.1524" layer="91"/>
<pinref part="U4" gate="A" pin="+IN"/>
<wire x1="25.4" y1="81.28" x2="22.86" y2="81.28" width="0.1524" layer="91"/>
<wire x1="25.4" y1="101.6" x2="25.4" y2="106.68" width="0.1524" layer="91"/>
<junction x="25.4" y="101.6"/>
<pinref part="R23" gate="G$1" pin="2"/>
<wire x1="25.4" y1="106.68" x2="40.64" y2="106.68" width="0.1524" layer="91"/>
<label x="15.24" y="101.6" size="1.778" layer="95"/>
</segment>
</net>
<net name="ELEC_N" class="2">
<segment>
<pinref part="J2" gate="G$1" pin="3"/>
<wire x1="12.7" y1="96.52" x2="20.32" y2="96.52" width="0.1524" layer="91"/>
<wire x1="20.32" y1="96.52" x2="20.32" y2="60.96" width="0.1524" layer="91"/>
<pinref part="U4" gate="B" pin="+IN"/>
<wire x1="20.32" y1="60.96" x2="22.86" y2="60.96" width="0.1524" layer="91"/>
<wire x1="20.32" y1="96.52" x2="40.64" y2="96.52" width="0.1524" layer="91"/>
<junction x="20.32" y="96.52"/>
<pinref part="R24" gate="G$1" pin="2"/>
<label x="15.24" y="96.52" size="1.778" layer="95"/>
</segment>
</net>
<net name="GUARD_P" class="0">
<segment>
<pinref part="U4" gate="A" pin="OUT"/>
<wire x1="48.26" y1="76.2" x2="55.88" y2="76.2" width="0.1524" layer="91"/>
<wire x1="55.88" y1="76.2" x2="55.88" y2="68.58" width="0.1524" layer="91"/>
<wire x1="55.88" y1="68.58" x2="10.16" y2="68.58" width="0.1524" layer="91"/>
<pinref part="J2" gate="G$1" pin="2"/>
<wire x1="10.16" y1="68.58" x2="10.16" y2="99.06" width="0.1524" layer="91"/>
<wire x1="10.16" y1="99.06" x2="12.7" y2="99.06" width="0.1524" layer="91"/>
<label x="50.8" y="76.2" size="1.778" layer="95"/>
</segment>
</net>
<net name="GUARD_N" class="0">
<segment>
<pinref part="U4" gate="B" pin="OUT"/>
<wire x1="48.26" y1="55.88" x2="55.88" y2="55.88" width="0.1524" layer="91"/>
<wire x1="55.88" y1="55.88" x2="55.88" y2="48.26" width="0.1524" layer="91"/>
<wire x1="55.88" y1="48.26" x2="7.62" y2="48.26" width="0.1524" layer="91"/>
<wire x1="7.62" y1="48.26" x2="7.62" y2="93.98" width="0.1524" layer="91"/>
<pinref part="J2" gate="G$1" pin="4"/>
<wire x1="7.62" y1="93.98" x2="12.7" y2="93.98" width="0.1524" layer="91"/>
<label x="50.8" y="55.88" size="1.778" layer="95"/>
</segment>
</net>
<!-- THS4551 to ADC -->
<net name="FDA_OUTP" class="2">
<segment>
<pinref part="U3" gate="G$1" pin="OUT+"/>
<wire x1="91.44" y1="106.68" x2="99.06" y2="106.68" width="0.1524" layer="91"/>
<pinref part="R10" gate="G$1" pin="1"/>
<wire x1="99.06" y1="106.68" x2="104.14" y2="106.68" width="0.1524" layer="91"/>
<label x="93.98" y="106.68" size="1.778" layer="95"/>
</segment>
</net>
<net name="FDA_OUTN" class="2">
<segment>
<pinref part="U3" gate="G$1" pin="OUT-"/>
<wire x1="91.44" y1="96.52" x2="99.06" y2="96.52" width="0.1524" layer="91"/>
<pinref part="R11" gate="G$1" pin="1"/>
<wire x1="99.06" y1="96.52" x2="104.14" y2="96.52" width="0.1524" layer="91"/>
<label x="93.98" y="96.52" size="1.778" layer="95"/>
</segment>
</net>
<net name="AIN0P" class="2">
<segment>
<pinref part="R10" gate="G$1" pin="2"/>
<wire x1="114.3" y1="106.68" x2="119.38" y2="106.68" width="0.1524" layer="91"/>
<pinref part="C20" gate="G$1" pin="1"/>
<wire x1="119.38" y1="104.14" x2="119.38" y2="106.68" width="0.1524" layer="91"/>
<wire x1="119.38" y1="106.68" x2="127" y2="106.68" width="0.1524" layer="91"/>
<junction x="119.38" y="106.68"/>
<wire x1="127" y1="106.68" x2="127" y2="83.82" width="0.1524" layer="91"/>
<pinref part="U2" gate="G$1" pin="AIN0P"/>
<wire x1="127" y1="83.82" x2="137.16" y2="83.82" width="0.1524" layer="91"/>
<label x="121.92" y="106.68" size="1.778" layer="95"/>
</segment>
</net>
<net name="AIN0N" class="2">
<segment>
<pinref part="R11" gate="G$1" pin="2"/>
<wire x1="114.3" y1="96.52" x2="116.84" y2="96.52" width="0.1524" layer="91"/>
<pinref part="C21" gate="G$1" pin="1"/>
<wire x1="116.84" y1="96.52" x2="119.38" y2="96.52" width="0.1524" layer="91"/>
<wire x1="119.38" y1="93.98" x2="119.38" y2="96.52" width="0.1524" layer="91"/>
<wire x1="119.38" y1="96.52" x2="124.46" y2="96.52" width="0.1524" layer="91"/>
<junction x="119.38" y="96.52"/>
<wire x1="124.46" y1="96.52" x2="124.46" y2="81.28" width="0.1524" layer="91"/>
<pinref part="U2" gate="G$1" pin="AIN0N"/>
<wire x1="124.46" y1="81.28" x2="137.16" y2="81.28" width="0.1524" layer="91"/>
<label x="121.92" y="96.52" size="1.778" layer="95"/>
</segment>
</net>
<!-- ADC CAP Pin -->
<net name="CAP" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="CAP"/>
<wire x1="172.72" y1="83.82" x2="190.5" y2="83.82" width="0.1524" layer="91"/>
<pinref part="C17" gate="G$1" pin="1"/>
<label x="175.26" y="83.82" size="1.778" layer="95"/>
</segment>
</net>
<!-- SPI to ADC -->
<net name="ADC_SCLK" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="SCLK"/>
<wire x1="172.72" y1="68.58" x2="195.58" y2="68.58" width="0.1524" layer="91"/>
<label x="175.26" y="68.58" size="1.778" layer="95"/>
</segment>
</net>
<net name="ADC_MOSI" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="DIN"/>
<wire x1="172.72" y1="73.66" x2="195.58" y2="73.66" width="0.1524" layer="91"/>
<label x="175.26" y="73.66" size="1.778" layer="95"/>
</segment>
</net>
<net name="ADC_MISO" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="DOUT"/>
<wire x1="172.72" y1="71.12" x2="195.58" y2="71.12" width="0.1524" layer="91"/>
<label x="175.26" y="71.12" size="1.778" layer="95"/>
</segment>
</net>
<net name="ADC_CS" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="!CS"/>
<wire x1="172.72" y1="63.5" x2="195.58" y2="63.5" width="0.1524" layer="91"/>
<label x="175.26" y="63.5" size="1.778" layer="95"/>
</segment>
</net>
<net name="ADC_DRDY" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="!DRDY"/>
<wire x1="172.72" y1="66.04" x2="195.58" y2="66.04" width="0.1524" layer="91"/>
<label x="175.26" y="66.04" size="1.778" layer="95"/>
</segment>
</net>
<net name="ADC_SYNC" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="!SYNC/!RST"/>
<wire x1="172.72" y1="60.96" x2="195.58" y2="60.96" width="0.1524" layer="91"/>
<label x="175.26" y="60.96" size="1.778" layer="95"/>
</segment>
</net>
</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>

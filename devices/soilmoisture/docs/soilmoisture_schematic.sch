<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<eagle version="7.7.0">
<drawing>
<settings>
<setting alwaysvectorfont="no"/>
<setting verticaltext="up"/>
</settings>
<grid distance="0.1" unitdist="inch" unit="inch" style="lines" multiple="1" display="no" altdistance="0.01" altunitdist="inch" altunit="inch"/>
<layers>
<layer number="1" name="Top" color="4" fill="1" visible="yes" active="yes"/>
<layer number="16" name="Bottom" color="1" fill="1" visible="yes" active="yes"/>
<layer number="17" name="Pads" color="2" fill="1" visible="yes" active="yes"/>
<layer number="18" name="Vias" color="2" fill="1" visible="yes" active="yes"/>
<layer number="19" name="Unrouted" color="6" fill="1" visible="yes" active="yes"/>
<layer number="20" name="Dimension" color="15" fill="1" visible="yes" active="yes"/>
<layer number="21" name="tPlace" color="7" fill="1" visible="yes" active="yes"/>
<layer number="22" name="bPlace" color="7" fill="1" visible="yes" active="yes"/>
<layer number="23" name="tOrigins" color="15" fill="1" visible="yes" active="yes"/>
<layer number="24" name="bOrigins" color="15" fill="1" visible="yes" active="yes"/>
<layer number="25" name="tNames" color="7" fill="1" visible="yes" active="yes"/>
<layer number="26" name="bNames" color="7" fill="1" visible="yes" active="yes"/>
<layer number="27" name="tValues" color="7" fill="1" visible="yes" active="yes"/>
<layer number="28" name="bValues" color="7" fill="1" visible="yes" active="yes"/>
<layer number="29" name="tStop" color="7" fill="3" visible="no" active="yes"/>
<layer number="30" name="bStop" color="7" fill="6" visible="no" active="yes"/>
<layer number="31" name="tCream" color="7" fill="4" visible="no" active="yes"/>
<layer number="32" name="bCream" color="7" fill="5" visible="no" active="yes"/>
<layer number="33" name="tFinish" color="6" fill="3" visible="no" active="yes"/>
<layer number="34" name="bFinish" color="6" fill="6" visible="no" active="yes"/>
<layer number="35" name="tGlue" color="7" fill="4" visible="no" active="yes"/>
<layer number="36" name="bGlue" color="7" fill="5" visible="no" active="yes"/>
<layer number="37" name="tTest" color="7" fill="1" visible="no" active="yes"/>
<layer number="38" name="bTest" color="7" fill="1" visible="no" active="yes"/>
<layer number="39" name="tKeepout" color="4" fill="11" visible="yes" active="yes"/>
<layer number="40" name="bKeepout" color="1" fill="11" visible="yes" active="yes"/>
<layer number="41" name="tRestrict" color="4" fill="10" visible="yes" active="yes"/>
<layer number="42" name="bRestrict" color="1" fill="10" visible="yes" active="yes"/>
<layer number="43" name="vRestrict" color="2" fill="10" visible="yes" active="yes"/>
<layer number="44" name="Drills" color="7" fill="1" visible="no" active="yes"/>
<layer number="45" name="Holes" color="7" fill="1" visible="no" active="yes"/>
<layer number="46" name="Milling" color="3" fill="1" visible="no" active="yes"/>
<layer number="47" name="Measures" color="7" fill="1" visible="no" active="yes"/>
<layer number="48" name="Document" color="7" fill="1" visible="yes" active="yes"/>
<layer number="49" name="Reference" color="7" fill="1" visible="yes" active="yes"/>
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
<description>AgSys Soil Moisture Sensor - Wiring Diagram
nRF52832 + RFM95C LoRa + FM25V02 FRAM + H-Bridge Capacitance Sensor</description>
<libraries>
</libraries>
<attributes>
<attribute name="AUTHOR" value="AgSys Control"/>
<attribute name="REVISION" value="1.0"/>
<attribute name="TITLE" value="Soil Moisture Sensor"/>
</attributes>
<variantdefs>
</variantdefs>
<classes>
<class number="0" name="default" width="0.254" drill="0.3302">
</class>
<class number="1" name="power" width="0.4064" drill="0.3302">
</class>
</classes>
<parts>
<!-- nRF52832 MCU -->
<part name="U1" library="nordic" deviceset="NRF52832" device="" value="nRF52832"/>
<!-- LoRa Module -->
<part name="U2" library="rfm" deviceset="RFM95" device="" value="RFM95C-915"/>
<!-- FRAM -->
<part name="U3" library="fram" deviceset="FM25V02" device="" value="FM25V02-8KB"/>
<!-- SPI NOR Flash for firmware backup -->
<part name="U5" library="flash" deviceset="W25Q16" device="" value="W25Q16-2MB"/>
<!-- H-Bridge MOSFETs -->
<part name="Q1" library="mosfet" deviceset="SSM6P15FU" device="" value="SSM6P15FU"/>
<part name="Q2" library="mosfet" deviceset="SSM6P15FU" device="" value="SSM6P15FU"/>
<part name="Q3" library="mosfet" deviceset="2SK2009" device="" value="2SK2009"/>
<part name="Q4" library="mosfet" deviceset="2SK2009" device="" value="2SK2009"/>
<!-- Flyback Diodes -->
<part name="D1" library="diode" deviceset="BAT54S" device="" value="BAT54S"/>
<part name="D2" library="diode" deviceset="BAT54S" device="" value="BAT54S"/>
<!-- LDO Regulator -->
<part name="U4" library="regulator" deviceset="MCP1700" device="" value="MCP1700-2502E"/>
<!-- Battery -->
<part name="BT1" library="battery" deviceset="21700" device="" value="21700-5000mAh"/>
<!-- LEDs -->
<part name="LED1" library="led" deviceset="LED" device="" value="GREEN"/>
<part name="LED2" library="led" deviceset="LED" device="" value="YELLOW"/>
<part name="LED3" library="led" deviceset="LED" device="" value="BLUE"/>
<!-- Resistors -->
<part name="R1" library="resistor" deviceset="R" device="" value="100K"/>
<part name="R2" library="resistor" deviceset="R" device="" value="100K"/>
<part name="R3" library="resistor" deviceset="R" device="" value="330"/>
<part name="R4" library="resistor" deviceset="R" device="" value="330"/>
<part name="R5" library="resistor" deviceset="R" device="" value="330"/>
<part name="R6" library="resistor" deviceset="R" device="" value="10K"/>
<!-- Capacitors -->
<part name="C1" library="capacitor" deviceset="C" device="" value="10uF"/>
<part name="C2" library="capacitor" deviceset="C" device="" value="100nF"/>
<part name="C3" library="capacitor" deviceset="C" device="" value="100nF"/>
<part name="C4" library="capacitor" deviceset="C" device="" value="100nF"/>
<!-- Button -->
<part name="SW1" library="switch" deviceset="TACTILE" device="" value="OTA_BTN"/>
<!-- Antenna -->
<part name="ANT1" library="antenna" deviceset="ANTENNA" device="" value="915MHz"/>
<!-- Capacitive Probe -->
<part name="PROBE1" library="connector" deviceset="PROBE" device="" value="CAP_PROBE"/>
<!-- Power Supply Labels -->
<part name="VCC" library="supply" deviceset="VCC" device="" value="2.5V"/>
<part name="GND" library="supply" deviceset="GND" device=""/>
<part name="VBAT" library="supply" deviceset="VBAT" device="" value="3.0-4.2V"/>
</parts>
<sheets>
<sheet>
<description>Main Schematic</description>
<plain>
<!-- Title Block -->
<text x="200" y="10" size="3.81" layer="97">AgSys Soil Moisture Sensor</text>
<text x="200" y="5" size="2.54" layer="97">nRF52832 + RFM95C + FM25V02 + H-Bridge</text>
<text x="200" y="0" size="1.778" layer="97">Rev 1.0 - January 2026</text>

<!-- Section Labels -->
<text x="10" y="180" size="2.54" layer="97" font="vector">POWER SUPPLY</text>
<text x="80" y="180" size="2.54" layer="97" font="vector">MCU - nRF52832</text>
<text x="160" y="180" size="2.54" layer="97" font="vector">SPI BUS</text>
<text x="10" y="100" size="2.54" layer="97" font="vector">H-BRIDGE DRIVER</text>
<text x="160" y="100" size="2.54" layer="97" font="vector">LORA MODULE</text>
<text x="160" y="50" size="2.54" layer="97" font="vector">FRAM</text>

<!-- Connection Notes -->
<text x="10" y="70" size="1.778" layer="97">100kHz AC drive via Timer2+PPI</text>
<text x="10" y="65" size="1.778" layer="97">Envelope detector to AIN0 (P0.02)</text>
</plain>
<instances>
<!-- Component placements would go here -->
<!-- Note: Actual X,Y coordinates need adjustment in Eagle -->
</instances>
<busses>
<bus name="SPI:SCK,MOSI,MISO">
<segment>
<wire x1="150" y1="150" x2="150" y2="50" width="0.762" layer="92"/>
<label x="152" y="100" size="1.778" layer="95"/>
</segment>
</bus>
</busses>
<nets>
<!-- VCC Net -->
<net name="VCC" class="1">
<segment>
<pinref part="U1" gate="G$1" pin="VDD"/>
<pinref part="U2" gate="G$1" pin="VCC"/>
<pinref part="U3" gate="G$1" pin="VCC"/>
<wire x1="50" y1="170" x2="100" y2="170" width="0.4064" layer="91"/>
<label x="75" y="172" size="1.778" layer="95"/>
</segment>
</net>
<!-- GND Net -->
<net name="GND" class="1">
<segment>
<pinref part="U1" gate="G$1" pin="GND"/>
<pinref part="U2" gate="G$1" pin="GND"/>
<pinref part="U3" gate="G$1" pin="GND"/>
<pinref part="U4" gate="G$1" pin="GND"/>
<wire x1="50" y1="10" x2="200" y2="10" width="0.4064" layer="91"/>
<label x="100" y="12" size="1.778" layer="95"/>
</segment>
</net>
<!-- SPI Clock -->
<net name="SPI_SCK" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.23"/>
<pinref part="U2" gate="G$1" pin="SCK"/>
<pinref part="U3" gate="G$1" pin="SCK"/>
<wire x1="120" y1="140" x2="180" y2="140" width="0.254" layer="91"/>
<label x="140" y="142" size="1.778" layer="95"/>
</segment>
</net>
<!-- SPI MOSI -->
<net name="SPI_MOSI" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.24"/>
<pinref part="U2" gate="G$1" pin="MOSI"/>
<pinref part="U3" gate="G$1" pin="SI"/>
<wire x1="120" y1="135" x2="180" y2="135" width="0.254" layer="91"/>
<label x="140" y="137" size="1.778" layer="95"/>
</segment>
</net>
<!-- SPI MISO -->
<net name="SPI_MISO" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.25"/>
<pinref part="U2" gate="G$1" pin="MISO"/>
<pinref part="U3" gate="G$1" pin="SO"/>
<wire x1="120" y1="130" x2="180" y2="130" width="0.254" layer="91"/>
<label x="140" y="132" size="1.778" layer="95"/>
</segment>
</net>
<!-- LoRa Chip Select -->
<net name="LORA_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.27"/>
<pinref part="U2" gate="G$1" pin="NSS"/>
<wire x1="120" y1="120" x2="160" y2="120" width="0.254" layer="91"/>
<label x="135" y="122" size="1.778" layer="95"/>
</segment>
</net>
<!-- LoRa Reset -->
<net name="LORA_RST" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.30"/>
<pinref part="U2" gate="G$1" pin="RESET"/>
<wire x1="120" y1="115" x2="160" y2="115" width="0.254" layer="91"/>
<label x="135" y="117" size="1.778" layer="95"/>
</segment>
</net>
<!-- LoRa DIO0 Interrupt -->
<net name="LORA_DIO0" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.31"/>
<pinref part="U2" gate="G$1" pin="DIO0"/>
<wire x1="120" y1="110" x2="160" y2="110" width="0.254" layer="91"/>
<label x="135" y="112" size="1.778" layer="95"/>
</segment>
</net>
<!-- FRAM Chip Select -->
<net name="NVRAM_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.11"/>
<pinref part="U3" gate="G$1" pin="CS"/>
<wire x1="120" y1="100" x2="160" y2="60" width="0.254" layer="91"/>
<label x="135" y="80" size="1.778" layer="95"/>
</segment>
</net>
<!-- H-Bridge Drive A -->
<net name="HBRIDGE_A" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.14"/>
<pinref part="Q1" gate="G$1" pin="G"/>
<pinref part="Q3" gate="G$1" pin="G"/>
<wire x1="80" y1="90" x2="40" y2="90" width="0.254" layer="91"/>
<label x="50" y="92" size="1.778" layer="95"/>
</segment>
</net>
<!-- H-Bridge Drive B -->
<net name="HBRIDGE_B" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.15"/>
<pinref part="Q2" gate="G$1" pin="G"/>
<pinref part="Q4" gate="G$1" pin="G"/>
<wire x1="80" y1="85" x2="40" y2="85" width="0.254" layer="91"/>
<label x="50" y="87" size="1.778" layer="95"/>
</segment>
</net>
<!-- H-Bridge Power Enable -->
<net name="HBRIDGE_PWR" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.16"/>
<wire x1="80" y1="80" x2="30" y2="80" width="0.254" layer="91"/>
<label x="40" y="82" size="1.778" layer="95"/>
</segment>
</net>
<!-- Moisture ADC -->
<net name="MOISTURE_ADC" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.02/AIN0"/>
<wire x1="80" y1="160" x2="30" y2="60" width="0.254" layer="91"/>
<label x="35" y="62" size="1.778" layer="95"/>
</segment>
</net>
<!-- Battery ADC -->
<net name="VBAT_ADC" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.28/AIN4"/>
<pinref part="R1" gate="G$1" pin="2"/>
<wire x1="80" y1="155" x2="30" y2="170" width="0.254" layer="91"/>
<label x="35" y="165" size="1.778" layer="95"/>
</segment>
</net>
<!-- SPI Flash Chip Select -->
<net name="FLASH_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.12"/>
<pinref part="U5" gate="G$1" pin="CS"/>
<wire x1="120" y1="95" x2="160" y2="55" width="0.254" layer="91"/>
<label x="135" y="75" size="1.778" layer="95"/>
</segment>
</net>
<!-- LED Status -->
<net name="LED_STATUS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.17"/>
<pinref part="LED1" gate="G$1" pin="A"/>
<wire x1="120" y1="70" x2="140" y2="70" width="0.254" layer="91"/>
<label x="125" y="72" size="1.778" layer="95"/>
</segment>
</net>
<!-- LED SPI -->
<net name="LED_SPI" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.19"/>
<pinref part="LED2" gate="G$1" pin="A"/>
<wire x1="120" y1="65" x2="140" y2="65" width="0.254" layer="91"/>
<label x="125" y="67" size="1.778" layer="95"/>
</segment>
</net>
<!-- LED BLE -->
<net name="LED_CONN" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.20"/>
<pinref part="LED3" gate="G$1" pin="A"/>
<wire x1="120" y1="60" x2="140" y2="60" width="0.254" layer="91"/>
<label x="125" y="62" size="1.778" layer="95"/>
</segment>
</net>
<!-- OTA Button -->
<net name="OTA_BTN" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.07"/>
<pinref part="SW1" gate="G$1" pin="1"/>
<pinref part="R6" gate="G$1" pin="1"/>
<wire x1="80" y1="50" x2="60" y2="50" width="0.254" layer="91"/>
<label x="65" y="52" size="1.778" layer="95"/>
</segment>
</net>
</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>

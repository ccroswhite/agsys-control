<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<!--
  Power Supply Unit (PSU) with Battery Backup Schematic
  For Valve Controller System
  
  Features:
  - AC Mains input (120/240V)
  - 24V DC output for valve motors and CAN bus
  - 7S Li-ion battery backup (25.9V nominal)
  - Automatic switchover on power fail
  - Power fail signal output to controller
  
  Rev 1.0 - Initial design
  
  This schematic is Eagle 7.x compatible
-->
<eagle version="7.7.0">
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
</layers>
<schematic>
<libraries>
</libraries>
<attributes>
</attributes>
<variantdefs>
</variantdefs>
<classes>
<class number="0" name="default" width="0.254" drill="0.3302"/>
<class number="1" name="power" width="0.4064" drill="0.3302"/>
<class number="2" name="highcurrent" width="1.016" drill="0.5080"/>
<class number="3" name="mains" width="1.524" drill="0.6096"/>
</classes>
<parts>
<!-- ================================================================== -->
<!-- AC INPUT SECTION -->
<!-- ================================================================== -->
<part name="J1" value="IEC-C14" device="PANEL"/>
<part name="F1" value="5A/250V" device="FUSE-5x20"/>
<part name="MOV1" value="275VAC" device="MOV"/>
<part name="SW1" value="DPST" device="ROCKER"/>

<!-- ================================================================== -->
<!-- AC-DC CONVERTER -->
<!-- ================================================================== -->
<part name="PS1" value="MEAN-WELL-LRS-50-24" device="ENCLOSED"/>

<!-- ================================================================== -->
<!-- BATTERY MANAGEMENT SYSTEM (7S) -->
<!-- ================================================================== -->
<part name="U1" value="7S-BMS-20A" device="MODULE"/>
<part name="BT1" value="7S-LIION-25.9V" device="PACK"/>
<part name="J2" value="XT60" device="CONNECTOR"/>

<!-- ================================================================== -->
<!-- BATTERY CHARGER -->
<!-- ================================================================== -->
<part name="U2" value="LTC4020" device="QFN"/>
<part name="L1" value="22uH/5A" device="SHIELDED"/>
<part name="Q1" value="IRF3205" device="TO-220"/>
<part name="D1" value="SS54" device="SMC"/>
<part name="C1" value="100uF/35V" device="ELECTROLYTIC"/>
<part name="C2" value="100uF/35V" device="ELECTROLYTIC"/>
<part name="R1" value="10K" device="0603"/>
<part name="R2" value="2K" device="0603"/>
<part name="R3" value="0.05R/2W" device="2512"/>

<!-- ================================================================== -->
<!-- POWER PATH / AUTO-SWITCHOVER -->
<!-- ================================================================== -->
<part name="U3" value="LTC4412" device="SOT-23-6"/>
<part name="Q2" value="SI4435DDY" device="SO-8"/>
<part name="Q3" value="SI4435DDY" device="SO-8"/>
<part name="D2" value="SS54" device="SMC"/>
<part name="D3" value="SS54" device="SMC"/>

<!-- ================================================================== -->
<!-- AC SENSE / POWER FAIL DETECTION -->
<!-- ================================================================== -->
<part name="U4" value="PC817" device="DIP-4"/>
<part name="R4" value="100K/1W" device="2512"/>
<part name="R5" value="100K/1W" device="2512"/>
<part name="R6" value="10K" device="0603"/>
<part name="C3" value="100nF" device="0603"/>
<part name="D4" value="1N4148" device="SOD-123"/>

<!-- ================================================================== -->
<!-- OUTPUT PROTECTION -->
<!-- ================================================================== -->
<part name="F2" value="15A" device="BLADE"/>
<part name="D5" value="SMBJ28A" device="SMB"/>
<part name="C4" value="470uF/35V" device="ELECTROLYTIC"/>
<part name="C5" value="100nF" device="0805"/>

<!-- ================================================================== -->
<!-- OUTPUT CONNECTOR -->
<!-- ================================================================== -->
<part name="J3" value="XPC-3PIN" device="PHOENIX"/>

<!-- ================================================================== -->
<!-- STATUS LEDS -->
<!-- ================================================================== -->
<part name="LED1" value="GREEN" device="5MM"/>
<part name="LED2" value="YELLOW" device="5MM"/>
<part name="LED3" value="RED" device="5MM"/>
<part name="R7" value="1K" device="0603"/>
<part name="R8" value="1K" device="0603"/>
<part name="R9" value="1K" device="0603"/>

</parts>
<sheets>
<sheet>
<plain>
<!-- Title Block -->
<text x="200" y="10" size="3.81" layer="94">POWER SUPPLY UNIT</text>
<text x="200" y="5" size="2.54" layer="94">24V DC with 7S Li-ion Backup</text>
<text x="280" y="10" size="2.54" layer="94">Rev 1.0</text>
<text x="280" y="5" size="1.778" layer="94">Sheet 1 of 1</text>

<!-- Section Labels -->
<text x="10" y="180" size="2.54" layer="94">AC INPUT</text>
<text x="80" y="180" size="2.54" layer="94">AC-DC CONVERTER</text>
<text x="10" y="130" size="2.54" layer="94">BATTERY CHARGER</text>
<text x="120" y="130" size="2.54" layer="94">POWER PATH CONTROL</text>
<text x="10" y="80" size="2.54" layer="94">AC SENSE / POWER FAIL</text>
<text x="120" y="80" size="2.54" layer="94">OUTPUT</text>
<text x="200" y="130" size="2.54" layer="94">BATTERY PACK</text>

<!-- Notes -->
<text x="10" y="20" size="1.778" layer="94">NOTES:</text>
<text x="10" y="16" size="1.524" layer="94">1. AC Input: 100-240VAC, 50/60Hz</text>
<text x="10" y="12" size="1.524" layer="94">2. DC Output: 24V @ 2A continuous, 5A peak</text>
<text x="10" y="8" size="1.524" layer="94">3. Battery: 7S Li-ion (21V-29.4V), 25.9V nominal</text>
<text x="10" y="4" size="1.524" layer="94">4. Power Fail Signal: Active LOW when on battery</text>
<text x="120" y="16" size="1.524" layer="94">5. Auto-switchover time: &lt;10ms</text>
<text x="120" y="12" size="1.524" layer="94">6. Charge current: 2A max</text>
<text x="120" y="8" size="1.524" layer="94">7. LED1=Mains OK, LED2=Charging, LED3=Battery Low</text>
</plain>
<instances>
</instances>
<busses>
</busses>
<nets>
<!-- ================================================================== -->
<!-- AC MAINS INPUT -->
<!-- ================================================================== -->
<net name="AC_L" class="3">
<segment>
<pinref part="J1" gate="G$1" pin="L"/>
<pinref part="F1" gate="G$1" pin="1"/>
<wire x1="20" y1="170" x2="40" y2="170" width="1.524" layer="91"/>
<label x="25" y="172" size="1.778" layer="95"/>
</segment>
</net>
<net name="AC_L_FUSED" class="3">
<segment>
<pinref part="F1" gate="G$1" pin="2"/>
<pinref part="SW1" gate="G$1" pin="1"/>
<pinref part="MOV1" gate="G$1" pin="1"/>
<wire x1="50" y1="170" x2="60" y2="170" width="1.524" layer="91"/>
<label x="52" y="172" size="1.778" layer="95"/>
</segment>
</net>
<net name="AC_N" class="3">
<segment>
<pinref part="J1" gate="G$1" pin="N"/>
<pinref part="SW1" gate="G$1" pin="3"/>
<pinref part="MOV1" gate="G$1" pin="2"/>
<wire x1="20" y1="160" x2="60" y2="160" width="1.524" layer="91"/>
<label x="25" y="162" size="1.778" layer="95"/>
</segment>
</net>
<net name="AC_L_SW" class="3">
<segment>
<pinref part="SW1" gate="G$1" pin="2"/>
<pinref part="PS1" gate="G$1" pin="L"/>
<pinref part="R4" gate="G$1" pin="1"/>
<wire x1="70" y1="170" x2="90" y2="170" width="1.524" layer="91"/>
<label x="75" y="172" size="1.778" layer="95"/>
</segment>
</net>
<net name="AC_N_SW" class="3">
<segment>
<pinref part="SW1" gate="G$1" pin="4"/>
<pinref part="PS1" gate="G$1" pin="N"/>
<pinref part="R5" gate="G$1" pin="1"/>
<wire x1="70" y1="160" x2="90" y2="160" width="0.254" layer="91"/>
<label x="75" y="162" size="1.778" layer="95"/>
</segment>
</net>
<net name="PE" class="3">
<segment>
<pinref part="J1" gate="G$1" pin="PE"/>
<pinref part="PS1" gate="G$1" pin="PE"/>
<wire x1="20" y1="150" x2="90" y2="150" width="1.524" layer="91"/>
<label x="50" y="152" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- DC FROM AC-DC CONVERTER -->
<!-- ================================================================== -->
<net name="DC_24V_PSU" class="2">
<segment>
<pinref part="PS1" gate="G$1" pin="+V"/>
<pinref part="U3" gate="G$1" pin="VIN1"/>
<pinref part="U2" gate="G$1" pin="VIN"/>
<wire x1="130" y1="170" x2="150" y2="170" width="1.016" layer="91"/>
<label x="135" y="172" size="1.778" layer="95"/>
</segment>
</net>
<net name="DC_GND" class="2">
<segment>
<pinref part="PS1" gate="G$1" pin="-V"/>
<pinref part="U1" gate="G$1" pin="B-"/>
<pinref part="U2" gate="G$1" pin="GND"/>
<pinref part="U3" gate="G$1" pin="GND"/>
<pinref part="J3" gate="G$1" pin="2"/>
<wire x1="130" y1="150" x2="250" y2="150" width="1.016" layer="91"/>
<label x="180" y="152" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- BATTERY CONNECTIONS -->
<!-- ================================================================== -->
<net name="BAT+" class="2">
<segment>
<pinref part="BT1" gate="G$1" pin="+"/>
<pinref part="U1" gate="G$1" pin="B+"/>
<pinref part="J2" gate="G$1" pin="1"/>
<wire x1="200" y1="140" x2="220" y2="140" width="1.016" layer="91"/>
<label x="205" y="142" size="1.778" layer="95"/>
</segment>
</net>
<net name="BAT-" class="2">
<segment>
<pinref part="BT1" gate="G$1" pin="-"/>
<pinref part="U1" gate="G$1" pin="B-"/>
<pinref part="J2" gate="G$1" pin="2"/>
<wire x1="200" y1="130" x2="220" y2="130" width="1.016" layer="91"/>
<label x="205" y="132" size="1.778" layer="95"/>
</segment>
</net>
<net name="BAT_PACK+" class="2">
<segment>
<pinref part="U1" gate="G$1" pin="P+"/>
<pinref part="U3" gate="G$1" pin="VIN2"/>
<wire x1="180" y1="140" x2="150" y2="135" width="1.016" layer="91"/>
<label x="155" y="137" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- CHARGER OUTPUT -->
<!-- ================================================================== -->
<net name="CHG_OUT" class="1">
<segment>
<pinref part="U2" gate="G$1" pin="VOUT"/>
<pinref part="U1" gate="G$1" pin="C+"/>
<wire x1="80" y1="120" x2="180" y2="135" width="0.4064" layer="91"/>
<label x="100" y="125" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- POWER PATH OUTPUT -->
<!-- ================================================================== -->
<net name="DC_24V_OUT" class="2">
<segment>
<pinref part="U3" gate="G$1" pin="VOUT"/>
<pinref part="F2" gate="G$1" pin="1"/>
<wire x1="170" y1="170" x2="190" y2="170" width="1.016" layer="91"/>
<label x="175" y="172" size="1.778" layer="95"/>
</segment>
</net>
<net name="DC_24V_FUSED" class="2">
<segment>
<pinref part="F2" gate="G$1" pin="2"/>
<pinref part="D5" gate="G$1" pin="A"/>
<pinref part="C4" gate="G$1" pin="+"/>
<pinref part="J3" gate="G$1" pin="1"/>
<wire x1="210" y1="170" x2="250" y2="170" width="1.016" layer="91"/>
<label x="220" y="172" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- AC SENSE / POWER FAIL SIGNAL -->
<!-- ================================================================== -->
<net name="AC_SENSE" class="0">
<segment>
<pinref part="R4" gate="G$1" pin="2"/>
<pinref part="R5" gate="G$1" pin="2"/>
<pinref part="D4" gate="G$1" pin="A"/>
<pinref part="U4" gate="G$1" pin="A"/>
<wire x1="50" y1="90" x2="60" y2="90" width="0.254" layer="91"/>
<label x="52" y="92" size="1.778" layer="95"/>
</segment>
</net>
<net name="POWER_FAIL" class="0">
<segment>
<pinref part="U4" gate="G$1" pin="C"/>
<pinref part="R6" gate="G$1" pin="1"/>
<pinref part="C3" gate="G$1" pin="1"/>
<pinref part="J3" gate="G$1" pin="3"/>
<wire x1="80" y1="90" x2="250" y2="90" width="0.254" layer="91"/>
<label x="150" y="92" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- STATUS LEDS -->
<!-- ================================================================== -->
<net name="LED_MAINS" class="0">
<segment>
<pinref part="R7" gate="G$1" pin="1"/>
<pinref part="LED1" gate="G$1" pin="A"/>
<wire x1="130" y1="70" x2="140" y2="70" width="0.254" layer="91"/>
<label x="132" y="72" size="1.778" layer="95"/>
</segment>
</net>
<net name="LED_CHARGING" class="0">
<segment>
<pinref part="R8" gate="G$1" pin="1"/>
<pinref part="LED2" gate="G$1" pin="A"/>
<wire x1="150" y1="70" x2="160" y2="70" width="0.254" layer="91"/>
<label x="152" y="72" size="1.778" layer="95"/>
</segment>
</net>
<net name="LED_BAT_LOW" class="0">
<segment>
<pinref part="R9" gate="G$1" pin="1"/>
<pinref part="LED3" gate="G$1" pin="A"/>
<wire x1="170" y1="70" x2="180" y2="70" width="0.254" layer="91"/>
<label x="172" y="72" size="1.778" layer="95"/>
</segment>
</net>

</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>

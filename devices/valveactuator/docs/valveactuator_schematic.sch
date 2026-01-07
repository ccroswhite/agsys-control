<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<!--
  Valve Actuator Schematic
  Target: Nordic nRF52810 + MCP2515 CAN + Discrete H-Bridge
  
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
<class number="2" name="highcurrent" width="0.762" drill="0.4064"/>
</classes>
<parts>
<!-- ================================================================== -->
<!-- MICROCONTROLLER - nRF52810 -->
<!-- ================================================================== -->
<part name="U1" value="nRF52810-QFAA" device="QFN32"/>

<!-- ================================================================== -->
<!-- CAN BUS - MCP2515 + SN65HVD230 -->
<!-- ================================================================== -->
<part name="U2" value="MCP2515-I/SO" device="SOIC-18"/>
<part name="U3" value="SN65HVD230DR" device="SOIC-8"/>
<part name="Y1" value="16MHz" device="HC49"/>
<part name="C10" value="22pF" device="0603"/>
<part name="C11" value="22pF" device="0603"/>
<part name="R10" value="120R" device="0603"/>

<!-- ================================================================== -->
<!-- POWER - BUCK CONVERTER (24V to 3.3V) -->
<!-- ================================================================== -->
<part name="U4" value="TPS54202DDCR" device="SOT-23-6"/>
<part name="L1" value="10uH" device="1210"/>
<part name="C1" value="10uF/50V" device="0805"/>
<part name="C2" value="22uF/10V" device="0805"/>
<part name="R1" value="100K" device="0603"/>
<part name="R2" value="31.6K" device="0603"/>

<!-- ================================================================== -->
<!-- H-BRIDGE - DISCRETE MOSFETS -->
<!-- ================================================================== -->
<!-- High-side P-channel -->
<part name="Q1" value="AO3401A" device="SOT-23"/>
<part name="Q2" value="AO3401A" device="SOT-23"/>
<!-- Low-side N-channel -->
<part name="Q3" value="AO3400A" device="SOT-23"/>
<part name="Q4" value="AO3400A" device="SOT-23"/>
<!-- Flyback diodes -->
<part name="D1" value="SS14" device="SMA"/>
<part name="D2" value="SS14" device="SMA"/>
<part name="D3" value="SS14" device="SMA"/>
<part name="D4" value="SS14" device="SMA"/>
<!-- Gate resistors -->
<part name="R3" value="100R" device="0603"/>
<part name="R4" value="100R" device="0603"/>
<part name="R5" value="100R" device="0603"/>
<part name="R6" value="100R" device="0603"/>
<!-- Current sense -->
<part name="R7" value="0.1R/1W" device="2512"/>

<!-- ================================================================== -->
<!-- SURGE PROTECTION -->
<!-- ================================================================== -->
<part name="D5" value="SMBJ28A" device="SMB"/>
<part name="D6" value="SMBJ28A" device="SMB"/>
<part name="D7" value="SMBJ5.0A" device="SMB"/>
<part name="D8" value="SMBJ5.0A" device="SMB"/>
<part name="F1" value="2A PTC" device="1206"/>

<!-- ================================================================== -->
<!-- STATUS LEDS -->
<!-- ================================================================== -->
<part name="LED1" value="GREEN" device="0603"/>
<part name="LED2" value="YELLOW" device="0603"/>
<part name="LED3" value="RED" device="0603"/>
<part name="LED4" value="BLUE" device="0603"/>
<part name="R11" value="1K" device="0603"/>
<part name="R12" value="10K" device="0603"/>
<part name="R13" value="1K" device="0603"/>
<part name="R14" value="1K" device="0603"/>

<!-- ================================================================== -->
<!-- DIP SWITCH - 10 POSITION -->
<!-- ================================================================== -->
<part name="SW1" value="DIP-10" device="SMD"/>

<!-- ================================================================== -->
<!-- CONNECTORS -->
<!-- ================================================================== -->
<part name="J1" value="XPC-5PIN" device="PHOENIX"/>
<part name="J2" value="XPC-4PIN" device="PHOENIX"/>
<part name="J3" value="XPC-4PIN" device="PHOENIX"/>

<!-- ================================================================== -->
<!-- DECOUPLING CAPACITORS -->
<!-- ================================================================== -->
<part name="C3" value="100nF" device="0603"/>
<part name="C4" value="100nF" device="0603"/>
<part name="C5" value="100nF" device="0603"/>
<part name="C6" value="100nF" device="0603"/>
<part name="C7" value="100nF" device="0603"/>

</parts>
<sheets>
<sheet>
<plain>
<!-- Title Block -->
<text x="200" y="10" size="3.81" layer="94">VALVE ACTUATOR</text>
<text x="200" y="5" size="2.54" layer="94">nRF52810 + CAN + H-Bridge</text>
<text x="280" y="10" size="2.54" layer="94">Rev 1.0</text>
<text x="280" y="5" size="1.778" layer="94">Sheet 1 of 1</text>

<!-- Section Labels -->
<text x="10" y="180" size="2.54" layer="94">MICROCONTROLLER</text>
<text x="120" y="180" size="2.54" layer="94">CAN BUS</text>
<text x="10" y="130" size="2.54" layer="94">POWER (24V to 3.3V)</text>
<text x="120" y="130" size="2.54" layer="94">H-BRIDGE</text>
<text x="10" y="80" size="2.54" layer="94">STATUS LEDS</text>
<text x="120" y="80" size="2.54" layer="94">DIP SWITCH / CONNECTORS</text>
<text x="200" y="130" size="2.54" layer="94">SURGE PROTECTION</text>

<!-- Notes -->
<text x="10" y="20" size="1.778" layer="94">NOTES:</text>
<text x="10" y="16" size="1.524" layer="94">1. DIP SW 1-6: Address (1-63), SW 10: CAN Termination</text>
<text x="10" y="12" size="1.524" layer="94">2. H-Bridge: AO3401 (P-ch high), AO3400 (N-ch low)</text>
<text x="10" y="8" size="1.524" layer="94">3. Current sense: 0.1R shunt, 100mV/A</text>
<text x="10" y="4" size="1.524" layer="94">4. TVS protection on all valve connections</text>
</plain>
<instances>
</instances>
<busses>
</busses>
<nets>
<!-- ================================================================== -->
<!-- POWER NETS -->
<!-- ================================================================== -->
<!-- 24V Input -->
<net name="VIN_24V" class="2">
<segment>
<pinref part="J2" gate="G$1" pin="3"/>
<pinref part="J3" gate="G$1" pin="3"/>
<pinref part="U4" gate="G$1" pin="VIN"/>
<pinref part="F1" gate="G$1" pin="1"/>
<pinref part="Q1" gate="G$1" pin="S"/>
<pinref part="Q2" gate="G$1" pin="S"/>
<wire x1="130" y1="160" x2="200" y2="160" width="0.762" layer="91"/>
<label x="150" y="162" size="1.778" layer="95"/>
</segment>
</net>
<!-- 3.3V Rail -->
<net name="VCC" class="1">
<segment>
<pinref part="U4" gate="G$1" pin="VOUT"/>
<pinref part="U1" gate="G$1" pin="VDD"/>
<pinref part="U2" gate="G$1" pin="VDD"/>
<pinref part="U3" gate="G$1" pin="VCC"/>
<wire x1="50" y1="140" x2="150" y2="140" width="0.4064" layer="91"/>
<label x="80" y="142" size="1.778" layer="95"/>
</segment>
</net>
<!-- Ground -->
<net name="GND" class="1">
<segment>
<pinref part="U1" gate="G$1" pin="GND"/>
<pinref part="U2" gate="G$1" pin="VSS"/>
<pinref part="U3" gate="G$1" pin="GND"/>
<pinref part="U4" gate="G$1" pin="GND"/>
<pinref part="J2" gate="G$1" pin="4"/>
<pinref part="J3" gate="G$1" pin="4"/>
<pinref part="R7" gate="G$1" pin="2"/>
<wire x1="50" y1="10" x2="200" y2="10" width="0.4064" layer="91"/>
<label x="100" y="12" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- SPI BUS -->
<!-- ================================================================== -->
<net name="SPI_SCK" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.14"/>
<pinref part="U2" gate="G$1" pin="SCK"/>
<wire x1="80" y1="170" x2="130" y2="170" width="0.254" layer="91"/>
<label x="90" y="172" size="1.778" layer="95"/>
</segment>
</net>
<net name="SPI_MOSI" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.12"/>
<pinref part="U2" gate="G$1" pin="SI"/>
<wire x1="80" y1="165" x2="130" y2="165" width="0.254" layer="91"/>
<label x="90" y="167" size="1.778" layer="95"/>
</segment>
</net>
<net name="SPI_MISO" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.13"/>
<pinref part="U2" gate="G$1" pin="SO"/>
<wire x1="80" y1="160" x2="130" y2="160" width="0.254" layer="91"/>
<label x="90" y="162" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.11"/>
<pinref part="U2" gate="G$1" pin="CS"/>
<wire x1="80" y1="155" x2="130" y2="175" width="0.254" layer="91"/>
<label x="90" y="160" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_INT" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.08"/>
<pinref part="U2" gate="G$1" pin="INT"/>
<wire x1="80" y1="150" x2="130" y2="180" width="0.254" layer="91"/>
<label x="90" y="155" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- CAN BUS SIGNALS -->
<!-- ================================================================== -->
<net name="CAN_TX" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="TXCAN"/>
<pinref part="U3" gate="G$1" pin="D"/>
<wire x1="150" y1="175" x2="170" y2="175" width="0.254" layer="91"/>
<label x="155" y="177" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_RX" class="0">
<segment>
<pinref part="U2" gate="G$1" pin="RXCAN"/>
<pinref part="U3" gate="G$1" pin="R"/>
<wire x1="150" y1="170" x2="170" y2="170" width="0.254" layer="91"/>
<label x="155" y="172" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_H" class="0">
<segment>
<pinref part="U3" gate="G$1" pin="CANH"/>
<pinref part="J2" gate="G$1" pin="1"/>
<pinref part="J3" gate="G$1" pin="1"/>
<pinref part="R10" gate="G$1" pin="1"/>
<wire x1="190" y1="175" x2="220" y2="175" width="0.254" layer="91"/>
<label x="200" y="177" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_L" class="0">
<segment>
<pinref part="U3" gate="G$1" pin="CANL"/>
<pinref part="J2" gate="G$1" pin="2"/>
<pinref part="J3" gate="G$1" pin="2"/>
<pinref part="R10" gate="G$1" pin="2"/>
<wire x1="190" y1="170" x2="220" y2="170" width="0.254" layer="91"/>
<label x="200" y="172" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- H-BRIDGE CONTROL -->
<!-- ================================================================== -->
<net name="HBRIDGE_A" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.03"/>
<pinref part="R3" gate="G$1" pin="1"/>
<wire x1="80" y1="130" x2="130" y2="130" width="0.254" layer="91"/>
<label x="90" y="132" size="1.778" layer="95"/>
</segment>
</net>
<net name="HBRIDGE_B" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.04"/>
<pinref part="R4" gate="G$1" pin="1"/>
<wire x1="80" y1="125" x2="130" y2="125" width="0.254" layer="91"/>
<label x="90" y="127" size="1.778" layer="95"/>
</segment>
</net>
<net name="HBRIDGE_EN_A" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.05"/>
<pinref part="R5" gate="G$1" pin="1"/>
<wire x1="80" y1="120" x2="130" y2="120" width="0.254" layer="91"/>
<label x="90" y="122" size="1.778" layer="95"/>
</segment>
</net>
<net name="HBRIDGE_EN_B" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.06"/>
<pinref part="R6" gate="G$1" pin="1"/>
<wire x1="80" y1="115" x2="130" y2="115" width="0.254" layer="91"/>
<label x="90" y="117" size="1.778" layer="95"/>
</segment>
</net>
<!-- High-side gate drives -->
<net name="GATE_Q1" class="0">
<segment>
<pinref part="R3" gate="G$1" pin="2"/>
<pinref part="Q1" gate="G$1" pin="G"/>
<wire x1="140" y1="130" x2="150" y2="150" width="0.254" layer="91"/>
</segment>
</net>
<net name="GATE_Q2" class="0">
<segment>
<pinref part="R4" gate="G$1" pin="2"/>
<pinref part="Q2" gate="G$1" pin="G"/>
<wire x1="140" y1="125" x2="170" y2="150" width="0.254" layer="91"/>
</segment>
</net>
<!-- Low-side gate drives -->
<net name="GATE_Q3" class="0">
<segment>
<pinref part="R5" gate="G$1" pin="2"/>
<pinref part="Q3" gate="G$1" pin="G"/>
<wire x1="140" y1="120" x2="150" y2="110" width="0.254" layer="91"/>
</segment>
</net>
<net name="GATE_Q4" class="0">
<segment>
<pinref part="R6" gate="G$1" pin="2"/>
<pinref part="Q4" gate="G$1" pin="G"/>
<wire x1="140" y1="115" x2="170" y2="110" width="0.254" layer="91"/>
</segment>
</net>
<!-- Motor outputs -->
<net name="MOTOR_A" class="2">
<segment>
<pinref part="Q1" gate="G$1" pin="D"/>
<pinref part="Q3" gate="G$1" pin="D"/>
<pinref part="D1" gate="G$1" pin="A"/>
<pinref part="D3" gate="G$1" pin="K"/>
<pinref part="D5" gate="G$1" pin="A"/>
<pinref part="J1" gate="G$1" pin="1"/>
<wire x1="150" y1="140" x2="150" y2="120" width="0.762" layer="91"/>
<wire x1="150" y1="130" x2="200" y2="130" width="0.762" layer="91"/>
<label x="160" y="132" size="1.778" layer="95"/>
</segment>
</net>
<net name="MOTOR_B" class="2">
<segment>
<pinref part="Q2" gate="G$1" pin="D"/>
<pinref part="Q4" gate="G$1" pin="D"/>
<pinref part="D2" gate="G$1" pin="A"/>
<pinref part="D4" gate="G$1" pin="K"/>
<pinref part="D6" gate="G$1" pin="A"/>
<pinref part="J1" gate="G$1" pin="2"/>
<wire x1="170" y1="140" x2="170" y2="120" width="0.762" layer="91"/>
<wire x1="170" y1="130" x2="200" y2="125" width="0.762" layer="91"/>
<label x="175" y="127" size="1.778" layer="95"/>
</segment>
</net>
<!-- Current sense -->
<net name="CURRENT_SENSE" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.02/AIN0"/>
<pinref part="R7" gate="G$1" pin="1"/>
<wire x1="80" y1="110" x2="160" y2="100" width="0.254" layer="91"/>
<label x="100" y="105" size="1.778" layer="95"/>
</segment>
</net>
<!-- Low-side common to current sense -->
<net name="MOTOR_GND" class="2">
<segment>
<pinref part="Q3" gate="G$1" pin="S"/>
<pinref part="Q4" gate="G$1" pin="S"/>
<pinref part="R7" gate="G$1" pin="1"/>
<wire x1="150" y1="100" x2="170" y2="100" width="0.762" layer="91"/>
<label x="155" y="102" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- LIMIT SWITCHES -->
<!-- ================================================================== -->
<net name="LIMIT_OPEN" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.09"/>
<pinref part="D7" gate="G$1" pin="A"/>
<pinref part="J1" gate="G$1" pin="3"/>
<wire x1="80" y1="100" x2="200" y2="115" width="0.254" layer="91"/>
<label x="120" y="108" size="1.778" layer="95"/>
</segment>
</net>
<net name="LIMIT_CLOSED" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.10"/>
<pinref part="D8" gate="G$1" pin="A"/>
<pinref part="J1" gate="G$1" pin="4"/>
<wire x1="80" y1="95" x2="200" y2="110" width="0.254" layer="91"/>
<label x="120" y="102" size="1.778" layer="95"/>
</segment>
</net>
<net name="LIMIT_COM" class="0">
<segment>
<pinref part="J1" gate="G$1" pin="5"/>
<wire x1="200" y1="105" x2="210" y2="105" width="0.254" layer="91"/>
<label x="205" y="107" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- DIP SWITCH (ADDRESS + TERMINATION) -->
<!-- ================================================================== -->
<net name="DIP_1" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.15"/>
<pinref part="SW1" gate="G$1" pin="1"/>
<wire x1="80" y1="85" x2="130" y2="70" width="0.254" layer="91"/>
<label x="90" y="80" size="1.778" layer="95"/>
</segment>
</net>
<net name="DIP_2" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.16"/>
<pinref part="SW1" gate="G$1" pin="2"/>
<wire x1="80" y1="82" x2="135" y2="70" width="0.254" layer="91"/>
</segment>
</net>
<net name="DIP_3" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.17"/>
<pinref part="SW1" gate="G$1" pin="3"/>
<wire x1="80" y1="79" x2="140" y2="70" width="0.254" layer="91"/>
</segment>
</net>
<net name="DIP_4" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.18"/>
<pinref part="SW1" gate="G$1" pin="4"/>
<wire x1="80" y1="76" x2="145" y2="70" width="0.254" layer="91"/>
</segment>
</net>
<net name="DIP_5" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.19"/>
<pinref part="SW1" gate="G$1" pin="5"/>
<wire x1="80" y1="73" x2="150" y2="70" width="0.254" layer="91"/>
</segment>
</net>
<net name="DIP_6" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.20"/>
<pinref part="SW1" gate="G$1" pin="6"/>
<wire x1="80" y1="70" x2="155" y2="70" width="0.254" layer="91"/>
</segment>
</net>
<net name="DIP_10" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.24"/>
<pinref part="SW1" gate="G$1" pin="10"/>
<pinref part="R10" gate="G$1" pin="EN"/>
<wire x1="80" y1="60" x2="175" y2="70" width="0.254" layer="91"/>
<label x="120" y="65" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- STATUS LEDS -->
<!-- ================================================================== -->
<net name="LED_3V3" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.25"/>
<pinref part="R11" gate="G$1" pin="1"/>
<wire x1="80" y1="55" x2="40" y2="70" width="0.254" layer="91"/>
<label x="50" y="62" size="1.778" layer="95"/>
</segment>
</net>
<net name="LED_24V" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.26"/>
<pinref part="R12" gate="G$1" pin="1"/>
<wire x1="80" y1="50" x2="50" y2="70" width="0.254" layer="91"/>
<label x="55" y="60" size="1.778" layer="95"/>
</segment>
</net>
<net name="LED_STATUS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.27"/>
<pinref part="R13" gate="G$1" pin="1"/>
<wire x1="80" y1="45" x2="60" y2="70" width="0.254" layer="91"/>
<label x="62" y="57" size="1.778" layer="95"/>
</segment>
</net>
<net name="LED_VALVE_OPEN" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.28"/>
<pinref part="R14" gate="G$1" pin="1"/>
<wire x1="80" y1="40" x2="70" y2="70" width="0.254" layer="91"/>
<label x="72" y="55" size="1.778" layer="95"/>
</segment>
</net>

</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>

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
<!-- MICROCONTROLLER - nRF52810-QFAA -->
<!-- ================================================================== -->
<part name="U1" value="nRF52810-QFAA" device="QFN32"/>

<!-- nRF52810 32MHz Crystal + Load Caps (optional - can use internal RC) -->
<part name="Y2" value="32MHz" device="2520" value="32MHz"/>
<part name="C20" value="12pF" device="0402"/>
<part name="C21" value="12pF" device="0402"/>

<!-- nRF52810 Decoupling (per datasheet) -->
<part name="C22" value="100nF" device="0402"/>
<part name="C23" value="100nF" device="0402"/>
<part name="C24" value="10uF" device="0805"/>
<part name="C25" value="100nF" device="0402"/>
<part name="C26" value="10nF" device="0402"/>
<part name="C27" value="1uF" device="0402"/>
<part name="L2" value="10nH" device="0402"/>

<!-- ================================================================== -->
<!-- CAN BUS - MCP2515 + SN65HVD230 -->
<!-- ================================================================== -->
<part name="U2" value="MCP2515-I/SO" device="SOIC-18"/>
<part name="U3" value="SN65HVD230DR" device="SOIC-8"/>
<part name="Y1" value="16MHz" device="HC49"/>
<part name="C10" value="22pF" device="0402"/>
<part name="C11" value="22pF" device="0402"/>
<part name="R10" value="120R" device="0603"/>
<part name="C12" value="100nF" device="0402"/>
<part name="C13" value="100nF" device="0402"/>

<!-- ================================================================== -->
<!-- MEMORY - FRAM + FLASH (STANDARD PINS) -->
<!-- ================================================================== -->
<part name="U5" value="MB85RS1MTPNF" device="SOIC-8"/>
<part name="C40" value="100nF" device="0402"/>
<part name="U6" value="W25Q16JVSSIQ" device="SOIC-8"/>
<part name="C41" value="100nF" device="0402"/>

<!-- ================================================================== -->
<!-- POWER - BUCK CONVERTER (24V to 3.3V) + LDO (3.3V to 2.5V) -->
<!-- ================================================================== -->
<part name="U4" value="TPS54202DDCR" device="SOT-23-6"/>
<part name="L1" value="10uH" device="1210"/>
<part name="C1" value="10uF/50V" device="0805"/>
<part name="C2" value="22uF/10V" device="0805"/>
<part name="R1" value="100K" device="0603"/>
<part name="R2" value="31.6K" device="0603"/>
<part name="C8" value="100nF" device="0402"/>
<!-- LDO: 3.3V to 2.5V for MCU (TI TLV733, 300mA, 16µA Iq) -->
<part name="U8" value="TLV73325PDBVR" device="SOT-23-5"/>
<part name="C50" value="1uF" device="0402"/>
<part name="C51" value="1uF" device="0402"/>

<!-- ================================================================== -->
<!-- H-BRIDGE - DRV8876 INTEGRATED DRIVER -->
<!-- ================================================================== -->
<!-- DRV8876: 37V, 3.5A H-bridge with integrated current sensing -->
<part name="U7" value="DRV8876PWPR" device="HTSSOP-16"/>
<!-- DRV8876 decoupling -->
<part name="C30" value="100nF" device="0402"/>
<part name="C31" value="10uF/50V" device="0805"/>
<!-- Current limit reference (VREF sets max current: ITRIP = VREF / AIPROPI / RIPROPI) -->
<!-- For 2A limit with AIPROPI=1.2mA/A: VREF = 2A * 1.2mA/A * 1K = 2.4V -->
<!-- Use voltage divider from 3.3V: 3.3V * R_BOT/(R_TOP+R_BOT) = 2.4V -->
<!-- R_TOP=3.9K, R_BOT=10K gives 2.44V -->
<part name="R30" value="3.9K" device="0402"/>
<part name="R31" value="10K" device="0402"/>
<!-- IPROPI sense resistor (1K gives 1.2V/A output) -->
<part name="R32" value="1K" device="0402"/>
<!-- nSLEEP pull-up (active low sleep, pull high for normal operation) -->
<part name="R33" value="10K" device="0402"/>

<!-- ================================================================== -->
<!-- SURGE PROTECTION -->
<!-- ================================================================== -->
<part name="D5" value="SMBJ28A" device="SMB"/>
<part name="D6" value="SMBJ28A" device="SMB"/>
<part name="D7" value="SMBJ5.0A" device="SMB"/>
<part name="D8" value="SMBJ5.0A" device="SMB"/>
<part name="D9" value="PESD5V0S1BL" device="SOD323"/>
<part name="F1" value="2A PTC" device="1206"/>

<!-- ================================================================== -->
<!-- STATUS LEDS (P0.07, P0.21, P0.29, P0.30) -->
<!-- ================================================================== -->
<part name="LED1" value="GREEN" device="0603"/>
<part name="LED2" value="YELLOW" device="0603"/>
<part name="LED3" value="RED" device="0603"/>
<part name="LED4" value="BLUE" device="0603"/>
<part name="R11" value="1K" device="0402"/>
<part name="R12" value="1K" device="0402"/>
<part name="R13" value="1K" device="0402"/>
<part name="R14" value="1K" device="0402"/>

<!-- ================================================================== -->
<!-- DIP SWITCH - 10 POSITION + PULL-UPS -->
<!-- ================================================================== -->
<part name="SW1" value="DIP-10" device="SMD"/>
<part name="RN1" value="10K x 8" device="0603x4"/>
<part name="R22" value="10K" device="0402"/>
<part name="R23" value="10K" device="0402"/>

<!-- ================================================================== -->
<!-- PAIRING BUTTON (P0.31) -->
<!-- ================================================================== -->
<part name="SW2" value="TACTILE" device="6x6"/>
<part name="R15" value="10K" device="0402"/>
<part name="C9" value="100nF" device="0402"/>

<!-- ================================================================== -->
<!-- CONNECTORS -->
<!-- ================================================================== -->
<part name="J1" value="XPC-5PIN" device="PHOENIX"/>
<part name="J2" value="XPC-4PIN" device="PHOENIX"/>
<part name="J3" value="XPC-4PIN" device="PHOENIX"/>

<!-- ================================================================== -->
<!-- DECOUPLING CAPACITORS -->
<!-- ================================================================== -->
<part name="C3" value="100nF" device="0402"/>
<part name="C4" value="100nF" device="0402"/>
<part name="C5" value="100nF" device="0402"/>
<part name="C6" value="100nF" device="0402"/>
<part name="C7" value="100nF" device="0402"/>

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
<text x="200" y="80" size="2.54" layer="94">MEMORY (STANDARD PINS)</text>

<!-- Notes -->
<text x="10" y="20" size="1.778" layer="94">NOTES:</text>
<text x="10" y="16" size="1.524" layer="94">1. DIP SW 1-6: Address (1-63), SW 10: CAN Termination</text>
<text x="10" y="12" size="1.524" layer="94">2. H-Bridge: DRV8876 (37V, 3.5A integrated driver)</text>
<text x="10" y="8" size="1.524" layer="94">3. Current sense: IPROPI output, 1.2V/A with R32=1K</text>
<text x="10" y="4" size="1.524" layer="94">4. TVS protection on motor outputs, current limit via VREF</text>
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
<!-- 3.3V Rail (from buck, powers CAN, memory, LEDs, H-bridge logic) -->
<net name="VCC_3V3" class="1">
<segment>
<pinref part="U4" gate="G$1" pin="VOUT"/>
<pinref part="U8" gate="G$1" pin="VIN"/>
<pinref part="U2" gate="G$1" pin="VDD"/>
<pinref part="U3" gate="G$1" pin="VCC"/>
<pinref part="U5" gate="G$1" pin="VCC"/>
<pinref part="U6" gate="G$1" pin="VCC"/>
<pinref part="C50" gate="G$1" pin="1"/>
<wire x1="50" y1="140" x2="150" y2="140" width="0.4064" layer="91"/>
<label x="80" y="142" size="1.778" layer="95"/>
</segment>
</net>
<!-- 2.5V Rail (from LDO, powers MCU only) -->
<net name="VCC_2V5" class="1">
<segment>
<pinref part="U8" gate="G$1" pin="VOUT"/>
<pinref part="U1" gate="G$1" pin="VDD"/>
<pinref part="C51" gate="G$1" pin="1"/>
<wire x1="60" y1="135" x2="100" y2="135" width="0.4064" layer="91"/>
<label x="70" y="137" size="1.778" layer="95"/>
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
<!-- SPI BUS 0 - CAN -->
<!-- ================================================================== -->
<net name="CAN_SCK" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.14"/>
<pinref part="U2" gate="G$1" pin="SCK"/>
<wire x1="80" y1="170" x2="130" y2="170" width="0.254" layer="91"/>
<label x="90" y="172" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_MOSI" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.12"/>
<pinref part="U2" gate="G$1" pin="SI"/>
<wire x1="80" y1="165" x2="130" y2="165" width="0.254" layer="91"/>
<label x="90" y="167" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_MISO" class="0">
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
<!-- DRV8876 H-BRIDGE CONTROL -->
<!-- ================================================================== -->
<!-- Motor control inputs (directly from MCU GPIO) -->
<net name="DRV_IN1" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.03"/>
<pinref part="U7" gate="G$1" pin="IN1"/>
<wire x1="80" y1="130" x2="130" y2="130" width="0.254" layer="91"/>
<label x="90" y="132" size="1.778" layer="95"/>
</segment>
</net>
<net name="DRV_IN2" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.04"/>
<pinref part="U7" gate="G$1" pin="IN2"/>
<wire x1="80" y1="125" x2="130" y2="125" width="0.254" layer="91"/>
<label x="90" y="127" size="1.778" layer="95"/>
</segment>
</net>
<!-- Sleep control (directly from MCU GPIO, active low) -->
<net name="DRV_nSLEEP" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.05"/>
<pinref part="U7" gate="G$1" pin="nSLEEP"/>
<pinref part="R33" gate="G$1" pin="1"/>
<wire x1="80" y1="120" x2="130" y2="120" width="0.254" layer="91"/>
<label x="90" y="122" size="1.778" layer="95"/>
</segment>
</net>
<!-- Fault output (active low, open drain) -->
<net name="DRV_nFAULT" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.06"/>
<pinref part="U7" gate="G$1" pin="nFAULT"/>
<wire x1="80" y1="115" x2="130" y2="115" width="0.254" layer="91"/>
<label x="90" y="117" size="1.778" layer="95"/>
</segment>
</net>
<!-- Motor outputs from DRV8876 -->
<net name="MOTOR_A" class="2">
<segment>
<pinref part="U7" gate="G$1" pin="OUT1"/>
<pinref part="D5" gate="G$1" pin="A"/>
<pinref part="J1" gate="G$1" pin="1"/>
<wire x1="150" y1="130" x2="200" y2="130" width="0.762" layer="91"/>
<label x="160" y="132" size="1.778" layer="95"/>
</segment>
</net>
<net name="MOTOR_B" class="2">
<segment>
<pinref part="U7" gate="G$1" pin="OUT2"/>
<pinref part="D6" gate="G$1" pin="A"/>
<pinref part="J1" gate="G$1" pin="2"/>
<wire x1="150" y1="125" x2="200" y2="125" width="0.762" layer="91"/>
<label x="160" y="127" size="1.778" layer="95"/>
</segment>
</net>
<!-- Current sense output (IPROPI: 1.2mA/A through R32=1K gives 1.2V/A) -->
<net name="CURRENT_SENSE" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.02/AIN0"/>
<pinref part="U7" gate="G$1" pin="IPROPI"/>
<pinref part="R32" gate="G$1" pin="1"/>
<wire x1="80" y1="110" x2="130" y2="110" width="0.254" layer="91"/>
<label x="90" y="112" size="1.778" layer="95"/>
</segment>
</net>
<!-- IPROPI resistor to GND -->
<net name="IPROPI_GND" class="0">
<segment>
<pinref part="R32" gate="G$1" pin="2"/>
<wire x1="140" y1="110" x2="140" y2="100" width="0.254" layer="91"/>
<label x="142" y="102" size="1.778" layer="95"/>
</segment>
</net>
<!-- Current limit reference voltage (VREF) -->
<net name="DRV_VREF" class="0">
<segment>
<pinref part="U7" gate="G$1" pin="VREF"/>
<pinref part="R30" gate="G$1" pin="2"/>
<pinref part="R31" gate="G$1" pin="1"/>
<wire x1="130" y1="105" x2="145" y2="105" width="0.254" layer="91"/>
<label x="135" y="107" size="1.778" layer="95"/>
</segment>
</net>
<!-- DRV8876 motor supply -->
<net name="VM_24V" class="2">
<segment>
<pinref part="U7" gate="G$1" pin="VM"/>
<pinref part="C31" gate="G$1" pin="1"/>
<wire x1="130" y1="140" x2="130" y2="160" width="0.762" layer="91"/>
<label x="132" y="150" size="1.778" layer="95"/>
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
<pinref part="U1" gate="G$1" pin="P0.28"/>
<pinref part="SW1" gate="G$1" pin="10"/>
<pinref part="R10" gate="G$1" pin="EN"/>
<wire x1="80" y1="60" x2="175" y2="70" width="0.254" layer="91"/>
<label x="120" y="65" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- STATUS LEDS - Moved to avoid conflict with memory bus -->
<!-- ================================================================== -->
<net name="LED_POWER" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.07"/>
<pinref part="R11" gate="G$1" pin="1"/>
<wire x1="80" y1="45" x2="40" y2="70" width="0.254" layer="91"/>
<label x="50" y="57" size="1.778" layer="95"/>
</segment>
</net>
<net name="LED_24V" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.21"/>
<pinref part="R12" gate="G$1" pin="1"/>
<wire x1="80" y1="40" x2="50" y2="70" width="0.254" layer="91"/>
<label x="55" y="55" size="1.778" layer="95"/>
</segment>
</net>
<net name="LED_STATUS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.29"/>
<pinref part="R13" gate="G$1" pin="1"/>
<wire x1="80" y1="35" x2="60" y2="70" width="0.254" layer="91"/>
<label x="62" y="52" size="1.778" layer="95"/>
</segment>
</net>
<net name="LED_VALVE_OPEN" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.30"/>
<pinref part="R14" gate="G$1" pin="1"/>
<wire x1="80" y1="30" x2="70" y2="70" width="0.254" layer="91"/>
<label x="72" y="50" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- SPI BUS 1 - MEMORY (STANDARD PINS P0.22-P0.26) -->
<!-- ================================================================== -->
<net name="MEM_SCK" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.26"/>
<wire x1="80" y1="50" x2="200" y2="75" width="0.254" layer="91"/>
<label x="180" y="77" size="1.778" layer="95"/>
</segment>
</net>
<net name="MEM_MOSI" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.25"/>
<wire x1="80" y1="55" x2="200" y2="70" width="0.254" layer="91"/>
<label x="180" y="72" size="1.778" layer="95"/>
</segment>
</net>
<net name="MEM_MISO" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.24"/>
<wire x1="80" y1="60" x2="200" y2="65" width="0.254" layer="91"/>
<label x="180" y="67" size="1.778" layer="95"/>
</segment>
</net>
<net name="FRAM_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.23"/>
<wire x1="80" y1="35" x2="200" y2="60" width="0.254" layer="91"/>
<label x="180" y="62" size="1.778" layer="95"/>
</segment>
</net>
<net name="FLASH_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.22"/>
<wire x1="80" y1="30" x2="200" y2="55" width="0.254" layer="91"/>
<label x="180" y="57" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- PAIRING BUTTON (P0.31) -->
<!-- ================================================================== -->
<net name="PAIRING_BTN" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.31"/>
<pinref part="SW2" gate="G$1" pin="1"/>
<pinref part="R15" gate="G$1" pin="1"/>
<pinref part="C9" gate="G$1" pin="1"/>
<pinref part="D9" gate="G$1" pin="A"/>
<wire x1="80" y1="25" x2="60" y2="40" width="0.254" layer="91"/>
<label x="65" y="32" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- nRF52810 CRYSTAL CIRCUIT (optional - can use internal RC) -->
<!-- ================================================================== -->
<net name="XC1" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="XC1"/>
<pinref part="Y2" gate="G$1" pin="1"/>
<pinref part="C20" gate="G$1" pin="1"/>
<wire x1="60" y1="175" x2="70" y2="175" width="0.254" layer="91"/>
</segment>
</net>
<net name="XC2" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="XC2"/>
<pinref part="Y2" gate="G$1" pin="2"/>
<pinref part="C21" gate="G$1" pin="1"/>
<wire x1="60" y1="170" x2="70" y2="170" width="0.254" layer="91"/>
</segment>
</net>

<!-- nRF52810 DEC pins -->
<net name="DEC1" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="DEC1"/>
<pinref part="C26" gate="G$1" pin="1"/>
<wire x1="60" y1="165" x2="65" y2="165" width="0.254" layer="91"/>
</segment>
</net>
<net name="DCC" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="DCC"/>
<pinref part="L2" gate="G$1" pin="1"/>
<wire x1="60" y1="160" x2="65" y2="160" width="0.254" layer="91"/>
</segment>
</net>
<net name="DCCH" class="0">
<segment>
<pinref part="L2" gate="G$1" pin="2"/>
<pinref part="C27" gate="G$1" pin="1"/>
<wire x1="75" y1="160" x2="80" y2="160" width="0.254" layer="91"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- MEMORY SPI BUS ACTIVE CONNECTIONS -->
<!-- ================================================================== -->
<net name="MEM_SCK_ACTIVE" class="0">
<segment>
<pinref part="U5" gate="G$1" pin="SCK"/>
<pinref part="U6" gate="G$1" pin="CLK"/>
<wire x1="200" y1="75" x2="220" y2="75" width="0.254" layer="91"/>
</segment>
</net>
<net name="MEM_MOSI_ACTIVE" class="0">
<segment>
<pinref part="U5" gate="G$1" pin="SI"/>
<pinref part="U6" gate="G$1" pin="DI"/>
<wire x1="200" y1="70" x2="220" y2="70" width="0.254" layer="91"/>
</segment>
</net>
<net name="MEM_MISO_ACTIVE" class="0">
<segment>
<pinref part="U5" gate="G$1" pin="SO"/>
<pinref part="U6" gate="G$1" pin="DO"/>
<wire x1="200" y1="65" x2="220" y2="65" width="0.254" layer="91"/>
</segment>
</net>
<net name="FRAM_CS_ACTIVE" class="0">
<segment>
<pinref part="U5" gate="G$1" pin="CS"/>
<wire x1="200" y1="60" x2="210" y2="60" width="0.254" layer="91"/>
</segment>
</net>
<net name="FLASH_CS_ACTIVE" class="0">
<segment>
<pinref part="U6" gate="G$1" pin="CS"/>
<wire x1="220" y1="55" x2="230" y2="55" width="0.254" layer="91"/>
</segment>
</net>

</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>

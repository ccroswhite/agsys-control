<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<!--
  Valve Controller Schematic
  Target: Nordic nRF52832 + RFM95C LoRa + MCP2515 CAN
  
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
</classes>
<parts>
<!-- ================================================================== -->
<!-- MICROCONTROLLER - nRF52832-QFAA -->
<!-- ================================================================== -->
<part name="U1" value="nRF52832-QFAA" device="QFN48"/>

<!-- nRF52832 Decoupling (per datasheet) -->
<part name="C20" value="100nF" device="0402"/>
<part name="C21" value="100nF" device="0402"/>
<part name="C22" value="10uF" device="0805"/>
<part name="C23" value="100nF" device="0402"/>
<part name="C24" value="10nF" device="0402"/>
<part name="C25" value="1uF" device="0402"/>
<part name="L1" value="10nH" device="0402"/>

<!-- ================================================================== -->
<!-- LORA MODULE - RFM95C -->
<!-- ================================================================== -->
<part name="U2" value="RFM95C-915S2" device="SMD"/>
<part name="C30" value="100nF" device="0402"/>
<part name="C31" value="10uF" device="0805"/>

<!-- Antenna Matching Network (Pi-network for 50 ohm) -->
<part name="L2" value="5.6nH" device="0402"/>
<part name="C32" value="1.5pF" device="0402"/>
<part name="C33" value="1.2pF" device="0402"/>
<part name="ANT1" value="915MHz-WIRE" device=""/>

<!-- ================================================================== -->
<!-- CAN BUS - MCP2515 + SN65HVD230 -->
<!-- ================================================================== -->
<part name="U3" value="MCP2515-I/SO" device="SOIC-18"/>
<part name="U4" value="SN65HVD230DR" device="SOIC-8"/>
<part name="Y3" value="16MHz" device="HC49"/>
<part name="C10" value="22pF" device="0402"/>
<part name="C11" value="22pF" device="0402"/>
<part name="R10" value="120R" device="0603"/>
<part name="C40" value="100nF" device="0402"/>
<part name="C41" value="100nF" device="0402"/>

<!-- CAN Bus ESD Protection -->
<part name="D1" value="PESD1CAN" device="SOT23"/>

<!-- ================================================================== -->
<!-- MEMORY - FRAM + FLASH (STANDARD PINS) -->
<!-- ================================================================== -->
<part name="U5" value="MB85RS1MTPNF" device="SOIC-8"/>
<part name="C50" value="100nF" device="0402"/>
<part name="U6" value="W25Q16JVSSIQ" device="SOIC-8"/>
<part name="C51" value="100nF" device="0402"/>

<!-- ================================================================== -->
<!-- RTC - RV-3028-C7 (I2C on P0.02/P0.03) -->
<!-- ================================================================== -->
<part name="U7" value="RV-3028-C7" device="SMD"/>
<part name="BT1" value="CR2032" device="HOLDER"/>
<part name="C52" value="100nF" device="0402"/>

<!-- I2C Pull-up Resistors -->
<part name="R20" value="4.7K" device="0402"/>
<part name="R21" value="4.7K" device="0402"/>

<!-- ================================================================== -->
<!-- POWER - LDO (24V to 3.3V) -->
<!-- ================================================================== -->
<part name="U8" value="MCP1700-3302E" device="SOT-23"/>
<part name="C1" value="10uF/50V" device="0805"/>
<part name="C2" value="10uF" device="0805"/>

<!-- 24V Input Protection -->
<part name="D2" value="SMBJ28A" device="SMB"/>
<part name="F1" value="500mA PTC" device="1206"/>

<!-- Reverse Polarity Protection -->
<part name="Q1" value="SI2301CDS" device="SOT-23"/>
<part name="R22" value="10K" device="0402"/>

<!-- ================================================================== -->
<!-- CRYSTALS -->
<!-- ================================================================== -->
<part name="Y1" value="32MHz" device="3215"/>
<part name="Y2" value="32.768kHz" device="2012"/>
<part name="C3" value="12pF" device="0402"/>
<part name="C4" value="12pF" device="0402"/>
<part name="C5" value="6.8pF" device="0402"/>
<part name="C6" value="6.8pF" device="0402"/>

<!-- ================================================================== -->
<!-- STATUS LEDS -->
<!-- ================================================================== -->
<part name="LED1" value="GREEN" device="0603"/>
<part name="LED2" value="YELLOW" device="0603"/>
<part name="LED3" value="RED" device="0603"/>
<part name="R1" value="1K" device="0402"/>
<part name="R2" value="1K" device="0402"/>
<part name="R3" value="1K" device="0402"/>

<!-- ================================================================== -->
<!-- CONNECTORS -->
<!-- ================================================================== -->
<part name="J1" value="M12-4PIN" device="PANEL"/>
<part name="J2" value="M12-4PIN" device="PANEL"/>
<part name="J3" value="PWR-3PIN" device="XPC"/>

<!-- ================================================================== -->
<!-- PAIRING BUTTON -->
<!-- ================================================================== -->
<part name="SW1" value="TACTILE" device="6x6"/>
<part name="R4" value="10K" device="0402"/>
<part name="C60" value="100nF" device="0402"/>
<part name="D3" value="PESD5V0S1BL" device="SOD323"/>

<!-- ================================================================== -->
<!-- POWER FAIL INPUT -->
<!-- ================================================================== -->
<part name="R5" value="10K" device="0402"/>
<part name="R6" value="10K" device="0402"/>
<part name="C61" value="100nF" device="0402"/>
<part name="D4" value="PESD5V0S1BL" device="SOD323"/>

<!-- ================================================================== -->
<!-- DECOUPLING CAPACITORS (General) -->
<!-- ================================================================== -->
<part name="C7" value="100nF" device="0402"/>
<part name="C8" value="100nF" device="0402"/>
<part name="C9" value="100nF" device="0402"/>

</parts>
<sheets>
<sheet>
<plain>
<!-- Title Block -->
<text x="200" y="10" size="3.81" layer="94">VALVE CONTROLLER</text>
<text x="200" y="5" size="2.54" layer="94">nRF52832 + LoRa + CAN Bus</text>
<text x="280" y="10" size="2.54" layer="94">Rev 1.0</text>
<text x="280" y="5" size="1.778" layer="94">Sheet 1 of 1</text>

<!-- Section Labels -->
<text x="10" y="180" size="2.54" layer="94">MICROCONTROLLER</text>
<text x="10" y="130" size="2.54" layer="94">LORA MODULE</text>
<text x="120" y="180" size="2.54" layer="94">CAN BUS</text>
<text x="120" y="130" size="2.54" layer="94">MEMORY (STANDARD PINS)</text>
<text x="200" y="180" size="2.54" layer="94">RTC</text>
<text x="200" y="130" size="2.54" layer="94">POWER</text>
<text x="10" y="80" size="2.54" layer="94">STATUS LEDS</text>
<text x="120" y="80" size="2.54" layer="94">CONNECTORS</text>

<!-- Notes -->
<text x="10" y="20" size="1.778" layer="94">NOTES:</text>
<text x="10" y="16" size="1.524" layer="94">1. All decoupling caps 100nF unless noted</text>
<text x="10" y="12" size="1.524" layer="94">2. CAN bus termination 120R at both ends</text>
<text x="10" y="8" size="1.524" layer="94">3. Power input: 24VDC from external PSU</text>
<text x="10" y="4" size="1.524" layer="94">4. Power fail signal: Active LOW when on battery</text>
</plain>
<instances>
</instances>
<busses>
</busses>
<nets>
<!-- ================================================================== -->
<!-- POWER NETS -->
<!-- ================================================================== -->
<!-- 3.3V Rail -->
<net name="VCC" class="1">
<segment>
<pinref part="U8" gate="G$1" pin="VOUT"/>
<pinref part="U1" gate="G$1" pin="VDD"/>
<pinref part="U2" gate="G$1" pin="VCC"/>
<pinref part="U3" gate="G$1" pin="VDD"/>
<pinref part="U4" gate="G$1" pin="VCC"/>
<pinref part="U5" gate="G$1" pin="VCC"/>
<pinref part="U6" gate="G$1" pin="VCC"/>
<pinref part="U7" gate="G$1" pin="VDD"/>
<wire x1="50" y1="200" x2="200" y2="200" width="0.4064" layer="91"/>
<label x="100" y="202" size="1.778" layer="95"/>
</segment>
</net>
<!-- Ground -->
<net name="GND" class="1">
<segment>
<pinref part="U1" gate="G$1" pin="GND"/>
<pinref part="U2" gate="G$1" pin="GND"/>
<pinref part="U3" gate="G$1" pin="VSS"/>
<pinref part="U4" gate="G$1" pin="GND"/>
<pinref part="U5" gate="G$1" pin="GND"/>
<pinref part="U6" gate="G$1" pin="GND"/>
<pinref part="U7" gate="G$1" pin="GND"/>
<pinref part="U8" gate="G$1" pin="GND"/>
<wire x1="50" y1="10" x2="200" y2="10" width="0.4064" layer="91"/>
<label x="100" y="12" size="1.778" layer="95"/>
</segment>
</net>
<!-- 24V Input -->
<net name="VIN_24V" class="1">
<segment>
<pinref part="J3" gate="G$1" pin="1"/>
<pinref part="U8" gate="G$1" pin="VIN"/>
<wire x1="130" y1="140" x2="150" y2="140" width="0.4064" layer="91"/>
<label x="135" y="142" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- SPI BUS 0 - PERIPHERALS (CAN + LORA) -->
<!-- ================================================================== -->
<net name="PERIPH_SCK" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.27"/>
<pinref part="U2" gate="G$1" pin="SCK"/>
<pinref part="U3" gate="G$1" pin="SCK"/>
<wire x1="80" y1="170" x2="140" y2="170" width="0.254" layer="91"/>
<label x="100" y="172" size="1.778" layer="95"/>
</segment>
</net>
<net name="PERIPH_MOSI" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.28"/>
<pinref part="U2" gate="G$1" pin="MOSI"/>
<pinref part="U3" gate="G$1" pin="SI"/>
<wire x1="80" y1="165" x2="140" y2="165" width="0.254" layer="91"/>
<label x="100" y="167" size="1.778" layer="95"/>
</segment>
</net>
<net name="PERIPH_MISO" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.29"/>
<pinref part="U2" gate="G$1" pin="MISO"/>
<pinref part="U3" gate="G$1" pin="SO"/>
<wire x1="80" y1="160" x2="140" y2="160" width="0.254" layer="91"/>
<label x="100" y="162" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- SPI BUS 2 - MEMORY (STANDARD PINS P0.22-P0.26) -->
<!-- ================================================================== -->
<net name="MEM_SCK" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.26"/>
<pinref part="U5" gate="G$1" pin="SCK"/>
<pinref part="U6" gate="G$1" pin="CLK"/>
<wire x1="80" y1="155" x2="180" y2="155" width="0.254" layer="91"/>
<label x="145" y="157" size="1.778" layer="95"/>
</segment>
</net>
<net name="MEM_MOSI" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.25"/>
<pinref part="U5" gate="G$1" pin="SI"/>
<pinref part="U6" gate="G$1" pin="DI"/>
<wire x1="80" y1="150" x2="180" y2="150" width="0.254" layer="91"/>
<label x="145" y="152" size="1.778" layer="95"/>
</segment>
</net>
<net name="MEM_MISO" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.24"/>
<pinref part="U5" gate="G$1" pin="SO"/>
<pinref part="U6" gate="G$1" pin="DO"/>
<wire x1="80" y1="145" x2="180" y2="145" width="0.254" layer="91"/>
<label x="145" y="147" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- CHIP SELECTS -->
<!-- ================================================================== -->
<!-- Peripheral SPI CS -->
<net name="CAN_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.30"/>
<pinref part="U3" gate="G$1" pin="CS"/>
<wire x1="80" y1="140" x2="130" y2="170" width="0.254" layer="91"/>
<label x="90" y="150" size="1.778" layer="95"/>
</segment>
</net>
<net name="LORA_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.31"/>
<pinref part="U2" gate="G$1" pin="NSS"/>
<wire x1="80" y1="135" x2="100" y2="140" width="0.254" layer="91"/>
<label x="85" y="138" size="1.778" layer="95"/>
</segment>
</net>
<!-- Memory SPI CS (Standard Pins) -->
<net name="FRAM_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.23"/>
<pinref part="U5" gate="G$1" pin="CS"/>
<wire x1="80" y1="130" x2="180" y2="140" width="0.254" layer="91"/>
<label x="145" y="135" size="1.778" layer="95"/>
</segment>
</net>
<net name="FLASH_CS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.22"/>
<pinref part="U6" gate="G$1" pin="CS"/>
<wire x1="80" y1="125" x2="180" y2="135" width="0.254" layer="91"/>
<label x="145" y="130" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- LORA SIGNALS -->
<!-- ================================================================== -->
<net name="LORA_RST" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.16"/>
<pinref part="U2" gate="G$1" pin="RESET"/>
<wire x1="80" y1="120" x2="100" y2="120" width="0.254" layer="91"/>
<label x="85" y="122" size="1.778" layer="95"/>
</segment>
</net>
<net name="LORA_DIO0" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.15"/>
<pinref part="U2" gate="G$1" pin="DIO0"/>
<wire x1="80" y1="115" x2="100" y2="115" width="0.254" layer="91"/>
<label x="85" y="117" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- CAN BUS SIGNALS -->
<!-- ================================================================== -->
<net name="CAN_INT" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.14"/>
<pinref part="U3" gate="G$1" pin="INT"/>
<wire x1="80" y1="110" x2="130" y2="165" width="0.254" layer="91"/>
<label x="90" y="120" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_TX" class="0">
<segment>
<pinref part="U3" gate="G$1" pin="TXCAN"/>
<pinref part="U4" gate="G$1" pin="D"/>
<wire x1="150" y1="175" x2="170" y2="175" width="0.254" layer="91"/>
<label x="155" y="177" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_RX" class="0">
<segment>
<pinref part="U3" gate="G$1" pin="RXCAN"/>
<pinref part="U4" gate="G$1" pin="R"/>
<wire x1="150" y1="170" x2="170" y2="170" width="0.254" layer="91"/>
<label x="155" y="172" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_H" class="0">
<segment>
<pinref part="U4" gate="G$1" pin="CANH"/>
<pinref part="J1" gate="G$1" pin="1"/>
<pinref part="J2" gate="G$1" pin="1"/>
<pinref part="R10" gate="G$1" pin="1"/>
<wire x1="190" y1="175" x2="220" y2="175" width="0.254" layer="91"/>
<label x="200" y="177" size="1.778" layer="95"/>
</segment>
</net>
<net name="CAN_L" class="0">
<segment>
<pinref part="U4" gate="G$1" pin="CANL"/>
<pinref part="J1" gate="G$1" pin="2"/>
<pinref part="J2" gate="G$1" pin="2"/>
<pinref part="R10" gate="G$1" pin="2"/>
<wire x1="190" y1="170" x2="220" y2="170" width="0.254" layer="91"/>
<label x="200" y="172" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- I2C BUS (RTC) - Moved to P0.02/P0.03 to avoid conflict with memory bus -->
<!-- ================================================================== -->
<net name="I2C_SDA" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.02"/>
<pinref part="U7" gate="G$1" pin="SDA"/>
<pinref part="R20" gate="G$1" pin="1"/>
<wire x1="80" y1="100" x2="210" y2="170" width="0.254" layer="91"/>
<label x="150" y="140" size="1.778" layer="95"/>
</segment>
</net>
<net name="I2C_SCL" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.03"/>
<pinref part="U7" gate="G$1" pin="SCL"/>
<pinref part="R21" gate="G$1" pin="1"/>
<wire x1="80" y1="95" x2="210" y2="165" width="0.254" layer="91"/>
<label x="150" y="135" size="1.778" layer="95"/>
</segment>
</net>
<!-- I2C Pull-ups to VCC -->
<net name="I2C_PULLUP" class="0">
<segment>
<pinref part="R20" gate="G$1" pin="2"/>
<pinref part="R21" gate="G$1" pin="2"/>
<wire x1="215" y1="175" x2="215" y2="180" width="0.254" layer="91"/>
<label x="217" y="177" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- STATUS LEDS -->
<!-- ================================================================== -->
<net name="LED_3V3" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.18"/>
<pinref part="R1" gate="G$1" pin="1"/>
<wire x1="80" y1="90" x2="40" y2="70" width="0.254" layer="91"/>
<label x="50" y="80" size="1.778" layer="95"/>
</segment>
</net>
<net name="LED_24V" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.19"/>
<pinref part="R2" gate="G$1" pin="1"/>
<wire x1="80" y1="85" x2="50" y2="70" width="0.254" layer="91"/>
<label x="55" y="78" size="1.778" layer="95"/>
</segment>
</net>
<net name="LED_STATUS" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.20"/>
<pinref part="R3" gate="G$1" pin="1"/>
<wire x1="80" y1="80" x2="60" y2="70" width="0.254" layer="91"/>
<label x="62" y="75" size="1.778" layer="95"/>
</segment>
</net>

<!-- ================================================================== -->
<!-- CONTROL SIGNALS -->
<!-- ================================================================== -->
<net name="POWER_FAIL" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.17"/>
<pinref part="J3" gate="G$1" pin="3"/>
<wire x1="80" y1="75" x2="130" y2="130" width="0.254" layer="91"/>
<label x="90" y="90" size="1.778" layer="95"/>
</segment>
</net>
<net name="PAIRING_BTN" class="0">
<segment>
<pinref part="U1" gate="G$1" pin="P0.11"/>
<pinref part="SW1" gate="G$1" pin="1"/>
<wire x1="80" y1="70" x2="60" y2="50" width="0.254" layer="91"/>
<label x="65" y="60" size="1.778" layer="95"/>
</segment>
</net>

</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>

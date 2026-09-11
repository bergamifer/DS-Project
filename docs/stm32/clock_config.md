# STM32H7 RCC (Reset and Clock Control) - Synthesized for Claude

## Overview
The RCC manages clock and reset generation for the STM32H7 microcontroller. Located in D3 domain, it handles resets, clock distribution, and low-power modes.

## Main Features
- **Reset Block:** System, local, and bidirectional pin resets; supports WWDG.
- **Clock Generation:** 3 PLLs (integer/fractional), smart gating for power savings.
- **Oscillators:**
  - External: HSE (4-48 MHz), LSE (32 kHz)
  - Internal: HSI, HSI48 (48 MHz), CSI (low-power), LSI
- **Outputs:** Buffered clocks (MCO1/MCO2), interrupts for security/events.
- **Low-Power:** Handles clocks in Stop/Standby; D3 autonomous mode.

## Key Signals
- **Inputs:** NRST (reset), OSC_IN/OUT (HSE), OSC32_IN/OUT (LSE), I2S_CKIN (audio), ETH clocks, USB_PHY1.
- **Outputs:** MCO1/MCO2, resets (peripheral, domain), clocks (CPU, AHB, APB, AXI, peripheral kernel/bus).
- **Internal:** Interrupts (general, HSE/LSE CSS), events, low-power requests.

## Reset Types
- **Power-On/Off Reset (pwr_por_rst):** Full reset on VDD low; can be disabled via PDR_ON.
- **System Reset (nreset):** Resets all except backup domain; sources: NRST, POR, BOR, IWDG, WWDG, software (SYSRESETREQ).
- **Local Resets:** CPU reset (CPURST), domain resets (D1/D2 on exit from low-power), standby reset.
- **Backup Domain Reset:** Software (BDRST) or VSW out of range; resets RTC, BDCR, but not backup RAM.
- **Low-Power Security Reset (lpwr_rst):** Prevents accidental low-power entry; enabled via option bytes.

## Reset Source Identification
Check RCC_RSR (or RCC_C1_RSR) flags:
- PORRSTF: Power-on
- PINRSTF: NRST pin
- BORRSTF: Brownout
- SFTRSTF: Software
- CPURSTF: CPU
- WWDG1RSTF: Window watchdog
- IWDG1RSTF: Independent watchdog
- D1RSTF/D2RSTF: Domain exits
- LPWRRSTF: Low-power security

Clear flags with RMVF bit.

## Clock Sources and Generation
- **PLL1-3:** Generate high-frequency clocks; support fractional ratios (on-the-fly changeable).
- **System Clock:** From HSI, CSI, HSE, or PLL.
- **CPU Clock (rcc_c_ck):** Up to 480 MHz.
- **Bus Clocks:** AHB (rcc_ahb_ck), APB (rcc_apb_ck), AXI (rcc_axi_ck).
- **Peripheral Clocks:** Kernel (rcc_perx_ker_ck) and bus (rcc_perx_bus_ck) for each peripheral.
- **Audio Clocks:** I2S_CKIN for SAI/SPI/DFSDM; supports external audio clocks.

## Configuration for Audio Project
- Use HSE (external crystal) for stable base clock.
- Configure PLL1 for 480 MHz system clock.
- Derive SAI clocks (MCLK, BCK, LRCK) from PLL; ensure 12.288 MHz MCLK for 48 kHz audio.
- Enable clock security (CSS) for HSE/LSE to detect failures.
- In low-power modes, RCC handles domain-specific clock restoration.

## Low-Power Handling
- RCC generates low-power requests (rcc_pwd_d[3:1]_req) to PWR for DStop.
- Wake-up restores clocks via pwr_d[3:1]_wkup.
- Autonomous D3 mode for peripherals.

This synthesis covers essentials for STM32H7 clock/reset management, optimized for audio applications.
7.1 RCC main features
Reset block
• Generation of local and system reset
• Bidirectional pin reset allowing to reset the microcontroller or external devices
• Hold Boot function
• WWDG reset supported
Clock generation block
• Generation and dispatching of clocks for the complete device
• 3 separate PLLs using integer or fractional ratios
• Possibility to change the PLL fractional ratios on-the-fly
• Smart clock gating to reduce power dissipation
• 2 external oscillators:
– High-speed external oscillator (HSE) supporting a wide range of crystals from 4 to
48 MHz frequency
– Low-speed external oscillator (LSE) for the 32 kHz crystals
• 4 internal oscillators
– High-speed internal oscillator (HSI)
– 48 MHz RC oscillator (HSI48)
– Low-power Internal oscillator (CSI)
– Low-speed internal oscillator (LSI)
• Buffered clock outputs for external devices
• Generation of two types of interrupts lines:
– Dedicated interrupt lines for clock security management
– One general interrupt line for other events
• Clock generation handling in Stop and Standby mode
• D3 domain Autonomous mode

Table 48. RCC input/output signals connected to package pins or balls
Signal name Signal
type Description
NRST I/O System reset, can be used to provide reset to external devices
OSC32_IN I 32 kHz oscillator input
OSC32_OUT O 32 kHz oscillator output
OSC_IN I System oscillator input
OSC_OUT O System oscillator output
MCO1 O Output clock 1 for external devices
MCO2 O Output clock 2 for external devices
I2S_CKIN I External kernel clock input for digital audio interfaces: SPI/I2S, SAI, and DFSDM
ETH_MII_TX_CLK I External TX clock provided by the Ethernet MII interface
ETH_MII_RX_CLK I External RX clock provided by the Ethernet MII interface
ETH_RMII_REF_CLK I External reference clock provided by the Ethernet RMII interface
USB_PHY1 I USB clock input provided by the external USB PHY (OTG_HS_ULPI_CK)

Table 49. RCC internal input/output signals
New Signal name Signal
type Description
rcc_it O General interrupt request line
rcc_hsecss_it O HSE clock security failure interrupt
rcc_lsecss_it O LSE clock security failure interrupt
rcc_ckfail_evt O Event indicating that a HSE clock security failure is detected. This signal is
connected to TIMERS
nreset I/O System reset
iwdg1_out_rst I Reset line driven by the IWDG1, indicating that a timeout occurred.
wwdg1_out_rst I Reset line driven by the WWDG1, indicating that a timeout occurred.
pwr_bor_rst I Brownout reset generated by the PWR block
pwr_por_rst I Power-on reset generated by the PWR block
pwr_vsw_rst I Power-on reset of the VSW domain generated by the PWR block
rcc_perx_rst O Reset generated by the RCC for the peripherals.
pwr_d[3:1]_wkup I Wake-up domain request generated by the PWR. Generally used to restore the
clocks a domain when this domain exits from DStop
rcc_pwd_d[3:1]_req O Low-Power request generated by the RCC. Generally used to ask to the PWR
to set a domain into low-power mode, when a domain is in DStop.
pwr_hold_ctrl I Signals generated by the PWR, in order to set the processor into CStop when
exiting from system Stop mode.
c_sleep I Signal generated by the CPU, indicating if the CPU is in CRun, CSleep or
c_deepsleep I CStop.
perx_ker_ckreq I Signal generated by some peripherals in order to request the activation of their
kernel clock.
rcc_perx_ker_ck O Kernel clock signals generated by the RCC, for some peripherals.
rcc_perx_bus_ck O Bus interface clock signals generated by the RCC for peripherals.
rcc_bus_ck O Clocks for APB (rcc_apb_ck), AHB (rcc_ahb_ck) and AXI (rcc_axi_ck) bridges
generated by the RCC.
rcc_c_ck O
Clock for the CPU, generated by the RCC.
rcc_fclk_c O

7.4 RCC reset block functional description
Several sources can generate a reset:
• An external device via NRST pin
• A failure on the supply voltage applied to VDD
• A watchdog timeout
• A software command
The reset scope depends on the source that generates the reset. Three reset categories
exist:
• Power-on/off reset
• System reset
• Local resets

Power-on/off reset
The power-on/off reset (pwr_por_rst) is generated by the power controller block (PWR). It
is activated when the input voltage (VDD) is below a threshold level. This is the most
complete reset since it resets the whole circuit, except the backup domain. The power-on/off
reset function can be disabled through PDR_ON pin (see Section 5.5: Power supply
supervision).
Refer to Table 50: Reset distribution summary for details

7.4.2 System reset
A system reset (nreset) resets all registers to their default values except for the reset status
flags in the RCC_RSR (or RCC_C1_RSR) register, the debug features, the Flash memory
and the Backup domain registers.
A system reset can be generated from one of the following sources:
• A reset from NRST pin (external reset)
• A reset from the power-on/off reset block (pwr_por_rst)
• A reset from the brownout reset block (pwr_bor_rst)
Refer to Section 5.5.2: Brownout reset (BOR) for a detailed description of the BOR
function.
• A reset from the independent watchdogs (iwdg1_out_rst)
• A software reset from the Cortex®-M7 core
It is generated via the SYSRESETREQ signal issued by the Cortex®-M7 core. This
signal is also named SFTRESET in this document.
• A reset from the window watchdogs depending on WWDG configuration
(wwdg1_out_rst)
• A reset from the low-power mode security reset, depending on option byte
configuration (lpwr[2:1]_rst)

Note: The SYSRESETREQ bit in Cortex®-M7 Application Interrupt and Reset Control Register
must be set to force a software reset on the device. Refer to the Cortex®-M7 with FPU
technical reference manual for more details (see http://infocenter.arm.com).
As shown in Figure 39, some internal sources (such as pwr_por_rst, pwr_bor_rst,
iwdg1_out_rst) perform a system reset of the circuit, which is also propagated to the NRST
pin to reset the connected external devices. The pulse generator guarantees a minimum
reset pulse duration of 20 μs for each internal reset source. In case of an external reset, the
reset pulse is generated while the NRST pin is asserted Low.
Note: It is not recommended to let the NRST pin unconnected. When it is not used, connect this
pin to ground via a 10 to 100 nFcapacitor (CR in Figure 39).

7.4.3 Local resets
CPU reset
The CPU can reset itself by means of the CPURST bit in RCC AHB3 Reset Register
(RCC_AHB3RSTR).
Domain reset
Some resets also dependent on the domain status. For example, when D1 domain exits
from DStandby, it is reset (d1_rst). The same mechanism applies to D2.
When the system exits from Standby mode, a stby_rst reset is applied. The stby_rst signal
generates a reset of the complete VCORE domain as long the VCORE voltage provided by the
internal regulator is not valid.
Table 50 gives a detailed overview of reset sources and scopes.

7.4.4 Reset source identification
The CPU can identify the reset source by checking the reset flags in the RCC_RSR (or
RCC_C1_RSR) register.
The CPU can reset the flags by setting RMVF bit.
Table 51 shows how the status bits of RCC_RSR (or RCC_C1_RSR) register behaves,
according to the situation that generated the reset. For example when an IWDG1 timeout
occurs (line #10), if the CPU is reading the RCC_RSR (or RCC_C1_RSR) register during
the boot phase, both PINRSTF and IWDG1RSTF bits are set, indicating that the IWDG1
also generated a pin reset.
Table 51. Reset source identification (RCC_RSR)(1)
# Situations Generating a Reset
LPWRRSTF
WWDG1RSTF
IWDG1RSTF
SFTRSTF
PORRSTF
PINRSTF
BORRSTF
D2RSTF
D1RSTF
CPURSTF
1 Power-on reset (pwr_por_rst) 0 0 0 0 1 1 1 1 1 1
2 Pin reset (NRST) 0 0 0 0 0 1 0 0 0 1
3 Brownout reset (pwr_bor_rst) 0 0 0 0 0 1 1 0 0 1
4 System reset generated by CPU (SFTRESET) 0 0 0 1 0 1 0 0 0 1
5 CPU reset (CPURST) 0 0 0 0 0 0 0 0 0 1
6 WWDG1 reset (wwdg1_out_rst) 0 1 0 0 0 1 0 0 0 1
8 IWDG1 reset (iwdg1_out_rst) 0 0 1 0 0 1 0 0 0 1
10 D1 exits DStandby mode 0 0 0 0 0 0 0 0 1 0
11 D2 exits DStandby mode 0 0 0 0 0 0 0 1 0 0
12 D1 erroneously enters DStandby mode or
CPU erroneously enters CStop mode 1 0 0 0 0 1 0 0 0 1

7.4.5 Low-power mode security reset (lpwr_rst)
To prevent critical applications from mistakenly enter a low-power mode, two low-power
mode security resets are available. When enabled through nRST_STOP_D1 option bytes, a
system reset is generated if the following conditions are met:
• CPU accidentally enters CStop mode
This type of reset is enabled by resetting nRST_STOP_D1 user option byte. In this
case, whenever the CPU CStop mode entry sequence is successfully executed, a
system reset is generated.
• D1 domain accidentally enters DStandby mode
This type of reset is enabled by resetting nRST_STDBY_D1 user option byte. In this
case, whenever a D1 domain DStandby mode entry sequence is successfully
executed, a system reset is generated.
LPWRRSTF bits in RCC Reset Status Register (RCC_RSR) indicates that a low-power
mode security reset occurred (see line #12 in Table 51).
lpwr_rst is activated when a low-power mode security reset due to D1 or CPU occurred.
Refer to Section 3.4: FLASH option bytes for additional information.
7.4.6 Backup domain reset
A backup domain reset is generated when one of the following events occurs:
• A software reset, triggered by setting BDRST bit in the RCC Backup Domain Control
Register (RCC_BDCR). All RTC registers and the RCC_BDCR register are reset to
their default values. The backup RAM is not affected.
• VSW voltage is outside the operating range. All RTC registers and the RCC_BDCR
register are reset to their default values. In this case the content of the backup RAM is
no longer valid.
There are two ways to reset the backup RAM:
• through the Flash memory interface by requesting a protection level change from 1 to 0
• when a tamper event occurs.
Refer to Section 5.4.4: Backup domain section of PWR block for additional information.
7.4.7 Power-on and wakeup sequences
For detailed diagrams refer to Section 5.4.1: System supply startup in the PWR section.
The time interval between the event which exits the product from a low-power and the
moment where the CPU is able to execute code, depends on the system state and on its
configuration. Figure 40 shows the most usual examples.
Power-on wakeup sequence
The power-on wakeup sequence shown in Figure 40 gives the most significant phases of
the power-on sequence. It is the longest sequence since the circuit was not powered. Note
that this sequence remains unchanged, whatever VBAT was present or not.

RCC clock block functional description
The RCC provides a wide choice of clock generators:
• HSI (High-speed internal oscillator) clock: ~ 8, 16, 32 or 64 MHz
• HSE (High-speed external oscillator) clock: 4 to 48 MHz
• LSE (Low-speed external oscillator) clock: 32 kHz
• LSI (Low-speed internal oscillator) clock: ~ 32 kHz
• CSI (Low-power internal oscillator) clock: ~4 MHz
• HSI48 (High-speed 48 MHz internal oscillator) clock: ~48 MHz
It offers a high flexibility for the application to select the appropriate clock for CPU and
peripherals, in particular for peripherals that require a specific clock such as Ethernet, USB
OTG-FS and HS, SPI/I2S, SAI and SDMMC.
To optimize the power consumption, each clock source can be switched ON or OFF
independently.
The RCC provides up to 3 PLLs; each of them can be configured with integer or fractional
ratios.
As shown in the Figure 41, the RCC offers 2 clock outputs (MCO1 and MCO2), with a great
flexibility on the clock selection and frequency adjustment.
The SCGU block (System Clock Generation Unit) contains several prescalers used to
configure the CPU and bus matrix clock frequencies.
The PKSU block (Peripheral Kernel clock Selection Unit) provides several dynamic switches
allowing a large choice of kernel clock distribution to peripherals.
The PKEU (Peripheral Kernel clock Enable Unit) and SCEU (System Clock Enable Unit)
blocks perform the peripheral kernel clock gating, and the bus interface/cores/bus matrix
clock gating, respectively.

7.5.1 Clock naming convention
The RCC provides clocks to the complete circuit. To avoid misunderstanding, the following
terms are used in this document:
• Peripheral clocks
The peripheral clocks are the clocks provided by the RCC to the peripherals. Two kinds
of clock are available:
– The bus interface clocks
– The kernel clocks
A peripheral receives from the RCC a bus interface clock in order to access its
registers, and thus control the peripheral operation. This clock is generally the AHB,
APB or AXI clock depending on which bus the peripheral is connected to. Some
peripherals only need a bus interface clock (e.g. RNG, TIMx).
Some peripherals also require a dedicated clock to handle the interface function. This
clock is named “kernel clock”. As an example, peripherals such as SAI have to
generate specific and accurate master clock frequencies, which require dedicated
kernel clock frequencies. Another advantage of decoupling the bus interface clock from
the specific interface needs, is that the bus clock can be changed without
reprogramming the peripheral.
• CPU clocks
The CPU clock is the clock provided to the CPU. It is derived from the system clock
(sys_ck).
• Bus matrix clocks
The bus matrix clocks are the clocks provided to the different bridges (APB, AHB or
AXI). These clocks are derived from the system clock (sys_ck).
7.5.2 Oscillators description
HSE oscillator
The HSE block can generate a clock from two possible sources:
• External crystal/ceramic resonator
• External clock source

External clock source (HSE bypass)
In this mode, an external clock source must be provided to OSC_IN pin. This mode is
selected by setting the HSEBYP and HSEON bits of the RCC Source Control Register
(RCC_CR) to ‘1’. The external clock source (square, sinus or triangle) with ~50% duty cycle
has to drive the OSC_IN pin while the OSC_OUT pin should be left HI-Z (see Figure 42).
External crystal/ceramic resonator
The oscillator is enabled by setting the HSEBYP bit to ‘0’ and HSEON bit to ‘1’.
The HSE can be used when the product requires a very accurate high-speed clock.
The associated hardware configuration is shown in Figure 42: the resonator and the load
capacitors have to be placed as close as possible to the oscillator pins in order to minimize
output distortion and startup stabilization time. The loading capacitance values must be
adjusted according to the selected crystal or ceramic resonator. Refer to the electrical
characteristics section of the datasheet for more details.
The HSERDY flag of the RCC Source Control Register (RCC_CR), indicates whether the
HSE oscillator is stable or not. At startup, the hse_ck clock is not released until this bit is set
by hardware. An interrupt can be generated if enabled in the RCC Clock Source Interrupt
Enable Register (RCC_CIER).
The HSE can be switched ON and OFF through the HSEON bit. Note that the HSE cannot
be switched OFF if one of the two conditions is met:
• The HSE is used directly (via software mux) as system clock
• The HSE is selected as reference clock for PLL1, with PLL1 enabled and selected to
provide the system clock (via software mux).
In that case the hardware does not allow programming the HSEON bit to ‘0’.
The HSE is automatically disabled by hardware, when the system enters Stop or Standby
mode (refer to Section 7.5.7: Handling clock generators in Stop and Standby mode for
additional information).
In addition, the HSE clock can be driven to the MCO1 and MCO2 outputs and used as clock
source for other application components.
LSE oscillator
The LSE block can generate a clock from two possible sources:
• External crystal/ceramic resonator
• External user clock
External clock source (LSE bypass)
In this mode, an external clock source must be provided to OSC32_IN pin. The input clock
can have a frequency up to 1 MHz. This mode is selected by setting the LSEBYP and
LSEON bits of RCC Backup Domain Control Register (RCC_BDCR) to ‘1’. The external
clock signal (square, sinus or triangle) with ~50% duty cycle has to drive the OSC32_IN pin
while the OSC32_OUT pin should be left HI-Z (see Figure 42).

External crystal/ceramic resonator (LSE crystal)
The LSE clock is generated from a 32.768 kHz crystal or ceramic resonator. It has the
advantage to provide a low-power highly accurate clock source to the real-time clock (RTC)
for clock/calendar or other timing functions.
The LSERDY flag of the RCC Backup Domain Control Register (RCC_BDCR) indicates
whether the LSE crystal is stable or not. At startup, the LSE crystal output clock signal is not
released until this bit is set by hardware. An interrupt can be generated if enabled in the
RCC Clock Source Interrupt Enable Register (RCC_CIER).
The LSE oscillator is switched ON and OFF using the LSEON bit. The LSE remains enabled
when the system enters Stop or Standby mode.
In addition, the LSE clock can be driven to the MCO1 output and used as clock source for
other application components.
The LSE also offers a programmable driving capability (LSEDRV[1:0]) that can be used to
modulate the amplifier driving capability. The driving capability can be changed dynamically
from high drive to medium high drive, and then to medium low drive.
HSI oscillator
The HSI block provides the default clock to the product.
The HSI is a high-speed internal RC oscillator which can be used directly as system clock,
peripheral clock, or as PLL input. A predivider allows the application to select an HSI output
frequency of 8, 16, 32 or 64 MHz. This predivider is controlled by the HSIDIV.
The HSI advantages are the following:
• Low-cost clock source since no external crystal is required
• Faster startup time than HSE (a few microseconds)
The HSI frequency, even with frequency calibration, is less accurate than an external crystal
oscillator or ceramic resonator.
The HSI can be switched ON and OFF using the HSION bit. Note that the HSI cannot be
switched OFF if one of the two conditions is met:
• The HSI is used directly (via software mux) as system clock
• The HSI is selected as reference clock for PLL1, with PLL1 enabled and selected to
provide the system clock (via software mux).
In that case the hardware does not allow programming the HSION bit to ‘0’.
Note that the HSIDIV cannot be changed if the HSI is selected as reference clock for at least
one enabled PLL (PLLxON bit set to ‘1’). In that case the hardware does not update the
HSIDIV with the new value. However it is possible to change the HSIDIV if the HSI is used
directly as system clock.
The HSIRDY flag indicates if the HSI is stable or not. At startup, the HSI output clock is not
released until this bit is set by hardware.
The HSI clock can also be used as a backup source (auxiliary clock) if the HSE fails (refer to
Section : CSS on HSE). The HSI can be disabled or not when the system enters Stop mode,
please refer to Section 7.5.7: Handling clock generators in Stop and Standby mode for
additional information.
In addition, the HSI clock can be driven to the MCO1 output and used as clock source for
other application components.

Care must be taken when the HSI is used as kernel clock for communication peripherals,
the application must take into account the following parameters:
• the time interval between the moment where the peripheral generates a kernel clock
request and the moment where the clock is really available,
• the frequency accuracy.
Note: The HSI can remain enabled when the system is in Stop mode (see Section 7.5.7 for
additional information).
HSION, HSIRDY and HSIDIV bits are located in the RCC Source Control Register
(RCC_CR).
HSI calibration
RC oscillator frequencies can vary from one chip to another due to manufacturing process
variations. That is why each device is factory calibrated by STMicroelectronics to improve
accuracy (refer to the product datasheet for more information).
After a power-on reset, the factory calibration value is loaded in the HSICAL[11:0] bits.
If the application is subject to voltage or temperature variations, this may affect the RC
oscillator frequency. The user application can trim the HSI frequency using the
HSITRIM[5:0] bits.
Note: HSICAL[11:0] and HSITRIM[5:0] bits are located in the RCC Internal Clock Source
Calibration Register (RCC_ICSCR).
CSI oscillator
The CSI is a low-power RC oscillator which can be used directly as system clock, peripheral
clock, or PLL input.
The CSI advantages are the following:
• Low-cost clock source since no external crystal is required
• Faster startup time than HSE (a few microseconds)
• Very low-power consumption,
The CSI provides a clock frequency of about 4 MHz, while the HSI is able to provide a clock
up to 64 MHz.
CSI frequency, even with frequency calibration, is less accurate than an external crystal
oscillator or ceramic resonator.
The CSI can be switched ON and OFF through the CSION bit. The CSIRDY flag indicates
whether the CSI is stable or not. At startup, the CSI output clock is not released until this bit
is set by hardware.
The CSI cannot be switched OFF if one of the two conditions is met:
• The CSI is used directly (via software mux) as system clock
• The CSI is selected as reference clock for PLL1, with PLL1 enabled and selected to
provide the system clock (via software mux).
In that case the hardware does not allow programming the CSION bit to ‘0’.
The CSI can be disabled or not when the system enters Stop mode (refer to Section 7.5.7:
Handling clock generators in Stop and Standby mode for additional information).
In addition, the CSI clock can be driven to the MCO2 output and used as clock source for
other application components.

Even if the CSI settling time is faster than the HSI, care must be taken when the CSI is used
as kernel clock for communication peripherals: the application has to take into account the
following parameters:
• the time interval between the moment where the peripheral generates a kernel clock
request and the moment where the clock is really available,
• the frequency precision.
Note: CSION and CSIRDY bits are located in the RCC Source Control Register (RCC_CR).
CSI calibration
RC oscillator frequencies can vary from one chip to another due to manufacturing process
variations, this is why each device is factory calibrated by STMicroelectronics to achieve
improve (refer to the product datasheet for more information).
After reset, the factory calibration value is loaded in the CSICAL[7:0] bits.
If the application is subject to voltage or temperature variations, this may affect the RC
oscillator frequency. The user application can trim the CSI frequency using the
CSITRIM[4:0] bits.
Note: CSICAL[7:0] and CSITRIM[4:0] bits are located into the RCC Internal Clock Source
Calibration Register (RCC_ICSCR)
HSI48 oscillator
The HSI48 is an RC oscillator that delivers a 48 MHz clock that can be used directly as
kernel clock for some peripherals.
The HSI48 oscillator mainly aims at providing a high precision clock to the USB peripheral
by means of a special Clock Recovery System (CRS) circuitry, which could use the USB
SOF signal, the LSE, or an external signal, to automatically adjust the oscillator frequency
on-the-fly, in very small granularity.
The HSI48 oscillator is disabled as soon as the system enters Stop or Standby mode. When
the CRS is not used, this oscillator is free running and thus subject to manufacturing
process variations. That is why each device is factory calibrated by STMicroelectronics to
achieve an accuracy of ACCHSI48 (refer to the product datasheet of the for more
information).
For more details on how to configure and use the CRS, please refer to Section 8: Clock
recovery system (CRS).
The HSI48RDY flag indicates whether the HSI48 oscillator is stable or not. At startup, the
HSI48 output clock is not released until this bit is set by hardware.
The HSI48 can be switched ON and OFF using the HSI48ON bit.
The HSI48 clock can also be driven to the MCO1 multiplexer and used as clock source for
other application components.
Note: HSI48ON and HSI48RDY bits are located in the RCC Source Control Register (RCC_CR).

LSI oscillator
The LSI acts as a low-power clock source that can be kept running when the system is in
Stop or Standby mode for the independent watchdog (IWDG) and Auto-Wakeup Unit
(AWU). The clock frequency is around 32 kHz. For more details, refer to the electrical
characteristics section of the datasheet.
The LSI can be switched ON and OFF using the LSION bit. The LSIRDY flag indicates
whether the LSI oscillator is stable or not. If an independent watchdog is started either by
hardware or software, the LSI is forced ON and cannot be disabled.
The LSI remains enabled when the system enters Stop or Standby mode (refer to
Section 7.5.7: Handling clock generators in Stop and Standby mode for additional
information).
At LSI startup, the clock is not provided until the hardware sets the LSIRDY bit. An interrupt
can be generated if enabled in the RCC Clock Source Interrupt Enable Register
(RCC_CIER).
In addition, the LSI clock can be driven to the MCO2 output and used as a clock source for
other application components.
Note: Bits LSION and LSIRDY are located into the RCC Clock Control and Status Register
(RCC_CSR).
7.5.3 Clock Security System (CSS)
CSS on HSE
The clock security system can be enabled by software via the HSECSSON bit. The
HSECSSON bit can be enabled even when the HSEON is set to ‘0’.
The CSS on HSE is enabled by the hardware when the HSE is enabled and ready, and
HSECSSON set to ‘1’.
The CSS on HSE is disabled when the HSE is disabled. As a result, this function does not
work when the system is in Stop mode.
It is not possible to clear directly the HSECSSON bit by software.
The HSECSSON bit is cleared by hardware when a system reset occurs or when the
system enters Standby mode (see Section 7.4.2: System reset).
If a failure is detected on the HSE clock, the system automatically switches to the HSI in
order to provide a safe clock. The HSE is then automatically disabled, a clock failure event
is sent to the break inputs of advanced-control timers (TIM1, TIM8, TIM15, TIM16, and
TIM17), and an interrupt is generated to inform the software about the failure (CSS interrupt:
rcc_hsecss_it), thus allowing the MCU to perform rescue operations. If the HSE output
was used as clock source for PLLs when the failure occurred, the PLLs are also disabled.
If an HSE clock failure occurs when the CSS is enabled, the CSS generates an interrupt
which causes the automatic generation of an NMI. The HSECSSF flag in RCC Clock Source
Interrupt Flag Register (RCC_CIFR) is set to ‘1’ to allow the application to identify the failure
source. The NMI routine is executed indefinitely until the HSECSSF bit is cleared. As a
consequence, the application has to clear the HSECSSF flag in the NMI ISR by setting the
HSECSSC bit in the RCC Clock Source Interrupt Clear Register (RCC_CICR).
RM0433 Rev 5 325/3247
RM0433 Reset and Clock Control (RCC)
481
CSS on LSE
A clock security system on the LSE oscillator can be enabled by software by programming
the LSECSSON bit in the RCC Backup Domain Control Register (RCC_BDCR).
This bit can be disabled only by hardware when the following conditions are met:
• after a pwr_vsw_rst (VSW software reset)
• or after a failure detection on LSE.
LSECSSON bit must be written after the LSE is enabled (LSEON bit set by software) and
ready (LSERDY set by hardware), and after the RTC clock has been selected through the
RTCSEL bit.
The CSS on LSE works in all modes (Run, Stop and Standby) except VBAT.
If an LSE failure is detected, the LSE clock is no more delivered to the RTC but the value of
RTCSEL, LSECSSON and LSEON bits are not changed by the hardware.
A wakeup is generated in Standby mode. In other modes an interrupt (rcc_lsecss_it) can
be sent to wake up the software. The software must then disable the LSECSSON bit, stop
the defective LSE (clear LSEON bit), and can change the RTC clock source (no clock or LSI
or HSE) through RTCSEL bits, or take any required action to secure the application.
7.5.4 Clock output generation (MCO1/MCO2)
Two micro-controller clock output (MCO) pins, MCO1 and MCO2, are available. A clock
source can be selected for each output.The selected clock can be divided thanks to
configurable prescaler (refer to Figure 41 for additional information on signal selection).
MCO1 and MCO2 outputs are controlled via MCO1PRE[3:0], MCO1[2:0], MCO2PRE[3:0]
and MCO2[2:0] located in the RCC Clock Configuration Register (RCC_CFGR).
The GPIO port corresponding to each MCO pin, has to be programmed in alternate function
mode.
The clock provided to the MCOs outputs must not exceed the maximum pin speed (refer to
the product datasheet for information on the supported pin speed).
Reset and Clock Control (RCC) RM0433
326/3247 RM0433 Rev 5
7.5.5 PLL description
The RCC features three PLLs:
• A main PLL, PLL1, which is generally used to provide clocks to the CPU and to some
peripherals.
• Two dedicated PLLs, PLL2 and PLL3, which are used to generate the kernel clock for
peripherals.
The PLLs integrated into the RCC are completely independent. They offer the following
features:
• Two embedded VCOs:
– A wide-range VCO (VCOH)
– A low-frequency VCO (VCOL)
• Input frequency range:
– 1 to 2 MHz when VCOL is used
– 2 to 16 MHz when VCOH is used
• Capability to work either in integer or Fractional mode
• 13-bit Sigma-Delta modulator, allowing to fine-tune the VCO frequency by steps of 11
to 0.3 ppm.
• The Sigma-Delta modulator can be updated on-the-fly, without generating frequency
overshoots on PLLs outputs.
• Each PLL offer 3 outputs with post-dividers

The PLLs are controlled via RCC_PLLxDIVR, RCC_PLLxFRACR, RCC_PLLCFGR and
RCC_CR registers.
The frequency of the reference clock provided to the PLLs (refx_ck) must range from 1 to
16 MHz. The user application has to program properly the DIVMx dividers of the RCC PLLs
Clock Source Selection Register (RCC_PLLCKSELR) in order to match this condition. In
addition, the PLLxRGE of the RCC PLLs Configuration Register (RCC_PLLCFGR) field
must be set according to the reference input frequency to guarantee an optimal
performance of the PLL.
The user application can then configure the proper VCO: if the frequency of the reference
clock is lower or equal to 2 MHz, then VCOL must be selected, otherwise VCOH must be
chosen. To reduce the power consumption, it is recommended to configure the VCO output
to the lowest frequency.
DIVNx loop divider has to be programmed to achieve the expected frequency at VCO
output. In addition, the VCO output range must be respected.
The PLLs operate in integer mode when the value of SH_REG (FRACNx shadow register)
is set to ‘0’. The SH_REG is updated with the FRACNx value when PLLxFRACEN bit goes
from ‘0’ to ‘1’. The Sigma-Delta modulator is designed in order to minimize the jitter impact
while allowing very small frequency steps.
The PLLs can be enabled by setting PLLxON to ‘1’. The bits PLLxRDY indicate that the PLL
is ready (i.e. locked).
Note: Before enabling the PLLs, make sure that the reference frequency (refx_ck) provided to the
PLL is stable, so the hardware does not allow changing DIVMx when the PLLx is ON and it
is also not possible to change PLLSRC when one of the PLL is ON.
The hardware prevents writing PLL1ON to ‘0’ if the PLL1 is currently used to deliver the
system clock. There are other hardware protections on the clock generators (refer to
sections HSE oscillator, HSI oscillator and CSI oscillator).
The following PLL parameters cannot be changed once the PLL is enabled: DIVNx,
PLLxRGE, PLLxVCOSEL, DIVPx, DIVQx, DIVRx, DIVPxEN, DIVQxEN and DIVRxEN.
To insure an optimal behavior of the PLL when one of the post-divider (DIVP, DIVQ or DIVR)
is not used, the application shall set the enable bit (DIVyEN) as well as the corresponding
post-divider bits (DIVP, DIVQ or DIVR) to ‘0’.
If the above rules are not respected, the PLL output frequency is not guaranteed.
Output frequency computation
When the PLL is configured in integer mode (SH_REG = ‘0’), the VCO frequency (FVCO) is
given by the following expression:
When the PLL is configured in fractional mode (SH_REG different from ‘0’), the DIVN divider
must be initialized before enabling the PLLs. However, it is possible to change the value of
FRACNx on-the-fly without disturbing the PLL output.
FVCO = FREF_CK × DIVN
FPLL_y_CK = (FVCO ⁄ (DIVy + 1)) with y = P, Q or R
Reset and Clock Control (RCC) RM0433
328/3247 RM0433 Rev 5
This feature can be used either to generate a specific frequency from any crystal value with
a good accuracy, or to fine-tune the frequency on-the-fly.
For each PLL, the VCO frequency is given by the following formula:
Note: For PLL1, DIVP can only take odd values.
The PLLs are disabled by hardware when:
• The system enters Stop or Standby mode.
• An HSE failure occurs when HSE or PLL (clocked by HSE) are used as system clock.
PLL initialization phase
Figure 44 shows the recommended PLL initialization sequence in integer and fractional
mode. The PLLx are supposed to be disabled at the start of the initialization sequence:
1. Initialize the PLLs registers according to the required frequency.
– Set PLLxFRACEN of RCC PLLs Configuration Register (RCC_PLLCFGR) to ‘0’
for integer mode.
– For fractional mode, set FRACN to the required initial value (FracInitValue) and
then set PLLxFRACEN to ‘1’.
2. Once the PLLxON bit is set to ‘1’, the user application has to wait until PLLxRDY bit is
set to ‘1’. If the PLLx is in fractional mode, the PLLxFRACEN bit must not be set back
to ‘0’ as long as PLLxRDY = ‘0’.
3. Once the PLLxRDY bit is set to ‘1’, the PLLx is ready to be used.
4. If the application intends to tune the PLLx frequency on-the-fly (possible only in
fractional mode), then:
a) PLLxFRACEN must be set to ‘0’,
When PLLxFRACEN = ‘0’, the Sigma-Delta modulator is still operating with the
value latched into SH_REG.
b) A new value must be uploaded into PLLxFRACR (FracValue(n)).
c) PLLxFRACEN must be set to ‘1’, in order to latch the content of PLLxFRACR into
its shadow register.

7.5.6 System clock (sys_ck)
System clock selection
After a system reset, the HSI is selected as system clock and all PLLs are switched OFF.
When a clock source is used for the system clock, it is not possible for the software to
disable the selected source via the xxxON bits.
Of course, the system clock can be stopped by the hardware when the System enters Stop
or Standby mode.
When the system is running, the user application can select the system clock (sys_ck)
among the 4 following sources:
• HSE
• HSI
• CSI
• or pll1_p_ck
This function is controlled by programming the RCC Clock Configuration Register
(RCC_CFGR). A switch from one clock source to another occurs only if the target clock
source is ready (clock stable after startup delay or PLL locked). If a clock source that is not
yet ready is selected, the switch occurs when the clock source is ready.
The SWS status bits in the RCC Clock Configuration Register (RCC_CFGR) indicate which
clock is currently used as system clock. The other status bits in the RCC_CR register
indicate which clock(s) is (are) ready.
System clock generation
Figure 45 shows a simplified view of the clock distribution for the CPU and busses. All the
dividers shown in the block diagram can be changed on-the-fly without generating timing
violations. This feature is a very simply solution to adapt the busses frequencies to the
application needs, thus optimizing the power consumption.
The D1CPRE divider can be used to adjust the CPU clock. However this also impacts the
clock frequency of all bus matrix and HRTIM.
In the same way, HPRE divider can be used to adjust the clock for D1 domain bus matrix,
but this also impacts the clock frequency of bus matrix of D2 and D3 domains.
Most of the prescalers are controlled via RCC_D1CFGR, RCC_D2CFGR and
RCC_D3CFGR registers.

7.5.7 Handling clock generators in Stop and Standby mode
When the whole system enters Stop mode, all the clocks (system and kernel clocks) are
stopped as well as the following clock sources:
• CSI, HSI (depending on HSIKERON, and CSIKERON bits)
• HSE
• PLL1, PLL2 and PLL3
• HSI48
The content of the RCC registers is not altered except for PLL1ON, PLL2ON, PLL3ON
HSEON and HSI48ON which are set to ‘0’.
Exiting Stop mode
When the microcontroller exits system Stop mode via a wake-up event, the application can
select which oscillator (HSI and/or CSI) will be used to restart. The STOPWUCK bit selects
the oscillator used as system clock. The STOPKERWUCK bit selects the oscillator used as
kernel clock for peripherals. The STOPKERWUCK bit is useful if after a system Stop a
peripheral needs a kernel clock generated by an oscillator different from the one used for
the system clock.
All these bits belong to the RCC Clock Configuration Register (RCC_CFGR). Table 53 gives
a detailed description of their behavior.
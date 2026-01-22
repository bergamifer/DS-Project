50 Serial audio interface (SAI)
50.1 Introduction
The SAI interface (Serial Audio Interface) offers a wide set of audio protocols due to its
flexibility and wide range of configurations. Many stereo or mono audio applications may be
targeted. I2S standards, LSB or MSB-justified, PCM/DSP, TDM, and AC’97 protocols may
be addressed for example. SPDIF output is offered when the audio block is configured as a
transmitter.
To bring this level of flexibility and reconfigurability, the SAI contains two independent audio
sub-blocks. Each block has it own clock generator and I/O line controller.
The SAI can work in master or slave configuration. The audio sub-blocks can be either
receiver or transmitter and can work synchronously or not (with respect to the other one).
The SAI can be connected with other SAIs to work synchronously.
RM0433 Rev 5 2205/3247
RM0433 Serial audio interface (SAI)
2270
50.2 SAI main features
• Two independent audio sub-blocks which can be transmitters or receivers with their
respective FIFO.
• 8-word integrated FIFOs for each audio sub-block.
• Synchronous or asynchronous mode between the audio sub-blocks.
• Possible synchronization between multiple SAIs.
• Master or slave configuration independent for both audio sub-blocks.
• Clock generator for each audio block to target independent audio frequency sampling
when both audio sub-blocks are configured in master mode.
• Data size configurable: 8-, 10-, 16-, 20-, 24-, 32-bit.
• Audio protocol: I2S, LSB or MSB-justified, PCM/DSP, TDM, AC’97
• PDM interface, supporting up to 4 microphone pairs
• SPDIF output available if required.
• Up to 16 slots available with configurable size.
• Number of bits by frame can be configurable.
• Frame synchronization active level configurable (offset, bit length, level).
• First active bit position in the slot is configurable.
• LSB first or MSB first for data transfer.
• Mute mode.
• Stereo/Mono audio frame capability.
• Communication clock strobing edge configurable (SCK).
• Error flags with associated interrupts if enabled respectively.
– Overrun and underrun detection,
– Anticipated frame synchronization signal detection in slave mode,
– Late frame synchronization signal detection in slave mode,
– Codec not ready for the AC’97 mode in reception.
• Interruption sources when enabled:
– Errors,
– FIFO requests.
• 2-channel DMA interface.
Serial audio interface (SAI) RM0433
2206/3247 RM0433 Rev 5
50.3 SAI functional description
50.3.1 SAI block diagram
Figure 642 shows the SAI block diagram while Table 407 and Table 408 list SAI internal and
external signals.

The SAI is mainly composed of two audio sub-blocks with their own clock generator. Each
audio block integrates a 32-bit shift register controlled by their own functional state machine.
Data are stored or read from the dedicated FIFO. FIFO may be accessed by the CPU, or by
DMA in order to leave the CPU free during the communication. Each audio block is
independent. They can be synchronous with each other.
An I/O line controller manages a set of 4 dedicated pins (SD, SCK, FS, MCLK) for a given
audio block in the SAI. Some of these pins can be shared if the two sub-blocks are declared
as synchronous to leave some free to be used as general purpose I/Os. The MCLK pin can
be output, or not, depending on the application, the decoder requirement and whether the
audio block is configured as the master.
If one SAI is configured to operate synchronously with another one, even more I/Os can be
freed (except for pins SD_x).
The functional state machine can be configured to address a wide range of audio protocols.
Some registers are present to set-up the desired protocols (audio frame waveform
generator).

The audio sub-block can be a transmitter or receiver, in master or slave mode. The master
mode means the SCK_x bit clock and the frame synchronization signal are generated from
the SAI, whereas in slave mode, they come from another external or internal master. There
is a particular case for which the FS signal direction is not directly linked to the master or
slave mode definition. In AC’97 protocol, it will be an SAI output even if the SAI (link
controller) is set-up to consume the SCK clock (and so to be in Slave mode).

SAI pins and internal signals
50.3.3 Main SAI modes
Each audio sub-block of the SAI can be configured to be master or slave via MODE bits in
the SAI_xCR1 register of the selected audio block.
Master mode
In master mode, the SAI delivers the timing signals to the external connected device:
• The bit clock and the frame synchronization are output on pin SCK_x and FS_x,
respectively.
• If needed, the SAI can also generate a master clock on MCLK_x pin.
Table 407. SAI internal input/output signals
Internal signal name Signal type Description
sai_a_gbl_it/
sai_b_gbl_it Output Audio block A and B global interrupts.
sai_a_dma,
sai_b_dma Input/output Audio block A and B DMA acknowledges and requests.
sai_sync_out_sck,
sai_sync_out_fs Output Internal clock and frame synchronization output signals
exchanged with other SAI blocks.
sai_sync_in_sck,
sai_sync_in_fs Input Internal clock and frame synchronization input signals
exchanged with other SAI blocks.
sai_a_ker_ck/
sai_b_ker_ck Input Audio block A/B kernel clock.
sai_pclk Input APB clock.
Table 408. SAI input/output pins
Name Signal type Comments
SAI_SCK_A/B Input/output Audio block A/B bit clock.
SAI_MCLK_A/B Output Audio block A/B master clock.
SAI_SD_A/B Input/output Data line for block A/B.
SAI_FS_A/B Input/output Frame synchronization line for audio block A/B.
SAI_CK[4:1] Output PDM bitstream clock.
SAI_D[4:1] Input PDM bitstream data.


Both SCK_x, FS_x and MCLK_x are configured as outputs.
Slave mode
The SAI expects to receive timing signals from an external device.
• If the SAI sub-block is configured in asynchronous mode, then SCK_x and FS_x pins
are configured as inputs.
• If the SAI sub-block is configured to operate synchronously with another SAI interface
or with the second audio sub-block, the corresponding SCK_x and FS_x pins are left
free to be used as general purpose I/Os.
In slave mode, MCLK_x pin is not used and can be assigned to another function.
It is recommended to enable the slave device before enabling the master.
Configuring and enabling SAI modes
Each audio sub-block can be independently defined as a transmitter or receiver through the
MODE bit in the SAI_xCR1 register of the corresponding audio block. As a result, SAI_SD_x
pin will be respectively configured as an output or an input.
Two master audio blocks in the same SAI can be configured with two different MCLK and
SCK clock frequencies. In this case they have to be configured in asynchronous mode.
Each of the audio blocks in the SAI are enabled by SAIEN bit in the SAI_xCR1 register. As
soon as this bit is active, the transmitter or the receiver is sensitive to the activity on the
clock line, data line and synchronization line in slave mode.
In master TX mode, enabling the audio block immediately generates the bit clock for the
external slaves even if there is no data in the FIFO, However FS signal generation is
conditioned by the presence of data in the FIFO. After the FIFO receives the first data to
transmit, this data is output to external slaves. If there is no data to transmit in the FIFO, 0
values are then sent in the audio frame with an underrun flag generation.
In slave mode, the audio frame starts when the audio block is enabled and when a start of
frame is detected.
In Slave TX mode, no underrun event is possible on the first frame after the audio block is
enabled, because the mandatory operating sequence in this case is:
1. Write into the SAI_xDR (by software or by DMA).
2. Wait until the FIFO threshold (FLH) flag is different from 0b000 (FIFO empty).
3. Enable the audio block in slave transmitter mode.
50.3.4 SAI synchronization mode
There are two levels of synchronization, either at audio sub-block level or at SAI level.
Internal synchronization
An audio sub-block can be configured to operate synchronously with the second audio subblock
in the same SAI. In this case, the bit clock and the frame synchronization signals are
shared to reduce the number of external pins used for the communication. The audio block
configured in synchronous mode sees its own SCK_x, FS_x, and MCLK_x pins released
back as GPIOs while the audio block configured in asynchronous mode is the one for which
FS_x and SCK_x ad MCLK_x I/O pins are relevant (if the audio block is considered as
master).

Typically, the audio block in synchronous mode can be used to configure the SAI in full
duplex mode. One of the two audio blocks can be configured as a master and the other as
slave, or both as slaves with one asynchronous block (corresponding SYNCEN[1:0] bits set
to 00 in SAI_xCR1) and one synchronous block (corresponding SYNCEN[1:0] bits set to 01
in the SAI_xCR1).
Note: Due to internal resynchronization stages, PCLK APB frequency must be higher than twice
the bit rate clock frequency.
External synchronization
The audio sub-blocks can also be configured to operate synchronously with another SAI.
This can be done as follow:
1. The SAI, which is configured as the source from which the other SAI is synchronized,
has to define which of its audio sub-block is supposed to provide the FS and SCK
signals to other SAI. This is done by programming SYNCOUT[1:0] bits.
2. The SAI which shall receive the synchronization signals has to select which SAI will
provide the synchronization by setting the proper value on SYNCIN[1:0] bits. For each
of the two SAI audio sub-blocks, the user must then specify if it operates synchronously
with the other SAI via the SYNCEN bit.
Note: SYNCIN[1:0] and SYNCOUT[1:0] bits are located into the SAI_GCR register, and SYNCEN
bits into SAI_xCR1 register.
If both audio sub-blocks in a given SAI need to be synchronized with another SAI, it is
possible to choose one of the following configurations:
• Configure each audio block to be synchronous with another SAI block through the
SYNCEN[1:0] bits.
• Configure one audio block to be synchronous with another SAI through the
SYNCEN[1:0] bits. The other audio block is then configured as synchronous with the
second SAI audio block through SYNCEN[1:0] bits.
The following table shows how to select the proper synchronization signal depending on the
SAI block used. For example SAI2 can select the synchronization from SAI1 by setting SAI2
SYNCIN to 0. If SAI1 wants to select the synchronization coming from SAI2, SAI1 SYNCIN
must be set to 1. Positions noted as ‘Res.’ shall not be used

Table 409. External synchronization selection
Block instance SYNCIN= 3 SYNCIN= 2 SYNCIN= 1 SYNCIN= 0
SAI1 SAI4 sync SAI3 sync SAI2 sync Res.
SAI2 SAI4 sync SAI3 sync Res. SAI1 sync
SAI3 SAI4 sync Res. SAI2 sync SAI1 sync
SAI4 Res. SAI3 sync SAI2 sync SAI1 sync

50.3.5 Audio data size
The audio frame can target different data sizes by configuring bit DS[2:0] in the SAI_xCR1
register. The data sizes may be 8, 10, 16, 20, 24 or 32 bits. During the transfer, either the
MSB or the LSB of the data are sent first, depending on the configuration of bit LSBFIRST in
the SAI_xCR1 register.

50.3.6 Frame synchronization
The FS signal acts as the Frame synchronization signal in the audio frame (start of frame).
The shape of this signal is completely configurable in order to target the different audio
protocols with their own specificities concerning this Frame synchronization behavior. This
reconfigurability is done using register SAI_xFRCR. Figure 643 illustrates this flexibility

In AC’97 mode or in SPDIF mode (bit PRTCFG[1:0] = 10 or PRTCFG[1:0] = 01 in the
SAI_xCR1 register), the frame synchronization shape is forced to match the AC’97 protocol.
The SAI_xFRCR register value is ignored.
Each audio block is independent and consequently each one requires a specific
configuration.
Frame length
• Master mode
The audio frame length can be configured to up to 256 bit clock cycles, by setting
FRL[7:0] field in the SAI_xFRCR register.
If the frame length is greater than the number of declared slots for the frame, the
remaining bits to transmit will be extended to 0 or the SD line will be released to HI-z
depending the state of bit TRIS in the SAI_xCR2 register (refer to FS signal role). In
reception mode, the remaining bit is ignored.
If bit NOMCK is cleared, (FRL+1) must be equal to a power of 2, from 8 to 256, to
ensure that an audio frame contains an integer number of MCLK pulses per bit clock
cycle.
If bit NOMCK is set, the (FRL+1) field can take any value from 8 to 256. Refer to
Section 50.3.8: SAI clock generator”.
• Slave mode
The audio frame length is mainly used to specify to the slave the number of bit clock
cycles per audio frame sent by the external master. It is used mainly to detect from the
master any anticipated or late occurrence of the Frame synchronization signal during
an on-going audio frame. In this case an error will be generated. For more details refer
to Section 50.3.14: Error flags.
In slave mode, there are no constraints on the FRL[7:0] configuration in the
SAI_xFRCR register.
The number of bits in the frame is equal to FRL[7:0] + 1.

The minimum number of bits to transfer in an audio frame is 8.
Frame synchronization polarity
FSPOL bit in the SAI_xFRCR register sets the active polarity of the FS pin from which a
frame is started. The start of frame is edge sensitive.
In slave mode, the audio block waits for a valid frame to start transmitting or receiving. Start
of frame is synchronized to this signal. It is effective only if the start of frame is not detected
during an ongoing communication and assimilated to an anticipated start of frame (refer to
Section 50.3.14: Error flags).
In master mode, the frame synchronization is sent continuously each time an audio frame is
complete until the SAIEN bit in the SAI_xCR1 register is cleared. If no data are present in
the FIFO at the end of the previous audio frame, an underrun condition will be managed as
described in Section 50.3.14: Error flags), but the audio communication flow will not be
interrupted.
Frame synchronization active level length
The FSALL[6:0] bits of the SAI_xFRCR register allow configuring the length of the active
level of the Frame synchronization signal. The length can be set from 1 to 128 bit clock
cycles.
As an example, the active length can be half of the frame length in I2S, LSB or MSB-justified
modes, or one-bit wide for PCM/DSP or TDM mode.
Frame synchronization offset
Depending on the audio protocol targeted in the application, the Frame synchronization
signal can be asserted when transmitting the last bit or the first bit of the audio frame (this is
the case in I2S standard protocol and in MSB-justified protocol, respectively). FSOFF bit in
the SAI_xFRCR register allows to choose one of the two configurations.
FS signal role
The FS signal can have a different meaning depending on the FS function. FSDEF bit in the
SAI_xFRCR register selects which meaning it will have:
• 0: start of frame, like for instance the PCM/DSP, TDM, AC’97, audio protocols,
• 1: start of frame and channel side identification within the audio frame like for the I2S,
the MSB or LSB-justified protocols.
When the FS signal is considered as a start of frame and channel side identification within
the frame, the number of declared slots must be considered to be half the number for the left
channel and half the number for the right channel. If the number of bit clock cycles on half
audio frame is greater than the number of slots dedicated to a channel side, and TRIS = 0, 0
is sent for transmission for the remaining bit clock cycles in the SAI_xCR2 register.
Otherwise if TRIS = 1, the SD line is released to HI-Z. In reception mode, the remaining bit
clock cycles are not considered until the channel side changes.

If FSDEF bit in SAI_xFRCR is kept clear, so FS signal is equivalent to a start of frame, and if
the number of slots defined in NBSLOT[3:0] in SAI_xSLOTR multiplied by the number of bits
by slot configured in SLOTSZ[1:0] in SAI_xSLOTR is less than the frame size (bit FRL[7:0]
in the SAI_xFRCR register), then:
• if TRIS = 0 in the SAI_xCR2 register, the remaining bit after the last slot will be forced to
0 until the end of frame in case of transmitter,
• if TRIS = 1, the line will be released to HI-Z during the transfer of these remaining bits.
In reception mode, these bits are discarded.

The FS signal is not used when the audio block in transmitter mode is configured to get the
SPDIF output on the SD line. The corresponding FS I/O will be released and left free for
other purposes.
50.3.7 Slot configuration
The slot is the basic element in the audio frame. The number of slots in the audio frame is
equal to NBSLOT[3:0] + 1.
The maximum number of slots per audio frame is fixed at 16.
For AC’97 protocol or SPDIF (when bit PRTCFG[1:0] = 10 or PRTCFG[1:0] = 01), the
number of slots is automatically set to target the protocol specification, and the value of
NBSLOT[3:0] is ignored.
Each slot can be defined as a valid slot, or not, by setting SLOTEN[15:0] bits of the
SAI_xSLOTR register.
When a invalid slot is transferred, the SD data line is either forced to 0 or released to HI-z
depending on TRIS bit configuration (refer to Output data line management on an inactive
slot) in transmitter mode. In receiver mode, the received value from the end of this slot is
ignored. Consequently, there will be no FIFO access and so no request to read or write the
FIFO linked to this inactive slot status.
The slot size is also configurable as shown in Figure 646. The size of the slots is selected by
setting SLOTSZ[1:0] bits in the SAI_xSLOTR register. The size is applied identically for
each slot in an audio frame.

50.3.8 SAI clock generator
Each audio sub-block has its own clock generator that makes these two blocks completely
independent. There is no difference in terms of functionality between these two clock
generators.
When the audio block is configured as master, the clock generator provides the bit clock
(SCK_x) and the master clock (MCLK_x) for external decoders. The frame synchronization
(FS_x) is also derived from the signals provided by the clock generator. The clock source for
the SAI clock generator (sai_x_ker_ck) is delivered by the product clock controller (RCC).
When the audio block is defined as slave, the clock generator is OFF. The value of NOMCK,
MCKDIV and OSR bits are ignored. In addition, MCLK_x I/O pin is released and can be
used as a general purpose I/O.
The bit clock strobing edge of (SCK_x) can be configured through CKSTR bit in the
SAI_xCR1 register. This bit is functional in master and slave mode.
Figure 648 illustrates the architecture of the audio block clock generator.

The NOMCK bit of the SAI_xCR1 register is used to define whether the master clock is
generated or not.
When the SAI is used in master mode, the clock generator configuration differs depending
on whether a master clock (MCLK_x) needs to be provided or not.
If NOMCK is set to 1, the master clock is not generated, and the user has more flexibility to
select the frame length and frame synchronization frequency. In addition, MCLK_x signal is
driven Low if this pin is configured as the SAI pin in GPIO peripherals. MCKDIV can still be
used to adjust the SCK_x clock to the required frequency.
If NOMCK is set to 0, the master clock is generated, and can be used as reference clock for
external decoders. In this case, the frequency ratio between the frame synchronization and
the master clock is fixed to 512 or 256, and the frame length must be a power of 2. More
details are given hereafter.

Clock generator programming examples
Table 410 shows some programming examples for 48, 96 and 192 kHz.
50.3.9 Internal FIFOs
Each audio block in the SAI has its own FIFO. Depending if the block is defined to be a
transmitter or a receiver, the FIFO can be written or read, respectively. There is therefore
only one FIFO request linked to FREQ bit in the SAI_xSR register.
An interrupt is generated if FREQIE bit is enabled in the SAI_xIM register. This depends on:
• FIFO threshold setting (FLVL bits in SAI_xCR2)
• Communication direction (transmitter or receiver). Refer to Interrupt generation in
transmitter mode and Interrupt generation in reception mode.
Interrupt generation in transmitter mode
The interrupt generation depends on the FIFO configuration in transmitter mode:
• When the FIFO threshold bits in SAI_xCR2 register are configured as FIFO empty
(FTH[2:0] set to 0b000), an interrupt is generated (FREQ bit set by hardware to 1 in
SAI_xSR register) if no data are available in SAI_xDR register (FLVL[2:0] bits in SAI_xSR
is less than 001b). This Interrupt (FREQ bit in SAI_xSR register) is cleared by hardware
when the FIFO is no more empty (FLVL[2:0] bits in SAI_xSR are different from 0b000) i.e
one or more data are stored in the FIFO.
• When the FIFO threshold bits in SAI_xCR2 register are configured as FIFO quarter full
(FTH[2:0] set to 001b), an interrupt is generated (FREQ bit set by hardware to 1 in
SAI_xSR register) if less than a quarter of the FIFO contains data (FLVL[2:0] bits in
SAI_xSR are less than 0b010). This Interrupt (FREQ bit in SAI_xSR register) is cleared
by hardware when at least a quarter of the FIFO contains data (FLVL[2:0] bits in SAI_xSR
are higher or equal to 0b010).
• When the FIFO threshold bits in SAI_xCR2 register are configured as FIFO half full
(FTH[2:0] set to 0b010), an interrupt is generated (FREQ bit set by hardware to 1 in
Table 410. Clock generator programming examples
Input
sai_x_ker_ck
clock frequency
MCLK FMCLK/ FFS FRL (1) OSR NOMCK MCKDIV[5:0] Audio Sampling
frequency (FFS)
98.304 MHz
Y
512 2N-1 1 0 0 or 1 192 kHz
512 2N-1 1 0 2 96 kHz
512 2N-1 1 0 4 48 kHz
256 2N-1 0 0 2 192 kHz
256 2N-1 0 0 4 96 kHz
256 2N-1 0 0 8 48 kHz
N
- 63 - 1 8 192 kHz
- 63 - 1 16 96 kHz
- 63 - 1 32 48 kHz

SAI_xSR register) if less than half of the FIFO contains data (FLVL[2:0] bits in SAI_xSR
are less than 011b). This Interrupt (FREQ bit in SAI_xSR register) is cleared by hardware
when at least half of the FIFO contains data (FLVL[2:0] bits in SAI_xSR are higher or
equal to 011b).
• When the FIFO threshold bits in SAI_xCR2 register are configured as FIFO three quarter
(FTH[2:0] set to 011b), an interrupt is generated (FREQ bit is set by hardware to 1 in
SAI_xSR register) if less than three quarters of the FIFO contain data (FLVL[2:0] bits in
SAI_xSR are less than 0b100). This Interrupt (FREQ bit in SAI_xSR register) is cleared
by hardware when at least three quarters of the FIFO contain data (FLVL[2:0] bits in
SAI_xSR are higher or equal to 0b100).
• When the FIFO threshold bits in SAI_xCR2 register are configured as FIFO full (FTH[2:0]
set to 0b100), an interrupt is generated (FREQ bit is set by hardware to 1 in SAI_xSR
register) if the FIFO is not full (FLVL[2:0] bits in SAI_xSR is less than 101b). This Interrupt
(FREQ bit in SAI_xSR register) is cleared by hardware when the FIFO is full (FLVL[2:0]
bits in SAI_xSR is equal to 101b value).

Interrupt generation in reception mode
The interrupt generation depends on the FIFO configuration in reception mode:
• When the FIFO threshold bits in SAI_xCR2 register are configured as FIFO empty
(FTH[2:0] set to 0b000), an interrupt is generated (FREQ bit is set by hardware to 1 in
SAI_xSR register) if at least one data is available in SAI_xDR register(FLVL[2:0] bits in
SAI_xSR is higher or equal to 001b). This Interrupt (FREQ bit in SAI_xSR register) is
cleared by hardware when the FIFO becomes empty (FLVL[2:0] bits in SAI_xSR is equal
to 0b000) i.e no data are stored in FIFO.
• When the FIFO threshold bits in SAI_xCR2 register are configured as FIFO quarter fully
(FTH[2:0] set to 001b), an interrupt is generated (FREQ bit is set by hardware to 1 in
SAI_xSR register) if at least one quarter of the FIFO data locations are available
(FLVL[2:0] bits in SAI_xSR is higher or equal to 0b010). This Interrupt (FREQ bit in
SAI_xSR register) is cleared by hardware when less than a quarter of the FIFO data
locations become available (FLVL[2:0] bits in SAI_xSR is less than 0b010).
• When the FIFO threshold bits in SAI_xCR2 register are configured as FIFO half fully
(FTH[2:0] set to 0b010 value), an interrupt is generated (FREQ bit is set by hardware to
1 in SAI_xSR register) if at least half of the FIFO data locations are available (FLVL[2:0]
bits in SAI_xSR is higher or equal to 011b). This Interrupt (FREQ bit in SAI_xSR register)
is cleared by hardware when less than half of the FIFO data locations become available
(FLVL[2:0] bits in SAI_xSR is less than 011b).
• When the FIFO threshold bits in SAI_xCR2 register are configured as FIFO three quarter
full(FTH[2:0] set to 011b value), an interrupt is generated (FREQ bit is set by hardware to
1 in SAI_xSR register) if at least three quarters of the FIFO data locations are available
(FLVL[2:0] bits in SAI_xSR is higher or equal to 0b100). This Interrupt (FREQ bit in
SAI_xSR register) is cleared by hardware when the FIFO has less than three quarters of
the FIFO data locations avalable(FLVL[2:0] bits in SAI_xSR is less than 0b100).
• When the FIFO threshold bits in SAI_xCR2 register are configured as FIFO full(FTH[2:0]
set to 0b100), an interrupt is generated (FREQ bit is set by hardware to 1 in SAI_xSR
register) if the FIFO is full (FLVL[2:0] bits in SAI_xSR is equal to 101b). This Interrupt
(FREQ bit in SAI_xSR register) is cleared by hardware when the FIFO is not full
(FLVL[2:0] bits in SAI_xSR is less than 101b).

Like interrupt generation, the SAI can use the DMA if DMAEN bit in the SAI_xCR1 register is
set. The FREQ bit assertion mechanism is the same as the interruption generation
mechanism described above for FREQIE.
Each FIFO is an 8-word FIFO. Each read or write operation from/to the FIFO targets one
word FIFO location whatever the access size. Each FIFO word contains one audio slot.
FIFO pointers are incremented by one word after each access to the SAI_xDR register.
Data should be right aligned when it is written in the SAI_xDR.
Data received will be right aligned in the SAI_xDR.
The FIFO pointers can be reinitialized when the SAI is disabled by setting bit FFLUSH in the
SAI_xCR2 register. If FFLUSH is set when the SAI is enabled the data present in the FIFO
will be lost automatically.
50.3.10 PDM Interface
The PDM (Pulse Density Modulation) interface is provided in order to support digital
microphones. Up to 4 digital microphone pairs can be connected in parallel. Figure 649
shows a typical connection of a digital microphone pair via a PDM interface. Both
microphones share the same bitstream clock and data line. Thanks to a configuration pin
(LR), a microphone can provide valid data on SAI_CK[m] rising edge while the other
provides valid data on SAI_CK[m] falling edge (m being the number of clock lines).

The data processing sequence into the PDM is the following:
1. The PDM interface builds the bitstream clock from the bit clock received from the TDM
interface of SAI_A.
2. The bitstream data received from the microphones (SAI_D[n]) are de-interleaved and
go through a 7-bit delay line in order to fine-tune the delay of each microphone with the
accuracy of the bitstream clock.
3. The shift registers translate each serial bitstream into bytes.
4. The last operation consists in shifting-out the resulting bytes to SAI_A via the serial
data line of the TDM interface.
Figure 650 hereafter shows the block diagram of PDM interface, with a detailed view of a
de-interleaver.
Note: The PDM interface does not embed the decimation filter required to build-up the PCM audio
samples from the bitstream. It is up to the application software to perform this operation.


TDM configuration for PDM interface
SAI_A TDM interface is internally connected to the PDM interface to get the microphone
samples. The user application must configure the PDM interface as shown in Table 411 to
ensure a good connection with the PDM interface.

Table 411. TDM settings
Bit Fields Values Comments
MODE 0b01 Mode must be MASTER receiver
PRTCFG 0b00 Free protocol for TDM
DS X To be adjusted according to the required data format, in accordance to the
frame length and the number of slots (FRL and NBSLOT). See Table 412.
LSBFIRST X This parameter can be used according to the wanted data format
CKSTR 0 Signal transitions occur on the rising edge of the SCK_A bit clock. Signals
are stable on the falling edge of the bit clock.
MONO 0 Stereo mode
FRL X To be adjusted according to the number of microphones (MICNBR). See
Table 412.
FSALL 0 Pulse width is one bit clock cycle
FSDEF 0 FS signal is a start of frame
FSPOL 1 FS is active High
FSOFF 0 FS is asserted on the first bit of slot 0
FBOFF 0 No offset on slot
SLOTSZ 0 Slot size = data size
NBSLOT X To be adjusted according to the required data format, in accordance to the
slot size, and the frame length (FRL and DS). See Table 412.
SLOTEN X To be adjusted according to NBSLOT
NOMCK 1 No need to generate a master clock MCLK
MCKDIV X
Depends on the frequency provided to sai_a_ker_ck input.
This parameter shall be adjusted to generate the proper bitstream clock
frequency. See Table 412.
/*
 * librerias.c
 *
 *  Created on: 30/10/2013
 *      Author: Manolo
 */


#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/ssi.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"



#include "inc/hw_ints.h"
#include "driverlib/debug.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "FT800_TIVA.h"


extern int RELOJ;
/*
 * ======== Standard MSP430 includes ========
 */
//#include <msp430.h>

/*
 * ======== Grace related includes ========
 */
//#include <ti/mcu/msp430/Grace.h>

// =======================================================================
// Function Declarations
// =======================================================================
#define dword long
#define byte char

#define LEDOn()  ROM_GPIOPinWrite(CLP_D1_PORT, CLP_D1_PIN, CLP_D1_PIN);

#define LEDOff() ROM_GPIOPinWrite(CLP_D1_PORT, CLP_D1_PIN, 0)

extern char chipid ;                        // Holds value of Chip ID read from the FT800

extern unsigned long cmdBufferRd;         // Store the value read from the REG_CMD_READ register
extern unsigned long cmdBufferWr;         // Store the value read from the REG_CMD_WRITE register
extern unsigned int t;
// ############################################################################################################
// User Application - Initialization of MCU / FT800 / Display
// ############################################################################################################

extern int Fin_Rx;
extern char Buffer_Rx;
//extern unsigned long POSX, POSY, BufferXY;
extern unsigned int CMD_Offset;
extern unsigned long POSX, POSY, BufferXY;

uint32_t SSI_BASE,CS_PORT,CS_PIN,PD_PORT,PD_PIN;

// ############################################################################################################
// FT800 SPI Functions
// ############################################################################################################

// ############################################################################################################
// ############################################################################################################
// 										HAL functions
// ############################################################################################################
// ############################################################################################################
void HAL_Init_SPI(uint8_t Port, uint32_t RELOJ_ACT){
	//Inicializar el puerto SSI en función del BP usado, 1 ó 2
	switch (Port){
	case 1:

	    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI2);
	    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
	    //
	    //  pin PN3 es /CS
	    //
	    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_3);
	    CS_PORT=GPIO_PORTN_BASE;
	    CS_PIN=GPIO_PIN_3;

	    //
	    // pin PN2 es /PD
	    //
	    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_2);
	    PD_PORT=GPIO_PORTN_BASE;
	    PD_PIN=GPIO_PIN_2;

	    //
	    //  PD0 es SSI2 SSI2XDAT1
	    //
	    GPIOPinConfigure(GPIO_PD0_SSI2XDAT1);
	    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_0);

	    //
	    // PD1 es SSI2 SSI2XDAT0
	    //
	    GPIOPinConfigure(GPIO_PD1_SSI2XDAT0);
	    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_1);

	    //
	    // PD3 es SSI2 SSI2CLK
	    //
	    GPIOPinConfigure(GPIO_PD3_SSI2CLK);
	    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_3);
	    SSI_BASE=SSI2_BASE;
	    break;
	case 2:
		SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI3);
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOQ);
		SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
		//
		//  pin PQ1 es /CS
		//
		GPIOPinTypeGPIOOutput(GPIO_PORTQ_BASE, GPIO_PIN_1);
		CS_PORT=GPIO_PORTQ_BASE;
		CS_PIN=GPIO_PIN_1;

		//
		// pin PP3 es /PD
		//
		GPIOPinTypeGPIOOutput(GPIO_PORTP_BASE, GPIO_PIN_3);
		PD_PORT=GPIO_PORTP_BASE;
		PD_PIN=GPIO_PIN_3;

		//
		//  PQ3 es SSI3 SSI2XDAT1
		//
		GPIOPinConfigure(GPIO_PQ3_SSI3XDAT1);
		GPIOPinTypeSSI(GPIO_PORTQ_BASE, GPIO_PIN_3);

		//
		// PQ2 es SSI3 SSI3XDAT0
		//
		GPIOPinConfigure(GPIO_PQ2_SSI3XDAT0);
		GPIOPinTypeSSI(GPIO_PORTQ_BASE, GPIO_PIN_2);

		//
		// PQ0 es SSI3 SSI3CLK
		//
		GPIOPinConfigure(GPIO_PQ0_SSI3CLK);
		GPIOPinTypeSSI(GPIO_PORTQ_BASE, GPIO_PIN_0);
		SSI_BASE=SSI3_BASE;
		break;

	}
	SSIConfigSetExpClk(SSI_BASE, RELOJ_ACT, SSI_FRF_MOTO_MODE_0,
			SSI_MODE_MASTER, 1000000, 8);

	//
	// Enable the SSI module.
	//
	SSIEnable(SSI_BASE);




}

unsigned char HAL_SPI_ReadWrite(unsigned char data)
{
	uint32_t recibido;
    SSIDataPut(SSI_BASE, data);
    while(SSIBusy(SSI_BASE))
        {
        }
	asm(" NOP");
	asm(" NOP");asm(" NOP");asm(" NOP");asm(" NOP");asm(" NOP");asm(" NOP");

	SSIDataGet(SSI_BASE, &recibido);
	return (char)recibido;
}

// ============================================================================================================
// Puts Chip Select pin Low
// ============================================================================================================

void HAL_SPI_CSLow(void)
{
	GPIOPinWrite(CS_PORT, CS_PIN, 0);
}

// ============================================================================================================
// Puts Chip Select pin High
// ============================================================================================================

void HAL_SPI_CSHigh(void)
{
	GPIOPinWrite(CS_PORT, CS_PIN, CS_PIN);
}

// ============================================================================================================
// Puts FT800 Power_Down_# pin Low
// ============================================================================================================

void HAL_SPI_PDlow(void)
{
	GPIOPinWrite(PD_PORT, PD_PIN, 0);
}

// ============================================================================================================
// Puts FT800 Power_Down_# pin high
// ============================================================================================================

void HAL_SPI_PDhigh(void)
{
	GPIOPinWrite(PD_PORT, PD_PIN, PD_PIN);
}

// ============================================================================================================
// General Functions
// ============================================================================================================

#define MS 40000
void Delay(void)
{
    SysCtlDelay(200*MS);
}


// ============================================================================================================
// Send address of a register which is to be Written
// ============================================================================================================

// This function sends an address of a register which is to be written. It sets the MSBs to 10 to indicate a write
//
// Pass in:         00000000 xxxxxxxx yyyyyyyy zzzzzzzz     address as double word 32-bit value, only lower 3 bytes used
//
// Sends:           10xxxxxx                                bits 23 - 16 of the address, with bit 23/22 forced to 10
//                  yyyyyyyy                                bits 15 - 8  of the address
//                  zzzzzzzz                                bits 7 -  0  of the address
//
// Returns:         N/A

void FT800_SPI_SendAddressWR(dword Memory_Address)
{
    byte SPI_Writebyte = 0x00;

    // Write out the address. Only the lower 3 bytes are sent, with the most significant byte sent first
    SPI_Writebyte = ((Memory_Address & 0x00FF0000) >> 16);  // Mask off the first byte to send
    SPI_Writebyte = (SPI_Writebyte & 0xBF | 0x80);          // Since this is a write, the MSBs are forced to 10
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Call the low-level SPI routine for this MCU to send this byte                                                   //

    SPI_Writebyte = ((Memory_Address & 0x0000FF00) >> 8);   // Mask off the next byte to be sent
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Call the low-level SPI routine for this MCU to send this byte

    SPI_Writebyte = (Memory_Address & 0x000000FF);          // Mask off the next byte to be sent (least significant byte)
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Call the low-level SPI routine for this MCU to send this byte

}


// ============================================================================================================
// Send address of a register which is to be Read
// ============================================================================================================

// This function sends an address of a register which is to be read. It clears the two MS bits to indicate a read
//
// Pass in:         00000000 xxxxxxxx yyyyyyyy zzzzzzzz     address as double word 32-bit value, only lower 3 bytes used
//
// Sends:           00xxxxxx                                bits 23 - 16 of the address, with bits 23/22 forced to 0
//                  yyyyyyyy                                bits 15 - 8  of the address
//                  zzzzzzzz                                bits 7 -  0  of the address
//                  00000000                                dummy byte 0x00 written as described in FT800 datasheet
//
// Returns:         No return value

void FT800_SPI_SendAddressRD(dword Memory_Address)
 {
	long porsi;
	porsi=Memory_Address;
    byte SPI_Writebyte = 0x00;

    // Write out the address. Only the lower 3 bytes are sent, with the most significant byte sent first
    SPI_Writebyte = (char)((porsi & 0x00FF0000) >> 16);  // Mask off the first byte to send
    SPI_Writebyte = (SPI_Writebyte & 0x3F);                 // Since this is a read, the upper two bits are forced to 00
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Call the low-level SPI routine for this MCU to send this byte

    SPI_Writebyte = ((Memory_Address & 0x0000FF00) >> 8);   // Mask off the next byte to be sent
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Call the low-level SPI routine for this MCU to send this byte

    SPI_Writebyte = (Memory_Address & 0x000000FF);          // Mask off the next byte to be sent (least significant byte)
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Call the low-level SPI routine for this MCU to send this byte

    // Send dummy 00 as required in the FT800 datasheet when doing a read
    SPI_Writebyte = 0x00;                                   // Write dummy byte of 0
    HAL_SPI_ReadWrite(SPI_Writebyte);                       //

 }


// ============================================================================================================
// Write a 32-bit value
// ============================================================================================================

// This function writes a 32-bit value. Call FT800_SPI_SendAddressWR first to specify the register address
//
// Pass in:         pppppppp qqqqqqqq rrrrrrrr ssssssss             32-bit value to be written to the register
//
// Sends:           ssssssss         least signicant byte first
//                  rrrrrrrr
//                  qqqqqqqq
//                  pppppppp         most significant byte last
//
// Returns:         N/A

void FT800_SPI_Write32(dword SPIValue32)
{
    byte SPI_Writebyte = 0x00;

    SPI_Writebyte = (SPIValue32 & 0x000000FF);              //
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Write the first (least significant) byte

    SPI_Writebyte = ((SPIValue32 & 0x0000FF00) >> 8);       //
    HAL_SPI_ReadWrite(SPI_Writebyte);                       //

    SPI_Writebyte = (SPIValue32 >> 16);                     //
    HAL_SPI_ReadWrite(SPI_Writebyte);                       //

    SPI_Writebyte = ((SPIValue32 & 0xFF000000) >> 24);      //
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Write the last (most significant) byte

}


// ============================================================================================================
// Write a 16-bit value
// ============================================================================================================

// This function writes a 16-bit value. Call FT800_SPI_SendAddressWR first to specify the register address
//
// Pass in:         rrrrrrrr ssssssss               16-bit value to be written to the register
//
// Sends:           ssssssss                        least significant byte first
//                  rrrrrrrr                        most significant byte last
//
// Returns:         N/A

void FT800_SPI_Write16(unsigned int SPIValue16)
{
    byte SPI_Writebyte = 0x00;

    SPI_Writebyte = (SPIValue16 & 0x00FF);                  //
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Write the first (least significant) byte

    SPI_Writebyte = ((SPIValue16 & 0xFF00) >> 8);           //
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Write the last (most significant) byte
}


// ============================================================================================================
// Write an 8-bit value
// ============================================================================================================

// This function writes an 8-bit value. Call FT800_SPI_SendAddressWR first to specify the register address
//
// Pass in:         ssssssss          16-bit value to be written to the register
//
// Sends:           ssssssss          sends the specified byte
//
// Returns:         N/A

void FT800_SPI_Write8(byte SPIValue8)
{
    byte SPI_Writebyte = 0x00;

    // Write out the data
    SPI_Writebyte = (SPIValue8);                            //
    HAL_SPI_ReadWrite(SPI_Writebyte);                       // Write the data byte
}


// ============================================================================================================
// Read a 32-bit register
// ============================================================================================================

// This function reads the actual 8-bit data from a register. Call FT800_SPI_SendAddressRD first to specify the address
//
// Pass In:         N/A
//
// Sends:           00000000        dummy byte whilst clocking in the Least Significant byte (e.g. aaaaaaaa) from register
//                  00000000        dummy byte whilst clocking in the next byte (e.g. bbbbbbbb) from register
//                  00000000        dummy byte whilst clocking in the next byte (e.g. cccccccc) from register
//                  00000000        dummy byte whilst clocking in the Most Significant byte (e.g. dddddddd) from register
//
// Returns:         dddddddd cccccccc bbbbbbbb aaaaaaaa

dword FT800_SPI_Read32()
 {
    byte SPI_Writebyte = 0x00;
    byte SPI_Readbyte = 0x00;
    dword SPI_DWordRead = 0x00000000;
    dword DwordTemp = 0x00000000;

    // Read the first data byte (this is the least significant byte of the register). SPI writes out dummy byte of 0x00
    SPI_Writebyte = 0x00;                                   // Write a dummy 0x00
    SPI_Readbyte = HAL_SPI_ReadWrite(SPI_Writebyte);        // Get the byte which was read by the low-level MCU routine
    DwordTemp = SPI_Readbyte;                               // Put received byte into a temporary 32-bit value
    SPI_DWordRead = DwordTemp;                              // Put the byte into a 32-bit variable in the lower 8 bits

    // Read the actual data byte. We pass a 0x00 into the SPI routine since it always writes when it reads
    SPI_Writebyte = 0x00;                                   // Write a dummy 0x00
    SPI_Readbyte = HAL_SPI_ReadWrite(SPI_Writebyte);        // Get the byte which was read by the low-level MCU routine
    DwordTemp = SPI_Readbyte;                               // Put received byte into a temporary 32-bit value
    SPI_DWordRead |= (DwordTemp << 8);                      // Put the byte into a 32-bit variable (shifted 8 bits up)

    // Read the actual data byte. We pass a 0x00 into the SPI routine since it always writes when it reads
    SPI_Writebyte = 0x00;                                   // Write a dummy 0x00
    SPI_Readbyte = HAL_SPI_ReadWrite(SPI_Writebyte);        // Get the byte which was read by the low-level MCU routine
        DwordTemp = SPI_Readbyte;                           // Put received byte into a temporary 32-bit value
    SPI_DWordRead |= (DwordTemp << 16);                     // Put the byte into a 32-bit variable (shifted 16 bits up)

    // Read the actual data byte. We pass a 0x00 into the SPI routine since it always writes when it reads
    SPI_Writebyte = 0x00;                                   // Write a dummy 0x00
    SPI_Readbyte = HAL_SPI_ReadWrite(SPI_Writebyte);        // Get the byte which was read by the low-level MCU routine
        DwordTemp = SPI_Readbyte;                           // Put received byte into a temporary 32-bit value
    SPI_DWordRead |= (DwordTemp << 24);                     // Put the byte into a 32-bit variable (shifted 24 bits up)

    // Return the byte which we read
    return(SPI_DWordRead);                                  //
}


// ============================================================================================================
// Read an 8-bit register
// ============================================================================================================

// This function reads the actual 8-bit data from a register. Call FT800_SPI_SendAddressRD first to specify the address
//
// Pass in:         N/A
//
// Sends:           00000000                                Dummy write 0x00 whilst clocking in the byte
//
// Returns:         aaaaaaaa                                Byte which was clocked in

byte FT800_SPI_Read8()
 {
    byte SPI_Writebyte = 0x00;
    byte SPI_Readbyte = 0x00;

    // Read the data byte. We pass a 0x00 into the SPI routine since it always writes when it reads
    SPI_Writebyte = 0x00;                                   // Write a dummy 0x00
    SPI_Readbyte = HAL_SPI_ReadWrite(SPI_Writebyte);        // Get the byte which was read by the low-level MCU routine

    // Return the byte which we read
    return(SPI_Readbyte);                                   //
}


// ============================================================================================================
// Write a Host Command
// ============================================================================================================

// This function is specifically designed to send a Host Command
//
// Pass in:         xxhhhhhh             8-bit host command where upper 2 bits are dont-care
//
// Sends:           [Chip Select Low]
//                  01hhhhhh         Host command with bit 7 forced to 0 and bit 6 forced to 1
//                  00000000         Dummy 0x00
//                  00000000         Dummy 0x00
//                  [Chip Select High]
//
// Returns:         N/A

void FT800_SPI_HostCommand(byte Host_Command)
{
 //   byte PortRead = 0x00;
  //  byte SPI_Readbyte = 0x00;
    byte SPI_Writebyte = 0x00;

    // Chip Select Low
    HAL_SPI_CSLow();

    SPI_Writebyte = (Host_Command & 0x3F | 0x40);           // This is the command being sent
    HAL_SPI_ReadWrite(SPI_Writebyte);                       //

    SPI_Writebyte = 0x00;                                   // Sending dummy byte
    HAL_SPI_ReadWrite(SPI_Writebyte);                       //


    SPI_Writebyte = 0x00;                                   // Sending dummy byte
    HAL_SPI_ReadWrite(SPI_Writebyte);                       //


    // Chip Select High
    HAL_SPI_CSHigh();
}


// ============================================================================================================
// Host Command Dummy Read
// ============================================================================================================

// This function is specifically designed to send three 0x00 bytes to wake up the FT800
//
// Pass in:         N/A
//
// Sends:           [Chip Select Low]
//                  00000000         Dummy 0x00
//                  00000000         Dummy 0x00
//                  00000000         Dummy 0x00
//                  [Chip Select High]
//
// Returns:         N/A

void FT800_SPI_HostCommandDummyRead(void)
 {
 //   byte PortRead = 0x00;
  //  byte SPI_byte_Read = 0x00;
    byte SPI_Writebyte = 0x00;

    // Chip Select Low
    HAL_SPI_CSLow();

    // Read/Write
    SPI_Writebyte = 0x00;                                   // Sending dummy 00 byte
    HAL_SPI_ReadWrite(SPI_Writebyte);


    SPI_Writebyte = 0x00;                                   // Sending dummy 00 byte
    HAL_SPI_ReadWrite(SPI_Writebyte);


    SPI_Writebyte = 0x00;                                   // Sending dummy 00 byte
    HAL_SPI_ReadWrite(SPI_Writebyte);


    // Chip Select High
    HAL_SPI_CSHigh();
}

// ============================================================================================================
// Increment Command Offset
// ============================================================================================================

// This function is used when adding commands to the Command FIFO.
// After adding a command to the FIFO, this function will calculate the FIFO address offset where the next command
// should be written to.
// In general, the next command would be written to [previous offset plus the length of the last command] but due
// to the circular FIFO used, a command list may need to wrap round from an offset of 4095 to 0.
//
// Pass in:         N/A
//
// Sends:           [Chip Select Low]
//                  00000000         Dummy 0x00
//                  00000000         Dummy 0x00
//                  00000000         Dummy 0x00
//                  [Chip Select High]
//
// Returns:         N/A


unsigned int FT800_IncCMDOffset(unsigned int Current_Offset, byte Command_Size)
{
    unsigned int New_Offset;

    New_Offset = Current_Offset + Command_Size;

    if(New_Offset > 4095)
    {
        New_Offset = (New_Offset - 4096);
    }

    return New_Offset;
}




// ############################################################################################################
// ############################################################################################################
void ComEsperaFin(void){
do
{
    HAL_SPI_CSLow();                            //
    FT800_SPI_SendAddressRD(REG_CMD_WRITE);     //
    cmdBufferWr = FT800_SPI_Read32();           // Read the value of the REG_CMD_WRITE register
    HAL_SPI_CSHigh();                           //
    HAL_SPI_CSLow();                            //
    FT800_SPI_SendAddressRD(REG_CMD_READ);      //
    cmdBufferRd = FT800_SPI_Read32();           // Read the value of the REG_CMD_READ register
    HAL_SPI_CSHigh();                           //
} while(cmdBufferWr != cmdBufferRd);


// Check the actual value of the current WRITE register (pointer)

CMD_Offset = cmdBufferWr;
}


void EscribeRam32(unsigned long dato){
		HAL_SPI_CSLow();
        FT800_SPI_SendAddressWR(RAM_CMD + CMD_Offset);  // Writing to next available location in FIFO (FIFO base address + offset)
        FT800_SPI_Write32(dato);                  // Vertex 2F    01XXXXXX XXXXXXXX XYYYYYYY YYYYYYYY
        HAL_SPI_CSHigh();

        CMD_Offset = FT800_IncCMDOffset(CMD_Offset, 4); // Move the CMD Offset since we have just added 4 bytes to the FIFO
}

void EscribeRam16(unsigned int dato){
		HAL_SPI_CSLow();
        FT800_SPI_SendAddressWR(RAM_CMD + CMD_Offset);  // Writing to next available location in FIFO (FIFO base address + offset)
        FT800_SPI_Write16(dato);                  // Vertex 2F    01XXXXXX XXXXXXXX XYYYYYYY YYYYYYYY
        HAL_SPI_CSHigh();

        CMD_Offset = FT800_IncCMDOffset(CMD_Offset, 2); // Move the CMD Offset since we have just added 4 bytes to the FIFO
}

void EscribeRam8(char dato){
		HAL_SPI_CSLow();
        FT800_SPI_SendAddressWR(RAM_CMD + CMD_Offset);  // Writing to next available location in FIFO (FIFO base address + offset)
        FT800_SPI_Write8(dato);                  // Vertex 2F    01XXXXXX XXXXXXXX XYYYYYYY YYYYYYYY
        HAL_SPI_CSHigh();

        CMD_Offset = FT800_IncCMDOffset(CMD_Offset, 1); // Move the CMD Offset since we have just added 4 bytes to the FIFO
}



void EscribeRamTxt(char *cadena)
{
    while(*cadena) EscribeRam8(*cadena++);
    EscribeRam8(0);
}

void Comando(unsigned long COMM){
HAL_SPI_CSLow();                                //
FT800_SPI_SendAddressWR(RAM_CMD + CMD_Offset);  // Writing to next available location in FIFO (FIFO base address + offset)
FT800_SPI_Write32(COMM);                  // Write the DL_START command
HAL_SPI_CSHigh();                               //

CMD_Offset = FT800_IncCMDOffset(CMD_Offset, 4); // Move the CMD Offset since we have just added 4 bytes to the FIFO

}

void ComVertex2ff(int x,int y){
long coord;
		coord=x;
		coord=coord<<19;
		coord=coord+ (y<<4);
        HAL_SPI_CSLow();                                //
        FT800_SPI_SendAddressWR(RAM_CMD + CMD_Offset);  // Writing to next available location in FIFO (FIFO base address + offset)
        FT800_SPI_Write32(0x40000000+coord);                  // Vertex 2F    01XXXXXX XXXXXXXX XYYYYYYY YYYYYYYY
        HAL_SPI_CSHigh();                               //

        CMD_Offset = FT800_IncCMDOffset(CMD_Offset, 4); // Move the CMD Offset since we have just added 4 bytes to the FIFO
}

void ComColor(int R, int G, int B){
	long color;
#ifdef VM800B35
        color=R;
        color=color<<8;
        color+=G;
        color=color<<8;
        color+= B;
#endif
#ifdef VM800B50
        color=B;
        color=color<<8;
        color+=G;
        color=color<<8;
        color+= R;
#endif
	// Color RGB   00000100 BBBBBBBB GGGGGGGG RRRRRRRR  (B/G/R = Colour values)
	Comando(0x04000000+color);
}

void ComTXT(int x, int y, int fuente, int ops, char *cadena)
{
			EscribeRam32(CMD_TEXT);
	        EscribeRam16(x);
	        EscribeRam16(y);
	        EscribeRam16(fuente);
	        EscribeRam16(ops);
	        while(*cadena) EscribeRam8(*cadena++);
	        EscribeRam8(0);
	        PadFIFO();


}

void ComNum(int x, int y, int fuente, int ops, unsigned long Num)
{
			EscribeRam32(CMD_NUMBER);
	        EscribeRam16(x);
	        EscribeRam16(y);
	        EscribeRam16(fuente);
	        EscribeRam16(ops);
	        EscribeRam32(Num);
	        PadFIFO();
}
void ComTeclas(int x, int y, int w, int h, int fuente, unsigned int ops, char *Keys)
{
	EscribeRam32(CMD_KEYS);
    EscribeRam16(x);
	EscribeRam16(y);
    EscribeRam16(w);
	EscribeRam16(h);
	EscribeRam16(fuente);
	EscribeRam16(ops);
	while(*Keys) EscribeRam8(*Keys++);
    EscribeRam8(0);
PadFIFO();

}

void Ejecuta_Lista(void){
HAL_SPI_CSLow();                                //
FT800_SPI_SendAddressWR(REG_CMD_WRITE);         //
FT800_SPI_Write16(CMD_Offset);                  //
HAL_SPI_CSHigh();                               //
}

void PadFIFO(void){
	int i, pad;
	pad=CMD_Offset&0x0003;
	if(pad>0){
		for(i=0;i<4-pad;i++) EscribeRam8(0);
	}
}


void Dibuja(void){
	Comando(CMD_DISPLAY);
	Comando(CMD_SWAP);
	Ejecuta_Lista();
}



void Inicia_pantalla(void){
HAL_SPI_PDhigh();                               // Put the Power Down # pin high to wake FT800

Delay();                                        // Delay for power up of regulator

FT800_SPI_HostCommandDummyRead();               // Read location 0 to wake up FT800

FT800_SPI_HostCommand(FT_GPU_EXTERNAL_OSC);     // Change the PLL to external clock - optional

FT800_SPI_HostCommand(FT_GPU_PLL_48M);          //  Ensure configured to 48 MHz

// Note: Could now increase SPI clock rate up to 30MHz SPI if preferred

Delay();                                        // Delay

FT800_SPI_HostCommand(FT_GPU_CORE_RESET);       // Reset the core

Delay();                                        // Delay

FT800_SPI_HostCommand(FT_GPU_ACTIVE_M);         // Read address 0 to ensure FT800 is active

chipid = 0x00;                                  // Read the Chip ID to check comms with the FT800 - should be 0x7C
	while(chipid != 0x7C)
{
    HAL_SPI_CSLow();                            // CS low
    FT800_SPI_SendAddressRD(REG_ID);            // Send the address
    chipid = FT800_SPI_Read8();                 // Read the actual value
    HAL_SPI_CSHigh();
    Delay();// CS high
}


// =======================================================================
// Write the display registers on the FT800 for your particular display
// =======================================================================

HAL_SPI_CSLow();                                // CS low                   Write REG_HCYCLE to 548
FT800_SPI_SendAddressWR(REG_HCYCLE);            // Send the address
FT800_SPI_Write16(HCYCLE);                         // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_HOFFSET to 43
FT800_SPI_SendAddressWR(REG_HOFFSET);           // Send the address
FT800_SPI_Write16(HOFFSET);                          // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_HSYNC0 to 0
FT800_SPI_SendAddressWR(REG_HSYNC0);            // Send the address
FT800_SPI_Write16(HSYNC0);                           // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_HSYNC1 to 41
FT800_SPI_SendAddressWR(REG_HSYNC1);            // Send the address
FT800_SPI_Write16(HSYNC1);                          // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_VCYCLE to 292
FT800_SPI_SendAddressWR(REG_VCYCLE);            // Send the address
FT800_SPI_Write16(VCYCLE);                         // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_VOFFSET to 12
FT800_SPI_SendAddressWR(REG_VOFFSET);           // Send the address
FT800_SPI_Write16(VOFFSET);                          // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_VSYNC0 to 0
FT800_SPI_SendAddressWR(REG_VSYNC0);            // Send the address
FT800_SPI_Write16(VSYNC0);                           // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_VSYNC1 to 10
FT800_SPI_SendAddressWR(REG_VSYNC1);            // Send the address
FT800_SPI_Write16(VSYNC1);                          // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_SWIZZLE to 2
FT800_SPI_SendAddressWR(REG_SWIZZLE);           // Send the address
FT800_SPI_Write16(2);                           // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_PCLK_POL to 1
FT800_SPI_SendAddressWR(REG_PCLK_POL);          // Send the address
FT800_SPI_Write16(PCLK_POL);                           // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_HSIZE to 480
FT800_SPI_SendAddressWR(REG_HSIZE);             // Send the address
FT800_SPI_Write16(HSIZE);                         // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_VSIZE to 272
FT800_SPI_SendAddressWR(REG_VSIZE);             // Send the address
FT800_SPI_Write16(VSIZE);                         // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_PCLK to 5
FT800_SPI_SendAddressWR(REG_PCLK);              // Send the address
FT800_SPI_Write16(PCLK);                           // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high


// =======================================================================
// Configure the touch screen
// =======================================================================

HAL_SPI_CSLow();                                // CS low                   Write the touch threshold
FT800_SPI_SendAddressWR(REG_TOUCH_RZTHRESH);    // Send the address
FT800_SPI_Write16(1200);                        // Send the 16-bit value
HAL_SPI_CSHigh();                               // CS high


// =======================================================================
// Send an initial display list which will clear the screen to a black colour
// =======================================================================

// Set the colour which is used when the colour buffer is cleared
HAL_SPI_CSLow();                                //
FT800_SPI_SendAddressWR(RAM_DL + 0);            // Write first entry in Display List (first location in DL Ram)
FT800_SPI_Write32(0x02000000);                  // Clear Color RGB   00000010 BBBBBBBB GGGGGGGG RRRRRRRR  (B/G/R = Colour values)
HAL_SPI_CSHigh();                               //

// Clear the Colour, Stencil and Tag buffers. This will set the screen to the 'clear' colour set above.
HAL_SPI_CSLow();                                //
FT800_SPI_SendAddressWR(RAM_DL + 4);            // Write next entry in display list (each command is 4 bytes)
FT800_SPI_Write32(0x26000007);                  // Clear 00100110 -------- -------- -----CST  (C/S/T define which parameters to clear)
HAL_SPI_CSHigh();                               //

// Display command ends the display list
HAL_SPI_CSLow();                                //
FT800_SPI_SendAddressWR(RAM_DL + 8);            // Write next entry in display list
FT800_SPI_Write32(0x00000000);                  // DISPLAY command 00000000 00000000 00000000 00000000
HAL_SPI_CSHigh();                               //

// Writing to the DL_SWAP register tells the Display Engine to render the new screen designed above.
HAL_SPI_CSLow();                                //
FT800_SPI_SendAddressWR(REG_DLSWAP);            // Writing to the DL_SWAP register...value 10 means render after last frame complete
FT800_SPI_Write32(0x00000002);                  // 00000000 00000000 00000000 000000SS  (SS bits define when render occurs)
HAL_SPI_CSHigh();                               // CS high



// =======================================================================
// Set the GPIO pins of the FT800 to enable the display now
// =======================================================================

// note: Refer to the GPIO section in the FT800 datasheet and also the connections to the GPIO
// used on the particular development module being used

// Bit 7 enables the LCD Display. It is set to output driving high
// Bit 1 enables the Audio Amplifier. It is set to output driving low to shut down the amplifier since audio not used here
// Bit 0 is unused but set to output driving high.

HAL_SPI_CSLow();                                // CS low                   Write REG_GPIO_DIR
FT800_SPI_SendAddressWR(REG_GPIO_DIR);          // Send the address
FT800_SPI_Write8(0x83);                         // Send the 8-bit value
HAL_SPI_CSHigh();                               // CS high

HAL_SPI_CSLow();                                // CS low                   Write REG_GPIO
FT800_SPI_SendAddressWR(REG_GPIO);              // Send the address
FT800_SPI_Write8(0x83);                         // Send the 8-bit value
HAL_SPI_CSHigh();                               // CS high

}


void Nueva_pantalla(int R, int G, int B){
    long color;
#ifdef VM800B35
        color=R;
        color=color<<8;
        color+=G;
        color=color<<8;
        color+= B;
#endif
#ifdef VM800B50
        color=B;
        color=color<<8;
        color+=G;
        color=color<<8;
        color+= R;
#endif
    ComEsperaFin();
    Comando(CMD_DLSTART);
 	Comando(0x02000000+color);  // Clear Color RGB   00000010 BBBBBBBB GGGGGGGG RRRRRRRR  (B/G/R = Colour values)
 	Comando(0x26000007);                            // Clear 00100110 -------- -------- -----CST  (C/S/T define which parameters to clear))
}

void Lee_pantalla(void){
            HAL_SPI_CSLow();                            	//
            FT800_SPI_SendAddressRD(REG_TOUCH_SCREEN_XY);   //
            BufferXY = FT800_SPI_Read32();           // Lee valor pantalla tactil
            HAL_SPI_CSHigh();
            POSX=(BufferXY&0xffff0000)>>16;
            POSY=BufferXY&0x0000FFFF;
}

unsigned long Lee_Reg(unsigned long dir){
	unsigned long Buff_temp;
		HAL_SPI_CSLow();                       	//
        FT800_SPI_SendAddressRD(dir);   //
        Buff_temp = FT800_SPI_Read32();           // Lee valor
        HAL_SPI_CSHigh();
return(Buff_temp);
}

void Esc_Reg(unsigned long dir, unsigned long valor){
	HAL_SPI_CSLow();
        FT800_SPI_SendAddressWR(dir);
        FT800_SPI_Write32(valor);
        HAL_SPI_CSHigh();
}


void ComScrollbar(int x, int y, int w, int h, int ops, int val, int size, int range)
{
		EscribeRam32(CMD_SCROLLBAR);
	    EscribeRam16(x);
		EscribeRam16(y);
	    EscribeRam16(w);
		EscribeRam16(h);
		EscribeRam16(ops);
		EscribeRam16(val);
		EscribeRam16(size);
		EscribeRam16(range);

}

void ComFgcolor(int R, int G, int B){
	long color;
#ifdef VM800B35
        color=R;
        color=color<<8;
        color+=G;
        color=color<<8;
        color+= B;
#endif
#ifdef VM800B50
        color=B;
        color=color<<8;
        color+=G;
        color=color<<8;
        color+= R;
#endif

	EscribeRam32 (CMD_FGCOLOR);
	EscribeRam32 (color);

}


void ComBgcolor(int R, int G, int B){
    long color;
#ifdef VM800B35
        color=R;
        color=color<<8;
        color+=G;
        color=color<<8;
        color+= B;
#endif
#ifdef VM800B50
        color=B;
        color=color<<8;
        color+=G;
        color=color<<8;
        color+= R;
#endif


    EscribeRam32 (CMD_BGCOLOR);
    EscribeRam32 (color);

}


void ComButton(int x, int y, int w, int h, int font, int ops, char *cadena)
{
		EscribeRam32(CMD_BUTTON);
        EscribeRam16(x);
        EscribeRam16(y);
        EscribeRam16(w);
        EscribeRam16(h);
        EscribeRam16(font);
        EscribeRam16(ops);
        while(*cadena) EscribeRam8(*cadena++);
        EscribeRam8(0);

        PadFIFO();

}

char Boton(int x, int y, int w, int h, int font, char *cadena)
{
    char resul;
    Lee_pantalla();
    if(POSX>x && POSX<(x+w) && POSY>y && POSY<(y+h)){
        ComButton(x,y,w,h,font,OPT_FLAT,cadena);
        resul=1;
    }else{
        ComButton(x,y,w,h,font,0,cadena);
              resul=0;
    }
    return resul;
}
void Espera_pant(void)
{
	do{
		Lee_pantalla();

	}while(POSY==0x8000);
	Delay();
	do{
			Lee_pantalla();

		}while(POSY!=0x8000);

}

void ComLineWidth(int width){
    Comando(CMD_LINEWIDTH+width*16);
}

void ComPointSize(int size){
Comando(CMD_POINTSIZE+size*16);
}


void Com_Punto(uint16_t x, uint16_t y, uint16_t R)
{
    Comando(CMD_POINTSIZE+R*16);// Point Size  00001101 00000000 SSSSSSSS SSSSSSSS  (S = Size value)
                              Comando(CMD_BEGIN_POINTS);
                              ComVertex2ff(x,y);
                              Comando(CMD_END);
}

void Calibra_touch(void){
ComEsperaFin();
	Comando(CMD_DLSTART);
	Comando(0x02f7f7f7);  // Clear Color RGB   00000010 BBBBBBBB GGGGGGGG RRRRRRRR  (B/G/R = Colour values)
	Comando(0x26000007);                            // Clear 00100110 -------- -------- -----CST  (C/S/T define which parameters to clear))
	ComColor(0x00,0x00,0x00);

        ComTXT(60,30,27,0,"Pulsa en el punto");
        EscribeRam32(CMD_CALIBRATE);

        Dibuja();
    	SysCtlDelay(RELOJ/6);

        //espera(500);
        ComEsperaFin();
       	Comando(CMD_DLSTART);
       	Comando(0x02ffff00);  // Clear Color RGB   00000010 BBBBBBBB GGGGGGGG RRRRRRRR  (B/G/R = Colour values)
       	Comando(0x26000007);                            // Clear 00100110 -------- -------- -----CST  (C/S/T define which parameters to clear))
        Dibuja();
}

void ComGradient(int x0,int y0, long color0, int x1, int y1, int color1){
	EscribeRam32(CMD_GRADIENT);
	EscribeRam16(x0);
	EscribeRam16(y0);
	EscribeRam32(color0);
	EscribeRam16(x1);
	EscribeRam16(y1);
	EscribeRam32(color1);

}

void VolNota(unsigned char volumen){
	HAL_SPI_CSLow();
		        FT800_SPI_SendAddressWR(REG_VOL_SOUND);
		        FT800_SPI_Write8(volumen);
		        HAL_SPI_CSHigh();

}
void TocaNota( int instr, int nota)
{
//	char comando;
	HAL_SPI_CSLow();
	        FT800_SPI_SendAddressWR(REG_SOUND);
	        FT800_SPI_Write16(instr+(nota<<8));
	        HAL_SPI_CSHigh();

	        HAL_SPI_CSLow();
	        	        FT800_SPI_SendAddressWR(REG_PLAY);
	        	        FT800_SPI_Write8(1);
	        	        HAL_SPI_CSHigh();

	}

void FinNota(void)
{
//	char comando;
	HAL_SPI_CSLow();
	FT800_SPI_SendAddressWR(REG_SOUND);
	FT800_SPI_Write16(0x0000);
	HAL_SPI_CSHigh();

	HAL_SPI_CSLow();
	FT800_SPI_SendAddressWR(REG_PLAY);
	FT800_SPI_Write8(1);
	HAL_SPI_CSHigh();



}


void Fadeout(void)
{
   int i;

	for (i = 100; i >= 0; i -= 3)
	{
        HAL_SPI_CSLow();
        	        FT800_SPI_SendAddressWR(REG_PWM_DUTY);
       	        FT800_SPI_Write8(i);
    	        HAL_SPI_CSHigh();


    	    	SysCtlDelay(RELOJ/1500);


	}
}

/* API to perform display fadein effect by changing the display PWM from 0 till 100 and finally 128 */
void Fadein()
{
	int i;

	for (i = 0; i <=100 ; i += 3)
	{
		 HAL_SPI_CSLow();
		        	    FT800_SPI_SendAddressWR(REG_PWM_DUTY);
		       	        FT800_SPI_Write8(i);
		    	        HAL_SPI_CSHigh();
		    	    	SysCtlDelay(RELOJ/1500);

		       	        //espera(2);
	}
	i = 128;
	 HAL_SPI_CSLow();
		        	    FT800_SPI_SendAddressWR(REG_PWM_DUTY);
		       	        FT800_SPI_Write8(i);
		    	        HAL_SPI_CSHigh();

}

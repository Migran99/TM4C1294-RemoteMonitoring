/* --COPYRIGHT--,BSD
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//****************************************************************************
//
// HAL_I2C.c - Hardware abstraction layer for I2C with TM4C1294NCPDT
//
//****************************************************************************


#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "inc/hw_i2c.h"
#include "driverlib/i2c.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"


#include "HAL_I2C.h"

#define MSEC 40000


/* I2C Master Configuration Parameter */


/***************************************************************************//**
 * @brief  Reads data from the sensor
 * @param  writeByte Address of register to read from
 * @return Register contents
 ******************************************************************************/


unsigned int I2C_Base;

/**********************************
 * Detecta_BP(pos)
 * Funcion que detecta si hay un Sensors Boosterpack pinchado en una de las
 * posiciones de BP. Para ello, configura los pines como entrada y los lee:
 * Si está el BP, los pines estarán a 1 (Pull up del I2C). Si no, estarán a 0
 */


uint8_t Detecta_BP(int pos){
    int resul=0;
    switch (pos){
    case 1:
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
        GPIOPinTypeGPIOInput(GPIO_PORTB_BASE, GPIO_PIN_2|GPIO_PIN_3);
        GPIOPadConfigSet(GPIO_PORTB_BASE,GPIO_PIN_2|GPIO_PIN_3,GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD_WPD);
        resul+=GPIOPinRead(GPIO_PORTB_BASE,GPIO_PIN_2 |GPIO_PIN_3);
        break;
    case 2:
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
        GPIOPinTypeGPIOInput(GPIO_PORTN_BASE, GPIO_PIN_4|GPIO_PIN_5);
        GPIOPadConfigSet(GPIO_PORTN_BASE,GPIO_PIN_4|GPIO_PIN_5,GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD_WPD);
        resul+=GPIOPinRead(GPIO_PORTN_BASE,GPIO_PIN_4 |GPIO_PIN_5);
        break;
    }
    return resul;
}

/**************************************
 * Funcion maestra de configuración del BP elegido. En función del BP elegido se configura el I2C correspondiente,
 * los pines asociados, y se da valor a la variable global I2C_Base que se usa en todas las funciones de HAL
 *
 */
void Conf_Boosterpack(int pos, int RELOJ){
    switch (pos){
    case 1:
        I2C_Base=I2C0_BASE;
        SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
        GPIOPinConfigure(GPIO_PB2_I2C0SCL);
        GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
        GPIOPinConfigure(GPIO_PB3_I2C0SDA);
        GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);
        I2CMasterInitExpClk(I2C0_BASE, RELOJ, true );
        I2CMasterEnable(I2C0_BASE);
        break;
    case 2:
        I2C_Base=I2C2_BASE;
        SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C2);
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
        GPIOPinConfigure(GPIO_PN5_I2C2SCL);
        GPIOPinTypeI2CSCL(GPIO_PORTN_BASE, GPIO_PIN_5);
        GPIOPinConfigure(GPIO_PN4_I2C2SDA);
        GPIOPinTypeI2C(GPIO_PORTN_BASE, GPIO_PIN_4);
        I2CMasterInitExpClk(I2C2_BASE, RELOJ, true );
        I2CMasterEnable(I2C2_BASE);
        break;
    }
}

uint8_t Test_I2C_Dir(uint8_t DIR)
{

    I2CMasterSlaveAddrSet(I2C_Base, DIR, 1);
    I2CMasterControl(I2C_Base, I2C_MASTER_CMD_SINGLE_SEND);
    SysCtlDelay(20*MSEC);
    return !(I2CMasterErr(I2C_Base));


}



void Espera_I2C(uint32_t Base){
	while(!(I2CMasterBusy(Base)));	//Espera que empiece
	while((I2CMasterBusy(Base)));	//Espera que acabe
}

int I2C_read16(unsigned int slaveAdr, unsigned char writeByte)
{
    volatile int val = 0;
    volatile int valScratch = 0;
    I2CMasterSlaveAddrSet(I2C_Base, slaveAdr, 0);	//Modo ESCRITURA
    I2CMasterDataPut(I2C_Base, writeByte);
    I2CMasterControl(I2C_Base, I2C_MASTER_CMD_BURST_SEND_START); //esc. Multiple
    Espera_I2C(I2C_Base);
    I2CMasterSlaveAddrSet(I2C_Base, slaveAdr, 1);  //MODO LECTURA
    I2CMasterControl(I2C_Base, I2C_MASTER_CMD_BURST_RECEIVE_START); //Lect. Multiple
    Espera_I2C(I2C_Base);
    val=I2CMasterDataGet(I2C_Base);
    I2CMasterControl(I2C_Base,I2C_MASTER_CMD_BURST_RECEIVE_FINISH); //Lect. Final
    Espera_I2C(I2C_Base);
    val = (val << 8);
    valScratch=I2CMasterDataGet(I2C_Base);
    return(val|valScratch);
}


/***************************************************************************//**
 * @brief  Writes data to the sensor
 * @param  pointer  Address of register you want to modify
 * @param  writeByte Data to be written to the specified register
 * @return none
 ******************************************************************************/

void I2C_write16 (unsigned char pointer, unsigned int writeByte)
{
	I2CMasterDataPut(I2C_Base, pointer);
	I2CMasterControl(I2C_Base, I2C_MASTER_CMD_BURST_SEND_START);
    Espera_I2C(I2C_Base);
	I2CMasterDataPut(I2C_Base,(unsigned char)(writeByte>>8) );
	I2CMasterControl(I2C_Base, I2C_MASTER_CMD_BURST_SEND_CONT);
    Espera_I2C(I2C_Base);
	I2CMasterDataPut(I2C_Base, (unsigned char)(writeByte&0xFF));
	I2CMasterControl(I2C_Base,I2C_MASTER_CMD_BURST_SEND_FINISH);
    Espera_I2C(I2C_Base);
}


void I2C_setslave(unsigned int slaveAdr)
{
    /* Specify slave address for I2C */
	I2CMasterSlaveAddrSet(I2C_Base, slaveAdr, 0);


    return;
}


bool writeI2C (char dev_addr, char reg_addr, char *reg_data, unsigned int cnt)
{
    unsigned int idx=0;
    I2CMasterSlaveAddrSet(I2C_Base, dev_addr, 0);
    I2CMasterDataPut(I2C_Base, reg_addr);
    if(cnt>1){
    I2CMasterControl(I2C_Base, I2C_MASTER_CMD_BURST_SEND_START);
    Espera_I2C(I2C_Base);
    for(idx=0;idx<(cnt-1);idx++){
    I2CMasterDataPut(I2C_Base,reg_data[idx]);
    I2CMasterControl(I2C_Base, I2C_MASTER_CMD_BURST_SEND_CONT);
    Espera_I2C(I2C_Base);
    }

    I2CMasterDataPut(I2C_Base,reg_data[cnt-1] );
    I2CMasterControl(I2C_Base,I2C_MASTER_CMD_BURST_SEND_FINISH);
    Espera_I2C(I2C_Base);
    }
    else{
        I2CMasterControl(I2C_Base, I2C_MASTER_CMD_BURST_SEND_START);
        Espera_I2C(I2C_Base);
        I2CMasterDataPut(I2C_Base,reg_data[0]);
        I2CMasterControl(I2C_Base,I2C_MASTER_CMD_BURST_SEND_FINISH);
        Espera_I2C(I2C_Base);
    }

    return true;
}




bool readI2C(uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data, uint8_t cnt)
{
    volatile int val = 0;
    volatile int valScratch = 0;
    unsigned int idx=0;

    I2CMasterSlaveAddrSet(I2C_Base, dev_addr, 0);  //Modo ESCRITURA
    I2CMasterDataPut(I2C_Base, reg_addr);
    if(cnt>1){
    I2CMasterControl(I2C_Base, I2C_MASTER_CMD_BURST_SEND_START); //esc. Multiple
    Espera_I2C(I2C_Base);
    I2CMasterSlaveAddrSet(I2C_Base, dev_addr, 1);  //MODO LECTURA

    I2CMasterControl(I2C_Base, I2C_MASTER_CMD_BURST_RECEIVE_START); //Lect. Multiple
    Espera_I2C(I2C_Base);
    reg_data[0]=I2CMasterDataGet(I2C_Base);
    while(idx<cnt-2){
        idx++;
        I2CMasterControl(I2C_Base,I2C_MASTER_CMD_BURST_RECEIVE_CONT); //Lect. Final
        Espera_I2C(I2C_Base);
        reg_data[idx]=I2CMasterDataGet(I2C_Base);
    }
    idx++;

    I2CMasterControl(I2C_Base,I2C_MASTER_CMD_BURST_RECEIVE_FINISH); //Lect. Final
    Espera_I2C(I2C_Base);
    reg_data[idx]=I2CMasterDataGet(I2C_Base);
    }
    else{
        I2CMasterControl(I2C_Base, I2C_MASTER_CMD_SINGLE_SEND); //esc. simple
        Espera_I2C(I2C_Base);
        I2CMasterSlaveAddrSet(I2C_Base, dev_addr, 1);  //MODO LECTURA
        I2CMasterControl(I2C_Base, I2C_MASTER_CMD_SINGLE_RECEIVE); //Lect. simple
        Espera_I2C(I2C_Base);
        *reg_data=I2CMasterDataGet(I2C_Base);
    }
    return true;
}


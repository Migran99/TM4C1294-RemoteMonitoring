#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_gpio.h"
#include "driverlib/flash.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/emac.h"
#include "drivers/pinout.h"
#include "utils/cmdline.h"
#include "utils/locator.h"
#include "utils/flash_pb.h"
#include "utils/lwiplib.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "eth_client_lwip.h"
#include "main.h"
#include "string.h"

#define ETH_CLIENT_EVENT_DHCP          0x00000001
#define ETH_CLIENT_EVENT_DISCONNECT    0x00000002
#define ETH_CLIENT_EVENT_DNS           0x00000003
#define ETH_CLIENT_EVENT_CONNECT       0x00000004
#define ETH_CLIENT_EVENT_RECEIVE       0x00000005
#define ETH_CLIENT_EVENT_SEND          0x00000006
#define ETH_CLIENT_EVENT_ERROR         0x00000007

//*****************************************************************************
//
// Connection states for weather application.
//
//*****************************************************************************
volatile enum
{
    STATE_NOT_CONNECTED,
    STATE_NEW_CONNECTION,
    STATE_CONNECTED_IDLE,
    STATE_WAIT_DATA,
    STATE_UPDATE,
    STATE_WAIT_NICE,
    STATE_WAIT_CONNECTION,
}
g_iState = STATE_NOT_CONNECTED;

const char* stateName[] = {"NOT CONNECTED","NEW CONNECTION","IDLE","WAIT DATA","UPDATING","WAIT NEXT","WAIT FOR CONNECTION"};

//const char* nombreDominio = "api.telegram.org";
//const char* nombreDominio = "api.openweathermap.org";
const char* nombreDominio = "httptohttps.mrtimcakes.com";
uint16_t puertoConexion = 80;

int8_t g_exampleRequest[] =
        "GET /https://api.telegram.org/bot1435063235:AAHmzw5HcVKpZvCxf3kgwenqtsIXOHz167E/sendMessage?chat_id=@sepaGIERM&text=TIVAHEY HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n";

const char* requestHeader = "GET /https://api.telegram.org/bot";
char* APIkey = "1435063235:AAHmzw5HcVKpZvCxf3kgwenqtsIXOHz167E";
const char* sendMessage = "sendMessage?chat_id=@";
char* chatName = "sepaGIERM";
const char* textHeader = "&text=";
char* messageToSend;
const char* endRequest =  "HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n";

//*****************************************************************************

//*****************************************************************************
//
// The delay count to reduce traffic to the weather server.
//
//*****************************************************************************
volatile uint32_t g_ui32Delay;

//*****************************************************************************
//
// Global to track number of times the app has cycled through the list of
// cities.
//
//*****************************************************************************
volatile uint32_t g_ui32Cycles;

//*****************************************************************************
//
// The delay count to update the UART.
//
//*****************************************************************************
uint32_t g_ui32UARTDelay;

//*****************************************************************************
//
// System Clock rate in Hertz.
//
//*****************************************************************************
uint32_t g_ui32SysClock;

//*****************************************************************************
//
// Interrupt priority definitions.  The top 3 bits of these values are
// significant with lower values indicating higher priority interrupts.
//
//*****************************************************************************
#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xC0

//*****************************************************************************
//
// MAC address.
//
//*****************************************************************************
char g_pcMACAddr[40];

//*****************************************************************************
//
// IP address.
//
//*****************************************************************************

uint32_t g_ui32IPaddr;

char g_pcIPAddr[20];

// UART
uint16_t actualizarUART;


//*****************************************************************************
//
// Update the IP address string.
//
//*****************************************************************************
void
UpdateIPAddress(char *pcAddr, uint32_t ipAddr)
{
    uint8_t *pui8Temp = (uint8_t *)&ipAddr;

    if(ipAddr == 0)
    {
        ustrncpy(pcAddr, "IP: ---.---.---.---", sizeof(g_pcIPAddr));
    }
    else
    {
        usprintf(pcAddr,"IP: %d.%d.%d.%d", pui8Temp[0], pui8Temp[1],
                 pui8Temp[2], pui8Temp[3]);
    }
}


//*****************************************************************************
//
// Update the MAC address string.
//
//*****************************************************************************
void
UpdateMACAddr(void)
{
    uint8_t pui8MACAddr[6];

    EthClientMACAddrGet(pui8MACAddr);

    usprintf(g_pcMACAddr, "MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             pui8MACAddr[0], pui8MACAddr[1], pui8MACAddr[2], pui8MACAddr[3],
             pui8MACAddr[4], pui8MACAddr[5]);
}

//*****************************************************************************
//
// Print the IP address string.
//
//*****************************************************************************
void
PrintIPAddress(char *pcAddr, uint32_t ipaddr)
{
    uint8_t *pui8Temp = (uint8_t *)&ipaddr;

    //
    // Convert the IP Address into a string.
    //
    UARTprintf("%d.%d.%d.%d\n", pui8Temp[0], pui8Temp[1], pui8Temp[2],
               pui8Temp[3]);
}
//*****************************************************************************
//
// The interrupt handler for the SysTick interrupt.
//
//*****************************************************************************
void
SysTickIntHandler(void) // Cada 10 ms
{
    //
    // Call the lwIP timer handler.
    //
    EthClientTick(SYSTEM_TICK_MS);

    if(actualizarUART > 0) actualizarUART--;
    //
    // Handle the delay between accesses to the weather server.
    //
    if(g_ui32Delay > 0)
    {
        g_ui32Delay--;
    }
}

//*****************************************************************************
//
// Network events handler.
//
//*****************************************************************************
void
EnetEvents(uint32_t ui32Event, void *pvData, uint32_t ui32Param)
{
    if(ui32Event == ETH_CLIENT_EVENT_CONNECT)
    {
        g_iState = STATE_NEW_CONNECTION;

        UpdateIPAddress(g_pcIPAddr, EthClientAddrGet());
    }
    else if(ui32Event == ETH_CLIENT_EVENT_DISCONNECT)
    {
        g_iState = STATE_NOT_CONNECTED;


        UpdateIPAddress(g_pcIPAddr, 0);
    }
	else if(ui32Event == ETH_CLIENT_EVENT_SEND){
		g_iState = STATE_UPDATE;
	}
}

// Variables DEBUG

int32_t DNSResuleto;
int32_t conexionTelegram;
int32_t resEnvio;
int32_t telegramIP;
int32_t i32Idx;
//*****************

int makeRequest(char *text, char *finalRequest){
    sprintf(finalRequest,"GET /https://api.telegram.org/bot%s/sendMessage?chat_id=@%s&text=%s HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n",APIkey,chatName,text);
}

int
main(void)
{

    char myRequest[MAX_REQUEST_SIZE];
    char *letter = "aver";

    SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);

    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
            SYSCTL_OSC_MAIN |
            SYSCTL_USE_PLL |
            SYSCTL_CFG_VCO_240), 120000000);

    PinoutSet(true, false);

    //
    // Initialize the UART.
    //
    UARTStdioConfig(0, 115200, g_ui32SysClock);

    //
    // Configure SysTick for a periodic interrupt at 10ms.
    //
    SysTickPeriodSet((g_ui32SysClock / 1000) * SYSTEM_TICK_MS);
    SysTickEnable();
    SysTickIntEnable();


    // Set the IP address to 0.0.0.0.
    //
    UpdateIPAddress(g_pcIPAddr, 0);

    //
    // Enable processor interrupts.
    //
    IntMasterEnable();

    IntPriorityGroupingSet(4);
    IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
    IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);



    EthClientProxySet(0,0);
	EthClientInit(g_ui32SysClock,EnetEvents);

    UpdateMACAddr();

    do{
        g_ui32IPaddr = EthClientAddrGet();
        SysCtlDelay(50000);
    }while(g_ui32IPaddr == 0 || g_ui32IPaddr == 0xffffffff);

    UARTprintf("\n\n>Mi IP: ");
    PrintIPAddress(0, g_ui32IPaddr); //IP Local ok


    EthClientHostSet(nombreDominio, puertoConexion);
    do{
        DNSResuleto = EthClientDNSResolve();
        SysCtlDelay(50000);
    }while(DNSResuleto != 0);

    telegramIP = EthClientServerAddrGet();
    UARTprintf("\n\n>Server IP: ");
    PrintIPAddress(0, telegramIP); //IP Telegram ok

    actualizarUART = 50;


    messageToSend = "a";

    while(1)
    {
        if(g_iState == STATE_NEW_CONNECTION)
        {
            g_iState = STATE_CONNECTED_IDLE;
        }
        else if(g_iState == STATE_CONNECTED_IDLE)
        {
            g_ui32Delay = 1000; // 10 segundos
            g_iState = STATE_WAIT_DATA;

            makeRequest(letter, myRequest);
            resEnvio=EthClientSend(myRequest,sizeof(myRequest));
            //letter++;
            UARTprintf("\n%s",myRequest);
            //Debug
            if(!conexionTelegram)
                UARTprintf("\n\n>Conexion TCP establecida");
            if(!resEnvio)
                UARTprintf("\n>Mensaje Enviado");

        }
        else if(g_iState == STATE_UPDATE)
        {
            //Entraremos en este estado desde el manejador de eventos si todo sale bien
            g_iState = STATE_WAIT_NICE;

            g_ui32Delay = 500; //5 segundos
        }
        else if(g_iState == STATE_WAIT_NICE)
        {
            if(g_ui32Delay == 0)
            {
                EthClientTCPDisconnect();
                g_iState = STATE_NOT_CONNECTED;
            }
        }
        else if(g_iState == STATE_WAIT_DATA)
        {

            if(g_ui32Delay == 0)
            {
                UARTprintf("\n\n>Ha habido algun fallo, cerramos la conexion");
                EthClientTCPDisconnect();
                g_iState = STATE_NOT_CONNECTED;
            }
        }
		else if(g_iState == STATE_NOT_CONNECTED){
			conexionTelegram = EthClientTCPConnect();
			g_iState = STATE_NEW_CONNECTION;
			g_ui32Delay = 1000;
			//SysCtlDelay(5000000);
		}
		else if(g_iState == STATE_WAIT_CONNECTION){
		    if(g_ui32Delay == 0)
		    {
		        UARTprintf("\n\n>Ha habido algun fallo, cerramos la conexion");
		        EthClientTCPDisconnect();
		        g_iState = STATE_NOT_CONNECTED;
		    }
		}

        if(actualizarUART == 0){
            UARTprintf("\n\nESTADO: %s",stateName[g_iState]);
            actualizarUART = 50;
        }

        SysCtlDelay(500);
    }
}


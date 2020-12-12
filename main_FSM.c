#include <stdint.h>
#include <stdbool.h>
#include "driverlib2.h"
#include "drivers/pinout.h"
#include "utils/cmdline.h"
#include "utils/locator.h"
#include "utils/flash_pb.h"
#include "utils/lwiplib.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "eth_client_lwip.h"
#include <string.h>
#include <stdio.h>
#include "Sensor_HMI.h"


#define SYSTEM_TICK_MS          10
#define SYSTEM_TICK_S           100


//*****************************************************************************
//
// Input command line buffer size.
//
//*****************************************************************************
#define APP_INPUT_BUF_SIZE                  1024

#define MAX_REQUEST_SIZE                    1024


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
const char* endRequest =  " HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n";

char myRequest[MAX_REQUEST_SIZE];
char textoRequest[200];



// Sensores e informe
const int numSensores = 4;

bool configInforme[numSensores] = {true,true,true,true};
float medidasSensores[numSensores] = {20.1, 385.7, 1024.1, 12.6};
char *infoSensores[]={"Temperatura","Luz","Presion","Humedad"};

char unidades[4][10];
char decimales[4][10];

//RELOJ y proridades de interrupcion
volatile uint32_t g_ui32Delay;
uint32_t g_ui32SysClock;

#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xC0

// DIRECCIONES
char g_pcMACAddr[40];
uint32_t g_ui32IPaddr;
char g_pcIPAddr[20];

// UART
uint16_t actualizarUART;
uint32_t g_ui32UARTDelay;

//Variables para el control temporal de la HMI
int tAct=0,tCursor=0;
bool actualizaHMI=1;


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

    /*Activamos un refesco de la pantalla cada 100 ms*/
    tAct++;
    if(tAct > 10)
    {
        tAct=0;
        actualizaHMI=1;
    }


    /*Hacemos que el cursor de la pantalla aparezca y desaparezca cada 0.5s*/
    tCursor++;
    if (tCursor > 50)
    {
        tCursor=0;
        muestraCursor=!muestraCursor;
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


/* URL ENCONDING
   ' ' ->%20
   '\n'->%0A
*/
void separaDecimales(float *in, char unidad[][10], char decimal[][10], int N){
    int i;
    int ud,dec;

    for(i = 0; i < N; i++){
        char aux[10];
        ud = in[i];
        dec = (in[i]-ud)*100;

        sprintf(aux,"%d",ud);
        strcpy(unidad[i],aux);
        sprintf(aux,"%d",dec);
        strcpy(decimal[i],aux);
    }
}


void informeSensores(char *nombreDisp ,char *cadenaOut, char *info[], char unMedidas[][10], char decMedidas[][10], bool *config, int numeroMedidas){
    //sprintf(cadenaOut,"----ENVIADO%20DESDE%20%s---%0A%0ATemperatura%5BC%5D%3A%20%f%0ALuz%3A%20%f%09%0APresion%3A%20%f%0AHumedad%20Rel.%3A%20%f",nombreDisp,temp,luz,presion,hum);
    int i;

    strcpy(cadenaOut,"--ENVIADO%20DESDE%20");
    strcat(cadenaOut,nombreDisp);
    strcat(cadenaOut,"%20--%0A");

    for(i = 0; i < numeroMedidas; i++){
        if(config[i]){
            strcat(cadenaOut,"%0A");
            strcat(cadenaOut,info[i]);
            strcat(cadenaOut,"%20");
            strcat(cadenaOut,unidades[i]);
            strcat(cadenaOut,"%2E");
            strcat(cadenaOut,decimales[i]);
        }
    }

}
void copySt(char *in, char *out){
    sprintf(out,"%s",in);
}

void makeRequest(char *text, char *finalRequest){
    sprintf(finalRequest,"GET /https://api.telegram.org/bot%s/sendMessage?chat_id=@%s&text=%s HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n",APIkey,chatName,text);
    /*sprintf(finalRequest,"GET /https://api.telegram.org/bot%s/sendMessage?chat_id=@%s&text=",APIkey,chatName);
    strcat(finalRequest,text);
    strcat(finalRequest,endRequest);*/
}

int
main(void)
{

    SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);

    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
            SYSCTL_OSC_MAIN |
            SYSCTL_USE_PLL |
            SYSCTL_CFG_VCO_240), 120000000);

    /*Configuramos el sensor y la pantalla*/
    configuraSensor(g_ui32SysClock);
    configuraPantalla(2,g_ui32SysClock);    //Pantalla en el BoosterPack 2

    PinoutSet(true, false);

    // UART
    //
    UARTStdioConfig(0, 115200, g_ui32SysClock);


    // Systick a 10 ms
    //
    SysTickPeriodSet((g_ui32SysClock / 1000) * SYSTEM_TICK_MS);
    SysTickEnable();
    SysTickIntEnable();


    // Poner la IP a 0.0.0.0.
    //
    UpdateIPAddress(g_pcIPAddr, 0);


    // Interrupciones
    //
    IntMasterEnable();

    IntPriorityGroupingSet(4);
    IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
    IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);


    // Inicializacion Ethernet
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

            separaDecimales(medidasSensores, unidades, decimales, 4);
            informeSensores("MikeTIVA", textoRequest,infoSensores, unidades, decimales, configInforme, numSensores);
            makeRequest(textoRequest, myRequest);
            resEnvio=EthClientSend(myRequest,sizeof(myRequest));
            //letter++;
            //UARTprintf("\n%s",myRequest);
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
			g_iState = STATE_WAIT_CONNECTION;
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

        /*---Leemos los sensores y actualizamos la pantalla---*/
        if (actualizaHMI)
        {
            actualizaHMI = 0;
            leeSensores();
            HMI();
        }

        SysCtlDelay(500);
    }
}


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

#define APP_INPUT_BUF_SIZE                  1024
#define MAX_REQUEST_SIZE                    1024


#define ETH_CLIENT_EVENT_DHCP          0x00000001
#define ETH_CLIENT_EVENT_DISCONNECT    0x00000002
#define ETH_CLIENT_EVENT_DNS           0x00000003
#define ETH_CLIENT_EVENT_CONNECT       0x00000004
#define ETH_CLIENT_EVENT_RECEIVE       0x00000005
#define ETH_CLIENT_EVENT_SEND          0x00000006
#define ETH_CLIENT_EVENT_ERROR         0x00000007

/////////////////////////////
// CONFIGURACION
//

#define BP_PANTALLA         1
#define NUMERO_SENSORES     4

///////////////////////////


/////////////////////////////
// Conexion y variables de RED
//

//Estados maquina estado internet

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

//Dominio, puerto y request de ejemplo

const char* nombreDominio = "httptohttps.mrtimcakes.com";
uint16_t puertoConexion = 80;

int8_t g_exampleRequest[] =
        "GET /https://api.telegram.org/bot1435063235:AAHmzw5HcVKpZvCxf3kgwenqtsIXOHz167E/sendMessage?chat_id=@sepaGIERM&text=TIVAHEY HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n";

//Partes Request

const char* requestHeader = "GET /https://api.telegram.org/bot";
char* APIkey = "1435063235:AAHmzw5HcVKpZvCxf3kgwenqtsIXOHz167E";
const char* sendMessage = "sendMessage?chat_id=@";
char* chatName = "sepaGIERM";
const char* textHeader = "&text=";
char* messageToSend;
const char* endRequest =  " HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n";

char myRequest[MAX_REQUEST_SIZE];
char textoRequest[200];
///////////////////////////

/////////////////////////////
// Sensores e informe
//

bool configInforme[NUMERO_SENSORES] = {true,true,true,true};                      //Configuracion por defecto
float medidasSensores[NUMERO_SENSORES] = {20.1, 385.7, 1024.1, 12.6};             //Valores por defecto (DEBUG)
char *infoSensores[20]={"Temperatura","Luz","Presion","Humedad"}; //Nombre por defecto

char nombreDispositivo[20] = "TIVA"; //Por defecto TIVA
char unidades[NUMERO_SENSORES][10];
char decimales[NUMERO_SENSORES][10];
///////////////////////////

/////////////////////////////
// RELOJ y proridades de interrupcion
//
volatile uint32_t g_ui32Delay;
uint32_t g_ui32SysClock;

#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xC0
/////////////////////////////

/////////////////////////////
// DIRECCIONES
//
char g_pcMACAddr[40];
uint32_t g_ui32IPaddr;
char g_pcIPAddr[20];
///////////////////////////

/////////////////////////////
// UART
//
uint16_t actualizarUART;
uint32_t g_ui32UARTDelay;
///////////////////////////

/////////////////////////////
// Variables para el control temporal de la HMI
//
int tAct=0,tCursor=0;
bool actualizaHMI=1;
///////////////////////////

/////////////////////////////
// Variables para el control de comandos del HMI
//
char comandoHMI[5];
char parametrosHMI[10];
///////////////////////////

/////////////////////////////
// Variables DEBUG
//
int32_t DNSResuleto;
int32_t conexionTelegram;
int32_t resEnvio;
int32_t telegramIP;
int32_t i32Idx;
///////////////////////////

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

//*****************************************************************************
//
// Separa un float en unidades y decimales (2 decimales de precision)
//
//*****************************************************************************

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

//*****************************************************************************
//
// Crea el informe de la lectura de los sensores - Condificacion URL
//
//*****************************************************************************

/* URL ENCONDING
   ' ' ->%20
   '\n'->%0A
*/
void informeSensores(char *nombreDisp ,char *cadenaOut, char *info[], char unMedidas[NUMERO_SENSORES][], char decMedidas[NUMERO_SENSORES][], bool *config, int numeroMedidas){
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

//*****************************************************************************
//
// Hace la request HTTP 1.1 final para enviarla al servidor con el mensaje que queramos
//
//*****************************************************************************

void makeRequest(char *text, char *finalRequest){
    sprintf(finalRequest,"GET /https://api.telegram.org/bot%s/sendMessage?chat_id=@%s&text=%s HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n",APIkey,chatName,text);
}

//*****************************************************************************
//
// Actualiza vector de medidas
//
//*****************************************************************************

void actualizarMedidas(float *nuevosValores, float *arrayOut, int N){
    int i;
    for(i = 0; i < N; i++) arrayOut[i] = nuevosValores[i];
}


//*****************************************************************************
//
// Actualiza nombre dispositivo
//
//*****************************************************************************

void actualizaNombreDisp(char *nuevoNombre, char *stringOut){
    strcpy(stringOut,nuevoNombre);
}

//*****************************************************************************
//
// Cambia la cofiguracion de envio de medidas
//
//*****************************************************************************

void cambiaConfig(bool nuevaConfig, bool *arrayOut, int opcion){
    arrayOut[opcion] = nuevaConfig;
}

//*****************************************************************************
//
// Cambia la cofiguracion de envio de medidas
//
//*****************************************************************************

void cambiaInfo(char *nuevaConfig, char *arrayOut[], int opcion){
    arrayOut[opcion] = nuevaConfig;
}


//*****************************************************************************
//
// Parser para los comandos del teclado de la pantalla
//
//*****************************************************************************
int commandParser(char *buffer, char *orden, char *parametros){
    int resultado;
    resultado = sscanf(buffer,"%s %s",orden,parametros);
    if(parametros[strlen(parametros-1)] == '_') parametros[strlen(parametros-1)] = '\0';
    UARTprintf("\n%d -> ESCANEADO: %s %s",resultado,orden,parametros);
    return resultado; //Si devuelve 2 significa que se ha parseado correctamente
}

//*****************************************************************************
//
// Procesado de comandos de la pantalla
//
//*****************************************************************************
void commandAction(char *orden, char *parametros){
    int opcion;
    int valorBool;
    bool finalBool;
    char valorString[20];

    int res;

    if(!strcmp(orden,"INFO")){
        res = sscanf(parametros,"%d,%s",&opcion,valorString);
        if(res){
            cambiaInfo(valorString, infoSensores, opcion);
            UARTprintf("\n%d -> CAMBIADA INFO[%d]: %s ",res,opcion,valorString);
        }

    }
    else if(!strcmp(orden,"NAME")){
        res = sscanf(parametros,"%s",valorString);
        if(res){
            UARTprintf("\n%d -> CAMBIADO NOMBRE: %s ",res,valorString);
            actualizaNombreDisp(valorString, nombreDispositivo);
        }

    }
    else if(!strcmp(orden,"CONF")){
        res = sscanf(parametros,"%d,%d",opcion, &valorBool);
        if(res){
            UARTprintf("\n%d -> CAMBIADA CONF[%d]: %d ",res,opcion,valorBool);
            finalBool = (valorBool != 0);
            //cambiaConfig(finalBool, configInforme, opcion);
        }
    }
}



//*****************************************************************************
//
// Main
//
//*****************************************************************************

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
    configuraPantalla(BP_PANTALLA,g_ui32SysClock);    //Pantalla en el BoosterPack definido en la config

    PinoutSet(true, false);

    // UART
    //
    UARTStdioConfig(0, 115200, g_ui32SysClock);
    UARTprintf("\nINIT ");

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

            separaDecimales(medidasSensores, unidades, decimales, NUMERO_SENSORES);
            informeSensores(nombreDispositivo, textoRequest,infoSensores, unidades, decimales, configInforme, NUMERO_SENSORES);
            makeRequest(textoRequest, myRequest);
            resEnvio=EthClientSend(myRequest,sizeof(myRequest));

            //Debug
            UARTprintf("\n%s",myRequest);
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
            //UARTprintf("\n\nESTADO: %s",stateName[g_iState]);
            actualizarUART = 50;
        }

        /*---Leemos los sensores y actualizamos la pantalla---*/
        if (actualizaHMI)
        {
            actualizaHMI = 0;
            leeSensores();
            HMI();
            actualizarMedidas(vectorMedidas, medidasSensores, NUMERO_SENSORES);
            if(comandoEnviado){
                comandoEnviado = 0;
                commandParser(strConsOutput, comandoHMI, parametrosHMI);
                commandAction(comandoHMI, parametrosHMI);
            }
        }

        SysCtlDelay(500);
    }
}


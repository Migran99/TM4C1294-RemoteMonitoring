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

////////////////////////////////////////////////
//  CONFIGURACIÓN
//

#define BP_PANTALLA         2
#define NUMERO_SENSORES     4

////////////////////////////////////////////////

////////////////////////////////////////////////
// CONEXIÓN Y VARIABLES DE RED
//

//Estados de la máquina de estados de internet

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

//Dominio, puerto y request para la conexión con nuesta API

const char* nombreDominio = "httptohttps.mrtimcakes.com";
uint16_t puertoConexion = 80;

int8_t g_exampleRequest[] =
        "GET /https://api.telegram.org/bot1435063235:AAHmzw5HcVKpZvCxf3kgwenqtsIXOHz167E/sendMessage?chat_id=@sepaGIERM&text=TIVAHEY HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n";

//Partes que conforman el Request

const char* requestHeader = "GET /https://api.telegram.org/bot";
char* APIkey = "1435063235:AAHmzw5HcVKpZvCxf3kgwenqtsIXOHz167E";
const char* sendMessage = "sendMessage?chat_id=@";
char* chatName = "sepaGIERM";
const char* textHeader = "&text=";
char* messageToSend;
const char* endRequest =  " HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n";

char myRequest[MAX_REQUEST_SIZE];
char textoRequest[200];

////////////////////////////////////////////////

////////////////////////////////////////////////
//  VARIABLES DE SENSORES Y DEL INFORME
//

bool configInforme[NUMERO_SENSORES] = {true,true,true,true};                      //Configuracion por defecto
float medidasSensores[NUMERO_SENSORES] = {20.1, 385.7, 1024.1, 12.6};             //Valores por defecto (DEBUG)
char infoSensores[NUMERO_SENSORES][20]={"Temperatura","Luz","Presion","Humedad"}; //Nombre por defecto

char nombreDispositivo[20] = "TIVA"; //Por defecto TIVA
char unidades[NUMERO_SENSORES][10];
char decimales[NUMERO_SENSORES][10];
int tiempoEntreMensajes = 500;      //Ciclos de 10 ms entre requests (por defecto 5s)
////////////////////////////////////////////////

////////////////////////////////////////////////
//  RELOJ Y PRIORIDADES DE INTERRUPCIÓN
//

volatile uint32_t g_ui32Delay;
uint32_t g_ui32SysClock;

#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xC0

////////////////////////////////////////////////

////////////////////////////////////////////////
//  DIRECCIONES
//

char g_pcMACAddr[40];
uint32_t g_ui32IPaddr;
char g_pcIPAddr[20];

////////////////////////////////////////////////

////////////////////////////////////////////////
//  UART
//

uint16_t actualizarUART;
uint32_t g_ui32UARTDelay;

////////////////////////////////////////////////

////////////////////////////////////////////////
// VARIABLES PARA EL CONTROL TEMPORAL DE LA HMI
//

int tAct=0,tCursor=0;
bool actualizaHMI=1;

////////////////////////////////////////////////

////////////////////////////////////////////////
// VARIABLES PARA EL CONTROL DE COMANDOS DE LA HMI
//

char comandoHMI[5];
char parametrosHMI[10];

////////////////////////////////////////////////

////////////////////////////////////////////////
// VARIABLES DEBUG
//

int32_t DNSResuleto;
int32_t conexionTelegram;
int32_t resEnvio;
int32_t telegramIP;
int32_t i32Idx;

////////////////////////////////////////////////

//*****************************************************************************
//
//  FUNCIÓN QUE GUARDA EN UN STRING LA IP LOCAL
//
//*****************************************************************************

void UpdateIPAddress(char *pcAddr, uint32_t ipAddr)
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
// ACTUALIZA LA DIRECCIÓN MAC
//
//*****************************************************************************

void UpdateMACAddr(void)
{
    uint8_t pui8MACAddr[6];

    EthClientMACAddrGet(pui8MACAddr);

    usprintf(g_pcMACAddr, "MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             pui8MACAddr[0], pui8MACAddr[1], pui8MACAddr[2], pui8MACAddr[3],
             pui8MACAddr[4], pui8MACAddr[5]);
}

//*****************************************************************************
//
// ESCRIBE POR LA UART NUESTRA DIRECCIÓN IP
//
//*****************************************************************************

void PrintIPAddress(char *pcAddr, uint32_t ipaddr)
{
    uint8_t *pui8Temp = (uint8_t *)&ipaddr;

    UARTprintf("%d.%d.%d.%d\n", pui8Temp[0], pui8Temp[1], pui8Temp[2],
               pui8Temp[3]);
}

//*****************************************************************************
//
// FUNCIÓN HANDLER DE LAS INTERRUPCIONES SysTick (CADA 10 MS)
//
//*****************************************************************************

void SysTickIntHandler(void)
{
    // Call the lwIP timer handler.
    EthClientTick(SYSTEM_TICK_MS);

    //Timeout para actualizar la UART
    if(actualizarUART > 0)
        actualizarUART--;

    //Variable para realizar timeout y delays en la conexión
    if(g_ui32Delay > 0)
    {
        g_ui32Delay--;
    }

    //Refrescamos la pantalla cada 100 ms
    tAct++;
    if(tAct > 10)
    {
        tAct=0;
        actualizaHMI=1;
    }


    //Hacemos que el cursor de la pantalla aparezca y desaparezca cada 0.5s
    tCursor++;
    if (tCursor > 50)
    {
        tCursor=0;
        muestraCursor=!muestraCursor;
    }
}

//*****************************************************************************
//
// MANEJADOR DE INTERRUPCIONES DE LOS EVNETOS DE INTERNET
//
//*****************************************************************************

void EnetEvents(uint32_t ui32Event, void *pvData, uint32_t ui32Param)
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
//  SEPARA UN FLOAT EN UNIDADES Y DECIMALES (2 DEC DE PRECISIÓN)
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
//  CREA EL INFORME DE LA LECTURA DE LOS SENSORES - COFIFICACIÓN URL
//
//*****************************************************************************

void informeSensores(char *nombreDisp ,char *cadenaOut, char info[][20], char unMedidas[NUMERO_SENSORES][], char decMedidas[NUMERO_SENSORES][], bool *config, int numeroMedidas){
    int i;

    //////////////////////
    //  URL ENCONDING   //
    //   ' ' ->%20      //
    //   '\n'->%0A      //
    //////////////////////

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
//  CONSTRUYE EL MENSAJE DE REQUEST HTTP 1.1 PERSONALIZADO
//
//*****************************************************************************

void makeRequest(char *text, char *finalRequest){
    sprintf(finalRequest,"GET /https://api.telegram.org/bot%s/sendMessage?chat_id=@%s&text=%s HTTP/1.1\r\nHost: httptohttps.mrtimcakes.com\r\nConnection: close\r\n\r\n",APIkey,chatName,text);
}

//*****************************************************************************
//
//  ACTUALIZA EL VECTOR DE MEDIDAS
//
//*****************************************************************************

void actualizarMedidas(float *nuevosValores, float *arrayOut, int N){
    int i;
    for(i = 0; i < N; i++) arrayOut[i] = nuevosValores[i];
}


//*****************************************************************************
//
// ACTUALIZA EL NOMBRE DEL DISPOSITIVO
//
//*****************************************************************************

void actualizaNombreDisp(char *nuevoNombre, char *stringOut){
    strcpy(stringOut,nuevoNombre);
}

//*****************************************************************************
//
//  CAMBIA LA CONFIGURACIÓN DEL ENVÍO DE MEDIDAS
//
//*****************************************************************************

void cambiaConfig(bool nuevaConfig, bool *arrayOut, int opcion){
    arrayOut[opcion] = nuevaConfig;
}

//*****************************************************************************
//
//  CAMBIA EL NOMBRE DE LOS PARÁMETROS
//
//*****************************************************************************

void cambiaInfo(char *nuevaConfig, char arrayOut[][20], int opcion){
    strcpy(arrayOut[opcion],nuevaConfig);
}

//*****************************************************************************
//
//  PARSER PARA LOS COMANDOS DEL TECLADO DE LA PANTALLA
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
//  PROCESADO DE COMANDOS DE LA PANTALLA
//
//*****************************************************************************

void commandAction(char *orden, char *parametros){
    int opcion;
    int valorBool;
    bool finalBool;
    char valorString[20];
    int res;

    // INFO
    if(!strcmp(orden,"INFO")){
        res = sscanf(parametros,"%d,%s",&opcion,valorString);
        if(res){
            cambiaInfo(valorString, infoSensores, opcion);
            UARTprintf("\n%d -> CAMBIADA INFO[%d]: %s ",res,opcion,valorString);
        }

    }

    // NAME
    else if(!strcmp(orden,"NAME")){
        res = sscanf(parametros,"%s",valorString);
        if(res){
            UARTprintf("\n%d -> CAMBIADO NOMBRE: %s ",res,valorString);
            actualizaNombreDisp(valorString, nombreDispositivo);
        }

    }

    // CONF
    else if(!strcmp(orden,"CONF")){
        res = sscanf(parametros,"%d,%d",&opcion, &valorBool);
        if(res){
            UARTprintf("\n%d -> CAMBIADA CONF[%d]: %d ",res,opcion,valorBool);
            finalBool = (valorBool != 0);
            cambiaConfig(finalBool, configInforme, opcion);
        }
    }

    //FREC
    else if(!strcmp(orden,"FREC"))
    {
        res = sscanf(parametros,"%d",&opcion);
        if(res){
            UARTprintf("\n%d -> CAMBIADA FRECUENCIA A: %d S ",res,opcion);
            tiempoEntreMensajes = 100*opcion;
        }

    }
}

//*****************************************************************************
//
// MAIN
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

    //  Configuramos el sensor y la pantalla
    //
    configuraSensor(g_ui32SysClock);
    configuraPantalla(BP_PANTALLA,g_ui32SysClock);    //Pantalla en el BoosterPack definido en la config

    PinoutSet(true, false);

    //  UART
    //
    UARTStdioConfig(0, 115200, g_ui32SysClock);
    UARTprintf("\nINIT ");

    //  Systick a 10 ms
    //
    SysTickPeriodSet((g_ui32SysClock / 1000) * SYSTEM_TICK_MS);
    SysTickEnable();
    SysTickIntEnable();


    //  Poner la IP a 0.0.0.0.
    //
    UpdateIPAddress(g_pcIPAddr, 0);


    //  Interrupciones
    //
    IntMasterEnable();
    IntPriorityGroupingSet(4);
    IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
    IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);


    //  Inicializacion Ethernet
    //
    EthClientProxySet(0,0);
    EthClientInit(g_ui32SysClock,EnetEvents);

    //  Actualizamos la dirección MAC
    //
    UpdateMACAddr();

    //  Actualizamos la dirección IP
    //
    do{
        g_ui32IPaddr = EthClientAddrGet();
        SysCtlDelay(50000);
    }while(g_ui32IPaddr == 0 || g_ui32IPaddr == 0xffffffff);

    UARTprintf("\n\n>Mi IP: ");
    PrintIPAddress(0, g_ui32IPaddr);    //IP Local ok

    //  Fijamos el servidor y resolvemos el DNS
    //
    EthClientHostSet(nombreDominio, puertoConexion);

    do{
        DNSResuleto = EthClientDNSResolve();
        SysCtlDelay(50000);
    }while(DNSResuleto != 0);

    telegramIP = EthClientServerAddrGet();
    UARTprintf("\n\n>Server IP: ");
    PrintIPAddress(0, telegramIP);      //IP Telegram ok

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
            g_ui32Delay = 1000; //  Timeout de 10s
            g_iState = STATE_WAIT_DATA;

            separaDecimales(medidasSensores, unidades, decimales, NUMERO_SENSORES);
            informeSensores(nombreDispositivo, textoRequest,infoSensores, unidades, decimales, configInforme, NUMERO_SENSORES);
            makeRequest(textoRequest, myRequest);
            resEnvio=EthClientSend(myRequest,sizeof(myRequest));

            //  Debug
            //
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

            g_ui32Delay = tiempoEntreMensajes;
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


        //Leemos los sensores y actualizamos la pantalla
        //
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


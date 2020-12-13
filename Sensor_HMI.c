#include <stdint.h>
#include <stdbool.h>
#include "driverlib2.h"
#include "FT800_TIVA.h"
#include <stdio.h>
#include <string.h>
#include "HAL_I2C.h"
#include "sensorlib2.h"
#include <math.h>

/*---Variables para el control de la pantalla---*/
char chipid = 0;                                // Holds value of Chip ID read from the FT800
unsigned int CMD_Offset = 0;
unsigned long cmdBufferRd = 0x00000000;         // Store the value read from the REG_CMD_READ register
unsigned long cmdBufferWr = 0x00000000;         // Store the value read from the REG_CMD_WRITE register
unsigned int t=0;
const int32_t REG_CAL[6]={21696,-78,-614558,498,-17021,15755638};
const int32_t REG_CAL5[6]={32146, -1428, -331110, -40, -18930, 18321010};
unsigned long POSX, POSY, BufferXY;

/*---Variables para el control de los sensores---*/
uint8_t Sensor_OK=0;
uint8_t Opt_OK, Bme_OK;

// BME280
int returnRslt;
int g_s32ActualTemp   = 0;
unsigned int g_u32ActualPress  = 0;
unsigned int g_u32ActualHumity = 0;
float T_act,P_act,H_act;

//OPT
float lux;
int lux_i;

//Vector Medidas
float vectorMedidas[4];

/*---Variables de control de la HMI---*/
int cursor = 0;                 //Posición del cursor dentro de la cadena
char strTecl[2];                //Cadena para pintar el teclado
char strConsMedidas[60];        //Cadena que muestra las variables de los sensores en la consola
int indent;
const char teclado[4][10] = {{'1','2','3','4','5','6','7','8','9','0'},     //Teclado alfanumérico
                             {'q','w','e','r','t','y','u','i','o','p'},
                             {'a','s','d','f','g','h','j','k','l', 0},
                             { 0 ,'z','x','c','v','b','n','m', 0 , 0 }};

int flanco = 0;                 //Almacenará un numero que identificará el botón en el que acaba de haber un flanco
bool mayuscAct=0;               //Bloqueo de mayusculas activado
bool muestraCursor=0;           //Variable que debe cambiarse alternativamente para mostrar el parpadeo del cursor
char strCons[51];               //Texto de la consola

char strConsOutput[51]={""};    //Texto que sale de la consola cuando se presiona enter
bool comandoEnviado = 0;        //Se queda a 1 cuando se pulsa enter


/* ------FUNCIÓN DE CONFIGURACIÓN DE LOS SENSORES--------*/
void configuraSensor(int frecReloj)
{
    if(Detecta_BP(1))
        Conf_Boosterpack(1, frecReloj);
    else if(Detecta_BP(2))
        Conf_Boosterpack(2, frecReloj);
    else
        return 0;

    //OPT3001
    Sensor_OK=Test_I2C_Dir(OPT3001_SLAVE_ADDRESS);
    if(!Sensor_OK)
    {
        Opt_OK=0;
    }
    else
    {
        OPT3001_init();
        Opt_OK=1;
    }

    //BME280
    Sensor_OK=Test_I2C_Dir(BME280_I2C_ADDRESS2);
    if(!Sensor_OK)
    {
        Bme_OK=0;
    }
    else
    {
        bme280_data_readout_template();
        bme280_set_power_mode(BME280_NORMAL_MODE);
        Bme_OK=1;
    }
}

/*------FUNCIÓN DE CONFIGURACIÓN DE LA PANTALLA--------
 * BP    -> Boosterpack de conexión
 * RELOJ -> Frecuencia de reloj configurada
 */
void configuraPantalla(int BP, int frecReloj)
{
    int i;
    HAL_Init_SPI(BP, frecReloj);
    Inicia_pantalla();
    SysCtlDelay(frecReloj/3);
    /*Calibración de la pantalla*/
#ifdef VM800B35
    for(i=0;i<6;i++)    Esc_Reg(REG_TOUCH_TRANSFORM_A+4*i, REG_CAL[i]);
#endif
#ifdef VM800B50
    for(i=0;i<6;i++)    Esc_Reg(REG_TOUCH_TRANSFORM_A+4*i, REG_CAL5[i]);
#endif
}

/*---------FUNCIÓN PARA LEER EL VALOR DE LOS SENSORES------*/
void leeSensores(void)
{
    /*---LECTURA DE DATOS---*/
    //OPT3000
    if(Opt_OK)
    {
        lux=OPT3001_getLux();
        lux_i=(int) round(lux);
    }

    //BME280
    if(Bme_OK)
    {
        returnRslt = bme280_read_pressure_temperature_humidity(&g_u32ActualPress, &g_s32ActualTemp, &g_u32ActualHumity);
        T_act=(float)g_s32ActualTemp/100.0;
        P_act=(float)g_u32ActualPress/100.0;
        H_act=(float)g_u32ActualHumity/1000.0;
    }
    vectorMedidas[0] = T_act;
    vectorMedidas[1] = lux;
    vectorMedidas[2] = P_act;
    vectorMedidas[3] = H_act;
}


/* ------- PINTAMOS Y EJECUTAMOS LA HMI -------
 *
 */

void HMI (void)
{
    int i,j;        //Índices

    /*---PINTAMOS LA HMI---*/
    //Pintamos la pantalla
    Nueva_pantalla(0,0,0);
    ComLineWidth(1);

    //Fondo+Marco
    ComColor(0x7F,0xCC,0xA2);
    Comando(CMD_BEGIN_RECTS);
    ComVertex2ff(2,2);
    ComVertex2ff(HSIZE-2,VSIZE-2);

    //Ventana de texto
    ComColor(0xFF,0xFF,0xFF);
    ComVertex2ff(3,3);
    ComVertex2ff(HSIZE-3,(VSIZE*3)/8);
    Comando(CMD_END);

    ComColor(0x7F,0xCC,0xA2);
    Comando(CMD_BEGIN_LINES);
    ComVertex2ff(4,4);
    ComVertex2ff(HSIZE-4,4);
    ComVertex2ff(HSIZE-4,4);
    ComVertex2ff(HSIZE-4,(VSIZE*3)/8);
    ComVertex2ff(HSIZE-4,(VSIZE*3)/8);
    ComVertex2ff(4,(VSIZE*3)/8);
    ComVertex2ff(4,(VSIZE*3)/8);
    ComVertex2ff(4,4);
    Comando(CMD_END);

    //Pintamos el teclado y vemos que teclas están pulsadas

    //--Letras--
    ComColor(0x00,0x2B,0x38);
    ComFgcolor(0x96,0xB7,0xB5);
    indent=0;
    for (j=0;j<4;j++)
    {
        if (j>1) // Identamos las dos filas de abajo
            indent=HSIZE/20;
        for (i=0;i<10;i++)
        {
            if(teclado[j][i])
            {
                if (mayuscAct && j!=0)
                    sprintf(strTecl,"%c",teclado[j][i]+('A'-'a'));
                else
                    sprintf(strTecl,"%c",teclado[j][i]);
                if(Boton(indent+6+i*(HSIZE/10-1),((VSIZE*3)/8+5)+(VSIZE/8-1)*j, HSIZE/10-3, VSIZE/8-5, 21,strTecl))
                {
                    //Si se da un flanco de subida
                    if (!flanco)
                    {
                        strCons[cursor]=strTecl[0];
                        flanco=j*10+i;
                        cursor++;
                    }
                }
                else if (j*10+i==flanco) //Si el botón para el cuál activamos el flanco se ha dejado de pulsar...
                    flanco = 0;
            }
        }
    }

    //--Edición de texto y caracteres especiales--

    //Espacio
    if(Boton(6+3*(HSIZE/10-1),((VSIZE*3)/8+5)+(VSIZE/8-1)*4, 4*(HSIZE/10)-6, VSIZE/8-5, 21,"space"))
    {
        if (!flanco)
        {
            strCons[cursor]=' ';
            cursor++;
            flanco=100;
        }
    }
    else if (flanco==100)
        flanco = 0;

    //Coma
    if(Boton(6+2*(HSIZE/10-1),((VSIZE*3)/8+5)+(VSIZE/8-1)*4, (HSIZE/10)-3, VSIZE/8-5, 21,","))
    {
        if (!flanco)
        {
            strCons[cursor]=',';
            cursor++;
            flanco=101;
        }
    }
    else if (flanco==101)
        flanco = 0;

    //Punto
    if(Boton(6+7*(HSIZE/10-1),((VSIZE*3)/8+5)+(VSIZE/8-1)*4, (HSIZE/10)-3, VSIZE/8-5, 21,"."))
    {
        if (!flanco)
        {
            strCons[cursor]='.';
            cursor++;
            flanco=102;

        }
    }
    else if (flanco==102)
        flanco = 0;

    //Mayusc
    if (mayuscAct)
        ComFgcolor(0x56,0x87,0x85);
    else
        ComFgcolor(0x96,0xB7,0xB5);

    if (Boton(indent+6,((VSIZE*3)/8+5)+(VSIZE/8-1)*3, HSIZE/10-3, VSIZE/8-5, 21,"^"))
    {
        if (!flanco)
        {
            mayuscAct=!mayuscAct;
            flanco=103;
        }
    }
    else if (flanco==103)
        flanco = 0;

    ComFgcolor(0x96,0xB7,0xB5);

    //Borrar
    if (Boton(indent+6+8*(HSIZE/10-1),((VSIZE*3)/8+5)+(VSIZE/8-1)*3, HSIZE/10-3, VSIZE/8-5, 21,"<-"))
    {
        if (!flanco && cursor>0)
        {
            cursor --;
            flanco=104;
        }
    }
    else if (flanco==104)
        flanco = 0;

    //Enter
    if (Boton(6+8*(HSIZE/10-1),((VSIZE*3)/8+5)+(VSIZE/8-1)*4, (3*(HSIZE/10))/2-3, VSIZE/8-5, 21,"ENTER"))
    {
        if (!flanco)
        {
            comandoEnviado=1;
            strcpy(strConsOutput,strCons);
            strConsOutput[cursor] ='\0';
            cursor=0;
            flanco=105;
        }
    }
    else if (flanco==105)
        flanco = 0;


    //--Texto Consola--
    //Cursor
    //El cursor no debe llegar a la posición final de la cadena o estaremos escribiendo el terminador final fuera
    if (cursor>50)
        cursor=50;

    if (muestraCursor)
        strCons[cursor]='_';
    else
        strCons[cursor]=' ';

    strCons[cursor + 1]='\0';
    ComColor(0,0,0);
    ComTXT(HSIZE/32,VSIZE/4, 27, OPT_MONO,strCons);

    //------MEDIDAS AMBIENTALES----------
    if(Opt_OK && Bme_OK)
    {
        ComColor(0xA6,0x39,0x2C);
        sprintf(strConsMedidas,"LUX: %d --- T:%.2f C --- P:%.2fmbar --- H:%.3f",lux_i,T_act,P_act,H_act);
        ComTXT(HSIZE/2,VSIZE/16, 20, OPT_CENTER,strConsMedidas);
    }

    Dibuja();
}





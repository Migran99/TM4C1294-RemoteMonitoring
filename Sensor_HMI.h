

void configuraSensor(int frecReloj);            //Configura el sensor

void configuraPantalla(int BP, int frecReloj);  //Configura la pantalla

void leeSensores(void);                         //Lecutra de los valores de los sensores

void HMI (void);                                //Interfaz HMI

extern bool muestraCursor;          //Alternar el valor de esta variable para realizar el parpadeo del cursor

extern char strConsOutput[51];      //Mensaje enviado

extern bool comandoEnviado;                //Flag que indica cuando tenemos un comando pendiente a tratar

extern float lux;                   //Luminosidad medida

extern float T_act;                 //Temperatura medida

extern float P_act;                 //Presión medida

extern float H_act;                 //Humedad medida

extern float vectorMedidas[4];

//MSP_Bomba5 - Tinaco4
#include <msp430.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
char resp[20]={};
char dist[5]={};
char temp[5]={};
char checkS[5]={};
int seg=0,seg2=0,seg3=0,seg4=0,bomba=0,i,ct=0,cuarto=0,comm=0;
int tmpCheck = 0,tmpDist=0,tmpTemp=0,sum=0;

void calibrar_osc(void);
void config_uart(void);
void recepcionCadena(char[]);
void separarVal(char[]);
void borrarCadena(void);
void errorComm(void);
void compararCheck(void);

int main(void){
    WDTCTL = WDTPW | WDTHOLD;
    calibrar_osc();
    config_uart();

    P1DIR |= BIT5; //P1.5 Señal a relay
    P1OUT |= BIT5;
    P2DIR |= BIT5; //P2.5 LED

    for(i=3;i>0;i--){   //3 parpadeos de inicio
        P2OUT |= BIT5 ;
        __delay_cycles(1000000);
        P2OUT &= ~BIT5;
        __delay_cycles(1000000);
    }

    TACCR0 = 50000;
    TA0CTL = TASSEL_2 + MC_1;
    TACCTL0 = CCIE;

    __bis_SR_register(LPM0_bits + GIE);
}

// Interrupción del Timer
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
    /*seg2++;
    if( seg2 >= 20*900 ){
       cuarto++;
       if( cuarto == 24 )
           WDTCTL &= ~(WDTPW + WDTHOLD);        // Resetear cada 6 hrs
    }*/

    if( comm > 2 ){                            // Si han pasado 3 o más intentos sin communicación
        errorComm();                                // Método error de communicación
    }

    seg++;
    if( (seg >= 20*15)){                        // Cada quince segundos solicitar estatus
        for(i=2;i>0;i--){
            P2OUT |= BIT5;
            __delay_cycles(100000);
            P2OUT &= ~BIT5;
            __delay_cycles(100000);
        }
        while (!(IFG2&UCA0TXIFG));
        UCA0TXBUF = 's';                        // MSP_Bomba solicia estatus a MSP_Tinaco.
        seg=0;
        comm++;                                 // Incrementar intentos de communicación
    }

    seg3++;
    if((seg3 >= 20*900) && bomba == 1){      //Si la bomba tiene más de 15 min encendida, apagar
        P1OUT |= BIT5;
    }
}

//Interrupción de recepción
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
    comm = 0;
    while (!(IFG2&UCA0TXIFG));
    resp[ct] = UCA0RXBUF;                          // Recepción de respuesta de MSP_Tinaco y guardado en arreglo de chars

    if( resp[ ct ] == '#'){                        //La recepción ha llegado al final de trama (#)
        recepcionCadena(resp);                          //Método para verificar respuesta de tapa
        borrarCadena();                                 //Método para limpiar la cadena de respuesta
    }else if (resp[ct] == 'i' ) {                  //Si la recepción está en su inicio de trama, iniciar el contador
        ct=0;                                        // Reiniciar contador caractares = 0; Reiniciar intentos de comunicación;
    }else {                                       //Si el caracter recibido es parte de la trama, el contador va avanzando
        ct++;
    }
}

void recepcionCadena(char respu[] ){
    separarVal(respu);                  //Método para separar cadena de distancia y temperatura
    compararCheck();
}

void separarVal(char respu[20] ){
    int b1=0,b2=0;
    int j=0,k=0,l=0,m=0;
    int fin=0;

    while( !fin ){
        if( respu[j] == '$' && b1 == 0 ){ // Si es el primer $
            j++;b1 = 1;                             //activamos bandera1=1, terminamos con la primera variable (distancia)
        } else if (b1 == 1){              //Sí la bandera1==1, estamos en la segunda variable (temperatura)
            if(respu[j] == '$' && b2 == 0){     //Si es el segundo $, comenzamos con el tercer valor
                j++;b2=1;                           //activamos bandera2=1, terminamos con la segunda variable (temperatura)
            }else if(b2 == 1){                      //Si la bandera2==1, estamos en el tercer valor (check)
                if(respu[j] == '$'){                    //Si es el tercer $, hemos terminado de serparar
                    dist[k] = '\0';
                    temp[l] = '\0';
                    checkS[m] = '\0';
                    fin=1;
                } else {
                    checkS[m] = respu[j];                //Si no, seguimos guardando el tercer valor (check)
                    m++;j++;
                }
            }else{                              //Si no, seguimos guardando la segunda variable
                temp[l] = respu[j];
                l++;j++;
            }
        } else{                          //Si no hemos llegado al primer $, guardamos la primera variable
            dist[k] = respu[j];
            k++;j++;
        }
    }
}

void borrarCadena(void){    //Limpiar cadena de respuesta recibida
    int z;
    for(z = 0; z<21; z++){
        resp[i]=' ';
    }
}

void errorComm(void){
    int z;
    for(z = 8; z>0; z--){
        P2OUT |= BIT5;
        __delay_cycles(100000);
        P2OUT &= ~BIT5;
        __delay_cycles(100000);
    }
    comm=2;
}

void compararCheck(void){
    tmpCheck = 0,tmpDist=0,tmpTemp=0,sum=0;

    tmpTemp = atoi(temp);
    tmpDist = atoi(dist);
    tmpCheck = atoi(checkS);

    sum = tmpDist + tmpTemp;

    if ( tmpCheck != sum ){
            for(i=2;i>0;i--){               // Hacer una nueva solicitud
                P2OUT |= BIT5;
                __delay_cycles(100000);
                P2OUT &= ~BIT5;
                __delay_cycles(100000);
            }
            while (!(IFG2&UCA0TXIFG));
            UCA0TXBUF = 's';
            seg=0;
    }else{
        if(tmpDist <= 50 ){                  // Si le falta agua a MSP_Tinaco
            P2OUT |= BIT5;                          //Parpadear una vez
            __delay_cycles(100000);
            P2OUT &= ~BIT5;
            __delay_cycles(100000);
            P1OUT &= ~BIT5;                         // Cerrar circuito del relay
            if(!bomba){                             // Si la bandera de bomba encendida aún no está marcada
                bomba = 1;                              // Marcar bandera de bomba=encendida
                seg3 = 0;                               // Iniciar contador de bomba encendida
            }
        }else if (tmpDist >= 15 ){           // Si tinaco está lleno
          P1OUT |= BIT5;                            // Apagar bomba
          bomba = 0;                                // Marcar bandera de bomba=apagada
        }
    }

}

void config_uart(void){
        P1SEL = BIT1 + BIT2 ;                     // P1.1 = RXD, P1.2=TXD
        P1SEL2 = BIT1 + BIT2 ;                    // P1.1 = RXD, P1.2=TXDs
        UCA0CTL1 |= UCSSEL_2;                     // SMCLK
        UCA0BR0 = 104;                            // 1MHz 9600 (1 seg/9600 = 104 microSeg)
        UCA0BR1 = 0;                              // 1MHz 9600
        UCA0MCTL = UCBRS0;                        // Modulation UCBRSx = 1
        UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
        IE2 |= UCA0RXIE;                          // Habilito interrupción de RX.
}

void calibrar_osc(void){
    if (CALBC1_1MHZ==0xFF)                    // If calibration constant erased
        {
            while(1);                               // do not load, trap CPU!!
        }
        DCOCTL = 0;                               // Select lowest DCOx and MODx settings
        BCSCTL1 = CALBC1_1MHZ;                    // Set DCO
        DCOCTL = CALDCO_1MHZ;
}

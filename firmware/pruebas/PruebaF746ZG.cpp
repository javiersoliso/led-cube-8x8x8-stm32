//Prueba de arranque NUCLEO-F746ZG: 5 secuencias cambiadas por interrupción externa (pulsador B1)
//Javier Solís - Sistemas Embebidos - Universidad ECCI - 2026
//Tarjeta: NUCLEO-F746ZG (STM32F746ZGT6, placa MB1137: mismo conector y mismos leds/pulsador que la NUCLEO-F767ZI del curso)
//En Keil seleccionar el dispositivo STM32F746ZGTx (CMSIS->CORE y Device->Startup)
//Estilo del curso: registros directos, sin HAL. operation 'or' (|) for set bit and operation 'and' (&) for clear bit
//
//Pines usados:
//  LD1 verde  = PB0   LD2 azul = PB7   LD3 rojo = PB14   (leds de la tarjeta)
//  B1 usuario = PC13  (entrega 3.3 V al presionar -> flanco de subida, EXTI13)
//  Bus de COLUMNAS de prueba = PE0..PE7  -> entradas A del 74HCT245 -> base del 2N2907 (PNP, lado alto a 5 V)
//  Bus de FILAS (pisos)      = PF0..PF7  -> entradas A del 74HCT245 -> compuerta del IRFZ44N (lado bajo a GND)
//
//Polaridad: con 74HCT245 + 2N2907 la columna se ENCIENDE cuando el pin del micro está en BAJO
//(el 74HCT245 baja la base y el PNP conduce). Con un led directo del pin a GND (sin etapa de potencia)
//la lógica es la contraria: poner COLUMNAS_ACTIVAS_EN_BAJO en 0.
//
//NOTA (11/09/2026): esta prueba usa PE0-PE7 / PF0-PF7, que en el contrato de pines version 2 son las filas y4/y6
//de la baquela 2. Para la baquela 1 (banco 1 = PD0-PD7, pisos PD10-PD15/PG10/PG12) usar PruebaEstatica.cpp.
#define COLUMNAS_ACTIVAS_EN_BAJO 1

#include <stdio.h>
#include "stm32f7xx.h"

#define NUM_SECUENCIAS 5
#define T 400000u   //duración aproximada de cada paso (~150 ms con el reloj interno HSI de 16 MHz que trae por defecto)

volatile uint8_t secuencia = 0;  //0..4, la cambia la interrupción del pulsador
volatile uint8_t cambio = 0;     //bandera: hubo cambio de secuencia, reiniciar el patrón

//Retardo por software que el compilador no puede eliminar (volatile + instrucción NOP)
void Delay(volatile uint32_t t){
	while(t--){
		__NOP();
	}
}

//Leds de la tarjeta por BSRR (bits 0-15 ponen en 1, bits 16-31 ponen en 0; escritura atómica, no toca otros pines)
void leds(uint8_t verde, uint8_t azul, uint8_t rojo){
	GPIOB->BSRR = (verde ? (1u<<0)  : (1u<<16))
	            | (azul  ? (1u<<7)  : (1u<<23))
	            | (rojo  ? (1u<<14) : (1u<<30));
}

//Escribe un byte en PE0..PE7 (columnas). v=1 significa "columna encendida"
void columnas(uint8_t v){
#if COLUMNAS_ACTIVAS_EN_BAJO
	v = (uint8_t)~v;
#endif
	GPIOE->BSRR = (uint32_t)v | ((uint32_t)((uint8_t)~v) << 16);
}

//Escribe un byte en PF0..PF7 (filas o pisos). v=1 significa "piso conectado a GND" (MOSFET conduciendo)
void filas(uint8_t v){
	GPIOF->BSRR = (uint32_t)v | ((uint32_t)((uint8_t)~v) << 16);
}

//Espera 't' pero sale antes si el pulsador cambió la secuencia
void esperar(uint32_t t){
	while(t > 0 && !cambio){
		Delay(2000);
		t = (t > 2000) ? (t - 2000) : 0;
	}
}

extern "C"{
	void EXTI15_10_IRQHandler(void){
		EXTI->PR = (1<<13);   //Bajar la bandera: se escribe 1 en el bit (con '=' y no con '|=' para no borrar otras banderas pendientes)
		Delay(20000);         //Antirrebote sencillo (unos milisegundos)
		if(((GPIOC->IDR & (1<<13)) >> 13) == 1){   //Sigue presionado -> cuenta como pulsación válida
			secuencia = (uint8_t)((secuencia + 1) % NUM_SECUENCIAS);
			cambio = 1;
		}
	}
}

int main(){
	uint8_t contador = 0;

	//1. Habilitar reloj de los puertos B (leds), C (pulsador), E (columnas) y F (filas) - RCC_AHB1ENR bits 1,2,4,5
	RCC->AHB1ENR |= (1<<1) | (1<<2) | (1<<4) | (1<<5);

	//2. Leds PB0, PB7 y PB14 como salida (01), push-pull (0), muy alta velocidad (11), sin pull (00)
	GPIOB->MODER   &= ~((0b11<<0) | (0b11<<14) | (0b11<<28));
	GPIOB->MODER   |=  ((1<<0)    | (1<<14)    | (1<<28));
	GPIOB->OTYPER  &= ~((1<<0)    | (1<<7)     | (1<<14));
	GPIOB->OSPEEDR |=  ((0b11<<0) | (0b11<<14) | (0b11<<28));
	GPIOB->PUPDR   &= ~((0b11<<0) | (0b11<<14) | (0b11<<28));

	//3. Pulsador PC13 como entrada (00) con pull-down (10) -> bits 27:26
	GPIOC->MODER &= ~(0b11<<26);
	GPIOC->PUPDR &= ~(0b11<<26);
	GPIOC->PUPDR |=  (1<<27);

	//4. PE0..PE7 y PF0..PF7 como salida push-pull, muy alta velocidad, sin pull
	GPIOE->MODER   &= ~0xFFFFu;  GPIOE->MODER   |= 0x5555u;   //01 en cada uno de los 8 pines
	GPIOE->OTYPER  &= ~0xFFu;
	GPIOE->OSPEEDR |=  0xFFFFu;
	GPIOE->PUPDR   &= ~0xFFFFu;

	GPIOF->MODER   &= ~0xFFFFu;  GPIOF->MODER   |= 0x5555u;
	GPIOF->OTYPER  &= ~0xFFu;
	GPIOF->OSPEEDR |=  0xFFFFu;
	GPIOF->PUPDR   &= ~0xFFFFu;

	columnas(0x00);   //todo apagado al arrancar
	filas(0x00);

	//5. Interrupción externa en PC13 (línea EXTI13), flanco de subida
	RCC->APB2ENR |= (1<<14);            //Reloj de SYSCFG
	SYSCFG->EXTICR[3] &= ~(0b1111<<4);  //EXTICR4, campo EXTI13 = bits 7:4
	SYSCFG->EXTICR[3] |=  (1<<5);       //0010 = puerto C
	EXTI->IMR  |=  (1<<13);             //Desenmascarar la línea 13
	EXTI->RTSR |=  (1<<13);             //Flanco de subida
	EXTI->FTSR &= ~(1<<13);             //Sin flanco de bajada
	NVIC_EnableIRQ(EXTI15_10_IRQn);     //Habilitar en el controlador de interrupciones

	while(1){
		cambio = 0;
		switch(secuencia){

			case 0: //Secuencia 0 - "Barrido columnas": un bit recorre PE0..PE7 con el piso 0 activo. Led verde
				leds(1,0,0);
				filas(0x01);
				for(uint8_t i = 0; i < 8 && !cambio; i++){
					columnas((uint8_t)(1u<<i));
					esperar(T);
				}
				break;

			case 1: //Secuencia 1 - "Barrido pisos": un bit recorre PF0..PF7 con todas las columnas activas. Led azul
				leds(0,1,0);
				columnas(0xFF);
				for(uint8_t i = 0; i < 8 && !cambio; i++){
					filas((uint8_t)(1u<<i));
					esperar(T);
				}
				break;

			case 2: //Secuencia 2 - "Parpadeo total": todo encendido / todo apagado. Led rojo
				leds(0,0,1);
				columnas(0xFF); filas(0xFF);
				esperar(T);
				columnas(0x00); filas(0x00);
				esperar(T);
				break;

			case 3: //Secuencia 3 - "Contador binario" en las columnas, piso 0 activo. Leds verde + azul
				leds(1,1,0);
				filas(0x01);
				columnas(contador++);
				esperar(T);
				break;

			case 4: //Secuencia 4 - "Ajedrez": columnas y pisos alternados. Los tres leds
				leds(1,1,1);
				columnas(0x55); filas(0xAA);
				esperar(T);
				columnas(0xAA); filas(0x55);
				esperar(T);
				break;

			default:
				secuencia = 0;
				break;
		}
	}
}

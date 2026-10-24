//PruebaLCD.cpp - Prueba SOLO de la pantalla LCD 16x2, sin tocar ningun pin del cubo.
//Javier Solis y William Uriza - Sistemas Embebidos - Universidad ECCI - 2026-II
//Tarjeta: NUCLEO-F746ZG.
//
//Reinicializa la pantalla y reescribe el texto CADA 2 SEGUNDOS, para siempre. Asi no importa en
//que orden se conecten la fuente de 5 V y el cable del computador: apenas la pantalla tenga
//energia, en menos de dos segundos aparece el texto.
//
//El led verde LD1 de la tarjeta cambia en cada ciclo: si parpadea, el programa esta corriendo.
//El B1 cambia el mensaje, para comprobar que la pantalla se refresca de verdad.
//
//Conexion (10 cables):
//  GND -> tierra comun          VDD -> 5 V            VO -> cursor del potenciometro de 10 k
//  RS  -> PA4                   RW  -> tierra         E  -> PA15
//  D4  -> PB12   D5 -> PG14     D6  -> PG15           D7 -> PA6
//  D0..D3 al aire.  Luz de fondo: BLA -> 220 ohm -> 5 V, BLK -> tierra (opcional).
//
//Si despues de esto siguen los cuadros negros y LD1 parpadea, el programa corre y la pantalla no
//entiende: revisar RW a tierra, la tierra comun, y probar VDD a 3.3 V en vez de 5 V.

#include "stm32f7xx.h"

#define MS_ENTRE_INTENTOS  2000
#define MS_ANTIRREBOTE     250

volatile uint32_t ms = 0;
volatile uint8_t  mensaje = 0;
volatile uint32_t tUltimoB1 = 0;

static void Delay_us(uint32_t us){ volatile uint32_t n = us * 3u; while(n--){ __NOP(); } }
static void Delay_ms(uint32_t t){ uint32_t t0 = ms; while((ms - t0) < t){ } }
static void Salida(GPIO_TypeDef* p, uint8_t n){
	p->MODER   &= ~(3u << (2u*n));  p->MODER   |= (1u << (2u*n));
	p->OTYPER  &= ~(1u << n);
	p->OSPEEDR &= ~(3u << (2u*n));  p->OSPEEDR |= (1u << (2u*n));
	p->PUPDR   &= ~(3u << (2u*n));
}
static void PinAlto(GPIO_TypeDef* p, uint8_t n){ p->BSRR = (1u << n); }
static void PinBajo(GPIO_TypeDef* p, uint8_t n){ p->BSRR = (1u << (n + 16u)); }
static void LedsTarjeta(uint8_t n){
	GPIOB->BSRR = ((n & 1u) ? (1u<<0) : (1u<<16)) | ((n & 2u) ? (1u<<7) : (1u<<23)) | ((n & 4u) ? (1u<<14) : (1u<<30));
}

/* LCD 16x2 HD44780, 4 bits: RS PA4, E PA15, D4 PB12, D5 PG14, D6 PG15, D7 PA6 */
static void LCD_Pulso(void){ PinAlto(GPIOA, 15); Delay_us(5); PinBajo(GPIOA, 15); Delay_us(80); }
static void LCD_Nibble(uint8_t n){
	if(n & 1u) PinAlto(GPIOB, 12); else PinBajo(GPIOB, 12);
	if(n & 2u) PinAlto(GPIOG, 14); else PinBajo(GPIOG, 14);
	if(n & 4u) PinAlto(GPIOG, 15); else PinBajo(GPIOG, 15);
	if(n & 8u) PinAlto(GPIOA, 6);  else PinBajo(GPIOA, 6);
	Delay_us(2);
	LCD_Pulso();
}
static void LCD_Comando(uint8_t c){ PinBajo(GPIOA, 4); LCD_Nibble(c >> 4); LCD_Nibble(c & 0x0F); if(c <= 0x03) Delay_ms(3); }
static void LCD_Dato(uint8_t d){ PinAlto(GPIOA, 4); LCD_Nibble(d >> 4); LCD_Nibble(d & 0x0F); }
static void LCD_Iniciar(void){
	Delay_ms(60); PinBajo(GPIOA, 4); PinBajo(GPIOA, 15);
	LCD_Nibble(0x3); Delay_ms(6);
	LCD_Nibble(0x3); Delay_ms(1);
	LCD_Nibble(0x3); Delay_ms(1);
	LCD_Nibble(0x2); Delay_ms(1);
	LCD_Comando(0x28);   //4 bits, 2 lineas, 5x8
	LCD_Comando(0x08);   //pantalla apagada
	LCD_Comando(0x01);   //borrar
	LCD_Comando(0x06);   //avanzar cursor
	LCD_Comando(0x0C);   //pantalla encendida, sin cursor
}
static void LCD_Texto(uint8_t fila, const char* s){
	uint8_t i; LCD_Comando(fila ? 0xC0 : 0x80);
	for(i=0; i<16; i++){ if(*s){ LCD_Dato((uint8_t)*s); s++; } else LCD_Dato(' '); }
}

extern "C" {
void SysTick_Handler(void){ ms++; }
void EXTI15_10_IRQHandler(void){
	EXTI->PR = (1u << 13);
	if((ms - tUltimoB1) >= MS_ANTIRREBOTE){ tUltimoB1 = ms; mensaje = (uint8_t)((mensaje + 1) % 5); }
}
}

static const char* const ARRIBA[5] = { "CUBO 8x8x8 LCD", "1 PLANOS", "2 LLUVIA", "3 CUBO", "4 ESPIRAL" };
static const char* const ABAJO[5]  = { "B1 CAMBIA TEXTO", "SECUENCIA 1", "SECUENCIA 2", "SECUENCIA 3", "SECUENCIA 4" };

int main(){
	uint8_t ciclo = 0;
	RCC->AHB1ENR |= (1u<<0) | (1u<<1) | (1u<<5);          //GPIOA, GPIOB, GPIOG
	Salida(GPIOA, 4); Salida(GPIOA, 15); Salida(GPIOA, 6);
	Salida(GPIOB, 12); Salida(GPIOG, 14); Salida(GPIOG, 15);
	PinBajo(GPIOA, 4); PinBajo(GPIOA, 15);
	Salida(GPIOB, 0); Salida(GPIOB, 7); Salida(GPIOB, 14); LedsTarjeta(0);
	GPIOC->MODER &= ~(3u << 26); GPIOC->PUPDR &= ~(3u << 26); GPIOC->PUPDR |= (2u << 26);
	SysTick_Config(16000u); NVIC_SetPriority(SysTick_IRQn, 1);
	RCC->APB2ENR |= (1u << 14);
	SYSCFG->EXTICR[3] &= ~(0xFu << 4); SYSCFG->EXTICR[3] |= (0x2u << 4);
	EXTI->IMR |= (1u << 13); EXTI->RTSR |= (1u << 13); EXTI->FTSR &= ~(1u << 13); EXTI->PR = (1u << 13);
	NVIC_SetPriority(EXTI15_10_IRQn, 5); NVIC_EnableIRQ(EXTI15_10_IRQn);

	while(1){                                             //reintenta para siempre
		LCD_Iniciar();
		LCD_Texto(0, ARRIBA[mensaje]);
		LCD_Texto(1, ABAJO[mensaje]);
		ciclo ^= 1u;
		LedsTarjeta(ciclo);                               //LD1 parpadea: el programa corre
		Delay_ms(MS_ENTRE_INTENTOS);
	}
}

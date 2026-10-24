//CuboVivo.cpp - Cinco secuencias + SELECTOR DE PISOS EN VIVO, para el cubo 8x8x8.
//Javier Solis y William Uriza - Sistemas Embebidos - Universidad ECCI - 2026-II
//Tarjeta: NUCLEO-F746ZG. Mismo mapa de pines y polaridad que CuboLed8x8x8.cpp (version 2).
//
//COMO SE USA EL B1
//  Pulsacion CORTA  -> cambia de secuencia (1 a 5).
//  Pulsacion LARGA (mas de 1.2 s) -> entra o sale del MODO PISOS.
//     En modo pisos, cada pulsacion corta mueve el par de pisos activos uno hacia arriba y los
//     enciende completos. Los leds de la tarjeta muestran el numero del piso de abajo del par.
//     Cuando veas encendido el par que si funciona, sales con otra pulsacion larga y las cinco
//     secuencias corren sobre esos pisos. No hay que grabar nada nuevo.
//  El numero de pisos del par se cambia con B1 largo dos veces seguidas? No: el par es fijo de 2.
//  Para usar TODOS los pisos, manten el B1 presionado mientras oprimes el reset.
//
//LAS SECUENCIAS SON FIGURAS DE 8x8 EN EL PLANO HORIZONTAL, repetidas en los pisos activos. Con
//pocos pisos no hay altura para figuras verticales, asi que se ven MIRANDO EL CUBO DESDE ARRIBA.
//  1 PLANOS   los pisos activos encienden uno por uno
//  2 ANILLOS  rombos que crecen y se encogen desde el centro
//  3 LETRAS   texto corriendo: FELICES 30 / CUBO 8X8X8 ECCI / JAVIER Y WILLIAM
//  4 ESPIRAL  una cola de 10 leds que recorre un espiral de afuera hacia adentro
//  5 ONDA     franjas diagonales que avanzan

#include "stm32f7xx.h"

#define COLUMNAS_ACTIVAS_EN_BAJO 1
#define PISOS_ACTIVOS_EN_ALTO    1
#define MS_POR_PISO              2
#define NUM_SECUENCIAS           5
#define MS_ANTIRREBOTE           220
#define MS_PULSACION_LARGA       1200

enum { pB = 0, pC = 1, pD = 2, pE = 3, pF = 4, pG = 5 };
static GPIO_TypeDef* const PUERTO[6] = { GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG };

static const uint8_t COL_PUERTO[8][8] = {
	{ pD, pD, pD, pD, pD, pD, pD, pD },
	{ pG, pG, pG, pG, pG, pG, pG, pG },
	{ pC, pC, pC, pC, pC, pC, pC, pB },
	{ pB, pB, pB, pB, pB, pB, pB, pB },
	{ pE, pE, pE, pE, pE, pE, pE, pE },
	{ pE, pE, pE, pE, pE, pE, pE, pE },
	{ pF, pF, pF, pF, pF, pF, pF, pF },
	{ pF, pF, pF, pF, pF, pF, pF, pF }
};
static const uint8_t COL_PIN[8][8] = {
	{  0,  1,  2,  3,  4,  5,  6,  7 },
	{  0,  1,  2,  3,  4,  5,  8,  9 },
	{  6,  7,  8,  9, 10, 11, 12, 15 },
	{  1,  2,  3,  4,  5,  6, 10, 11 },
	{  0,  1,  2,  3,  4,  5,  6,  7 },
	{  8,  9, 10, 11, 12, 13, 14, 15 },
	{  0,  1,  2,  3,  4,  5,  6,  7 },
	{  8,  9, 10, 11, 12, 13, 14, 15 }
};
//Pisos: MOSFET 1 es el piso de ARRIBA (medido el 18/09: la lluvia subia), asi que z0 = abajo = MOSFET 8.
static const uint8_t PISO_PUERTO[8] = { pG, pG, pD, pD, pD, pD, pD, pD };   //z0 (abajo) ... z7 (arriba)
static const uint8_t PISO_PIN[8]    = { 12, 10, 15, 14, 13, 12, 11, 10 };   //PG12 = MOSFET 8 ... PD10 = MOSFET 1

static uint8_t PISOS[8] = { 4, 5, 0, 0, 0, 0, 0, 0 };
static volatile uint8_t nPisos = 2;
static volatile uint8_t base = 4;                 //piso de abajo del par (indice 0..7)

volatile uint8_t  cubo[8][8];
volatile uint32_t ms = 0;
volatile uint8_t  secuencia = 0;
volatile uint8_t  cambio = 0;
volatile uint8_t  modo = 0;                       //0 secuencias, 1 seleccion de pisos
volatile uint32_t tUltimoB1 = 0;
volatile uint8_t  largaHecha = 0;
static uint8_t    ESPIRALX[64], ESPIRALY[64];

static void Delay_us(uint32_t us){ volatile uint32_t n = us * 3u; while(n--){ __NOP(); } }
static void Esperar(uint32_t t){ uint32_t t0 = ms; while((ms - t0) < t){ } }
static void Salida(GPIO_TypeDef* p, uint8_t n){
	p->MODER   &= ~(3u << (2u*n));  p->MODER   |= (1u << (2u*n));
	p->OTYPER  &= ~(1u << n);
	p->OSPEEDR &= ~(3u << (2u*n));  p->OSPEEDR |= (1u << (2u*n));
	p->PUPDR   &= ~(3u << (2u*n));
}
static void PinAlto(GPIO_TypeDef* p, uint8_t n){ p->BSRR = (1u << n); }
static void PinBajo(GPIO_TypeDef* p, uint8_t n){ p->BSRR = (1u << (n + 16u)); }
static void Columna(uint8_t x, uint8_t y, uint8_t e){
	GPIO_TypeDef* p = PUERTO[COL_PUERTO[y][x]]; uint8_t n = COL_PIN[y][x];
#if COLUMNAS_ACTIVAS_EN_BAJO
	if(e) PinBajo(p, n); else PinAlto(p, n);
#else
	if(e) PinAlto(p, n); else PinBajo(p, n);
#endif
}
static void Piso(uint8_t z, uint8_t e){
	GPIO_TypeDef* p = PUERTO[PISO_PUERTO[z]]; uint8_t n = PISO_PIN[z];
#if PISOS_ACTIVOS_EN_ALTO
	if(e) PinAlto(p, n); else PinBajo(p, n);
#else
	if(e) PinBajo(p, n); else PinAlto(p, n);
#endif
}
static void TodoApagado(void){
	uint8_t x, y, z;
	for(y=0; y<8; y++) for(x=0; x<8; x++) Columna(x, y, 0);
	for(z=0; z<8; z++) Piso(z, 0);
}
static void LedsTarjeta(uint8_t n){
	GPIOB->BSRR = ((n & 1u) ? (1u<<0) : (1u<<16)) | ((n & 2u) ? (1u<<7) : (1u<<23)) | ((n & 4u) ? (1u<<14) : (1u<<30));
}
static void Limpiar(void){ uint8_t z, y; for(z=0; z<8; z++) for(y=0; y<8; y++) cubo[z][y] = 0; }
static void PuntoTodos(int x, int y){        //el mismo punto en todos los pisos activos
	uint8_t k;
	if(x < 0 || x > 7 || y < 0 || y > 7) return;
	for(k = 0; k < nPisos; k++) cubo[PISOS[k]][y] = (uint8_t)(cubo[PISOS[k]][y] | (1u << x));
}
static void ArmarPisos(void){
	uint8_t i;
	for(i = 0; i < 8; i++) PISOS[i] = 0;
	PISOS[0] = base;
	PISOS[1] = (uint8_t)((base + 1u) & 7u);
	nPisos = 2;
}

/* ---- LCD ---- */
static void LCD_Pulso(void){ PinAlto(GPIOA, 15); Delay_us(5); PinBajo(GPIOA, 15); Delay_us(80); }
static void LCD_Nibble(uint8_t n){
	if(n & 1u) PinAlto(GPIOB, 12); else PinBajo(GPIOB, 12);
	if(n & 2u) PinAlto(GPIOG, 14); else PinBajo(GPIOG, 14);
	if(n & 4u) PinAlto(GPIOG, 15); else PinBajo(GPIOG, 15);
	if(n & 8u) PinAlto(GPIOA, 6);  else PinBajo(GPIOA, 6);
	Delay_us(2); LCD_Pulso();
}
static void LCD_Comando(uint8_t c){ PinBajo(GPIOA,4); LCD_Nibble(c>>4); LCD_Nibble(c&0x0F); if(c<=0x03) Esperar(3); }
static void LCD_Dato(uint8_t d){ PinAlto(GPIOA,4); LCD_Nibble(d>>4); LCD_Nibble(d&0x0F); }
static void LCD_Iniciar(void){
	Esperar(60); PinBajo(GPIOA,4); PinBajo(GPIOA,15);
	LCD_Nibble(0x3); Esperar(6); LCD_Nibble(0x3); Esperar(2); LCD_Nibble(0x3); Esperar(2); LCD_Nibble(0x2); Esperar(2);
	LCD_Comando(0x28); LCD_Comando(0x08); LCD_Comando(0x01); LCD_Comando(0x06); LCD_Comando(0x0C);
}
static void LCD_Texto(uint8_t fila, const char* s){
	uint8_t i; LCD_Comando(fila ? 0xC0 : 0x80);
	for(i=0; i<16; i++){ if(*s){ LCD_Dato((uint8_t)*s); s++; } else LCD_Dato(' '); }
}

/* ---- interrupciones ---- */
extern "C" {
void SysTick_Handler(void){
	static uint8_t cuenta = 0, idx = 0;
	uint32_t poner[6] = {0,0,0,0,0,0}, quitar[6] = {0,0,0,0,0,0};
	uint8_t x, y, p, n, fila, z;
	ms++;
	if(++cuenta < MS_POR_PISO) return;
	cuenta = 0;
	Piso(PISOS[idx], 0);
	idx++; if(idx >= nPisos) idx = 0;
	z = PISOS[idx];
	for(y=0; y<8; y++){
		fila = cubo[z][y];
		for(x=0; x<8; x++){
			p = COL_PUERTO[y][x]; n = COL_PIN[y][x];
#if COLUMNAS_ACTIVAS_EN_BAJO
			if((fila >> x) & 1u) quitar[p] |= (1u << n); else poner[p] |= (1u << n);
#else
			if((fila >> x) & 1u) poner[p]  |= (1u << n); else quitar[p] |= (1u << n);
#endif
		}
	}
	for(p=0; p<6; p++) PUERTO[p]->BSRR = poner[p] | (quitar[p] << 16);
	Piso(z, 1);
}
void EXTI15_10_IRQHandler(void){
	EXTI->PR = (1u << 13);
	if((ms - tUltimoB1) >= MS_ANTIRREBOTE){
		tUltimoB1 = ms;
		largaHecha = 0;
		if(modo == 0) secuencia = (uint8_t)((secuencia + 1u) % NUM_SECUENCIAS);
		else          base = (uint8_t)((base + 1u) & 7u);
		cambio = 1;
	}
}
}

/* ---- texto ---- */
static const char MENSAJE[] = "FELICES 30   CUBO 8X8X8 ECCI   JAVIER Y WILLIAM   ";
static const uint8_t FUENTE[21][5] = {
	{0x00,0x00,0x00,0x00,0x00},{0x3E,0x51,0x49,0x45,0x3E},{0x22,0x41,0x49,0x49,0x36},
	{0x36,0x49,0x49,0x49,0x36},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
	{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
	{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x40,0x40,0x40,0x40},
	{0x7F,0x02,0x0C,0x02,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x19,0x29,0x46},
	{0x46,0x49,0x49,0x49,0x31},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
	{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03}
};
static const char INDICE[] = " 038ABCEFIJLMORSUVWXY";
static uint8_t Glifo(char c){ uint8_t i; for(i=0; INDICE[i]; i++) if(INDICE[i]==c) return i; return 0; }
static uint8_t ColumnaTexto(uint32_t col){
	uint32_t total = (uint32_t)(sizeof(MENSAJE) - 1u) * 6u;
	uint32_t c = col % total, iChar = c / 6u, iCol = c % 6u;
	if(iCol == 5u) return 0;
	return FUENTE[Glifo(MENSAJE[iChar])][iCol];
}

/* ---- secuencias ---- */
static void Planos(uint32_t paso){
	uint8_t k, y;
	Limpiar();
	k = (uint8_t)(paso % nPisos);
	for(y=0; y<8; y++) cubo[PISOS[k]][y] = 0xFF;
}
static void Anillos(uint32_t paso){
	int x, y, D, R;
	uint32_t p = paso % 12u;
	R = 2 + 2 * (int)((p <= 6u) ? p : (12u - p));        //2,4,...,14,...,4
	Limpiar();
	for(y=0; y<8; y++){
		for(x=0; x<8; x++){
			D = ((2*x-7) < 0 ? -(2*x-7) : (2*x-7)) + ((2*y-7) < 0 ? -(2*y-7) : (2*y-7));
			if(D == R) PuntoTodos(x, y);
		}
	}
}
static void Letras(uint32_t paso){
	int x, y; uint8_t patron;
	Limpiar();
	for(x=0; x<8; x++){
		patron = ColumnaTexto(paso + (uint32_t)x);
		for(y=0; y<8; y++) if((patron >> y) & 1u) PuntoTodos(x, 7 - y);
	}
}
static void Espiral(uint32_t paso){
	int i, j;
	Limpiar();
	for(i=0; i<10; i++){
		j = (int)((paso + (uint32_t)i) % 64u);
		PuntoTodos((int)ESPIRALX[j], (int)ESPIRALY[j]);
	}
}
static void Onda(uint32_t paso){
	int x, y;
	Limpiar();
	for(y=0; y<8; y++) for(x=0; x<8; x++) if((((uint32_t)(x + y) + paso) & 7u) < 3u) PuntoTodos(x, y);
}
static void ArmarEspiral(void){
	int x0=0, x1=7, y0=0, y1=7, x, y, n=0;
	while(x0 <= x1 && y0 <= y1){
		for(x=x0; x<=x1; x++){ ESPIRALX[n]=(uint8_t)x; ESPIRALY[n]=(uint8_t)y0; n++; }
		y0++;
		for(y=y0; y<=y1; y++){ ESPIRALX[n]=(uint8_t)x1; ESPIRALY[n]=(uint8_t)y; n++; }
		x1--;
		if(y0 <= y1){ for(x=x1; x>=x0; x--){ ESPIRALX[n]=(uint8_t)x; ESPIRALY[n]=(uint8_t)y1; n++; } y1--; }
		if(x0 <= x1){ for(y=y1; y>=y0; y--){ ESPIRALX[n]=(uint8_t)x0; ESPIRALY[n]=(uint8_t)y; n++; } x0++; }
	}
}

static const char* const NOMBRE[NUM_SECUENCIAS] = { "1 PLANOS", "2 ANILLOS", "3 LETRAS", "4 ESPIRAL", "5 ONDA" };
static const uint16_t CUADRO[NUM_SECUENCIAS]    = { 400, 130, 130, 70, 140 };

static void ConfigurarPines(void){
	uint8_t x, y, z;
	RCC->AHB1ENR |= (1u<<0)|(1u<<1)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<6);
	for(y=0; y<8; y++) for(x=0; x<8; x++) Salida(PUERTO[COL_PUERTO[y][x]], COL_PIN[y][x]);
	for(z=0; z<8; z++) Salida(PUERTO[PISO_PUERTO[z]], PISO_PIN[z]);
	TodoApagado();
	Salida(GPIOA,4); Salida(GPIOA,15); Salida(GPIOA,6); Salida(GPIOB,12); Salida(GPIOG,14); Salida(GPIOG,15);
	PinBajo(GPIOA,4); PinBajo(GPIOA,15);
	Salida(GPIOB,0); Salida(GPIOB,7); Salida(GPIOB,14); LedsTarjeta(0);
	GPIOC->MODER &= ~(3u << 26); GPIOC->PUPDR &= ~(3u << 26); GPIOC->PUPDR |= (2u << 26);
}

static void MostrarPisos(void){
	char t[17] = "PISOS n y m     ";
	t[6] = (char)('1' + PISOS[0]);
	t[10] = (char)('1' + PISOS[1]);
	LCD_Texto(0, "MODO PISOS");
	LCD_Texto(1, t);
}

int main(){
	uint32_t paso = 0, tLCD = 0, tPresionado = 0;
	uint8_t i, todos = 0, actual;
	ConfigurarPines();
	ArmarEspiral();
	for(i=0; i<200; i++) __NOP();
	if(GPIOC->IDR & (1u << 13)) todos = 1;
	Limpiar();
	if(todos){ for(i=0; i<8; i++) PISOS[i] = i; nPisos = 8; }
	else ArmarPisos();

	SysTick_Config(16000u); NVIC_SetPriority(SysTick_IRQn, 1);
	RCC->APB2ENR |= (1u << 14);
	SYSCFG->EXTICR[3] &= ~(0xFu << 4); SYSCFG->EXTICR[3] |= (0x2u << 4);
	EXTI->IMR |= (1u << 13); EXTI->RTSR |= (1u << 13); EXTI->FTSR &= ~(1u << 13); EXTI->PR = (1u << 13);
	NVIC_SetPriority(EXTI15_10_IRQn, 5); NVIC_EnableIRQ(EXTI15_10_IRQn);

	LCD_Iniciar(); LCD_Texto(0, "CUBO 8x8x8 ECCI"); LCD_Texto(1, "JAVIER Y WILLIAM");
	for(i=0; i<nPisos; i++){ uint8_t y; for(y=0; y<8; y++) cubo[PISOS[i]][y] = 0xFF; }
	Esperar(1200);
	cambio = 1;

	while(1){
		if(GPIOC->IDR & (1u << 13)){                      //B1 sostenido
			if(tPresionado == 0) tPresionado = ms;
			else if(!largaHecha && (ms - tPresionado) >= MS_PULSACION_LARGA){
				largaHecha = 1;
				if(modo == 0){                            //entrar al modo pisos
					modo = 1;
					secuencia = (uint8_t)((secuencia + NUM_SECUENCIAS - 1u) % NUM_SECUENCIAS);
					if(!todos) ArmarPisos();
					MostrarPisos();
				}else{                                    //salir del modo pisos
					modo = 0;
					base = (uint8_t)((base + 7u) & 7u);
					ArmarPisos();
					LCD_Texto(0, "CUBO 8x8x8 ECCI");
					LCD_Texto(1, NOMBRE[secuencia]);
				}
				cambio = 1;
			}
		}else tPresionado = 0;

		if(cambio){
			cambio = 0; paso = 0;
			if(modo){
				if(!todos) ArmarPisos();
				MostrarPisos();
				LedsTarjeta((uint8_t)((PISOS[0] + 1u) & 7u));
			}else{
				LedsTarjeta((uint8_t)(secuencia + 1u));
				LCD_Texto(0, "CUBO 8x8x8 ECCI");
				LCD_Texto(1, NOMBRE[secuencia]);
			}
		}

		if(modo){                                          //en modo pisos: el par encendido, fijo
			uint8_t k, y;
			Limpiar();
			for(k=0; k<nPisos; k++) for(y=0; y<8; y++) cubo[PISOS[k]][y] = 0xFF;
			Esperar(60);
			continue;
		}

		actual = secuencia;
		switch(actual){
			case 0: Planos(paso);  break;
			case 1: Anillos(paso); break;
			case 2: Letras(paso);  break;
			case 3: Espiral(paso); break;
			default: Onda(paso);   break;
		}
		paso++;
		{ uint32_t t0 = ms; while((ms - t0) < CUADRO[actual] && !cambio){ } }
		if(ms < 20000u && (ms - tLCD) >= 2500u){
			tLCD = ms;
			LCD_Iniciar(); LCD_Texto(0, "CUBO 8x8x8 ECCI"); LCD_Texto(1, NOMBRE[secuencia]);
		}
	}
}

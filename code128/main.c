#define F_CPU 8000000UL //частота работы МК
#define BAUDRATE 9600L	//скорость передачи данных по usart

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LCD_DDR		DDRF				//порт дисплея
#define LCD_PORT	PORTF				//порт дисплея
#define E1			PORTF |= 0b00001000	//установка линии E в 1
#define E0			PORTF &= 0b11110111	//установка линии E в 0
#define RS1			PORTF |= 0b00000100 //установка линии RS в 1 (данные)
#define RS0			PORTF &= 0b11111011	//установка линии RS в 0 (команда)

#define B_DDR	DDRC
#define B_PORT	PORTC
#define B_PIN	PINC
#define BUT		0

char	data0[32];			//массив символов для приема по usart0
char	data1[32];			//массив символов для приема по usart1
char	buffer[32];			//массив для вывода символов на дисплей
char	queue[100][3] = {0};//массив для хранения очереди
uint8_t countCoupons = 0;	//кол-во талонов
uint8_t currentCoupon = 0;	//текущий талон
const char commandGetCount[] = "get count\r";

static int LCD_putchar(char c, FILE *stream);
static int USART0_putchar(char c, FILE *stream);
static int USART1_putchar(char c, FILE *stream);
static FILE lcd = FDEV_SETUP_STREAM(LCD_putchar, NULL, _FDEV_SETUP_WRITE);
static FILE usart0 = FDEV_SETUP_STREAM(USART0_putchar, NULL, _FDEV_SETUP_WRITE);
static FILE usart1 = FDEV_SETUP_STREAM(USART1_putchar, NULL, _FDEV_SETUP_WRITE);

//отправка полбайта в дисплей
void LCD_sendhalfbyte(unsigned char c)
{
	c <<= 4;
	E1;						//включение линии Е
	_delay_us(50);
	LCD_PORT &= 0b00001111; //стираем информацию на входах DB4-DB7, остальное не трогаем
	LCD_PORT |= c;
	E0;						//выключение линии Е
	_delay_us(50);
}

//отправка байта в дисплей
void LCD_sendbyte(unsigned char c, unsigned char mode)
{
	if (mode == 0)
	{
		RS0;
	}
	else
	{
		RS1;
	}
	
	unsigned char hc = 0;
	hc = c >> 4;
	
	LCD_sendhalfbyte(hc);
	LCD_sendhalfbyte(c);
}

//отправка символа в дисплей
void LCD_sendchar(unsigned char c)
{
	LCD_sendbyte(c, 1);
}

//отправка символа по usart
static int LCD_putchar(char c, FILE *stream)
{
	if (c == '\n')
	{
		LCD_putchar('\r', stream);
	}
	
	LCD_sendchar(c);
	
	return 0;
}

//установка координат каретки на дисплее
void LCD_setpos(unsigned char x, unsigned y)
{
	switch(y)
	{
		case 0:
		LCD_sendbyte(x | 0x80, 0);
		break;
		case 1:
		LCD_sendbyte((0x40 + x) | 0x80, 0);
		break;
		case 2:
		LCD_sendbyte((0x14 + x) | 0x80, 0);
		break;
		case 3:
		LCD_sendbyte((0x54 + x) | 0x80, 0);
		break;
	}
}

//инициализация дисплея
void LCD_init(void)
{
	stdout = &lcd;
	
	_delay_ms(15);
	LCD_sendhalfbyte(0b00000011);
	_delay_ms(4);
	LCD_sendhalfbyte(0b00000011);
	_delay_us(100);
	LCD_sendhalfbyte(0b00000011);
	_delay_ms(1);
	LCD_sendhalfbyte(0b00000010);
	_delay_ms(1);
	LCD_sendbyte(0b00101000, 0);
	_delay_ms(1);
	LCD_sendbyte(0b00001100, 0);
	_delay_ms(1);
	LCD_sendbyte(0b00000110, 0);
	_delay_ms(1);
}

//отчистка дисплея
void LCD_clear(void)
{
	LCD_sendbyte(0b00000001, 0);
	_delay_us(1500);
}

//Инициализация USART0
void USART0_init()
{
	stderr = &usart0;
	
	UBRR0L=51;
	UCSR0B=0x18;
	UCSR0C=0x06;
	UCSR0B |= (1<<RXCIE);
}

//Инициализация USART1
void USART1_init()
{
	stdin = &usart1;
	
	UBRR1L=51;
	UCSR1B=0x18;
	UCSR1C=0x06;
	UCSR1B |= (1<<RXCIE);
}

//вывод символа по usart0
static int USART0_putchar(char c, FILE *stream)
{
	if (c == '\n')
	USART0_putchar('\r', stream);
	while(!(UCSR0A & (1<<UDRE)));
	UDR0 = c;
	return 0;
}

//вывод символа по usart1
static int USART1_putchar(char c, FILE *stream)
{
	if (c == '\n')
	USART1_putchar('\r', stream);
	while(!(UCSR1A & (1<<UDRE)));
	UDR1 = c;
	return 0;
}

//прием данных по usart0
void USART0_receiving()
{
	memset(data0, 0, sizeof data0);
	
	int i = 0;
	do
	{
		while(!(UCSR0A&(1<<RXC)));
		data0[i] = UDR0;
		i++;
		
	} while (data0[i-1] != '\r');
}

//прием данных по usart0
void USART1_receiving()
{
	memset(data1, 0, sizeof data1);
	
	int i = 0;
	do
	{
		while(!(UCSR1A&(1<<RXC)));
		data1[i] = UDR1;
		i++;
		
	} while (data1[i-1] != '\r');
}

//прерывание usart0
ISR(USART0_RX_vect)
{
	USART0_receiving();

	if (strcmp(data0, commandGetCount) == 0)
	{
		fprintf(stderr, "%d%d\r", countCoupons / 10, countCoupons % 10);
	}
	else
	{
		queue[countCoupons][0] = data0[0];
		queue[countCoupons][1] = data0[1];
		queue[countCoupons][2] = data0[2];
		
		countCoupons++;
		
		if (countCoupons == 99)
		{
			countCoupons = 0;
		}
	}
}

//прерывание usart1
ISR(USART1_RX_vect)
{
	USART1_receiving();

	if (strcmp(data1, commandGetCount) == 0)
	{
		fprintf(stdin, "%d%d\r", countCoupons / 10, countCoupons % 10);
	}
	else
	{
		queue[countCoupons][0] = data1[0];
		queue[countCoupons][1] = data1[1];
		queue[countCoupons][2] = data1[2];
		
		countCoupons++;
		
		if (countCoupons == 99)
		{
			countCoupons = 0;
		}
	}
}

//отображение очереди на дисплее
void show_queue()
{
	LCD_setpos(0, 0);
	printf("Current: %c%c%c", queue[currentCoupon][0], queue[currentCoupon][1], queue[currentCoupon][2]);
	
	for (int i = 1; i < 4; i++)
	{
		LCD_setpos(0, i);
		printf("%10c%c%c", queue[currentCoupon + i][0], queue[currentCoupon + i][1], queue[currentCoupon + i][2]);
	}
}

//нажатие кнопки
void button()
{
	if (!(B_PIN&(0x01<<BUT)))
	{
		while (!(B_PIN&(0x01<<BUT)));
		currentCoupon++;
		if (currentCoupon == 99)
		{
			currentCoupon = 0;
		}
	}
}

int main(void)
{
	USART0_init();
	USART1_init();
	
	LCD_DDR = 0b11111100;
	LCD_PORT = 0x00;
	LCD_init();
	
	B_DDR = 0x00;
	B_PORT = 0xff;
	
	sei();

    while (1) 
    {
		button();
		show_queue();
    }
}


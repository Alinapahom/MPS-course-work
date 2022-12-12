#define F_CPU 8000000UL //частота работы ЊЉК
#define BAUDRATE 9600L	//скорость передачи данных по usart

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LCD_DDR		DDRA				//порт дисплеЯ
#define LCD_PORT	PORTA				//порт дисплеЯ
#define E1			PORTA |= 0b00001000	//установка линии E в 1
#define E0			PORTA &= 0b11110111	//установка линии E в 0
#define RS1			PORTA |= 0b00000100     //установка линии RS в 1 (данные)
#define RS0			PORTA &= 0b11111011	//установка линии RS в 0 (команда)

#define B_DDR	DDRC
#define B_PORT	PORTC
#define B_PIN	PINC
#define BUT		0

char	data[32];		//массив символов длЯ приема по usart
char	buffer[32];		//массив длЯ вывода символов на дисплей
uint8_t	count;			//полученное кол-во талонов

// длЯ того, чтобы работала функциЯ fprintf
static int LCD_putchar(char c, FILE *stream);  
static int USART_putchar(char c, FILE *stream); // принимает символ char и указатель на поток
static FILE lcd = FDEV_SETUP_STREAM(LCD_putchar, NULL, _FDEV_SETUP_WRITE); //настройка lcd, чтобы данные передавал в поток stdout //Ѓиблиотека avrlibc имеет встроенное средство длЯ того, чтобы свЯзать функцию вывода с выходными данными функции printf:
static FILE usart = FDEV_SETUP_STREAM(USART_putchar, NULL, _FDEV_SETUP_WRITE); //настройка usart, чтобы данные передавал в поток stderr (длЯ вывода ошибок)
//ЏредоставлЯетсЯ макрос fdev_setup_stream(), чтобы подготовить предоставленный пользователем буфер FILE длЯ работы с stdio
//так что все инициализации данных выполнЯетсЯ кодом start-up подсистемы Языка C

//отправка полбайта в дисплей
void LCD_sendhalfbyte(unsigned char c)
{
	c <<= 4;  // так как подаЮм на старшие биты, то сдвигаем на 4 
	E1;			//установка линии E в 1 //включение линии Е
	_delay_us(50);
	LCD_PORT &= 0b00001111; //стираем информацию на входах DB4-DB7, остальное не трогаем
	LCD_PORT |= c;
	E0;			//чтобы дисплей понЯл, что мы ему прислали что-то //выключение линии Е
	_delay_us(50); // ждем, чтобы биты точно записались 
}

//отправка байта в дисплей
void LCD_sendbyte(unsigned char c, unsigned char mode) //команда и данные передаем 
{
	if (mode == 0) //если команду
	{
		RS0; //установка линии RS в 0 (команда)
	}
	else
	{
		RS1; //установка линии RS в 1 (данные)
	}
	
	unsigned char hc = 0; //сначала старшую часть, потом младшую
	hc = c >> 4; //сдвигаем c влево на 4, старшую часть уводим в младшую
	
	LCD_sendhalfbyte(hc); //передаем старшую сначала
	LCD_sendhalfbyte(c); //потом младшую
}

//отправка символа в дисплей
void LCD_sendchar(unsigned char c)
{
	LCD_sendbyte(c, 1); //передача данных
}

//отправка символа по usart
static int LCD_putchar(char c, FILE *stream)
{
	if (c == '\n') //символ перевода строки 
	{
		LCD_putchar('\r', stream); //символ возврата коретки(возвращение на начало строки)
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
		LCD_sendbyte(x | 0x80, 0); //8 бит = 1, устанавливаем адрес, перемещаем указатель по адресу
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

//инициализациЯ дисплеЯ
void LCD_init(void)
{
	stdout = &lcd; // приравниваем потоку stdout указатель на stream lcd 
	
	_delay_ms(15); //перед тем как писать, подождать 15 мс
	LCD_sendhalfbyte(0b00000011);
	_delay_ms(4);
	LCD_sendhalfbyte(0b00000011);
	_delay_us(100); //микросекунд
	LCD_sendhalfbyte(0b00000011);
	_delay_ms(1);
	LCD_sendhalfbyte(0b00000010);
	_delay_ms(1);
	LCD_sendbyte(0b00101000, 0); //включаем 4бит режим, используем 2 линии (N=1)
	_delay_ms(1);
	LCD_sendbyte(0b00001100, 0); //включаем изображение (D=1), курсоры никакие не включаем
	_delay_ms(1);
	LCD_sendbyte(0b00000110, 0); //заставлЯем курсор двигатьсЯ справа налево
	_delay_ms(1);
}

//отчистка дисплеЯ
void LCD_clear(void)
{
	LCD_sendbyte(0b00000001, 0);
	_delay_us(1500);
}

//инициализациЯ usart 
void USART_init()
{
	stderr = &usart; 
	
	UBRRL = F_CPU / BAUDRATE / 16 - 1;	//8 000 000 / 9600 / 16 - 1 = 51 // оставлЯем только младшую часть(отсекаем ненужное-старшую часть), записываем 51 в регистр UBRR (скорость 9600 Ѓод при f=8 Њѓц)
	UCSRB = (1<<TXEN)|(1<<RXEN);		//разрешение приема и передачи
	UCSRC =	(1<<URSEL)|(3<<UCSZ0);		//обращаемсЯ именно к регистру UCSRC (URSEL =1),асинхронный режим (UNSEL=0),без контролЯ четности(UPM1 =0 и UPM0 =0) широта посылки = 8 бит
	UCSRB |= (1<<RXCIE);				//разрешение прерываниЯ при приЮме 
}

//отправка символа по usart
static int USART_putchar(char c, FILE *stream)
{
	if (c == '\n')
	{
		USART_putchar('\r', stream);
	}
	
	while(!(UCSRA & (1<<UDRE))); //отследим бит UDRE (длЯ передачи данных) - убедимсЯ что он свободен (не должен быть в единице)
	UDR = c; //начнем передавать данные, после того как убедимсЯ, что буфер пуст
	
	return 0;
}

//прием данных по usart
void USART_receiving()
{
	memset(data, 0, sizeof data); //массив data заполнЯем 0 длЯ того, чтобы отчистить массив от прошлых передач на всЯкий случай, чтобы никакого мусора не попало при новой передачи
	
	int i = 0;
	do // ждем, когда приходит новый символ, если символ пришел, то 
	{
		while(!(UCSRA&(1<<RXC)));
		data[i] = UDR; // запись нового символа (каждый байт) data[i] в UDR( регистр длЯ хранениЯ данных, которые приходЯт по usart)
		i++;
		
	} while (data[i-1] != '\r'); // записываем данные до тех пор, пока data не равна концу строки
}


//прерывание usart по приему(если приходит какой-то байт на шину USART, то вызываетсЯ это обработчик прерываниЯ)
ISR(USART_RXC_vect)
{
	USART_receiving();//прием по usart (в массив data записываютсЯ все символы, которые были переданы)
	
	count = ((data[0]&0b00001111) * 10) + (data[1]&0b00001111); //первый элемент массива data[0] логическим € умножаем на битовую конструкцию, чтобы перевести символы с кодировки ASCII в обычное число в двоичной системе счислениЯ, потом умножаем его на 10, потому что это символ десЯтков 
„альше аналогично добавлЯем второе число 
	
	fprintf(stderr, "A%d\r", count + 1); //данные в терминал( пишет A и значение count)
	
	LCD_clear(); // очистка дисплеЯ
	LCD_setpos(0, 0); // установка позиции в координаты 00
	fprintf(stdout, "A%d", count + 1); // выводим на дисплей то же самое, что и в терминал
}


//нажатие кнопки
void button()
{
	if (!(B_PIN&(0x01<<BUT))) //спрашиваем было ли изменение уровнЯ сигнала на контакте, на котором кнопка, если было то проваливаемсЯ в while
	{
		while (!(B_PIN&(0x01<<BUT))); //пока кнопка зажата 
		fprintf(stderr, "get count\r"); //кнопка отпустилась 
	}
}

int main(void)
{
//инициализациЯ портов длЯ дисплеЯ 
	LCD_DDR = 0b11111100; //последние 6 контактов настраиваютсЯ на выход, потому что данные будем отправлЯть на дисплей, первые 2 контакта не трогаем 
	LCD_PORT = 0x00; //контакты настроены на выход и мы выдаем пока что нули только
	

	B_DDR = 0x00; // порт кнопок настраиваем на вход 
	B_PORT = 0xff; // подтЯгиваем резистор к питанию 
	
	USART_init();
	LCD_init();
	
	sei(); //разрешает глобальное прерывание
	
    while (1)  // бесконечный while
    {
		 button();
    }
}


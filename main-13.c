//In this project we created a video game controller that interacted with a simple networked game.We used the Wi-Fi module to communicate with the game server using certain set of protocol
//The player controls consisted of an analog joystick and a single button which was used to fire.
//We used analog to digital conversion to identify the joystick position.We received the game name,handle name,score and ammunition left from the server and displayed these on the LCD

//header files
#include "defines.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <util/delay.h>
#include <stdbool.h>
#include "hd44780.h"
#include "lcd.h"

FILE lcd_str=FDEV_SETUP_STREAM(lcd_putchar, NULL, _FDEV_SETUP_WRITE);

#define F_CPU 1843200UL
#define BAUR 115200
#define BRC ((F_CPU/16/BAUR)-1)
#define buffersize 50
#define fire_BUTTON 3
#define BUTTON_GROUP (PINC&((1<<fire_BUTTON)))
#define unfire ((1<<fire_BUTTON))

char command55[] = "AT+CIPSEND=6\r\n";//sends 6 bytes of data
char command5[] = "AT+CIPSEND=12\r\n";//sends 12 bytes of data
char received_string[buffersize];
int fire = 0;

//Function Prototypes
void initialization ();
void uart_transmit (char data);
void uart_transmit_string (char *input);
unsigned char UART_receive (void);
int uart_recieve_string();
int BUTTON_PRESSED(void);
int BUTTON_OPERATIONS(void);
void ADC_initialization(void);
int ADC_read(int ADC_Channel);
void ADC_to_server(int c);
void wifi_server_connection(void);

int main (void){
	lcd_init();//Sets up the LCD controller. 
	stderr = &lcd_str;
	hd44780_wait_ready(1);//Waits till the LCD is not busy
	initialization();//Initializes UART and the fire button
	ADC_initialization();
	wifi_server_connection();//Connects to the wifi and server.
	_delay_ms(5000);
	while(1){
		fire = BUTTON_OPERATIONS();
		ADC_to_server(fire);
		_delay_ms(100);
	}
}

void initialization(){//Initializes UART and the button
	//UART INITIALIZATION
	UBRR0H = (BRC>>8);
	UBRR0L = BRC;
	
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
	
	//Button Initialization
	PORTC |= (1<<fire_BUTTON);//pull up low
	DDRC &= ~((1<<fire_BUTTON));//set as input
}

void uart_transmit (char data)//transmits one character to the server
{
	/* Wait for empty transmit buffer */
	while (!( UCSR0A & (1<<UDRE0))) ;
	
	UDR0 = data;//If empty sends a character
}

void uart_transmit_string (char *input)//transmits string to the server.
{
	int ctr=0;
	
	
	while(1){//transmits the entire string one character at a time
		if(input[ctr] == 0){//If it reaches the end it comes out of the loop
			break;
			} else {
			
			uart_transmit(input[ctr]);
			ctr++;
		}
	}
	
	return;
}

unsigned char UART_receive(void) {//receives one character from server
	
	 //Wait for data to be received
	while ( !(UCSR0A & (1<<RXC0))){}
	return UDR0;
}

int uart_recieve_string(char* received_string){	// receive strings from server
	int counter = 0;
	char received_char = UART_receive();
	while((received_char != '\n') && (buffersize> counter))//breaks out of the loop if \n is received or it exceeds max buffer size  
	{
		received_string[counter] = received_char;
		counter++;
		received_char = UART_receive();
		
	}
	received_string[counter] = 0;//adds 0 to the end of the string.
	return counter;
}

int BUTTON_PRESSED(){//Identifies if the button is being pressed
	int init_press=BUTTON_GROUP;
	int press_counter=0;
	while (press_counter<3)//checks 3 times whether the button is being pressed
	{
		_delay_us(100);
		if (init_press==BUTTON_GROUP){
			press_counter++;
		}
		else{
			press_counter=0;
			init_press=BUTTON_GROUP;
		}
		_delay_us(100);
	}
	int flag=1;
	while (flag)
	{
		if (BUTTON_GROUP==unfire) //checks if the button has been released
		{
			flag = 0;
		}
	}
	return init_press;
}

int BUTTON_OPERATIONS(){// returns the state of the button
	int button=BUTTON_PRESSED();
	int a = 0;
	if (button==unfire){
		a = 0;//returns 0 if it not pressed
	}
	else{
		a = 1;//returns 1 if it is pressed
	}
	return a;
}
	
void wifi_server_connection(){ //connects the MCU to the Wi-Fi and the game server
	int wifi_connection_check =0;
	
	while(wifi_connection_check ==0){

		char command1[] = "AT+CWMODE=1\r\n";//switch to Wi-Fi client mode
		uart_transmit_string(command1);
		_delay_ms(1000);
		char command2[] = "AT+CWDHCP=1,1\r\n"; //enable DHCP
		uart_transmit_string(command2);
		_delay_ms(1000);
		char command3[] = "AT+CWJAP=\"ece312\",\"ece312ece312\"\r\n"; //connect to Wi-Fi "ece312"
		uart_transmit_string(command3);
		_delay_ms(10000);
		char command4[] = "AT+CIPSTART=\"TCP\",\"192.168.1.1\",40000\r\n";//game server connection AT command.
		uart_transmit_string(command4);
		wifi_connection_check =1;
		int i =0;
		while (i == 0){//checks whether it is connected to the game server
			
			if (strncmp(received_string,"+IPD,18", 7)==00){
				fprintf(stderr,received_string + 8);//displays the game name on the LCD
				i =1;
				break;//Breaks out of the loop if its connected to the server
			}
			uart_recieve_string(received_string);
		}
	}

}

void ADC_initialization(){//Analog to Digital Converter initialization
	ADMUX |= (1<<REFS0); //AVCC with external capacitor at AREF pin
	ADCSRA |= (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);//enables analog to digital conversion and sets the division factor between the system clock frequency and the input clock to the ADC to 128. 
	DIDR0 |=(1<<4)|(1<<5);//the digital input buffer on ADC4 and ADC5 is disabled . It reduces power consumption in the digital input buffer.
}

int ADC_read(int ADC_Channel){//This function gets an analog value and converts it to digital
	ADMUX = (ADMUX & 0xF0) | (ADC_Channel & 0x0F);//selects the channel
	ADCSRA |= (1<<ADSC);//In Single Conversion mode,this bit is written to one to start each conversion. 
	int ADC = 0;
	while( ADCSRA & (1<<ADSC)){};//This loop breaks only when conversion is complete
	return ADC;
}

void ADC_to_server(int c){//communication with the game server.Sends information like joystick position,button status.The server returns handle name,score and ammunition left.
	int directionx;
	int directiony;
	int mid_x = 504;//Center position of x
    int mid_y = 530;//Center position  of y
	int i = 0;
	int x=0;
	int y=0;
	char result[buffersize];
	
	uart_transmit_string(command55);//Sends 6 bytes of data
	_delay_ms(20);
	uart_transmit_string("HSHP\r\n");//Transmits the players handle name to the server
	hd44780_outcmd(0xC0);
	int e =0;
	while (e == 0){
		uart_recieve_string(received_string);
		if (strncmp(received_string,"+IPD", 4)==00){//Checks if we received the user handle.
			fprintf(stderr,received_string+8);//if received displays the handle name on the lcd.
			e =1;
			break;
		}
		
	}
	_delay_ms(50);
	//right x positive, up y positive
	directionx = ADC_read(4);
	directiony = ADC_read(5);
	x = (directionx-mid_x)*0.183;//percentage of the current position of the player on the x axis 
	y = (directiony-mid_y)*0.178;//percentage of the current  position of the player on the y axis
	uart_transmit_string(command5);//sends 12 bytes of data
	sprintf(result,"C%+3.3d%+3.3d%1d\r\n",x,y,c);//sends the joystick position and the button status to the server
	_delay_ms(10);
	uart_transmit_string(result);//sends the position of the pointer to the server
	while (i == 0){
		uart_recieve_string(received_string);
		if (strncmp(received_string,"+IPD,10", 7)==00){//Checks if we received the score and ammunition string
			fprintf(stderr,received_string + 8);//if received displays the score and ammunition on the lcd
			i =1;
			break;
		}
	}
	_delay_ms(10);
}



/*
 * Project.cpp
 *
 * Created: 5/28/2016 5:01:37 PM
 * Author : Gillian
 */ 

#include <avr/io.h>
#include "include/mfrc522.h"
#include "mfrc522.cpp"
#include "include/usart_ATmega1284.h"
#include "include/io.h"
#include "io.cpp"
#include "include/timer.h"

//------------------------------------//
//GLOBAL VARIABLES
//------------------------------------//

//---checkRFID()-----//
MFRC522 card_reader;
MFRC522::Uid lastuid;
MFRC522::Uid doorUIDs[5];
int doorUid_index = 0;
bool first_see = true;
unsigned char c = 0;

byte seven[] = {0xD4, 0x77, 0x54, 0xeb};
byte two[] = {0xBD, 0xD5, 0xA8, 0xD5};
	
//----openDoor()------//
enum DoorStates {DoorStart, Pressed, Opening, Open, Closing, Closed} DoorState;
bool fully_open;
bool fully_closed;
bool close_time;

//----DispTimeToABGame()---//
enum DispStates {DispStart, DispWait, DispTime} DispState;

void checkRFID() {
	if(card_reader.PICC_IsNewCardPresent()) {
		if (first_see && card_reader.PICC_ReadCardSerial()) {
			//PORTA = 0xFF;
			const unsigned char* str;
			
			char hexstr[21];
			int i;
			for (i = 0; i < card_reader.uid.size; i++) {
				sprintf(hexstr + i*2, "%02x", card_reader.uid.uidByte[i]);
			}
			
			hexstr[21] = NULL;
			
			str = reinterpret_cast<const unsigned char *>(hexstr);
			
			LCD_DisplayString(1, str);
			
			if (doorUid_index < 5) {
				bool newUID = true;
				
				for (int j = 0; j < doorUid_index; j++) {
					if (card_reader.uid.size == doorUIDs[j].size &&
						memcmp(doorUIDs[j].uidByte, card_reader.uid.uidByte, card_reader.uid.size) ==0) {
							newUID = false;
						}
				}
				
				if (newUID) {
					doorUIDs[doorUid_index++] = card_reader.uid;
					PORTA |= (1 << (doorUid_index - 1));
				}
			}
			
			first_see = false;
		}
	}
	else {
		c++;
		if (c >= 2) {
			first_see = true;
			//LCD_ClearScreen();
			
			c = 0;
		}
		
	}
}

void openDoor() {
	//TRANSITION
	switch (DoorState) {
		case DoorStart:
		DoorState = Closed;
		break;
		
		case Pressed:
		if ((PINB && 0x01) == 0x01) {
			DoorState = Closed;
			if (doorUid_index > 1) {
				DoorState = Opening;
			}
		}
		break;
		
		case Opening:
		if (fully_open) {
			DoorState = Open;
		}
		break;
		
		case Open:
		if (close_time) {
			DoorState = Closing;
		}
		break;
		
		case Closing:
		if (fully_closed) {
			DoorState = Closed;
		}
		break;
		
		case Closed:
		if ((~PINB & 0x01) == 0x01) {
			DoorState = Pressed;
		}
		break;
		
		default:
			DoorState = DoorStart;
		break;
	}
	//ACTION
	switch (DoorState) {
		case DoorStart:
		break;
		
		case Pressed:
		break;
		
		case Opening:
			//SET MOTOR TO UP DIRECTION
			//TIME IT
		break;
		
		case Open:
			//WAIT 9 SECONDS THEN SET fully_open = true;
		break;
		
		case Closing:
			//SET MOTOR TO DOWN DIRECTION
		break;
		
		case Closed:
		break;
		
		default:
		break;
	}
}

void DisplayTimeToABGame() {
	//TRANSITION
	switch (DispState) {
		case DispStart:
		break;
		
		case DispWait:
			if ((~PINB & 0x03) == 0x03) {
				DispState = DispTime;
			}
		break;
		
		case DispTime:
			if (disp_time_off) {
				DispState = DispWait;
			}
		break;
		
		default:
			DispState = DispWait;
		break;
	}
	//ACTION
	switch (DispState) {
		case DispStart:
		break;
		
		case DispWait:
		break;
		
		case DispTime:
		break;
		
		default:
		DispState = DispWait;
		break;
	}
}

int main(void)
{
	DDRB &= 0xFC; PORTA = PINA | 0x03;
	DDRA = 0xFF; PORTA = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xFF; PORTD = 0x00;
	
	card_reader.PCD_Init();

	LCD_init();
	
	TimerSet(1000);
	TimerOn();
	
    /* Replace with your application code */
    while (1) 
    {
		while (!TimerFlag) {}
		TimerFlag = 0;
		
		checkRFID();
    }
}


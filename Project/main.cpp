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
#include "include/nRF24L01.h"
#include "nRF24L01.cpp"
#include "include/AB_macros.h"

#define MOTOR_UP 0x20
#define MOTOR_DOWN 0x40
//------------------------------------//
//GLOBAL VARIABLES
//------------------------------------//
const int AB_GAME_PERIOD = 10; //IN SECONDS THE TIME BETWEEN AB ROUNDS
int tick;
int seconds;

unsigned char* data;

unsigned char randColor[6]; //SET 
unsigned char colors[9] = {RED, BLUE, GREEN, MAGENTA, RED, CYAN, YELLOW, CYAN, MAGENTA};
unsigned char pairStatus[9] = {PAIR, SOLO, SOLO, PAIR, PAIR, PAIR, SOLO, PAIR};
int bp[9] = {3};
bool penalty[9] = {false};

//---checkRFID()-----//
MFRC522 card_reader;
MFRC522::Uid lastuid;
MFRC522::Uid doorUIDs[5];
int doorUid_index = 0;
bool first_see = true;
unsigned char c = 0;

byte seven[] = {0xD4, 0x77, 0x54, 0xeb};
byte five[] = {0xBD, 0xD5, 0xA8, 0xD5};
	
//----openDoor()------//
enum DoorStates {DoorStart, Pressed, Opening, Open, Closing, Closed} DoorState;
int door_tick;
int MotorTime = 3;

//----giveABGame()-----//
enum ABGmaeStates {ABGameStart, ABGameVoting, ABGameOver, ABRequestScoreN, ABReceiveScoreN, ABDisplayScores, ABSendScoreN, NotPlaying} ABGameState;
unsigned char votes[9];
unsigned char voteTimeStamp[9];
int AB_tick;
int requestN;
unsigned char sendN;

void checkRFID() {
	if(card_reader.PICC_IsNewCardPresent()) {
		if (first_see && card_reader.PICC_ReadCardSerial()) {
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
				} //ADD OPEN FOR PERSON WITH 9 BP
			}
		break;
		
		case Opening:
			if (door_tick / 5 >= MotorTime) {
				DoorState = Open;
				door_tick = 0;
			}
		break;
		
		case Open:
			if (door_tick / 5 >= 9) {
				DoorState = Closing;
				door_tick = 0;
			}
		break;
		
		case Closing:
			if (door_tick / 5 >= MotorTime) {
				DoorState = Closed;
				door_tick = 0;
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
			PORTA = MOTOR_UP;
			
			door_tick++;
			
			//const unsigned char* st = reinterpret_cast<const unsigned char *>("Opening");
			//LCD_DisplayString(1, st);
			//SET MOTOR TO UP DIRECTION
			//TIME IT
		break;
		
		case Open:
			door_tick++;
			//WAIT 9 SECONDS THEN SET fully_open = true;
		break;
		
		case Closing:
			PORTA = MOTOR_DOWN;
			door_tick++;
			//SET MOTOR TO DOWN DIRECTION
		break;
		
		case Closed:
		break;
		
		default:
		break;
	}
}

void setVotes() {
	bool finalized[9] = {false};
	for (int i = 0; i < 9; i++) {
		if (pairStatus[i] == SOLO) {
			finalized[i] = true;
			continue;
		}
		for (int j = i + 1; j < 9; j++) {
			if (finalized[j]) {
				continue;
			}
			if (colors[j] == colors[i]) {
				if (votes[i] == votes[j]) {
					;
				} else if (votes[i] == ABSTAIN && votes[j] != ABSTAIN) {
					votes[i] = votes[j];
				} else if (votes[i] != ABSTAIN && votes[j] == ABSTAIN) {
					votes[j] = votes[i];
				} else if (voteTimeStamp[i] == voteTimeStamp[j]) {
					votes[j] = votes[i] = BETRAY;
				} else if (voteTimeStamp[i] < voteTimeStamp[j]) {
					votes[j] = votes[i];
				} else {
					votes[i] = votes[j];
				}
				
				finalized[i] = finalized [j] = true;
				break;
			}
		}
	}
}

void calculateBP() {
	setVotes();
	
	for (int i = 0; i < 9; i++) {
		unsigned char oppColor = colors[i] + 3;
		
		if (colors[i] > 0x03) {
			oppColor = colors[i] - 3;
		}
		
		for (int j = 0; j < 9; j++) {
			if (colors[j] != oppColor) {
				continue;
			} 
			
			//CHECK IF PENALTY AND THEN SET ABSTAIN VOTES TO ALLY FOR LESS IF STATEMENTS
			if ((votes[i] == votes[j]) && (votes[i] == ABSTAIN)) {
				penalty[i] = true;
			} else if (votes[i] == ABSTAIN) {
				votes[i] = ALLY;
			} else if (votes[j] == ABSTAIN) {
				votes[j] = ALLY;
			}
			
			if ((votes[i] == votes[j]) && (votes[i] == ALLY)) {
				bp[i] += 2;
			} else if ((votes[i] != votes[j]) && (votes[i] == ALLY)) {
				bp[i] -= 2;
			} else if ((votes[i] != votes[j]) && (votes[i] == BETRAY)) {
				bp[i] += 3;
			}
		}
	}
}

void randomizeColors() {
	;
	
}

//enum ABGameStates {ABGameStart, ABGameVoting, ABGameOver, ABRequestScoreN, ABReceiveScoreN, ABDisplayScores, ABSendScoreN, NotPlaying} ABGameState;
//unsigned char votes[9];
//unsigned char voteTimeStamp[9];

void ABGame() {
	//TRANSITION
	switch (ABGameState) {
		case NotPlaying:
			if (seconds >= AB_GAME_PERIOD) {
				ABGameState = ABGameStart;
			}
		break;
		
		case ABGameStart:
			if (GetReg(STATUS) & (1 << 4) == 0) { //IF SENT SUCCESS
				ABGameState = ABGameVoting;
				AB_tick = 0;
				
				SETBIT(PORTA, 1);
			}
			reset();
		break;
		
		case ABGameVoting:
			if (AB_tick / 5 >= 9) {
				ABGameState = ABGameOver;
			}
		break;
		
		case ABGameOver:
			if (GetReg(STATUS) & (1 << 4) == 0) { //IF SENT SUCCESS
				ABGameState = ABReceiveScoreN;
				
				reset();
				changeToReceiver();
			}
			reset();
		break;
		
		case ABReceiveScoreN:
			if (requestN >= 9) {
				ABGameState = ABSendScoreN;
				
				changeToTransmitter();
				
				calculateBP();
				
				randomizeColors();
				
				requestN = 0;
			}
		break;
		
		case ABDisplayScores:
			//if ()
		break;
		
		case ABSendScoreN:
			if (GetReg(STATUS) & (1 << 4) == 0) { //IF SENT SUCCESS
				sendN++;
			}
			
			if (sendN > 9) {
				ABGameState = ABDisplayScores;
			}
		break;
		
		default:
		break;		
	}
	
	unsigned char send[5];
	unsigned char receive[5];
	
	switch (ABGameState) {
		case NotPlaying:
		break;
		
		case ABGameStart:
			send[0] = GAME_START;
			
			transmit_payload(send);
			reset();
		break;
		
		case ABGameVoting:
			AB_tick++;
		break;
		
		case ABGameOver:
			send[0] = VOTING_END;
			
			transmit_payload(send);
			reset();
			
			SETBIT(PORTA, 0);
		break;
		
		case ABReceiveScoreN:
			if ((GetReg(STATUS) & (1 << 6)) != 0 ) {
				receive_payload(receive);
				
				votes[receive[0] - 1] = receive[1];
				voteTimeStamp[receive[0] - 1] = receive[2];
				colors[receive[0] - 1] = receive[3];
				pairStatus[receive[0] - 1] = receive[4];
				
				requestN = receive[0];
			}
		break;
		
		case ABDisplayScores:
		break;
		
		case ABSendScoreN:
			send[0] = SENDING_RESULTS;
			send[1] = sendN;
			send[2] = bp[sendN - 1];
			send[3] = colors[sendN - 1];
			send[4] = pairStatus[sendN - 1];
			
			transmit_payload(send);
		break;
		
		default:
		break;
	}
}
	
int main(void)
{
	DDRB &= 0xFC; PORTB = PINB | 0x03;
	DDRA = 0xFF; PORTA = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xFF; PORTD = 0x00;
	
	card_reader.PCD_Init();
	
	SETBIT(PORTD, 2);
	CLEARBIT(PORTD, 1);
	
	nrf24L01_init();
	
	TimerSet(200);
	TimerOn();
	
	unsigned char test[5];
	//test[0] = GAME_START;
	
    /* Replace with your application code */
    while (1) 
    {
		while (!TimerFlag) {}
		TimerFlag = 0;
		
		tick++;
		seconds = tick / 5;
		
		ABGame();
		//DO ALL THINGS DOOR RELATED AND SUCH
		checkRFID();
		openDoor();
    }
}
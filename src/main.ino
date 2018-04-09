/***************************************************

 ****************************************************/

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>

// If using the breakout with SPI, define the pins for SPI communication.

#define PN532_IRQ   (6)
#define PN532_RESET (7)

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);
#define TFT_CS     10
#define TFT_RST    0  // 9you can also connect this to the Arduino reset
// in which case, set this #define pin to 0!
#define TFT_DC     8

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
#define TFT_SCLK 13   // set these to be whatever pins you like!
#define TFT_MOSI 11   // set these to be whatever pins you like!

struct user_t {
	char username[8];
	char password[8];
	uint32_t card;
	bool isAdmin;
} user;

user_t currentUser;
int currenMem;
int buzzer = 9; //buzzer pin
const byte ROWS = 4; //four rows
const byte COLS = 4; //three columns
char keys[ROWS][COLS] = { { '1', '2', '3', 'A' }, { '4', '5', '6', 'B' }, { '7',
		'8', '9', 'C' }, { '*', '0', '#', 'D' } };
int address = 0;
byte rowPins[ROWS] = { A0, A1, A2, A3 }; //connect to the row pinouts of the keypad
byte colPins[COLS] = { 2, 3, 4, 5 }; //connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

float p = 3.1415926;
bool menuFlag = false;
String currentPwd = "";
unsigned long lastPwdTime = 0;
unsigned long pwdDelay = 5000;

uint8_t success;
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
uint8_t uidLength;
bool allUsers[10];
void setup(void) {
	Serial.begin(115200);

	nfc.begin();

	uint32_t versiondata = nfc.getFirmwareVersion();
	if (!versiondata) {
		Serial.print("Didn't find PN53x board");
		//while (1); // halt
	}

	// Got ok data, print it out!
	Serial.print("Found chip PN5");
	Serial.println((versiondata >> 24) & 0xFF, HEX);
	Serial.print("Firmware ver. ");
	Serial.print((versiondata >> 16) & 0xFF, DEC);
	Serial.print('.');
	Serial.println((versiondata >> 8) & 0xFF, DEC);

	// Set the max number of retry attempts to read from a card
	// This prevents us from waiting forever for a card, which is
	// the default behaviour of the PN532.
	nfc.setPassiveActivationRetries(0x01);

	// configure board to read RFID tags
	nfc.SAMConfig();

	Serial.println("Waiting for an ISO14443A card");

	// Serial.print("Hello! ST7735 TFT Test");
	char identifier[24] = { 98, 98, 87, 84, 123, 89, 123, 49, 76, 67, 88, 39,
			122, 38, 68, 88, 98, 24, 36, 52, 43, 86, 87, 83 };
	Serial.println("reading");
	char id[24];
	bool users[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
	Serial.println(EEPROM_readAnything(1000, id));

	bool check = false;

	for (int i = 0; i < 24; i++) {
		if (id[i] != identifier[i]) {
			check = true;
		}
	}
	if (check) {
		user_t admin = { { 'p', 'e', 't', 'r', 'o', 's', 'k', 'y' }, { '9', '8',
				'7', '6', '5', '4', '3', '2' }, 2905626692, true };
		Serial.println("writing");
		Serial.println(EEPROM_writeAnything(1000, identifier));
		Serial.println(EEPROM_writeAnything(900, admin));
		Serial.println(EEPROM_writeAnything(985, users));
	}
	Serial.println(EEPROM_readAnything(985, allUsers));

	// Use this initializer if you're using a 1.8" TFT
	tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab

	Serial.println("Initialized");

	uint16_t time = millis();
	locked();
	Serial.println("done");
	//delay(1000);
}
void locked(){
	tft.fillScreen(ST7735_BLACK);
	tft.fillRoundRect(35, 50, 58, 30, 8, ST7735_WHITE);
	tft.fillRoundRect(45, 57, 38, 30, 8, ST7735_BLACK);
	tft.fillRoundRect(25, 70, 78, 60, 8, ST7735_WHITE);
}
void loop() {

	char key = keypad.getKey();

	// Length of the UID (4 or 7 bytes depending on ISO14443A card type)

	if (key) {
		lastPwdTime = millis();
		tone(buzzer, 6000, 100);
		currentPwd = currentPwd + key;

		if (currentPwd == "*#") {
			menuFlag = true;
			currentPwd = "";
		}

		if (currentPwd.length() == 8) {

			if (checkPwd(currentPwd)) {
				if (menuFlag) {
					if (userIsAdmin()) {
						showAdminMenu();
					} else {
						showUserMenu();
					}
					locked();
				} else {
					unlock();
					currentPwd = "";
				}
			} else {
				delay(300);
				tone(buzzer, 1000, 50);
				delay(200);
				tone(buzzer, 1000, 50);
				delay(200);
				tone(buzzer, 1000, 50);
				currentPwd = "";
			}
		}
	}

	// Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
	// 'uid' will be populated with the UID, and uidLength will indicate
	// if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
	success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

	if (success) {
		tone(buzzer, 500, 100);
		// Display some basic information about the card
		Serial.println("Found an ISO14443A card");
		Serial.print("  UID Length: ");
		Serial.print(uidLength, DEC);
		Serial.println(" bytes");
		Serial.print("  UID Value: ");
		for (size_t i = 0; i < 7; i++) {
			Serial.print(uid[i]);
			Serial.print(" ");
		}
		Serial.print("\n  UID Valuehex: ");
		nfc.PrintHex(uid, uidLength);

		if (uidLength == 4) {
			// We probably have a Mifare Classic card ...
			uint32_t cardid = uid[0];
			cardid <<= 8;
			cardid |= uid[1];
			cardid <<= 8;
			cardid |= uid[2];
			cardid <<= 8;
			cardid |= uid[3];
			Serial.print("Seems to be a Mifare Classic card #");

      for (int w = 0; w < 10; w++) {
    		if (allUsers[w] == 1) {
    			for (int j = 0; j < 901; j = j + 100) {
    				EEPROM_readAnything(j, currentUser);
  					if (cardid == currentUser.card) {
              w=11;
              unlock();
  						break;
  					}
          }
    		}
    	}
		}
	}
	if ((millis() - lastPwdTime) > pwdDelay) {
		currentPwd = "";
		menuFlag = false;
		lastPwdTime = millis();
	}
}

void unlock() {
	tft.fillScreen(ST7735_BLACK);
	tft.setCursor(0, 20);
	tft.setTextColor(ST7735_RED);
	tft.print("     Bienvenido \n\n      ");
	for (size_t ij = 0; ij < 8; ij++) {

		tft.print(currentUser.username[ij]);
	}
	tft.print("\n");
	tone(buzzer, 3000, 500);
	delay(3000);
	locked();
}

bool userIsAdmin() {
	return currentUser.isAdmin;
}

void deleteUser() {
	tft.fillScreen(ST7735_BLACK);
	String listUsers = "";
	int i = 0;
	tft.setCursor(0, 10);
	tft.setTextColor(ST7735_RED);
	  for (i; i < 10; i++) {
	    if (allUsers[i] == 1) {
				EEPROM_readAnything(i*100, currentUser);
				tft.setTextWrap(true);
				if (i==9){
					tft.print(0);
				}else {
					tft.print(i+1);
				}
				tft.print(". ");
				for (size_t ij = 0; ij < 8; ij++) {
					tft.print(currentUser.username[ij]);
				}
				tft.print("\n");
	    }
  }

	tft.print("Presione # para salir");


  while(true){
    char key = keypad.getKey();
		if (key) {

			int mem = 0;

			switch (key) {
				case '1':
				allUsers[0] = 0;
				break;
				case '2':
				allUsers[1] = 0;
				break;
				case '3':
				allUsers[2] = 0;
				break;
				case '4':
				allUsers[3] = 0;
				break;
				case '5':
				allUsers[4] = 0;
				break;
				case '6':
				allUsers[5] = 0;
				break;
				case '7':
				allUsers[6] = 0;
				break;
				case '8':
				allUsers[7] = 0;
				break;
				case '9':
				allUsers[8] = 0;
				break;
				case '0':
				allUsers[9] = 0;
				break;
				case '#':
				return;
			}
			EEPROM_writeAnything(985,allUsers);

			tft.setCursor(0, 10);
			tft.fillScreen(ST7735_BLACK);
			tft.setTextColor(ST7735_RED);
			tft.print("Usuario '");
			tft.print(key);
			tft.print("' eliminado ");
			delay(2000);

			break;
		}
  }
}

void addUser() {
	char username[8];
	char password[8];
	uint32_t card;
	bool isAdmin = false;
	tft.fillScreen(ST7735_BLACK);
	int counter = 0;
	for (size_t i = 0; i < 10; i++) {
		if(allUsers[i]==1){
			counter = counter+1;
		}
	}
	if (counter==10) {
		testdrawtext("\t No se pueden agregar mas usuarios. Elimine alguno.", ST7735_RED);
		delay(2000);
		return;
	}
	testdrawtext("Ingrese identicacion, # para terminar.", ST7735_RED);

	int i = 0;
	while (true) {
		char key = keypad.getKey();
		if (key) {
			tone(buzzer, 6000, 100);
			if (key == '#' && i > 0) {
				break;
			} else {
				username[i] = key;
				i = i + 1;
			}
		}
		if (i > 8)
			break;
	}
	while(i<8){
		username[i] = ' ';
		i = i + 1;
	}
	bool pwdIsEqual = true;
	while (pwdIsEqual) {
		tft.fillScreen(ST7735_BLACK);
		testdrawtext("Ingrese contrasena de 8 digitos.", ST7735_RED);

		i = 0;
		while (true) {
			char key = keypad.getKey();
			if (key) {
				tone(buzzer, 6000, 100);
				password[i] = key;
				i = i + 1;
			}
			if (i == 8)
				break;
		}
		tft.fillScreen(ST7735_BLACK);
		testdrawtext("Ingrese contrasena de nuevo.", ST7735_RED);

		i = 0;

		while (true) {
			char key = keypad.getKey();
			if (key) {
				tone(buzzer, 6000, 100);
				if (password[i] != key) {
					pwdIsEqual = false;
				}
				i = i + 1;
			}
			if (i == 8 && pwdIsEqual == false) {
				pwdIsEqual = true;
				tft.fillScreen(ST7735_BLACK);
				testdrawtext("contrasenas no coinciden", ST7735_RED);
				delay(1000);
				break;
			}
			if (i == 8 && pwdIsEqual == true) {
				pwdIsEqual = false;
				break;
			}

		}
	}
	tft.fillScreen(ST7735_BLACK);
	testdrawtext("Registrar tarjeta, # para omitir.", ST7735_RED);

	i = 0;
	while (true) {
		char key = keypad.getKey();
		if (key) {
			tone(buzzer, 6000, 100);
			if (key == '#') {
				break;
			}
		}
		success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

		if (success) {
			tone(buzzer, 500, 100);
			// We probably have a Mifare Classic card ...
			card = uid[0];
			card <<= 8;
			card |= uid[1];
			card <<= 8;
			card |= uid[2];
			card <<= 8;
			card |= uid[3];

			break;
		}
	}
	tft.fillScreen(ST7735_BLACK);
	testdrawtext("Es Administrador? \n1. Si \n2. No.", ST7735_RED);
	while (true) {
		char key = keypad.getKey();
		if (key) {
			tone(buzzer, 6000, 100);
			if (key == '1') {
				isAdmin = true;
			}
			break;
		}

	}
	user_t newUser;
	for (i = 0; i < 8; i++) {
		newUser.username[i] = username[i];
	}

	for (i = 0; i < 8; i++) {
		newUser.password[i] = password[i];
	}

	newUser.card = card;
	newUser.isAdmin = isAdmin;
	int mem = 0;
	i = 0;
	while (allUsers[i] == true) {
		mem = mem + 100;
		i = i + 1;
	}
	allUsers[i] = 1;
	EEPROM_writeAnything(mem, newUser);
	EEPROM_writeAnything(985, allUsers);
}

void listUsers() {
	tft.fillScreen(ST7735_BLACK);
	String listUsers = "";
	int i = 0;
	int screen = 10;
	tft.setCursor(0, screen);
	tft.setTextColor(ST7735_RED);
	  for (i; i < 10; i++) {
	    if (allUsers[i] == 1) {
				EEPROM_readAnything(i*100, currentUser);
				tft.setTextWrap(true);
				tft.print(i+1);
				tft.print(". ");
				for (size_t ij = 0; ij < 8; ij++) {
					tft.print(currentUser.username[ij]);
				}
				tft.print("\n");
	    }
  }

	tft.print("Presione 0 para salir");


  while(true){
    char key = keypad.getKey();
    if(key){
      if(key == '0'){
        break;
      }
    }
  }
}

bool checkPwd(String pwd) {
	bool pwdFlag = true;
	for (int w = 0; w < 10; w++) {
		if (allUsers[w] == 1) {
			for (int j = 0; j < 901; j = j + 100) {

				EEPROM_readAnything(j, currentUser);

				pwdFlag = true;
				for (int i = 0; i < 8; i++) {
					if (pwd.charAt(i) != currentUser.password[i]) {
						pwdFlag = false;
						break;
					}
				}
				if (pwdFlag) {
          currenMem = j;
					return true;
				}
			}
		}
	}

	return pwdFlag;
}

void editPwd() {
  char password[8];
  int i = 0;
	bool pwdIsEqual = true;
	while (pwdIsEqual) {
		tft.fillScreen(ST7735_BLACK);
		testdrawtext("Ingrese contrasena de 8 digitos.", ST7735_RED);
		while (true) {
			char key = keypad.getKey();
			if (key) {
				tone(buzzer, 6000, 100);
				password[i] = key;
				i = i + 1;
			}
			if (i == 8)
				break;
		}
		tft.fillScreen(ST7735_BLACK);
		testdrawtext("Ingrese contrasena de nuevo.", ST7735_RED);

		i = 0;

		while (true) {
			char key = keypad.getKey();
			if (key) {
				tone(buzzer, 6000, 100);
				if (password[i] != key) {
					pwdIsEqual = false;
				}
				i = i + 1;
			}
			if (i == 8 && pwdIsEqual == false) {
				pwdIsEqual = true;
				tft.fillScreen(ST7735_BLACK);
				testdrawtext("contrasenas no coinciden", ST7735_RED);
				delay(1000);
        return;
			}
			if (i == 8 && pwdIsEqual == true) {
				pwdIsEqual = false;
				break;
			}
		}
    for (i = 0; i < 8; i++) {
      currentUser.password[i] = password[i];
    }
		EEPROM_writeAnything(currenMem,currentUser);
    return;
	}
}

void showAdminMenu() {
	tft.fillScreen(ST7735_BLACK);
	testdrawtext("1. Agregar usuario. \n2. Listar usuarios. \n3. Eliminar usuario. \n0. Salir.  ", ST7735_RED);
	boolean NO_KEY = true;
	while (NO_KEY) {
		char key = keypad.getKey();
		if (key) {
			tone(buzzer, 6000, 100);
			switch (key) {
			case '1':
				addUser();
				tft.fillScreen(ST7735_BLACK);
				testdrawtext(
						"1. Agregar usuario. \n2. Listar usuarios. \n3. Eliminar usuario. \n0. Salir.  ",
						ST7735_RED);
				break;
			case '2':
				listUsers();
				tft.fillScreen(ST7735_BLACK);
				testdrawtext(
						"1. Agregar usuario. \n2. Listar usuarios. \n3. Eliminar usuario. \n0. Salir.  ",
						ST7735_RED);
				break;
			case '3':
				deleteUser();
				tft.fillScreen(ST7735_BLACK);
				testdrawtext(
						"1. Agregar usuario. \n2. Listar usuarios. \n3. Eliminar usuario. \n0. Salir.  ",
						ST7735_RED);
				break;
			case '0':
				tft.fillScreen(ST7735_BLACK);
				return;
			}
		}
	}
}

void showUserMenu() {
	tft.fillScreen(ST7735_BLACK);
	testdrawtext("1. Cambiar contrasena. \n0. Salir.  ", ST7735_RED);
	boolean NO_KEY = true;
	while (NO_KEY) {
		char key = keypad.getKey();

		switch (key) {
		case '1':
			editPwd();
      tft.fillScreen(ST7735_BLACK);
      testdrawtext("1. Cambiar contrasena. \n0. Salir.  ", ST7735_RED);
			break;
		case '0':
    tft.fillScreen(ST7735_BLACK);
			return;
		}
	}
}

void testdrawtext(char *text, uint16_t color) {

	tft.setCursor(0, 0);
	tft.setTextColor(color);
	tft.setTextWrap(true);
	tft.print(text);

}

template<class T> int EEPROM_writeAnything(int ee, const T& value) {
	const byte* p = (const byte*) (const void*) &value;
	unsigned int i;
	for (i = 0; i < sizeof(value); i++)
		EEPROM.write(ee++, *p++);
	return i;
}

template<class T> int EEPROM_readAnything(int ee, T& value) {
	byte* p = (byte*) (void*) &value;
	unsigned int i;
	for (i = 0; i < sizeof(value); i++)
		*p++ = EEPROM.read(ee++);

	return i;
}

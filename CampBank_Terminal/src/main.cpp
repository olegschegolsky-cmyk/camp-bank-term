#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <ArduinoJson.h>

// --- НАЛАШТУВАННЯ МЕРЕЖІ ТА СЕРВЕРА ---
const char* ssid = "brothership";        
const char* password = "God+bless+you";        // НЕ ЗАБУДЬ ВПИСАТИ ПАРОЛЬ
const String SERVER_URL = "http://172.16.40.43:8000"; 
const String TERMINAL_ID = "term_08";

// --- ПІДКЛЮЧЕННЯ ПЕРИФЕРІЇ ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
#define SS_PIN  4
#define RST_PIN 5
MFRC522 rfid(SS_PIN, RST_PIN);
const int BUZZER_PIN = 16;

// --- НАЛАШТУВАННЯ КЛАВІАТУРИ 4x4 ---
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 17, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- ЗМІННІ СТАНУ ---
enum State { 
    STATE_IDLE, 
    STATE_INPUT_SUM, 
    STATE_WAIT_PAY, 
    STATE_WAIT_BAL, 
    STATE_P2P_SENDER, 
    STATE_P2P_RECEIVER, 
    STATE_P2P_SUM 
};
State currentState = STATE_IDLE;
String inputAmount = "";
String p2pSenderUID = "";
String p2pReceiverUID = "";

// --- ФУНКЦІЇ ЗВУКУ ---
void beep(int duration, int times = 1) {
    for (int i = 0; i < times; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(duration);
        digitalWrite(BUZZER_PIN, LOW);
        if (i < times - 1) delay(100);
    }
}

// --- ОНОВЛЕННЯ ДИСПЛЕЯ ---
void goIdle() {
    currentState = STATE_IDLE;
    inputAmount = "";
    p2pSenderUID = "";
    p2pReceiverUID = "";
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Prystriy robocyi");
    lcd.setCursor(0,1); lcd.print("Gotovyi do robot");
}

void updateInputDisplay() {
    lcd.setCursor(0,1);
    lcd.print("Suma: " + inputAmount + "     "); 
}

// --- HTTP ЗАПИТИ ДО СЕРВЕРА ---
void showUserBalance(String uid) {
    HTTPClient http;
    http.begin(SERVER_URL + "/balance/" + uid);
    int code = http.GET();
    String response = http.getString();
    http.end();

    if (code == 200) {
        JsonDocument doc;
        deserializeJson(doc, response);
        String name = doc["name"].as<String>();
        float balance = doc["balance"];
        
        lcd.clear();
        lcd.setCursor(0,0); lcd.print(name.substring(0, 16));
        lcd.setCursor(0,1); lcd.print("Balans: " + String(balance));
        delay(3000);
    }
}

void sendPayment(String uid, float amount) {
    lcd.clear(); lcd.print("Obrobka...");
    HTTPClient http;
    http.begin(SERVER_URL + "/transaction?uid=" + uid + "&amount=" + String(amount) + "&term_id=" + TERMINAL_ID);
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST("");
    String response = http.getString();
    http.end();

    if (httpResponseCode == 200) {
        beep(100, 2); // УСПІХ - Подвійний короткий
        lcd.clear();
        lcd.setCursor(0,0); lcd.print("Oplata uspishna");
        lcd.setCursor(0,1); lcd.print("-" + String(amount) + " UAH");
        delay(4000);
        
        JsonDocument doc;
        deserializeJson(doc, response);
        String name = doc["name"].as<String>();
        float new_balance = doc["new_balance"];
        
        lcd.clear();
        lcd.setCursor(0,0); lcd.print(name.substring(0, 16));
        lcd.setCursor(0,1); lcd.print("Balans: " + String(new_balance));
        delay(3000);

    } else {
        beep(100, 3); // ПОМИЛКА - Потрійний короткий
        lcd.clear();
        lcd.setCursor(0,0); lcd.print("Oplata VIDHYLENA");
        
        if (httpResponseCode == 400) {
            lcd.setCursor(0,1); lcd.print("Nedostatno kosh.");
        } else if (httpResponseCode == 403) {
            lcd.setCursor(0,1); lcd.print("Zablokovano!");
        } else if (httpResponseCode == 404) {
            lcd.setCursor(0,1); lcd.print("Karta ne znayden");
        } else {
            lcd.setCursor(0,1); lcd.print("Zbiy systemy");
        }
        delay(4000);

        if (httpResponseCode != 404) {
            showUserBalance(uid); 
        }
    }
    goIdle();
}

void fetchBalance(String uid) {
    lcd.clear(); lcd.print("Zavantazhennya..");
    HTTPClient http;
    http.begin(SERVER_URL + "/balance/" + uid);
    
    int httpResponseCode = http.GET();
    String response = http.getString();
    http.end();

    if (httpResponseCode == 200) {
        beep(100, 1); // УСПІХ ЗЧИТУВАННЯ - Одинарний короткий
        JsonDocument doc;
        deserializeJson(doc, response);
        String name = doc["name"].as<String>();
        float balance = doc["balance"];
        
        lcd.clear();
        lcd.setCursor(0,0); lcd.print(name.substring(0, 16));
        lcd.setCursor(0,1); lcd.print("Balans: " + String(balance));
    } else {
        beep(100, 3); // ПОМИЛКА - Потрійний короткий
        lcd.clear();
        lcd.print("Karta ne znayden");
    }
    delay(4000);
    goIdle();
}

void sendP2PTransfer(String sender_uid, String receiver_uid, float amount) {
    lcd.clear(); lcd.print("Obrobka...");
    HTTPClient http;
    http.begin(SERVER_URL + "/transfer?from_uid=" + sender_uid + "&to_uid=" + receiver_uid + "&amount=" + String(amount));
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST("");
    http.end();

    if (httpResponseCode == 200) {
        beep(100, 2); // УСПІХ - Подвійний короткий
        lcd.clear();
        lcd.setCursor(0,0); lcd.print("Perekaz USPISHNY");
        lcd.setCursor(0,1); lcd.print(String(amount) + " UAH");
    } else {
        beep(100, 3); // ПОМИЛКА - Потрійний короткий
        lcd.clear();
        lcd.setCursor(0,0); lcd.print("Perekaz VIDHYLEN");
        
        if (httpResponseCode == 400) {
            lcd.setCursor(0,1); lcd.print("Nedostatno kosh.");
        } else if (httpResponseCode == 404) {
            lcd.setCursor(0,1); lcd.print("Karta ne znayden");
        }
    }
    delay(4000);
    goIdle();
}

// --- ЛОГІКА ЗЧИТУВАННЯ NFC ---
void checkNFC() {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
    
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
        uid += String(rfid.uid.uidByte[i], HEX);
    }
    rfid.PICC_HaltA(); 

    if (currentState == STATE_WAIT_PAY) {
        sendPayment(uid, inputAmount.toFloat());
    } else if (currentState == STATE_WAIT_BAL) {
        fetchBalance(uid);
    } else if (currentState == STATE_P2P_SENDER) {
        p2pSenderUID = uid;
        beep(100, 1); // Одинарний - карту прийнято
        currentState = STATE_P2P_RECEIVER;
        lcd.clear(); 
        lcd.setCursor(0,0); lcd.print("P2P Otrymuvach:");
        lcd.setCursor(0,1); lcd.print("Prykladit kartu");
    } else if (currentState == STATE_P2P_RECEIVER) {
        if (uid == p2pSenderUID) {
            beep(100, 3); // ПОМИЛКА (та сама карта) - Потрійний короткий
            lcd.clear(); lcd.print("Pomylka: Ta zama");
            lcd.setCursor(0,1); lcd.print("karta!");
            delay(3000);
            goIdle();
        } else {
            p2pReceiverUID = uid;
            beep(100, 1); // Одинарний - другу карту прийнято
            currentState = STATE_P2P_SUM;
            inputAmount = "";
            lcd.clear(); 
            lcd.setCursor(0,0); lcd.print("Suma perekazu:");
            updateInputDisplay();
        }
    }
}

// --- СЕРВІСНЕ МЕНЮ ---
void showServiceMenu() {
    beep(100, 2); // Подвійний при вході в меню
    // Екран 1: IP
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("IP Adresa:"); 
    lcd.setCursor(0,1); lcd.print(WiFi.localIP());
    delay(2000);
    
    // Екран 2: Wi-Fi та Заряд
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Wi-Fi Signal: " + String(WiFi.RSSI()));
    lcd.setCursor(0,1); lcd.print("Zaryad: USB 5V"); 
    delay(2000);

    // Екран 3: Ping
    lcd.clear();
    lcd.print("Ping do servera:");
    
    unsigned long startPing = millis();
    HTTPClient http;
    http.begin(SERVER_URL + "/");
    int httpResponseCode = http.GET();
    unsigned long pingTime = millis() - startPing;
    http.end();
    
    lcd.setCursor(0,1);
    if (httpResponseCode > 0) lcd.print(String(pingTime) + " ms");
    else lcd.print("ZVYAZKU NEMAE");
    
    delay(3000);
    goIdle();
}

// --- ЛОГІКА КНОПОК ---
void processKey(char key) {
    beep(50, 1); // Дуже короткий клік (50мс) при натисканні
    
    if (key >= '0' && key <= '9') {
        if (currentState == STATE_IDLE) { 
            currentState = STATE_INPUT_SUM; 
            lcd.clear(); lcd.print("Vvedit sumu:"); 
        }
        if (currentState == STATE_INPUT_SUM || currentState == STATE_P2P_SUM) { 
            inputAmount += key; 
            updateInputDisplay(); 
        }
    } 
    else if (key == '*') {
        if ((currentState == STATE_INPUT_SUM || currentState == STATE_P2P_SUM) && inputAmount.indexOf('.') == -1) { 
            inputAmount += '.'; 
            updateInputDisplay(); 
        }
    } 
    else if (key == '#') {
        goIdle();
    } 
    else if (key == 'D') {
        if (currentState == STATE_INPUT_SUM && inputAmount.length() > 0) {
            currentState = STATE_WAIT_PAY;
            lcd.clear(); lcd.setCursor(0,0); lcd.print("Prykladit kartu");
            lcd.setCursor(0,1); lcd.print("Do oplaty: " + inputAmount);
        }
        else if (currentState == STATE_P2P_SUM && inputAmount.length() > 0) {
            sendP2PTransfer(p2pSenderUID, p2pReceiverUID, inputAmount.toFloat());
        }
    } 
    else if (key == 'A') {
        if (currentState == STATE_IDLE) {
            currentState = STATE_P2P_SENDER;
            lcd.clear(); 
            lcd.setCursor(0,0); lcd.print("P2P Vidpravnyk:");
            lcd.setCursor(0,1); lcd.print("Prykladit kartu");
        } 
        else if (currentState == STATE_INPUT_SUM || currentState == STATE_P2P_SUM) {
            if (inputAmount.length() > 0) {
                inputAmount.remove(inputAmount.length() - 1); 
                lcd.setCursor(0, 1);
                lcd.print("                "); 
                updateInputDisplay(); 
            } else {
                goIdle();
            }
        }
    }
    else if (key == 'B' && currentState == STATE_IDLE) {
        currentState = STATE_WAIT_BAL;
        lcd.clear(); lcd.setCursor(0,0); lcd.print("Perevirka balans");
        lcd.setCursor(0,1); lcd.print("Prykladit kartu");
    } 
    else if (key == 'C' && currentState == STATE_IDLE) {
        showServiceMenu();
    }
}

// --- ГОЛОВНІ ЦИКЛИ ---
void setup() {
    Serial.begin(115200);
    SPI.begin();
    rfid.PCD_Init();
    lcd.init();
    lcd.backlight();
    pinMode(BUZZER_PIN, OUTPUT);
    
    lcd.setCursor(0,0); lcd.print("Uvimkneno uspish");
    lcd.setCursor(0,1); lcd.print("Use v normi");
    beep(100, 2); // Подвійний після увімкнення екрана
    delay(2000);
    
    WiFi.begin(ssid, password); 
    lcd.clear(); lcd.print("Merezha...");
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500); 
        lcd.print("."); 
    }
    
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Pid-ednano merez");
    lcd.setCursor(0,1); lcd.print("Prystriy gotovyi");
    beep(100, 2); // Подвійний після з'єднання з Wi-Fi
    delay(2000);
    // --- ДІАГНОСТИКА NFC ---
    byte nfcVersion = rfid.PCD_ReadRegister(rfid.VersionReg);
    lcd.clear();
    lcd.setCursor(0,0); 
    lcd.print("NFC Version:");
    lcd.setCursor(0,1);
    
    if (nfcVersion == 0x00 || nfcVersion == 0xFF) {
        lcd.print("POMYLKA SPI!"); // Зчитувач не підключено або згорів
        beep(500, 3);
    } else {
        lcd.print(String(nfcVersion, HEX)); // Має показати 91 або 92
        beep(100, 1);
    }
    delay(4000);
    // -----------------------
    goIdle();
}

void loop() {
    char key = keypad.getKey();

    if (key) {
        processKey(key);
    }

    if (currentState == STATE_WAIT_PAY || currentState == STATE_WAIT_BAL || 
        currentState == STATE_P2P_SENDER || currentState == STATE_P2P_RECEIVER) {
        checkNFC();
    }
}
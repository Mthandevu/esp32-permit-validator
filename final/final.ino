// MTHANDENI
// B.ENG

// ===========================
// Imports
// ===========================
#include <WiFi.h>
#include <HTTPClient.h>

#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "Lindelwa";
const char* password = "123456789";

// ===========================
// Keypad configuration
// ===========================
const byte ROWS = 4;  // Number of rows
const byte COLS = 3;  // Number of columns

// Define the keymap
char keys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};

// Connect the keypad rows to these ESP32 pins
byte rowPins[ROWS] = { 12, 14, 27, 26 };  // R1, R2, R3, R4
// Connect the keypad columns to these ESP32 pins
byte colPins[COLS] = { 25, 33, 32 };  // C1, C2, C3

// Create the Keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


// Letters/symbols assigned to each key (in order of taps)
const char* keyChars[ROWS][COLS] = {
  { "1!", "2ABC", "3DEF" },      // 1: 1 → ! | 2: A → B → C | 3: D → E → F
  { "4GHI", "5JKL", "6MNO" },    // 4: G → H → I | etc.
  { "7PQRS", "8TUV", "9WXYZ" },  // 7: P → Q → R → S | 9: W → X → Y → Z
  { "*", "0 ", "#" }             // *: * | 0: SPACE | #: #
};

// Variables for multi-tap detection
unsigned long lastPressTime = 0;
char lastKey = '\0';
byte tapCount = 0;


// --- Global Variables (add these outside any function) ---
char pendingOutputChar = '\0';                // Stores the character ready to be output
unsigned long outputReadyTime = 0;            // Timestamp when the character became ready for output
const unsigned long MULTI_TAP_TIMEOUT = 500;  // Define timeout for clarity



// ===========================
// I2C LCD configuration
// ===========================
LiquidCrystal_I2C lcd(0x27, 16, 2);


// ===========================
// Server details
// ===========================
String serverName = "http://192.168.13.69:8000";
String verifyServerPath = serverName + "/api/verify/";


// ===========================
// Barcode module setup
// ===========================
#define RX_PIN 16
#define TX_PIN 17

#define SCANNER_READ_TIMEOUT_MS 5000

// use this command to send a scan command to the barcode scanner
const byte SCAN_COMMAND[] = { 0x7E, 0x00, 0x08, 0x01, 0x00, 0x02, 0x01, 0xAB, 0xCD };
const size_t SCAN_COMMAND_LEN = sizeof(SCAN_COMMAND);

const byte SCAN_RESPONSE_EXPECTED[] = { 0x02, 0x00, 0x00, 0x01, 0x00, 0x33, 0x31 };
const size_t SCAN_RESPONSE_LEN = sizeof(SCAN_RESPONSE_EXPECTED);


void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  // setup LCD
  lcd.begin();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi......");

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected: ");
  Serial.print(WiFi.localIP());
  Serial.println("");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected WiFi.");
  lcd.setCursor(0, 1);
  lcd.print(String(ssid));

  keypad.setHoldTime(2000);

  delay(3000);
  printDefaults();
}


void loop() {

  // Check for key press
  char key = keypad.getKey();

  // If a key is pressed, check ->
  // - if # is pressed capture image and send for processing
  // - if long any key enter manual barcode input
  if (key) {
    Serial.print("Key Pressed: ");
    Serial.println(key);

    if (key == '#') {
      // the user has pressed the enter key, therefore capture and send the barcode for processing
      captureAndSendBarcode();
    } else if (key == '*') {
      // user has entered manual mode, entering barcode manually
      sendManualVerification();
    }
  }

  delay(100);
}



void printDefaults() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("#:Scan & Send...");
  lcd.setCursor(0, 1);
  lcd.print("*:Manual Mode...");

  Serial.println();
  Serial.println("#:Scan & Send...");
  Serial.println("*:Manual Mode...\n");
}

void captureAndSendBarcode() {
  // ==================================
  // Remote Upload Server configuration
  // ==================================
  Serial.println("User has entered automatic mode, will scan and capture barcode..\n");

  WiFiClient client;
  HTTPClient http;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scanning.......");
  lcd.setCursor(0, 1);
  lcd.print("Please wait.....");

  if (!sendCommandAndVerifyResponse(SCAN_COMMAND, SCAN_COMMAND_LEN, SCAN_RESPONSE_EXPECTED, SCAN_RESPONSE_LEN, 1000)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan CMD FAILED!!");
    lcd.setCursor(0, 1);
    lcd.print("reboot device....");
    delay(3000);
    printDefaults();  // Go back to default display
    return;           // Exit if scanner command fails
  }


  String scannedBarcode = getBarcode();

  if (scannedBarcode.length() > 0) {


    if (WiFi.status() == WL_CONNECTED) {

      http.begin(client, verifyServerPath.c_str());
      // configure http, set content-type to JSON
      http.addHeader("Content-Type", "application/json");

      scannedBarcode.trim(); // Removes leading and trailing whitespace characters (including \n, \r, spaces, tabs)

      // Construct HTTP Request Payload as JSON
      // Server expecting this format => {"barcode": "ASD3412BH", "is_manual": false}
      String httpRequestData = "{\"barcode\": \"";
      httpRequestData.concat(scannedBarcode);
      httpRequestData.concat("\", \"is_manual\": false}");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Verifying tax....");
      lcd.setCursor(0, 1);
      lcd.print("Please wait.....");
      delay(1500);


      // Send HTTP POST request
      int statusCode = http.POST(httpRequestData);


      // this is a very wrong way of handling apis, the server always return 200, haha!!
      if (statusCode == 200) {
        // successfully send barcode to server
        String payload = http.getString();
        proccessServerResponse(payload);
      }

      else if (statusCode == 403) {
        // failed, barcode has expired
        String payload = http.getString();
        proccessServerResponse(payload);
      }

      else if (statusCode == 404) {
        // barcode not found on our database
        String payload = http.getString();
        proccessServerResponse(payload);
      }

      else {
        // can't connect to internet, timeout
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Timeout........");
        lcd.setCursor(0, 1);
        lcd.print("Can't connect.");

        delay(3000);
        printDefaults();
      }
      http.end();

    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Your WiFi has   ");
      lcd.setCursor(0, 1);
      lcd.print("Disconnected....");

      delay(3000);
      printDefaults();
    }

  } else {
    // No barcode was scanned within the timeout after commanding
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No Barcode Read!");
    lcd.setCursor(0, 1);
    lcd.print("...Try again...");

    delay(3000);
    printDefaults();
  }
}



void sendManualVerification() {
  // function to send manual verification to the server
  // simple send an http post to the /manual-verification endpoint with the driver's barcode tag
  Serial.println("User has entered manual mode, begin to process keypad to the server\n");

  if (WiFi.status() == WL_CONNECTED) {

    WiFiClient client;
    HTTPClient http;

    // capture keypad press
    String manualBarcode = "";
    char key;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("->Enter code....");
    lcd.setCursor(0, 1);
    lcd.print("#: Done *: Clear");

    Serial.println("Enter code via keypad (#: finish), (*: clear):");

    while (true) {
      key = keypad.getKey();  // Get the current key press
      char charToAdd = '\0';  // This will hold the character to be appended to manualBarcode

      // --- Step 1: Process current key press (if any) ---
      if (key) {
        // If it's a special key, handle it immediately
        if (key == '#') {
          if (manualBarcode.length() > 0) {
            // Clear any pending character if '#' is pressed for immediate action
            pendingOutputChar = '\0';
            outputReadyTime = 0;

            Serial.println("\n\nDone...");
            break;  // Exit loop when '#' is pressed
          }
          // If '#' is pressed ;2but manualBarcode is empty, ignore it or provide feedback
          // (Current logic does nothing here, which is fine)
        } else if (key == '*') {
          // Clear any pending character if '*' is pressed for immediate action
          pendingOutputChar = '\0';
          outputReadyTime = 0;

          if (manualBarcode.length() == 0) {
            // if the user previously cleared, exit now
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Exiting manual.");
            lcd.setCursor(0, 1);
            lcd.print("barcode input..");
            delay(2000);  // Short delay to show message

            printDefaults();  // Assuming this function resets LCD/state
            return;           // the user has cancelled manual input mode.
          }

          // attempt to backspace
          manualBarcode.remove(manualBarcode.length() - 1);

          if (manualBarcode.length() == 0) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("-> ");
            lcd.setCursor(0, 1);
            lcd.print("------   *: Exit");
          } else {
            // print the new barcode after backspace
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("->");
            lcd.print(manualBarcode);
            lcd.setCursor(0, 1);
            lcd.print("#: Done *: Clear");
          }

        } else {  // It's an alphanumeric key
          // Process the multi-tap. This function will internally update pendingOutputChar.
          // It will *not* return the char directly for the current tap.
          // It might return a *previously pending* char if the timeout for that one expired.
          charToAdd = processMultiTap(key);  // This might return a finalized char from a *previous* tap sequence
        }
      } else {
        // No new key pressed
        // --- Step 2: Check for timeout of the PENDING character ---
        // If there's a character pending and its timeout has elapsed since last touch
        if (pendingOutputChar != '\0' && (millis() - lastPressTime > MULTI_TAP_TIMEOUT)) {
          charToAdd = pendingOutputChar;  // This character is now finalized
          pendingOutputChar = '\0';       // Clear pending
          outputReadyTime = 0;            // Reset time
        }
      }

      // --- Step 3: Append finalized character (if any) ---
      if (charToAdd != '\0') {
        manualBarcode += charToAdd;

        Serial.print("Added to barcode: ");  // Debugging: show what's added
        Serial.println(charToAdd);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("->");
        lcd.print(manualBarcode);
        lcd.setCursor(0, 1);
        lcd.print("#: Done *: Clear");
      }

      delay(50);  // debounce
    }

    http.begin(client, verifyServerPath.c_str());
    // configure http, set content-type to JSON
    http.addHeader("Content-Type", "application/json");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Verifying tax....");
    lcd.setCursor(0, 1);
    lcd.print("Please wait.....");
    delay(1500);

    // Construct HTTP Request Payload as JSON
    // Server expecting this format => {"barcode": "ASD3412BH", "is_manual": true}
    String httpRequestData = "{\"barcode\": \"";
    httpRequestData.concat(manualBarcode);
    httpRequestData.concat("\", \"is_manual\": true}");


    // Send HTTP POST request
    int statusCode = http.POST(httpRequestData);


    if (statusCode == 200) {
      // successfully send barcode to server
      String payload = http.getString();
      proccessServerResponse(payload);
    }

    else if (statusCode == 403) {
      // failed, barcode has expired
      String payload = http.getString();
      proccessServerResponse(payload);
    }

    else if (statusCode == 404) {
      // barcode not found on our database
      String payload = http.getString();
      proccessServerResponse(payload);
    }

    else {
      // can't connect to internet, timeout
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Timeout........");
      lcd.setCursor(0, 1);
      lcd.print("Can't connect.");

      delay(3000);
      printDefaults();
    }

    // Free resources
    http.end();

  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Your WiFi has   ");
    lcd.setCursor(0, 1);
    lcd.print("Disconnected....");

    delay(3000);
    printDefaults();
  }
}


void proccessServerResponse(String payload) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Failed to parse");
    lcd.setCursor(0, 1);
    lcd.print("server data....");

    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());

  } else {

    const char* lcd1 = doc["lcd1"];
    const char* lcd2 = doc["lcd2"];
    const char* details = doc["details"];

    Serial.print("Details:  ");
    Serial.println(details);
    Serial.print("LCD1: ");
    Serial.println(lcd1);
    Serial.print("LCD2: ");
    Serial.println(lcd2);
    Serial.println("\n");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(lcd1);
    lcd.setCursor(0, 1);
    lcd.print(lcd2);
  }

  delay(3000);
  printDefaults();
}


String getBarcode() {
  unsigned long startTime = millis();  // Record the start time

  // Loop while there's no data AND we haven't exceeded the timeout
  while (!Serial2.available() && (millis() - startTime < SCANNER_READ_TIMEOUT_MS)) {
    // Optional: Add a small delay to prevent busy-waiting
    delay(50);  // Adjust this if you need more responsiveness or less CPU usage
  }

  if (Serial2.available()) {
    String receivedData = Serial2.readStringUntil('\n');
    Serial.print("Received from scanner: ");
    Serial.println(receivedData);
    return receivedData;
  } else {
    // If the timeout occurred and no data was received
    Serial.println("Barcode scan timed out. No data received from scanner.");
    return "";  // Return an empty string or handle the timeout case as needed
  }
}


// Function to send a command to the scanner and verify its response
bool sendCommandAndVerifyResponse(
  const byte* command, size_t commandLen,
  const byte* expectedResponse, size_t expectedLen,
  unsigned long timeout) {

  // Clear any old data in the serial buffer before sending command
  while (Serial2.available()) {
    Serial2.read();
  }

  Serial.print("Sending command to scanner: ");
  for (size_t i = 0; i < commandLen; i++) {
    Serial.printf("%02X ", command[i]);
    Serial2.write(command[i]);  // Send byte by byte
  }
  Serial.println();

  unsigned long startTime = millis();
  byte receivedBytes[expectedLen];
  size_t receivedCount = 0;

  while (millis() - startTime < timeout) {
    if (Serial2.available()) {
      receivedBytes[receivedCount] = Serial2.read();
      Serial.printf("Received byte: %02X (expected %02X)\n", receivedBytes[receivedCount], expectedResponse[receivedCount]);  // Debugging received bytes

      // Check if the received byte matches the expected byte at the current position
      if (receivedBytes[receivedCount] == expectedResponse[receivedCount]) {
        receivedCount++;
        if (receivedCount == expectedLen) {
          Serial.println("Scanner response matched!");
          return true;  // Command successful, response received and matched
        }
      } else {
        // Mismatch: reset count and start looking for the response from the beginning
        Serial.println("Scanner response mismatch, resetting buffer.");
        receivedCount = 0;  // Reset to look for the start of the response again
        // Important: If the mismatch happens early, you might need to consume the rest of the current
        // invalid sequence or implement a more robust state machine for complex responses.
        // For a fixed, short response, resetting is usually fine.
      }
    }
    delay(1);  // Small delay to prevent busy-waiting
  }
  Serial.println("Scanner command response timed out or mismatch.");
  return false;  // Timeout or mismatch
}


char processMultiTap(char key) {
  unsigned long currentTime = millis();
  unsigned long timeDiff = currentTime - lastPressTime;
  char charToReturn = '\0';  // This will be the actual character returned by the function

  // --- Logic to handle outputting the *previously* pending character ---
  // If there's a character pending and the timeout has passed,
  // or if a new key is pressed (and it's not the same as the last one being tapped),
  // then output the pending character.
  if (pendingOutputChar != '\0' && (key != lastKey || timeDiff > MULTI_TAP_TIMEOUT)) {
    charToReturn = pendingOutputChar;  // Set the character to be returned
    pendingOutputChar = '\0';          // Clear the pending character
    outputReadyTime = 0;               // Reset the ready time
  }

  // --- Multi-tap detection for the *current* key press ---
  // Reset tap count if a new key is pressed or timeout (500ms) for current key processing
  if (key != lastKey || timeDiff > MULTI_TAP_TIMEOUT) {
    tapCount = 0;
    lastKey = key;
  }

  // Find the key's position in the keymap
  for (byte i = 0; i < ROWS; i++) {
    for (byte j = 0; j < COLS; j++) {
      if (keys[i][j] == key) {

        // Get the character from the multi-tap sequence
        char selectedChar = keyChars[i][j][tapCount];  // Directly use tapCount

        tapCount++;

        // Wrap around if exceeding max taps
        byte maxTaps = strlen(keyChars[i][j]);
        if (tapCount >= maxTaps) tapCount = 0;

        // This selectedChar is now the one that *would* be output if the user stops tapping
        pendingOutputChar = selectedChar;
        outputReadyTime = currentTime;  // Mark when this character became pending
        break;
      }
    }
  }

  lastPressTime = currentTime;  // Update the last press time for the current key

  // --- Final check for the last character if loop ends without a new key or timeout ---
  // This handles the very last sequence of taps where no new key is pressed.
  // We check this in `loop()` where `processMultiTap` is called.
  // `charToReturn` will hold the character from the *previous* completed sequence.
  return charToReturn;
}

#include <esp32-hal-i2c.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "DFRobot_AXP313A.h"


#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3  // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "HUAWEI-E456";
const char* password = "51B7QLRH371";

void startCameraServer();
void setupLedFlash(int pin);

DFRobot_AXP313A axp;

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
byte rowPins[ROWS] = { 14, 13, 3, 38 };  // R1, R2, R3, R4
// Connect the keypad columns to these ESP32 pins
byte colPins[COLS] = { 7, 18, 12 };  // C1, C2, C3

// Create the Keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


// ===========================
// I2C LCD configuration
// ===========================
LiquidCrystal_I2C lcd(0x27, 16, 2);


void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  while (axp.begin() != 0) {
    Serial.println("init error");
    delay(1000);
  }
  axp.enableCameraPower(axp.eOV2640);  // Setting Up the Camera Power

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.sccb_i2c_port = 0; // added this
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 8000000;  // 20000000; reduced the xclk_freq
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t* s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif


  // setup LCD
  lcd.begin();
  lcd.backlight();


  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting......");

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");


  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected WiFi.");
  lcd.setCursor(0, 1);
  lcd.print("");

  keypad.setHoldTime(2000);

  delay(1000);
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
      // the user has pressed the enter key, therefore capture and send the image for processing
      captureAndSendPhoto();
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
  lcd.print("#:Capture & Send");
  lcd.setCursor(0, 1);
  lcd.print("*:Manual Mode...");

  Serial.println();
  Serial.println("#:Capture & Send");
  Serial.println("*:Manual Mode...\n");
}

void captureAndSendPhoto() {
  // ==================================
  // Remote Upload Server configuration
  // ==================================
  String serverName = "http://192.168.8.101:8000";
  String uploadServerPath = serverName + "/api/upload/";

  WiFiClient client;
  HTTPClient http;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Capturing image.");
  lcd.setCursor(0, 1);
  lcd.print("Please wait.....");


  camera_fb_t* fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Capture failed..");
    lcd.setCursor(0, 1);
    lcd.print("Try again.......");

    delay(1000);
    printDefaults();
    // ESP.restart();
    return; // Exit the function if capture fails
  }

  if (WiFi.status() == WL_CONNECTED) {

    http.begin(client, uploadServerPath.c_str());
    http.addHeader("Content-Type", "image/jpeg");  // Set the content type for JPEG

    int httpResponseCode = http.POST(fb->buf, fb->len);
    esp_camera_fb_return(fb);  // Return the frame buffer

    if (httpResponseCode == 200) {
      String payload = http.getString();
      proccessServerResponse(payload);
    }

    else {
      // can't connect to internet, timeout
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Timeout.......");
      lcd.setCursor(0, 1);
      lcd.print("CAN'T CONNECT.");

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
}



void sendManualVerification() {
  // function to send manual verification to the server
  // simple send an http post to the /manual-verification endpoint with the driver's barcode tag
  Serial.println("User has entered manual mode, begin to process keypad to the server\n");

  // ==================================
  // Remote Upload Server configuration
  // ==================================
  String serverName = "http://192.168.8.101:8000";
  String manualVerificationPath = serverName + "/api/manual-verification/";

  if (WiFi.status() == WL_CONNECTED) {

    WiFiClient client;
    HTTPClient http;

    // capture keypad press
    String manualBarcode = "";
    char key;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter code......");
    lcd.setCursor(0, 1);
    lcd.print("#: Done *: Clear");
    Serial.println("Enter code via keypad (#: finish), (*: clear):");

    while (true) {
      key = keypad.getKey();
      if (key) {
        if (key == '#' && manualBarcode.length() > 0) {
          Serial.println("\n\nDone...");
          // user has confirmed barcode, therefore procceed and process the request to the server
          break;  // Exit loop when '#' is pressed
        } else if (key == '*') {
          if (manualBarcode.length() == 0) {
            // if the user previously cleared, exit now

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Exiting manual.");
            lcd.setCursor(0, 1);
            lcd.print("barcode input..");
            delay(2000);

            printDefaults();
            return;  // the user has cancelled manual input mode.
          }

          manualBarcode = "";

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Barcode cleared");
          lcd.setCursor(0, 1);
          lcd.print("#: Done *: Exit");
        } else {
          if (key != '#')
            manualBarcode += key;

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("->");
          lcd.print(manualBarcode);
          lcd.setCursor(0, 1);
          lcd.print("#: Done *: Clear");
        }
      }
      delay(50);  // Debounce
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sending barcode.");
    lcd.setCursor(0, 1);
    lcd.print("Please wait.....");
    delay(1000);


    http.begin(client, manualVerificationPath.c_str());
    // configure http, set content-type to JSON
    http.addHeader("Content-Type", "application/json");


    // Construct HTTP Request Payload as JSON
    // Server expecting this format => {"barcode": "212313131"}
    String httpRequestData = "{\"barcode\": \"";
    httpRequestData.concat(manualBarcode);
    httpRequestData.concat("\"}");


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

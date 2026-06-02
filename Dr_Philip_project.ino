/*
 * ESP32-S3 Adaptive Neural-PID Controller
 * Hardware: ESP32-S3, JGA25-370, L298N, INA219, SD Card, OLED
 * Features: 1D Kalman Filters for Sensor Fusion, Safe AI Retraining
 */

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA219.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include "webpage.h"
#include "ai_model.h"

// ================= Pin Definitions =================
#define ENCODER_A 42
#define ENCODER_B 41
#define ENA_PIN 40
#define IN1_PIN 39
#define IN2_PIN 38
#define I2C_SDA 8
#define I2C_SCL 9
#define SD_CS   10
#define SD_MOSI 11
#define SD_SCK  12
#define SD_MISO 13

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_INA219 ina219;
SPIClass spi = SPIClass(FSPI);
WebServer server(80);
AIMotorController aiModel; 

const int pwmFreq = 5000;
const int pwmResolution = 8; 
const float GEAR_RATIO = 65.0; 
const float BASE_PPR = 11.0;   
const float COUNTS_PER_REV = GEAR_RATIO * BASE_PPR;

const char* ssid = "AI_Motor_Network";
const char* password = "12345678";

// ================= Global Variables =================
volatile long encoderCount = 0;
long lastEncoderCount = 0;
unsigned long lastTime = 0;

float currentRPM = 0.0;
float targetRPM = 0.0; 
float current_mA = 0.0;
int currentPWM = 0; 
bool sdInitialized = false;
bool aiMode = false; 

// PID Variables
float integralError = 0;
float prevError = 0;
PIDGains currentGains = {1.5, 0.2, 0.05}; 

// ================= Kalman Filter Class =================
class SimpleKalmanFilter {
  private:
    float err_measure;
    float err_estimate;
    float q;
    float current_estimate;
    float last_estimate;
    float kalman_gain;

  public:
    SimpleKalmanFilter(float mea_e, float est_e, float q) {
      err_measure = mea_e;
      err_estimate = est_e;
      this->q = q;
    }

    float updateEstimate(float mea) {
      kalman_gain = err_estimate / (err_estimate + err_measure);
      current_estimate = last_estimate + kalman_gain * (mea - last_estimate);
      err_estimate =  (1.0 - kalman_gain) * err_estimate + abs(last_estimate - current_estimate) * q;
      last_estimate = current_estimate;
      return current_estimate;
    }
};

// Instantiate Kalman Filters 
// Parameters: (Measurement Noise, Estimation Error, Process Noise)
SimpleKalmanFilter rpmKalman(5.0, 5.0, 0.1); 
SimpleKalmanFilter currentKalman(10.0, 10.0, 0.2);

// ================= Interrupt =================
void IRAM_ATTR readEncoder() {
  if (digitalRead(ENCODER_B) > 0) encoderCount++;
  else encoderCount--;
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Wire.begin(I2C_SDA, I2C_SCL);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED failed"));
  } else {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0,0); display.println("Booting Edge AI..."); display.display();
  }
  
  if (!ina219.begin()) Serial.println(F("INA219 failed"));

  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), readEncoder, RISING);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  ledcAttach(ENA_PIN, pwmFreq, pwmResolution);

  spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spi)) {
    sdInitialized = false;
  } else {
    sdInitialized = true;
    File dataFile = SD.open("/motor_data.csv", FILE_APPEND);
    if (dataFile) {
      if (dataFile.size() == 0) dataFile.println("Timestamp_ms,TargetRPM,RealRPM,Current_mA,Kp,Ki,Kd,PWM,Mode");
      dataFile.close();
    }
  }

  WiFi.softAP(ssid, password);
  MDNS.begin("aimotor");

  server.on("/", HTTP_GET, []() { server.send(200, "text/html", INDEX_HTML); });
  
  server.on("/data", HTTP_GET, []() {
    String json = "{\"rpm\":" + String(currentRPM) + ",\"target\":" + String(targetRPM) + ",\"current\":" + String(current_mA) + 
                  ",\"kp\":" + String(currentGains.Kp) + ",\"ki\":" + String(currentGains.Ki) + ",\"kd\":" + String(currentGains.Kd) +
                  ",\"pwm\":" + String(currentPWM) + ",\"ai_mode\":" + (aiMode ? "true" : "false") + ",\"sd_ok\":" + (sdInitialized ? "true" : "false") + "}";
    server.send(200, "application/json", json);
  });
  
  server.on("/set_target", HTTP_GET, []() {
    if (server.hasArg("val")) {
      targetRPM = server.arg("val").toFloat();
      integralError = 0; 
      server.send(200, "text/plain", "Target Updated");
    }
  });

  server.on("/toggle_ai", HTTP_GET, []() {
    aiMode = !aiMode;
    server.send(200, "text/plain", aiMode ? "ON" : "OFF");
  });

  server.on("/clear", HTTP_GET, []() {
    if (SD.remove("/motor_data.csv")) {
      File f = SD.open("/motor_data.csv", FILE_WRITE);
      f.println("Timestamp_ms,TargetRPM,RealRPM,Current_mA,Kp,Ki,Kd,PWM,Mode");
      f.close();
      server.send(200, "text/plain", "Cleared");
    }
  });

  server.on("/download", HTTP_GET, []() {
    File file = SD.open("/motor_data.csv", FILE_READ);
    server.sendHeader("Content-Disposition", "attachment; filename=motor_data.csv");
    server.streamFile(file, "text/csv");
    file.close();
  });

  server.begin();
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  lastTime = millis();
}

// ================= Main Loop =================
void loop() {
  server.handleClient();

  unsigned long currentTime = millis();
  float dt = (currentTime - lastTime) / 1000.0;

  if (dt >= 0.1) { 
    noInterrupts();
    long currentCount = encoderCount;
    interrupts();

    // 1. Calculate Raw RPM
    float rawRPM = (((float)(currentCount - lastEncoderCount) / COUNTS_PER_REV) / dt) * 60.0;
    
    // 2. Apply Kalman Filter to RPM (Replaces EMA)
    currentRPM = rpmKalman.updateEstimate(rawRPM);

    lastEncoderCount = currentCount;
    lastTime = currentTime;

    // 3. Apply Kalman Filter to Current (mA)
    float rawCurrent = ina219.getCurrent_mA();
    current_mA = currentKalman.updateEstimate(rawCurrent);

    // ==========================================
    // PID & AI CONTROL LOGIC
    // ==========================================
    float error = targetRPM - currentRPM;
    
    if (targetRPM == 0) {
      currentPWM = 0;
      integralError = 0;
      error = 0;
    } else {
      integralError += error * dt;
      float derivativeError = (error - prevError) / dt;

      if (aiMode) {
        currentGains = aiModel.predictGains(currentRPM, current_mA);
        
        // Safety Retraining Deadband
        if (abs(error) > 3.0) {
            aiModel.onlineRetrain(error, 0.00001); 
        }
      } else {
        currentGains = {1.5, 0.2, 0.05}; 
      }

      float pid_output = (currentGains.Kp * error) + (currentGains.Ki * integralError) + (currentGains.Kd * derivativeError);
      currentPWM = constrain((int)pid_output, 0, 255);
    }
    
    prevError = error;
    ledcWrite(ENA_PIN, currentPWM);

    if (sdInitialized) {
      File dataFile = SD.open("/motor_data.csv", FILE_APPEND);
      if (dataFile) {
        dataFile.printf("%lu,%.1f,%.1f,%.1f,%.2f,%.3f,%.3f,%d,%d\n", 
          currentTime, targetRPM, currentRPM, current_mA, currentGains.Kp, currentGains.Ki, currentGains.Kd, currentPWM, aiMode ? 1 : 0);
        dataFile.close();
      }
    }
    updateOLED();
  }
}

// ================= Helper Functions =================
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); display.print("http://aimotor.local");
  display.drawLine(0, 9, 128, 9, WHITE);
  display.setCursor(0, 12); display.print("SP: "); display.print(targetRPM); display.print(" RPM");
  display.setCursor(0, 23); display.printf("RPM: %.1f | mA: %.1f\n", currentRPM, current_mA);
  display.setCursor(0, 34); display.printf("Kp:%.1f Ki:%.2f Kd:%.2f\n", currentGains.Kp, currentGains.Ki, currentGains.Kd);
  display.setCursor(0, 45); display.printf("PWM: %d [%s]\n", currentPWM, aiMode ? "AUTO" : "MAN");
  display.setCursor(0, 56); display.print(sdInitialized ? "SD: OK" : "SD: ERR");
  display.display();
}
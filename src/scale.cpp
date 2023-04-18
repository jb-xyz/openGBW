#include "scale.hpp"
#include <MathBuffer.h>
#include <AiEsp32RotaryEncoder.h>
#include <Preferences.h>

HX711 loadcell;
SimpleKalmanFilter kalmanFilter(0.2, 0.2, 0.05);

Preferences preferences;


AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

#define ABS(a) (((a) > 0) ? (a) : ((a) * -1))

TaskHandle_t ScaleTask;
TaskHandle_t ScaleStatusTask;

double scaleWeight = 0;
double setWeight = 0;
double cupWeight = 0;
double offset = 0;
unsigned long scaleLastUpdatedAt = 0;
unsigned long lastSignificantWeightChangeAt = 0;
unsigned long lastTareAt = 0; // if 0, should tare load cell, else represent when it was last tared
bool scaleReady = false;
int scaleStatus = STATUS_EMPTY;
double cupWeightEmpty = 0;
unsigned long startedGrindingAt = 0;
unsigned long finishedGrindingAt = 0;
bool newOffset = false;
int encoderValue = 0;
MathBuffer<double, 100> weightHistory;
int currentMenuItem = 0;
int currentSetting;
MenuItem menuItems[4] = {{1, false, "Offset", 0.1},{2, false, "Cup weight", 1},{3, false, "Calibrate", 0}, {4, false, "Exit", 0}};


void rotary_onButtonClick()
{
  static unsigned long lastTimePressed = 0;
  // ignore multiple press in that time milliseconds
  if (millis() - lastTimePressed < 500)
  {
    return;
  }
  if(scaleStatus == STATUS_EMPTY){
    scaleStatus = STATUS_IN_MENU;
    currentMenuItem = 0;
  }
  else if(scaleStatus == STATUS_IN_MENU){
    if(currentMenuItem == 3){
      scaleStatus = STATUS_EMPTY;
      Serial.println("Exited Menu");
    }
    else if (currentMenuItem == 0){
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 0;
      Serial.println("Offset Menu");
    }
    else if (currentMenuItem == 1)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 1;
      Serial.println("Cup Menu");
    }
    else if (currentMenuItem == 2)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 2;
      Serial.println("Calibration Menu");
    }
  }
  else if(scaleStatus == STATUS_IN_SUBMENU){
    if(currentSetting == 0){
      preferences.begin("scale", false);
      preferences.putDouble("offset", offset);
      preferences.end();
      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
    else if (currentSetting == 1)
    {
      cupWeight = scaleWeight;
      Serial.println(cupWeight);
      preferences.begin("scale", false);
      preferences.putDouble("cup", cupWeight);
      preferences.end();
      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
    else if (currentSetting == 2)
    {
      preferences.begin("scale", false);
      double newCalibrationValue = preferences.getDouble("calibration", newCalibrationValue) * (scaleWeight / 100);
      Serial.println(newCalibrationValue);
      preferences.putDouble("calibration", newCalibrationValue);
      preferences.end();
      loadcell.set_scale(newCalibrationValue);
      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
  }
}



void rotary_loop()
{
  if (rotaryEncoder.encoderChanged())
  {
    if(scaleStatus == STATUS_EMPTY){
        int newValue = rotaryEncoder.readEncoder();
        Serial.print("Value: ");
        
        setWeight -= ((float)newValue - (float)encoderValue) / 10;
        
        encoderValue = newValue;
        Serial.println(newValue);
        preferences.begin("scale", false);
        preferences.putDouble("setWeight", setWeight);
        preferences.end();
      }
    else if(scaleStatus == STATUS_IN_MENU){
      int newValue = rotaryEncoder.readEncoder();
      currentMenuItem = (currentMenuItem + (newValue - encoderValue)) % 4;
      currentMenuItem = currentMenuItem < 0 ? 4 + currentMenuItem : currentMenuItem;
      encoderValue = newValue;
      Serial.println(currentMenuItem);
    }
    else if(scaleStatus == STATUS_IN_SUBMENU){
      if(currentSetting == 0){ //offset menu
        int newValue = rotaryEncoder.readEncoder();
        Serial.print("Value: ");

        offset -= ((float)newValue - (float)encoderValue) / 100;
        encoderValue = newValue;
      }
    }
  }
  if (rotaryEncoder.isEncoderButtonClicked())
  {
    rotary_onButtonClick();
  }
}

void readEncoderISR()
{
  rotaryEncoder.readEncoder_ISR();
}

void tareScale() {
  Serial.println("Taring scale");
  loadcell.tare(TARE_MEASURES);
  lastTareAt = millis();
}

void updateScale( void * parameter) {
  float lastEstimate;


  for (;;) {
    if (lastTareAt == 0) {
      Serial.println("retaring scale");
      Serial.println("current offset");
      Serial.println(offset);
      tareScale();
    }
    if (loadcell.wait_ready_timeout(300)) {
      lastEstimate = kalmanFilter.updateEstimate(loadcell.get_units());
      scaleWeight = lastEstimate;
      scaleLastUpdatedAt = millis();
      weightHistory.push(scaleWeight);
      scaleReady = true;
    } else {
      Serial.println("HX711 not found.");
      scaleReady = false;
    }
  }
}

void grinderToggle()
{
  digitalWrite(GRINDER_ACTIVE_PIN, 1);
  delay(100);
  digitalWrite(GRINDER_ACTIVE_PIN, 0);
}

void scaleStatusLoop(void *p) {
  double tenSecAvg;
  for (;;) {
    tenSecAvg = weightHistory.averageSince((int64_t)millis() - 10000);
    // Serial.printf("Avg: %f, currentWeight: %f\n", tenSecAvg, scaleWeight);

    if (ABS(tenSecAvg - scaleWeight) > SIGNIFICANT_WEIGHT_CHANGE) {
      // Serial.printf("Detected significant change: %f\n", ABS(avg - scaleWeight));
      lastSignificantWeightChangeAt = millis();
    }


    if (scaleStatus == STATUS_EMPTY) {
      if (millis() - lastTareAt > TARE_MIN_INTERVAL && ABS(tenSecAvg) > 0.2 && tenSecAvg < 3 && scaleWeight < 3) {
        // tare if: not tared recently, more than 0.2 away from 0, less than 3 grams total (also works for negative weight)
        lastTareAt = 0;
      }

      if (ABS(weightHistory.minSince((int64_t)millis() - 1000) - cupWeight) < CUP_DETECTION_TOLERANCE &&
          ABS(weightHistory.maxSince((int64_t)millis() - 1000) - cupWeight) < CUP_DETECTION_TOLERANCE)
      {
        // using average over last 500ms as empty cup weight
        Serial.println("Starting grinding");
        cupWeightEmpty = weightHistory.averageSince((int64_t)millis() - 500);
        scaleStatus = STATUS_GRINDING_IN_PROGRESS;
        newOffset = true;
        startedGrindingAt = millis();
        //digitalWrite(GRINDER_ACTIVE_PIN, 1);
        grinderToggle();
        continue;
      }
    } else if (scaleStatus == STATUS_GRINDING_IN_PROGRESS) {
      if (!scaleReady) {
        //digitalWrite(GRINDER_ACTIVE_PIN, 0);
        grinderToggle();
        scaleStatus = STATUS_GRINDING_FAILED;
      }

      if (millis() - startedGrindingAt > MAX_GRINDING_TIME) {
        Serial.println("Failed because grinding took too long");
        //digitalWrite(GRINDER_ACTIVE_PIN, 0);
        grinderToggle();
        scaleStatus = STATUS_GRINDING_FAILED;
        continue;
      }

      if (
        millis() - startedGrindingAt > 2000 && // started grinding at least 2s ago
        scaleWeight - weightHistory.firstValueOlderThan(millis() - 2000) < 1 // less than a gram has been grinded in the last 2 second
      ) {
        Serial.println("Failed because no change in weight was detected");
        //digitalWrite(GRINDER_ACTIVE_PIN, 0);
        grinderToggle();
        scaleStatus = STATUS_GRINDING_FAILED;
        continue;
      }

      if (weightHistory.minSince((int64_t)millis() - 200) < cupWeightEmpty - CUP_DETECTION_TOLERANCE) {
        Serial.printf("Failed because weight too low, min: %f, min value: %f\n", weightHistory.minSince((int64_t)millis() - 200), CUP_WEIGHT + CUP_DETECTION_TOLERANCE);
        //digitalWrite(GRINDER_ACTIVE_PIN, 0);
        grinderToggle();
        scaleStatus = STATUS_GRINDING_FAILED;
        continue;
      }
      if (weightHistory.maxSince((int64_t)millis() - 200) >= cupWeightEmpty + setWeight + offset) {
        Serial.println("Finished grinding");
        finishedGrindingAt = millis();
        //digitalWrite(GRINDER_ACTIVE_PIN, 0);
        grinderToggle();
        scaleStatus = STATUS_GRINDING_FINISHED;
        continue;
      }
    } else if (scaleStatus == STATUS_GRINDING_FINISHED) {
      double currentWeight = weightHistory.averageSince((int64_t)millis() - 500);
      if (scaleWeight < 5) {
        Serial.println("Going back to empty");
        scaleStatus = STATUS_EMPTY;
        continue;
      }
      else if (currentWeight != setWeight + cupWeightEmpty && millis() - finishedGrindingAt > 1500 && newOffset)
      {
        offset += setWeight + cupWeightEmpty - currentWeight;
        preferences.begin("scale", false);
        preferences.putDouble("offset", offset);
        preferences.end();
        newOffset = false;
      }
    } else if (scaleStatus == STATUS_GRINDING_FAILED) {
      if (scaleWeight >= GRINDING_FAILED_WEIGHT_TO_RESET) {
        Serial.println("Going back to empty");
        scaleStatus = STATUS_EMPTY;
        continue;
      }
    }
    rotary_loop();
    delay(50);
  }
}



void setupScale() {
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  // set boundaries and if values should cycle or not
  // in this example we will set possible values between 0 and 1000;
  bool circleValues = true;
  rotaryEncoder.setBoundaries(-1000, 1000, circleValues); // minValue, maxValue, circleValues true|false (when max go to min and vice versa)

  /*Rotary acceleration introduced 25.2.2021.
   * in case range to select is huge, for example - select a value between 0 and 1000 and we want 785
   * without accelerateion you need long time to get to that number
   * Using acceleration, faster you turn, faster will the value raise.
   * For fine tuning slow down.
   */
  // rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
  rotaryEncoder.setAcceleration(100); // or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration


  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  pinMode(GRINDER_ACTIVE_PIN, OUTPUT);
  digitalWrite(GRINDER_ACTIVE_PIN, 0);

  preferences.begin("scale", false);
  double scaleFactor = preferences.getDouble("calibration", (double)LOADCELL_SCALE_FACTOR);
  loadcell.set_scale(scaleFactor);
  setWeight = preferences.getDouble("setWeight", (double)COFFEE_DOSE_WEIGHT);
  offset = preferences.getDouble("offset", (double)COFFEE_DOSE_OFFSET);
  cupWeight = preferences.getDouble("cup", (double)cupWeight);
  preferences.end();

  xTaskCreatePinnedToCore(
      updateScale, /* Function to implement the task */
      "Scale",     /* Name of the task */
      10000,       /* Stack size in words */
      NULL,        /* Task input parameter */
      0,           /* Priority of the task */
      &ScaleTask,  /* Task handle. */
      1);          /* Core where the task should run */

  xTaskCreatePinnedToCore(
      scaleStatusLoop, /* Function to implement the task */
      "ScaleStatus", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &ScaleStatusTask,  /* Task handle. */
      1);            /* Core where the task should run */
}

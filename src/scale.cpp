#include "scale.hpp"
#include <MathBuffer.h>
#include <AiEsp32RotaryEncoder.h>
#include <Preferences.h>

HX711 loadcell;
SimpleKalmanFilter kalmanFilter(0.02, 0.02, 0.01);

Preferences preferences;


AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

#define ABS(a) (((a) > 0) ? (a) : ((a) * -1))

TaskHandle_t ScaleTask;
TaskHandle_t ScaleStatusTask;

double scaleWeight = 0; //current weight
double setWeight = 0; //desired amount of coffee
double setCupWeight = 0; //cup weight set by user
double offset = 0; //stop x grams prios to set weight
bool scaleMode = false; //use as regular scale with timer if true
bool grindMode = false;  //false for impulse to start/stop grinding, true for continuous on while grinding
bool grinderActive = false; //needed for continuous mode
MathBuffer<double, 100> weightHistory;

unsigned long scaleLastUpdatedAt = 0;
unsigned long lastSignificantWeightChangeAt = 0;
unsigned long lastTareAt = 0; // if 0, should tare load cell, else represent when it was last tared
bool scaleReady = false;
int scaleStatus = STATUS_EMPTY;
double cupWeightEmpty = 0; //measured actual cup weight
unsigned long startedGrindingAt = 0;
unsigned long finishedGrindingAt = 0;
int encoderDir = 1;
bool greset = false;

bool newOffset = false;

int currentMenuItem = 0;
int currentSetting;
int encoderValue = 0;
int menuItemsCount = 7;
MenuItem menuItems[7] = {
    {1, false, "Cup weight", 1, &setCupWeight},
    {2, false, "Calibrate", 0},
    {3, false, "Offset", 0.1, &offset},
    {4, false, "Scale Mode", 0},
    {5, false, "Grinding Mode", 0},
    {6, false, "Exit", 0},
    {7, false, "Reset", 0}}; // structure is mostly useless for now, plan on making menu easier to customize later

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
    rotaryEncoder.setAcceleration(0);
  }
  else if(scaleStatus == STATUS_IN_MENU){
    if(currentMenuItem == 5){
      scaleStatus = STATUS_EMPTY;
      rotaryEncoder.setAcceleration(100);
      Serial.println("Exited Menu");
    }
    else if (currentMenuItem == 2){
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 2;
      Serial.println("Offset Menu");
    }
    else if (currentMenuItem == 0)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 0;
      Serial.println("Cup Menu");
    }
    else if (currentMenuItem == 1)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 1;
      Serial.println("Calibration Menu");
    }
    else if (currentMenuItem == 3)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 3;
      Serial.println("Scale Mode Menu");
    }
    else if (currentMenuItem == 4)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 4;
      Serial.println("Grind Mode Menu");
    }
    else if (currentMenuItem == 6)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 6;
      greset = false;
      Serial.println("Reset Menu");
    }
  }
  else if(scaleStatus == STATUS_IN_SUBMENU){
    if(currentSetting == 2){

      preferences.begin("scale", false);
      preferences.putDouble("offset", offset);
      preferences.end();
      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
    else if (currentSetting == 0)
    {
      if(scaleWeight > 30){       //prevent accidental setting with no cup
        setCupWeight = scaleWeight;
        Serial.println(setCupWeight);
        
        preferences.begin("scale", false);
        preferences.putDouble("cup", setCupWeight);
        preferences.end();
        scaleStatus = STATUS_IN_MENU;
        currentSetting = -1;
      }
    }
    else if (currentSetting == 1)
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
    else if (currentSetting == 3)
    {
      preferences.begin("scale", false);
      preferences.putBool("scaleMode", scaleMode);
      preferences.end();
      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
    else if (currentSetting == 4)
    {
      preferences.begin("scale", false);
      preferences.putBool("grindMode", grindMode);
      preferences.end();
      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
    else if (currentSetting == 6)
    {
      if(greset){
        preferences.begin("scale", false);
        preferences.putDouble("calibration", (double)LOADCELL_SCALE_FACTOR);
        setWeight = (double)COFFEE_DOSE_WEIGHT;
        preferences.putDouble("setWeight", (double)COFFEE_DOSE_WEIGHT);
        offset = (double)COFFEE_DOSE_OFFSET;
        preferences.putDouble("offset", (double)COFFEE_DOSE_OFFSET);
        setCupWeight = (double)CUP_WEIGHT;
        preferences.putDouble("cup", (double)CUP_WEIGHT);
        scaleMode = false;
        preferences.putBool("scaleMode", false);
        grindMode = false;
        preferences.putBool("grindMode", false);

        loadcell.set_scale((double)LOADCELL_SCALE_FACTOR);
        preferences.end();
      }
      
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

        setWeight += ((float)newValue - (float)encoderValue) / 10 * encoderDir;

        encoderValue = newValue;
        Serial.println(newValue);
        preferences.begin("scale", false);
        preferences.putDouble("setWeight", setWeight);
        preferences.end();
      }
    else if(scaleStatus == STATUS_IN_MENU){
      int newValue = rotaryEncoder.readEncoder();
      currentMenuItem = (currentMenuItem + (newValue - encoderValue) * -encoderDir) % menuItemsCount;
      currentMenuItem = currentMenuItem < 0 ? menuItemsCount + currentMenuItem : currentMenuItem;
      encoderValue = newValue;
      Serial.println(currentMenuItem);
    }
    else if(scaleStatus == STATUS_IN_SUBMENU){
      if(currentSetting == 2){ //offset menu
        int newValue = rotaryEncoder.readEncoder();
        Serial.print("Value: ");

        offset += ((float)newValue - (float)encoderValue) * encoderDir / 100;
        encoderValue = newValue;

        if(abs(offset) >= setWeight){
          offset = setWeight;     //prevent nonsensical offsets
        }
      }
      else if(currentSetting == 3){
        scaleMode = !scaleMode;
      }
      else if (currentSetting == 4)
      {
        grindMode = !grindMode;
      }
      else if (currentSetting == 6)
      {
        greset = !greset;
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
      lastEstimate = kalmanFilter.updateEstimate(loadcell.get_units(5));
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
  if(!scaleMode){
    if(grindMode){
      grinderActive = !grinderActive;
      digitalWrite(GRINDER_ACTIVE_PIN, grinderActive);
    }
    else{
      digitalWrite(GRINDER_ACTIVE_PIN, 1);
      delay(100);
      digitalWrite(GRINDER_ACTIVE_PIN, 0);
    }
  }
}

void scaleStatusLoop(void *p) {
  double tenSecAvg;
  for (;;) {
    tenSecAvg = weightHistory.averageSince((int64_t)millis() - 10000);
    

    if (ABS(tenSecAvg - scaleWeight) > SIGNIFICANT_WEIGHT_CHANGE) {
      
      lastSignificantWeightChangeAt = millis();
    }

    if (scaleStatus == STATUS_EMPTY) {
      if (millis() - lastTareAt > TARE_MIN_INTERVAL && ABS(tenSecAvg) > 0.2 && tenSecAvg < 3 && scaleWeight < 3) {
        // tare if: not tared recently, more than 0.2 away from 0, less than 3 grams total (also works for negative weight)
        lastTareAt = 0;
      }

      if (ABS(weightHistory.minSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE &&
          ABS(weightHistory.maxSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE)
      {
        // using average over last 500ms as empty cup weight
        Serial.println("Starting grinding");
        cupWeightEmpty = weightHistory.averageSince((int64_t)millis() - 500);
        scaleStatus = STATUS_GRINDING_IN_PROGRESS;
        
        if(!scaleMode){
          newOffset = true;
          startedGrindingAt = millis();
        }
        
        grinderToggle();
        continue;
      }
    } else if (scaleStatus == STATUS_GRINDING_IN_PROGRESS) {
      if (!scaleReady) {
        
        grinderToggle();
        scaleStatus = STATUS_GRINDING_FAILED;
      }
      //Serial.printf("Scale mode: %d\n", scaleMode);
      //Serial.printf("Started grinding at: %d\n", startedGrindingAt);
      //Serial.printf("Weight: %f\n", cupWeightEmpty - scaleWeight);
      if (scaleMode && startedGrindingAt == 0 && scaleWeight - cupWeightEmpty >= 0.1)
      {
        Serial.printf("Started grinding at: %d\n", millis());
        startedGrindingAt = millis();
        continue;
      }

      if (millis() - startedGrindingAt > MAX_GRINDING_TIME && !scaleMode) {
        Serial.println("Failed because grinding took too long");
        
        grinderToggle();
        scaleStatus = STATUS_GRINDING_FAILED;
        continue;
      }

      if (
          millis() - startedGrindingAt > 2000 &&                                  // started grinding at least 2s ago
          scaleWeight - weightHistory.firstValueOlderThan(millis() - 2000) < 1 && // less than a gram has been grinded in the last 2 second
          !scaleMode)
      {
        Serial.println("Failed because no change in weight was detected");
        
        grinderToggle();
        scaleStatus = STATUS_GRINDING_FAILED;
        continue;
      }

      if (weightHistory.minSince((int64_t)millis() - 200) < cupWeightEmpty - CUP_DETECTION_TOLERANCE && !scaleMode) {
        Serial.printf("Failed because weight too low, min: %f, min value: %f\n", weightHistory.minSince((int64_t)millis() - 200), CUP_WEIGHT + CUP_DETECTION_TOLERANCE);
        
        grinderToggle();
        scaleStatus = STATUS_GRINDING_FAILED;
        continue;
      }
      double currentOffset = offset;
      if(scaleMode){
        currentOffset = 0;
      }
      if (weightHistory.maxSince((int64_t)millis() - 200) >= cupWeightEmpty + setWeight + currentOffset) {
        Serial.println("Finished grinding");
        finishedGrindingAt = millis();
        
        grinderToggle();
        scaleStatus = STATUS_GRINDING_FINISHED;
        continue;
      }
    } else if (scaleStatus == STATUS_GRINDING_FINISHED) {
      double currentWeight = weightHistory.averageSince((int64_t)millis() - 500);
      if (scaleWeight < 5) {
        Serial.println("Going back to empty");
        startedGrindingAt = 0;
        scaleStatus = STATUS_EMPTY;
        continue;
      }
      else if (currentWeight != setWeight + cupWeightEmpty && millis() - finishedGrindingAt > 1500 && newOffset)
      {
        offset += setWeight + cupWeightEmpty - currentWeight;
        if(ABS(offset) >= setWeight){
          offset = COFFEE_DOSE_OFFSET;
        }
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
  rotaryEncoder.setBoundaries(-10000, 10000, circleValues); // minValue, maxValue, circleValues true|false (when max go to min and vice versa)

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
  setWeight = preferences.getDouble("setWeight", (double)COFFEE_DOSE_WEIGHT);
  offset = preferences.getDouble("offset", (double)COFFEE_DOSE_OFFSET);
  setCupWeight = preferences.getDouble("cup", (double)CUP_WEIGHT);
  scaleMode = preferences.getBool("scaleMode", false);
  grindMode = preferences.getBool("grindMode", false);

  preferences.end();
  
  loadcell.set_scale(scaleFactor);

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

/* Title: MIDILOOP
 * Author: Taylor Moss
 * Company: MOSSTECH
 * 
 */

#include <Wire.h>
#include <IOshield.h>
#include <TimerOne.h>
#include <LiquidCrystal.h>
#include <math.h>
#include <EEPROM.h>


#define MIDI_TIMING_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP 0xFC
#define MIDI_NOTE_ON 0x9
#define MIDI_NOTE_OFF 0x8
#define CLOCKS_PER_BEAT 24

//Assign local pins
#define BUTTON_TRACK_1_PIN 5
#define BUTTON_TRACK_2_PIN 6
#define BUTTON_TRACK_3_PIN 7
#define BUTTON_TRACK_4_PIN 8
#define BUTTON_RECORD_PIN 9
#define BUTTON_PLAY_PIN 10
#define BUTTON_STOP_PIN 11
#define CHAR_LCD_RS_PIN 12
#define CHAR_LCD_EN_PIN 13
#define CHAR_LCD_DB4_PIN 17
#define CHAR_LCD_DB5_PIN 16
#define CHAR_LCD_DB6_PIN 15
#define CHAR_LCD_DB7_PIN 14


//Assign remote pins
#define IO_TEST_PIN 0
#define RECORD_LED_TRACK_1_PIN 8
#define RECORD_LED_TRACK_2_PIN 9
#define RECORD_LED_TRACK_3_PIN 10
#define RECORD_LED_TRACK_4_PIN 11
#define PLAY_LED_TRACK_1_PIN 12
#define PLAY_LED_TRACK_2_PIN 13
#define PLAY_LED_TRACK_3_PIN 14
#define PLAY_LED_TRACK_4_PIN 15

#define BUTTON_LCD_UP_PIN 4
#define BUTTON_LCD_DOWN_PIN 5
#define BUTTON_LCD_LEFT_PIN 6
#define BUTTON_LCD_RIGHT_PIN 7

//Assign EEPROM registers
#define SETTINGS_ADDRESS_BARS 0
#define SETTINGS_ADDRESS_QDENOM 1
#define SETTINGS_TRACK_1_CHANNEL 2
#define SETTINGS_TRACK_2_CHANNEL 3
#define SETTINGS_TRACK_3_CHANNEL 4
#define SETTINGS_TRACK_4_CHANNEL 5

#define MAX_MESSAGES_PER_TRACK  32
#define NUMBER_OF_TRACKS 4

struct midi_message
{
  byte command;
  byte note;
  byte velocity;
  int timestamp;
};

struct user_settings
{
  int bars;
  int quantizeDenominator;

  int trackChannel[NUMBER_OF_TRACKS];
};

bool trackButtonState[NUMBER_OF_TRACKS];
bool trackButtonState_LS[NUMBER_OF_TRACKS];

bool recordButtonState = false;
bool recordButtonState_LS = false;
bool playButtonState = false;
bool playButtonState_LS = false;
bool stopButtonState = false;
bool stopButtonState_LS = false;
bool settingsUpButtonState = false;
bool settingsUpButtonState_LS = false;
bool settingsDownButtonState = false;
bool settingsDownButtonState_LS = false;
bool settingsLeftButtonState = false;
bool settingsLeftButtonState_LS = false;
bool settingsRightButtonState = false;
bool settingsRightButtonState_LS = false;

user_settings settings;
int settingsScreen = 0;

int bpm = 120;
long intervalMicroSeconds = 0;

bool indy_recordArmed[NUMBER_OF_TRACKS];
bool indy_recordActive[NUMBER_OF_TRACKS];
bool indy_playActive[NUMBER_OF_TRACKS];


bool trackDataRecorded[NUMBER_OF_TRACKS];
bool dataRecorded = false;

int midiMessage_Counter[NUMBER_OF_TRACKS];
midi_message midiMessages[NUMBER_OF_TRACKS][MAX_MESSAGES_PER_TRACK];


unsigned long recordStartTime[NUMBER_OF_TRACKS];
unsigned long recordEndTime[NUMBER_OF_TRACKS];

long playbackStartTime = 0;
long playbackLastScanTime[NUMBER_OF_TRACKS];

LiquidCrystal lcd(CHAR_LCD_RS_PIN, CHAR_LCD_EN_PIN, CHAR_LCD_DB4_PIN, CHAR_LCD_DB5_PIN, CHAR_LCD_DB6_PIN, CHAR_LCD_DB7_PIN);
IOshield IO; // create 64shield object

void setup() {

  Serial.begin(31250); //Start serial
  Wire.begin(); //start I2C

  IO.initialize(); // set all registers to default;

  pinMode(BUTTON_TRACK_1_PIN, INPUT);
  pinMode(BUTTON_TRACK_2_PIN, INPUT);
  pinMode(BUTTON_TRACK_3_PIN, INPUT);
  pinMode(BUTTON_TRACK_4_PIN, INPUT);
  pinMode(RECORD_LED_TRACK_1_PIN, OUTPUT);
  pinMode(RECORD_LED_TRACK_2_PIN, OUTPUT);
  pinMode(RECORD_LED_TRACK_3_PIN, OUTPUT);
  pinMode(RECORD_LED_TRACK_4_PIN, OUTPUT);
  pinMode(PLAY_LED_TRACK_1_PIN, OUTPUT);
  pinMode(PLAY_LED_TRACK_2_PIN, OUTPUT);
  pinMode(PLAY_LED_TRACK_3_PIN, OUTPUT);
  pinMode(PLAY_LED_TRACK_4_PIN, OUTPUT);

  IO.pinMode(BUTTON_LCD_UP_PIN,INPUT);
  IO.pinMode(BUTTON_LCD_DOWN_PIN,INPUT);
  IO.pinMode(BUTTON_LCD_LEFT_PIN,INPUT);
  IO.pinMode(BUTTON_LCD_RIGHT_PIN,INPUT);
  IO.pinMode(RECORD_LED_TRACK_1_PIN, OUTPUT);
  IO.pinMode(RECORD_LED_TRACK_2_PIN, OUTPUT);
  IO.pinMode(RECORD_LED_TRACK_3_PIN, OUTPUT);
  IO.pinMode(RECORD_LED_TRACK_4_PIN, OUTPUT);
  IO.pinMode(PLAY_LED_TRACK_1_PIN, OUTPUT);
  IO.pinMode(PLAY_LED_TRACK_2_PIN, OUTPUT);
  IO.pinMode(PLAY_LED_TRACK_3_PIN, OUTPUT);
  IO.pinMode(PLAY_LED_TRACK_4_PIN, OUTPUT);
  IO.pinMode(IO_TEST_PIN, OUTPUT);
  
  IO.digitalWrite(RECORD_LED_TRACK_1_PIN, LOW);
  IO.digitalWrite(RECORD_LED_TRACK_2_PIN, LOW);
  IO.digitalWrite(RECORD_LED_TRACK_3_PIN, LOW);
  IO.digitalWrite(RECORD_LED_TRACK_4_PIN, LOW);
  IO.digitalWrite(PLAY_LED_TRACK_1_PIN, LOW);
  IO.digitalWrite(PLAY_LED_TRACK_2_PIN, LOW);
  IO.digitalWrite(PLAY_LED_TRACK_3_PIN, LOW);
  IO.digitalWrite(PLAY_LED_TRACK_4_PIN, LOW);
  IO.digitalWrite(IO_TEST_PIN, HIGH);
  
  // Attach the interrupt to send the MIDI clock and start the timer
  Timer1.initialize(intervalMicroSeconds);
  Timer1.setPeriod(calculateIntervalMicroSecs(bpm));
  Timer1.attachInterrupt(sendClockPulse);

  loadSettings();
  
  lcd.begin(16,2);
  updateSettingsItem(settingsScreen);

  

}

void loadSettings() {
  //load bars from eeprom; if value not 1-8, load 8 as default
  int bars = EEPROM.read(SETTINGS_ADDRESS_BARS);
  if (bars >= 1 && bars <= 8)
    settings.bars = bars;
  else
    settings.bars = 8;

  //load quantize denominator;  if value not 4-64, load 64 as default
  int quantize = EEPROM.read(SETTINGS_ADDRESS_QDENOM);
  if (quantize >= 4 && quantize <= 64)
    settings.quantizeDenominator = quantize;
  else
    settings.quantizeDenominator = 64;

  //load track 1 channel;  if value not 0-15, load 0 as default
  int track1Channel = EEPROM.read(SETTINGS_TRACK_1_CHANNEL);
  if (track1Channel >= 0 && track1Channel <= 15)
    settings.trackChannel[0] =  track1Channel;
  else
    settings.trackChannel[0] = 0;

  //load track 2 channel;  if value not 0-15, load 0 as default
  int track2Channel = EEPROM.read(SETTINGS_TRACK_2_CHANNEL);
  if (track2Channel >= 0 && track2Channel <= 15)
    settings.trackChannel[1] =  track2Channel;
  else
    settings.trackChannel[1] = 0;

  //load track 3 channel;  if value not 0-15, load 0 as default
  int track3Channel = EEPROM.read(SETTINGS_TRACK_3_CHANNEL);
  if (track3Channel >= 0 && track3Channel <= 15)
    settings.trackChannel[2] =  track3Channel;
  else
    settings.trackChannel[2] = 0;

  //load track 4 channel;  if value not 0-15, load 0 as default
  int track4Channel = EEPROM.read(SETTINGS_TRACK_4_CHANNEL);
  if (track4Channel >= 0 && track4Channel <= 15)
    settings.trackChannel[3] =  track4Channel;
  else
    settings.trackChannel[3] = 0;
}

void saveSettings() {
  EEPROM.write(SETTINGS_ADDRESS_BARS, settings.bars);
  EEPROM.write(SETTINGS_ADDRESS_QDENOM, settings.quantizeDenominator);
  EEPROM.write(SETTINGS_TRACK_1_CHANNEL, settings.trackChannel[0]);
  EEPROM.write(SETTINGS_TRACK_2_CHANNEL, settings.trackChannel[1]);
  EEPROM.write(SETTINGS_TRACK_3_CHANNEL, settings.trackChannel[2]);
  EEPROM.write(SETTINGS_TRACK_4_CHANNEL, settings.trackChannel[3]);
}


//send MIDI message
void midiMessage(byte command, byte note, byte velocity) {

  Serial.write(command);//send note on or note off command
  Serial.write(note);//send pitch data
  Serial.write(velocity);//send velocity data
}

void checkMIDI(int trackNum)
{
  if (midiMessage_Counter[trackNum] < MAX_MESSAGES_PER_TRACK) {
    do {
      if (Serial.available()) {
        byte command = 0;
        byte note = 0;
        byte velocity = 0;

        byte command_nibble1 = 0;
        byte command_nibble2 = 0;
        byte note_nibble1 = 0;
        byte note_nibble2 = 0;
        byte velocity_nibble1 = 0;
        byte velocity_nibble2 = 0;

        command = Serial.read();
        do {
          note = Serial.read();
        } while (note >= 128);
        do {
          velocity = Serial.read();
        } while (velocity >= 128);

        command_nibble1 = (byte) (command & 0x0F);  //extract midi message type
        command_nibble2 = (byte)((command & 0xF0) >> 4);  //extract midi message channel

        note_nibble1 = (byte) (note & 0x0F);  //extract midi message type
        note_nibble2 = (byte)((note & 0xF0) >> 4);  //extract midi message channel

        velocity_nibble1 = (byte) (velocity & 0x0F);  //extract midi message type
        velocity_nibble2 = (byte)((velocity & 0xF0) >> 4);  //extract midi message channel

        //check that command, note, and velocity channel are all available to the current track
        if ((int)command_nibble2 == settings.trackChannel[trackNum] && (int)note_nibble2 == settings.trackChannel[trackNum] && (int)velocity_nibble2 == settings.trackChannel[trackNum]) {

          if (command == MIDI_NOTE_ON || command == MIDI_NOTE_OFF) {

            if (indy_recordArmed[trackNum]) {
              RecordStart(trackNum);
            }

            midiMessages[trackNum][midiMessage_Counter[trackNum]].command = command;//read first byte
            midiMessages[trackNum][midiMessage_Counter[trackNum]].note = note;//read next byte
            midiMessages[trackNum][midiMessage_Counter[trackNum]].velocity = velocity;//read final byte
            //if master track is selected....
            if (trackNum == 0)
              midiMessages[trackNum][midiMessage_Counter[trackNum]].timestamp = millis() - recordStartTime[0]; //timestamp duration since recording began
            else {
              int barTotalTime = recordEndTime[0] - recordStartTime[0];
              if (millis() <= playbackStartTime + barTotalTime)
                midiMessages[trackNum][midiMessage_Counter[trackNum]].timestamp = millis() - playbackStartTime; // timestamp duration since bar started
              else
                midiMessages[trackNum][midiMessage_Counter[trackNum]].timestamp = millis() - playbackStartTime - barTotalTime;  //rollover timestamp duration since bar started
            }
            midiMessage_Counter[trackNum]++;

          }
        }
      }

    }
    while (Serial.available() > 2);//when at least three bytes available
  }
  else {
    RecordStop(trackNum);
    PlayStart(trackNum);
  }

}

void sendClockPulse() {
  // Write the timing clock byte
  Serial.write(MIDI_TIMING_CLOCK);
}


void quantizeMIDI(int trackNum) {

  int totalSteps = settings.bars * settings.quantizeDenominator;
  int barTotalTime = (recordEndTime[0] - recordStartTime[0]);

  float barFrequency = 1 / (float)barTotalTime;

  for (int i = 0; i < midiMessage_Counter[trackNum]; i++)
  {

    float stepRounded = round((float)midiMessages[trackNum][i].timestamp * totalSteps * barFrequency);
    float timestampQuantized = (stepRounded * barTotalTime) / totalSteps;
    midiMessages[trackNum][i].timestamp = timestampQuantized;
  }
}


void updateBPM(int trackNum) {
  int totalSteps = settings.bars * settings.quantizeDenominator;
  int barTotalTime = (recordEndTime[0] - recordStartTime[0]);
  bpm = 60L * 1000 / (4 * (barTotalTime / totalSteps));
}

long calculateIntervalMicroSecs(int bpm) {
  // Take care about overflows!
  return 60L * 1000 * 1000 / bpm / CLOCKS_PER_BEAT;
}

void playbackMIDI(int trackNum) {

  long timeSinceBarStart = millis() - (unsigned long)playbackStartTime;

  for (int i = 0; i < midiMessage_Counter[trackNum]; i++)
  {
    if (((long)midiMessages[trackNum][i].timestamp > playbackLastScanTime[trackNum] && (long)midiMessages[trackNum][i].timestamp <= timeSinceBarStart)) {
      midiMessage(midiMessages[trackNum][i].command, midiMessages[trackNum][i].note, midiMessages[trackNum][i].velocity);//turn note on/off
    }

  }
  playbackLastScanTime[trackNum] = timeSinceBarStart;

}

void RecordArm(int trackNum) {
  indy_recordArmed[trackNum] = true;
}

void RecordStart(int trackNum) {
  indy_recordArmed[trackNum] = false;
  indy_recordActive[trackNum] = true;

  recordStartTime[trackNum] = millis();
}

void RecordStop(int trackNum) {

  indy_recordActive[trackNum] = false;

  //Stamp recordEndTime once recording stops
  recordEndTime[trackNum] = millis();

  if (trackNum == 0) {
    updateBPM(trackNum);
  }
  quantizeMIDI(trackNum);
  trackDataRecorded[trackNum] = true;

}

void PlayStart(int trackNum) {
  indy_playActive[trackNum] = true;

  if (trackNum == 0) {
    Serial.write(MIDI_START);
  }
}

void PlayStop(int trackNum) {
  indy_playActive[trackNum] = false;

  //All notes off!
  for (int i = 0; i < midiMessage_Counter[trackNum]; i++)
  {
    if (midiMessages[trackNum][i].command == MIDI_NOTE_ON) {
      midiMessage(MIDI_NOTE_OFF, midiMessages[trackNum][i].note, 0);
    }
  }
}

void ClearTrack(int trackNum) {


  trackDataRecorded[trackNum] = false;
  recordStartTime[trackNum] = 0;
  recordEndTime[trackNum] = 0;

  for (int i = 0; i < MAX_MESSAGES_PER_TRACK; i++)
  {
    midiMessages[trackNum][i].command = 0;
    midiMessages[trackNum][i].note = 0;
    midiMessages[trackNum][i].velocity = 0;
    midiMessages[trackNum][i].timestamp = 0;

    i++;
  }

}

void checkRecordButton() {
  //Record button pressed
  if (recordButtonState == true && recordButtonState_LS == false)
  {
    for (int trackNum = 0; trackNum < NUMBER_OF_TRACKS; trackNum++)
    {
      if (trackButtonState[trackNum]) {
        //If track is stopped...
        if (!indy_recordArmed[trackNum] && !indy_recordActive[trackNum] && !indy_playActive[trackNum]) {

          //if master track is selected...
          if (trackNum == 0) {
            for (int i = 0; i < NUMBER_OF_TRACKS; i++) {
              if (trackDataRecorded[i]) {
                ClearTrack(i);  //clear all tracks
                trackDataRecorded[i] = false;
              }
            }
            RecordArm(trackNum);
          }
          //if slave track is selected...
          else {

            //if master track has been recorded...
            if (trackDataRecorded[0]) {

              if (trackDataRecorded[trackNum]) {
                ClearTrack(trackNum);  //Clear slave track if there is track data recorded
                trackDataRecorded[trackNum] = false;
              }
              RecordArm(trackNum);
            }
            //if no master track has been recorded...
            else {
              //Do nothing. a master track must always be recorded first
            }

          }
        }

        //if track is armed...
        else if (indy_recordArmed[trackNum] && !indy_recordActive[trackNum] && !indy_playActive[trackNum]) {

          //if master track is selected...
          if (trackNum == 0) {
            RecordStart(trackNum);
          }
          //if slave track is selected
          else {

            //if master track has been recorded...
            if (trackDataRecorded[0]) {

              //if master track is plaing...
              if (indy_playActive[0]) {
                RecordStart(trackNum);
              }
              //if master track is not playing...
              else {
                PlayStart(0); //start playing master track
                RecordStart(trackNum);  //start recording slave track
              }

            }

          }
        }

        //track is recording...
        else if (!indy_recordArmed[trackNum] && indy_recordActive[trackNum] && !indy_playActive[trackNum]) {
          RecordStop(trackNum);
        }
        //track is playing...
        else if (!indy_recordArmed[trackNum] && !indy_recordActive[trackNum] && indy_playActive[trackNum]) {
          //Do nothing
        }
      }
    }
  }
}

void checkPlayButton() {
  //Play button pressed
  if (playButtonState == true && playButtonState_LS == false) {

    for (int trackNum = 0; trackNum < NUMBER_OF_TRACKS; trackNum++) {

      if (trackButtonState[trackNum]) {

        //if track is recording...
        if (!indy_recordArmed[trackNum] && indy_recordActive[trackNum] && !indy_playActive[trackNum]) {
          RecordStop(trackNum);
          PlayStart(trackNum);
        }

        //if track is stopped...
        if (!indy_recordArmed[trackNum] && !indy_recordActive[trackNum] && !indy_playActive[trackNum]) {
          if ( trackDataRecorded[trackNum]) {
            PlayStart(trackNum);
          }
        }
      }
    }

  }
}

void checkStopButton() {
  //Stop button pressed...
  if (stopButtonState == true && stopButtonState_LS == false) {
    //All tracks are selected...
    if (trackButtonState[0] && trackButtonState[1] && trackButtonState[2] && !trackButtonState[3]) {
      playbackStartTime = 0;
      Serial.write(MIDI_STOP);
    }

    for (int trackNum = 0; trackNum < NUMBER_OF_TRACKS; trackNum++) {

      //if track is playing...
      if (trackButtonState[trackNum]) {
        if (!indy_recordArmed[trackNum] && !indy_recordActive[trackNum] && indy_playActive[trackNum]) {
          PlayStop(trackNum);
        }
      }
    }
  }
}



void loop() {

  readInputs();

  //cannot navigate settings
  if (!indy_recordArmed[0] && !indy_recordArmed[1] && !indy_recordArmed[2] && !indy_recordArmed[3])
    if (!indy_recordActive[0] && !indy_recordActive[1] && !indy_recordActive[2] && !indy_recordActive[3])
      if (!indy_playActive[0] && !indy_playActive[1] && !indy_playActive[2] && !indy_playActive[3])
        navigateSettings();

  checkRecordButton();
  checkPlayButton();
  checkStopButton();


  int barTotalTime = recordEndTime[0] - recordStartTime[0];

  for (int trackNum = 0; trackNum < NUMBER_OF_TRACKS; trackNum++) {

    if (indy_recordArmed[trackNum]) {
      checkMIDI(trackNum);
    }

    //When a slave track is recording and the total bar time has elapsed, automatically stop recording and start playing
    if (trackNum != 0) {
      if (trackDataRecorded[0]) {
        if (indy_recordActive[trackNum]) {
          if (millis() - recordStartTime[trackNum] >= (unsigned long)barTotalTime) {
            RecordStop(trackNum);
            PlayStart(trackNum);
          }
          else {
            checkMIDI(trackNum);
          }
        }
      }
    }

    //if playback is active...
    if (indy_playActive[trackNum]) {
      //stamp playback start time once recording begins and restamp when each loop cycle completes
      if (playbackStartTime <= 0 || millis() - playbackStartTime >= (long)barTotalTime) {
        //All notes off!
        for (int i = 0; i < midiMessage_Counter[trackNum]; i++)
        {
          if (midiMessages[trackNum][i].command == MIDI_NOTE_ON) {
            midiMessage(MIDI_NOTE_OFF, midiMessages[trackNum][i].note, 0);
          }
        }

        playbackStartTime = millis();//sets the milliseconds since arduino began running
        playbackLastScanTime[trackNum] = -1;

      }

      playbackMIDI(trackNum);

    }
  }

  recordButtonState_LS = recordButtonState;
  playButtonState_LS = playButtonState;
  stopButtonState_LS = stopButtonState;
  settingsUpButtonState_LS = settingsUpButtonState;
  settingsDownButtonState_LS = settingsDownButtonState;
  settingsLeftButtonState_LS = settingsLeftButtonState;
  settingsRightButtonState_LS = settingsRightButtonState;

  writeOutputs();
}


void readInputs() {
  trackButtonState[0] = digitalRead(BUTTON_TRACK_1_PIN);
  trackButtonState[1] = digitalRead(BUTTON_TRACK_2_PIN);
  trackButtonState[2] = digitalRead(BUTTON_TRACK_3_PIN);
  trackButtonState[3] = digitalRead(BUTTON_TRACK_4_PIN);

  recordButtonState = digitalRead(BUTTON_RECORD_PIN);
  playButtonState = digitalRead(BUTTON_PLAY_PIN);
  stopButtonState = digitalRead(BUTTON_STOP_PIN);

  settingsUpButtonState = IO.digitalRead(BUTTON_LCD_UP_PIN);
  settingsDownButtonState = IO.digitalRead(BUTTON_LCD_DOWN_PIN);
  settingsLeftButtonState = IO.digitalRead(BUTTON_LCD_LEFT_PIN);
  settingsRightButtonState = IO.digitalRead(BUTTON_LCD_RIGHT_PIN);
}

void writeOutputs() {
  if (indy_recordArmed[0]) {
    int seconds = millis() / 250;
    if ( (seconds % 2) == 0) {
      IO.digitalWrite(RECORD_LED_TRACK_1_PIN, HIGH);
    }
    else {
      IO.digitalWrite(RECORD_LED_TRACK_1_PIN, LOW);
    }
  }
  else if (indy_recordActive[0]) {
    IO.digitalWrite(RECORD_LED_TRACK_1_PIN, HIGH);
  }
  else {
    IO.digitalWrite(RECORD_LED_TRACK_1_PIN, LOW);  //**can be optomized by making this conditional
  }

  if (indy_recordArmed[1]) {
    int seconds = millis() / 250;
    if ( (seconds % 2) == 0) {
      IO.digitalWrite(RECORD_LED_TRACK_2_PIN, HIGH);
    }
    else {
      IO.digitalWrite(RECORD_LED_TRACK_2_PIN, LOW);
    }
  }
  else if (indy_recordActive[1]) {
    IO.digitalWrite(RECORD_LED_TRACK_2_PIN, HIGH);
  }
  else {
    IO.digitalWrite(RECORD_LED_TRACK_2_PIN, LOW);  //**can be optomized by making this conditional
  }

  if (indy_recordArmed[2]) {
    int seconds = millis() / 250;
    if ( (seconds % 2) == 0) {
      IO.digitalWrite(RECORD_LED_TRACK_3_PIN, HIGH);
    }
    else {
      IO.digitalWrite(RECORD_LED_TRACK_3_PIN, LOW);
    }
  }
  if (indy_recordActive[2]) {
    IO.digitalWrite(RECORD_LED_TRACK_3_PIN, HIGH);
  }
  else {
    IO.digitalWrite(RECORD_LED_TRACK_3_PIN, LOW);  //**can be optomized by making this conditional
  }

  if (indy_recordArmed[3]) {
    int seconds = millis() / 250;
    if ( (seconds % 2) == 0) {
      IO.digitalWrite(RECORD_LED_TRACK_4_PIN, HIGH);
    }
    else {
      IO.digitalWrite(RECORD_LED_TRACK_4_PIN, LOW);
    }
  }
  if (indy_recordActive[3]) {
    IO.digitalWrite(RECORD_LED_TRACK_4_PIN, HIGH);
  }
  else {
    IO.digitalWrite(RECORD_LED_TRACK_4_PIN, LOW);  //**can be optomized by making this conditional
  }

  if (indy_playActive[0]) {
    IO.digitalWrite(PLAY_LED_TRACK_1_PIN, HIGH);
  }
  else {
    IO.digitalWrite(PLAY_LED_TRACK_1_PIN, LOW);  //**can be optomized by making this conditional
  }
  if (indy_playActive[1]) {
    IO.digitalWrite(PLAY_LED_TRACK_2_PIN, HIGH);
  }
  else {
    IO.digitalWrite(PLAY_LED_TRACK_2_PIN, LOW);  //>**can be optomized by making this conditional
  }
  if (indy_playActive[2]) {
    IO.digitalWrite(PLAY_LED_TRACK_3_PIN, HIGH);
  }
  else {
    IO.digitalWrite(PLAY_LED_TRACK_3_PIN, LOW);  //**can be optomized by making this conditional
  }
  if (indy_playActive[3]) {
    IO.digitalWrite(PLAY_LED_TRACK_4_PIN, HIGH);
  }
  else {
    IO.digitalWrite(PLAY_LED_TRACK_4_PIN, LOW);  //**can be optomized by making this conditional
  }
}

void navigateSettings() {
  if (settingsLeftButtonState == true && settingsLeftButtonState_LS == false) {
    if (settingsScreen > 0)
      settingsScreen--;
    updateSettingsItem(settingsScreen);
  }
  else if (settingsRightButtonState == true && settingsRightButtonState_LS == false) {
    if (settingsScreen < 6)
      settingsScreen++;
    updateSettingsItem(settingsScreen);
  }
  else if (settingsUpButtonState == true && settingsUpButtonState_LS == false) {
    updateSettingsValue(settingsScreen, true);
    saveSettings();
  }
  else if (settingsDownButtonState == true && settingsDownButtonState_LS == false) {
    updateSettingsValue(settingsScreen, false);
    saveSettings();
  }
}

void updateSettingsItem(int screen) {
  switch (screen) {
    case 0:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("    MOSSTECH    ");
      lcd.setCursor(0,1);
      lcd.print("    MIDILOOP    ");
      break;
    case 1:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Bars:           ");
      lcd.setCursor(0,1);
      lcd.print(getLCDString(settings.bars));
      break;
    case 2:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Quantize:       ");
      lcd.setCursor(0, 1);
      lcd.print("            1/" + String(settings.quantizeDenominator));
      break;
    case 3:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Track 1 Channel:");
      lcd.setCursor(0, 1);
      lcd.print(getLCDString(settings.trackChannel[0]));
      break;
    case 4:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Track 2 Channel:");
      lcd.setCursor(0, 1);
      lcd.print(getLCDString(settings.trackChannel[1]));
      break;
    case 5:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Track 3 Channel:");
      lcd.setCursor(0, 1);
      lcd.print(String(settings.trackChannel[2]));
      break;
    case 6:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Track 4 Channel:");
      lcd.setCursor(0, 1);
      lcd.print(getLCDString(settings.trackChannel[3]));
      break;
    default:
      settingsScreen = 0; //if invalid screen, return to home
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("    MOSSTECH    ");
      lcd.setCursor(0, 1);
      lcd.print("    MIDILOOP    ");
      break;
  }
}

String getLCDString(int val) {
  if (val >= 0 && val < 10)
    return "               " + String(val);
  else if (val >= 10 && val < 100)
    return "              " + String(val);
  else
    return "String Error";
}

void updateSettingsValue(int screen, bool navUp) {

  int settingsValue_index;

  switch (settingsScreen) {

    case 1: //bars

      if (navUp) {
        if (settings.bars <= 7) {
          settings.bars++;
        }
      }
      else {
        if (settings.bars >= 2) {
          settings.bars--;
        }
      }
      lcd.setCursor(0, 1);
      lcd.print(getLCDString(settings.bars));
      break;

    case 2: //quantize
      if (navUp) {
        if (settings.quantizeDenominator <= 32) {
          settings.quantizeDenominator = 2 * settings.quantizeDenominator;
        }
      }
      else {
        if (settings.quantizeDenominator >= 8) {
          settings.quantizeDenominator = settings.quantizeDenominator / 2;
        }
      }
      lcd.setCursor(0, 1);
      lcd.print(getLCDString(settings.quantizeDenominator));
      break;

    case 3: //track 1 channel

      if (navUp) {
        if (settings.trackChannel[0] <= 14) {
          settings.trackChannel[0]++;
        }
      }
      else {
        if (settings.trackChannel[0] >= 1) {
          settings.trackChannel[0]--;
        }
      }
      lcd.setCursor(0, 1);
      lcd.print(getLCDString(settings.trackChannel[0]));
      break;

    case 4: //track 2 channel

      if (navUp) {
        if (settings.trackChannel[1] <= 14) {
          settings.trackChannel[1]++;
        }
      }
      else {
        if (settings.trackChannel[1] >= 1) {
          settings.trackChannel[1]--;
        }
      }
      lcd.setCursor(0, 1);
      lcd.print(getLCDString(settings.trackChannel[1]));
      break;
    case 5: //track 3 channel

      if (navUp) {
        if (settings.trackChannel[2] <= 14) {
          settings.trackChannel[2]++;
        }
      }
      else {
        if (settings.trackChannel[2] >= 1) {
          settings.trackChannel[2]--;
        }
      }
      lcd.setCursor(0, 1);
      lcd.print(getLCDString(settings.trackChannel[2]));
      break;

    case 6: //track 4 channel

      if (navUp) {
        if (settings.trackChannel[3] <= 14) {
          settings.trackChannel[3]++;
        }
      }
      else {
        if (settings.trackChannel[3] >= 1) {
          settings.trackChannel[3]--;
        }
      }
      lcd.setCursor(0, 1);
      lcd.print(getLCDString(settings.trackChannel[3]));
      break;

    default:

      break;
  }

}



#include <MIDI.h>
#include <SoftwareSerial.h>

SoftwareSerial midiSerial(2, 4);
MIDI_CREATE_INSTANCE(SoftwareSerial, midiSerial, MIDI);

#define BPM_PIN A0
#define METRONOME_16 12
#define METRONOME_8 11
#define METRONOME_4 10
#define METRONOME_2 9
#define STEP_COUNT 64  // 256 ideally
#define STEP_SIZE 1
#define TOTAL_TIME 15000000
#define MIDI_NOTE_COUNT 128
#define CHORD_SIZE 3
#define SEND_CLOCK 1
#define DEBUG_LOOP 0

uint8_t counter = 0;
signed long bpm = 120;
unsigned long previousTime = 0;
unsigned long timeInterval = 0;

struct note {
    uint8_t pitch;
    uint8_t velocity;
};

struct step {
    struct note chord[CHORD_SIZE];
    uint8_t length = 0;
};

struct step *headOn;
struct step *headOff;
struct step noteOn[STEP_COUNT];
struct step noteOff[STEP_COUNT];
bool awaiting[127];
bool thru = false;

void metronome(uint8_t step) {
    digitalWrite(METRONOME_16, counter % 2 == 0);
    digitalWrite(METRONOME_8, counter / 8 % 2 == 0);
    digitalWrite(METRONOME_4, counter / 16 % 2 == 0);
    digitalWrite(METRONOME_2, counter / 32 % 2 == 0);
    // digitalWrite(METRONOME_8, counter % 4 == 0);
    // digitalWrite(METRONOME_2, counter % 16 == 0);
}

void setupMetronome() {
    pinMode(METRONOME_16, OUTPUT);
    pinMode(METRONOME_8, OUTPUT);
    pinMode(METRONOME_4, OUTPUT);
    pinMode(METRONOME_2, OUTPUT);
}

void printChord(struct step *step) {
    Serial.print(counter, 10);
    Serial.print(" ");
    for (int i = 0; i < step->length; i++) {
        Serial.print((int)step->chord[i].pitch, 2);
        Serial.print(" ");
    }

    Serial.println();
}

void tx() {
    for (int i = 0; i < CHORD_SIZE; i++) {
        if (headOn->length > i) {
            MIDI.sendNoteOn(headOn->chord[i].pitch, headOn->chord[i].velocity,
                            1);
        }
        if (headOff->length > i) {
            MIDI.sendNoteOff(headOff->chord[i].pitch, 0, 1);
        }
    }
}

void rx() {
    struct note capturedNote;
    switch (MIDI.getType()) {
        case midi::NoteOff:
            capturedNote = {pitch : MIDI.getData1(), velocity : 0};
            if (!awaiting[capturedNote.pitch]) break;
            if (headOff->length >= CHORD_SIZE) break;
            // TODO: if the chord is full, the note should be carried over to
            // the next step
            awaiting[capturedNote.pitch] = false;
            headOff->chord[headOff->length] = capturedNote;
            headOff->length++;

            break;
        case midi::NoteOn:
            if (headOn->length >= CHORD_SIZE) break;
            capturedNote = {
                pitch : MIDI.getData1(),
                velocity : MIDI.getData2()
            };
            if (awaiting[capturedNote.pitch]) break;
            // it shoudn't be possible to save two note on events without first
            // saving note off for the first one
            awaiting[capturedNote.pitch] = true;
            headOn->chord[headOn->length] = capturedNote;
            headOn->length++;

            break;
        default:
            break;
    }
}

void debugLoop() {
    int start = 24;
    int major[3] = {0, 3, 5};
    for (size_t i = 0; i < STEP_COUNT; i++) {
        if (i % 2 == 0) {
            for (size_t j = 0; j < 3; j++) {
                noteOn[i].chord[j].pitch = major[j] + start + i / 2;
                noteOn[i].chord[j].velocity = 60;
            }

            noteOn[i].length = 3;
        } else {
            for (size_t j = 0; j < 3; j++) {
                noteOff[i].chord[j].pitch = major[j] + start + i / 2;
                noteOff[i].chord[j].velocity = 0;
                noteOff[i].length = 3;
            }
        }
    }
}

void setup() {
    MIDI.begin();
    MIDI.turnThruOff();
    Serial.begin(9600);
    setupMetronome();
    if (DEBUG_LOOP) {
        debugLoop();
    }
}

void loop() {
    headOn = &noteOn[counter];
    headOff = &noteOff[counter];
    if (thru) {
        MIDI.turnThruOn();
    } else if (MIDI.read()) {
        MIDI.turnThruOff();
        rx();
    }

    unsigned long time = micros();

    bpm = map(analogRead(BPM_PIN), 0, 1023, 60, 180);
    timeInterval = (TOTAL_TIME / STEP_SIZE) / bpm;
    if (time - previousTime < timeInterval) return;
    // Serial.println(counter);

    tx();

    previousTime = time;
    counter = ++counter % STEP_COUNT;
    metronome(counter);
}

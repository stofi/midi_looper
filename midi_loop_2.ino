#include <MIDI.h>
#include <SoftwareSerial.h>

SoftwareSerial midiSerial(2, 3);
MIDI_CREATE_INSTANCE(SoftwareSerial, midiSerial, MIDI);

#define BPM_PIN A0
#define METRONOME_16 12
#define METRONOME_8 11
#define METRONOME_4 10
#define METRONOME_2 9
#define STEP_COUNT 32  // 256 ideally

uint8_t counter = 0;
signed long bpm = 120;
unsigned long previousTime = 0;
unsigned long timeInterval = 0;

struct note {
    uint8_t status;
    uint8_t pitch;
    uint8_t velocity;
};

struct step {
    struct note chord[6];
    uint8_t length = 0;
};

struct step tape[STEP_COUNT];

void metronome(uint8_t step) {
    digitalWrite(METRONOME_16, counter % 2 == 0);
    digitalWrite(METRONOME_8, counter % 4 == 0);
    digitalWrite(METRONOME_4, counter % 8 == 0);
    digitalWrite(METRONOME_2, counter % 16 == 0);
}

void setupMetronome() {
    pinMode(METRONOME_16, OUTPUT);
    pinMode(METRONOME_8, OUTPUT);
    pinMode(METRONOME_4, OUTPUT);
    pinMode(METRONOME_2, OUTPUT);
}

struct note getNote() {
    struct note capturedNote;
    switch (MIDI.getType()) {
        case midi::NoteOff:
            capturedNote = {
                status : 0b10000000 || MIDI.getChannel(),
                pitch : MIDI.getData1(),
                velocity : MIDI.getData2()
            };
            break;
        case midi::NoteOn:
            capturedNote = {
                status : 0b10010000 || MIDI.getChannel(),
                pitch : MIDI.getData1(),
                velocity : MIDI.getData2()
            };
            break;
        default:
            break;
    }
    return capturedNote;
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

void setup() {
    MIDI.begin();
    Serial.begin(9600);
    setupMetronome();
}

void loop() {
    struct step *head = &tape[counter];
    if (MIDI.read() && head->length < 6) {
        struct note incomingNote = getNote();
        head->chord[head->length] = incomingNote;
        head->length++;
    }

    unsigned long headTime = micros();
    bpm = map(analogRead(BPM_PIN), 0, 1023, 60, 180);
    timeInterval = (3750000 / bpm);
    if (headTime - previousTime < timeInterval) return;
    if (head->length > 0) {
        printChord(head);
    }

    // printChord(head->chord);
    // Serial.println(head->id);

    previousTime = headTime;
    counter = ++counter % STEP_COUNT;
    metronome(counter);
}

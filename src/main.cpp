#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Oscillator.h"

#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/scene/al_DistributedScene.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"

#include <cassert>
#include <vector>
#include <cstdio>

// using namespace gam;
using namespace al;


#include "notedefs.hpp"

const float dashLength = 1.f / 6.f;
const float BPM = 77.f;
const float wholeNote = 240.f / BPM * 1000.f;
const float halfNote = wholeNote / 2.f;
const float quarterNote = wholeNote / 4.f;
const float eighthNote = quarterNote / 2.f;
const float sixteenthNote = eighthNote / 2.f;
const float dottedHalfNote = halfNote * 1.5f;
const float dottedQuarterNote = quarterNote * 1.5f;
const float dottedEighthNote = dottedQuarterNote / 2.f;
const float dottedSixteenthNote = dottedEighthNote / 2.f;
const float amplitude = 0.25f;

#include "SineEnv.hpp"

class TimeSignature {
    private:
        int upper;
        int lower;

    public:
        TimeSignature() {
            this->upper = 4;
            this->lower = 4;
        }
};

class Note {
    private:
        float freq;
        float time;
        float duration;
        float amp;
        float attack;
        float release;
        float decay;
        float sustain;
    
    public:
        Note() {
            this->freq = 440.0;
            this->time = 0;
            this->duration = 0.5;
            this->amp = 0.2;
            this->attack = 0.05;
            this->release = 0.05;
            this->decay = 0.5;
            this->sustain = 0.05;
        }
        Note(float freq, float time = 0.0f, float duration = 0.5f, float amp = 0.2f, float attack = 0.05f, float release = 0.05f, float decay = 0.5f, float sustain = 0.05f) {
            this->freq = freq;
            this->time = time;
            this->duration = duration;
            this->amp = amp;
            this->attack = attack;
            this->release = release;
            this->decay = decay;
            this->sustain = sustain;
        }
        // Return an identical note, but offset by the
        // number of beats indicated by beatOffset,
        // and with amplitude multiplied by ampMult
        Note(const Note &n, float beatOffset, float ampMult = 1.0f) {
            this->freq = n.freq;
            this->time = n.time + beatOffset;
            this->duration = n.duration;
            this->amp = n.amp * ampMult;
            this->attack = n.attack;
            this->release = n.release;
            this->decay = n.decay;
            this->sustain = n.sustain;
        }
        Note(const Note &n) {
            this->freq = n.freq;
            this->time = n.time;
            this->duration = n.duration;
            this->amp = n.amp;
            this->attack = n.attack;
            this->release = n.release;
            this->decay = n.decay;
            this->sustain = n.sustain;
        }
        float getFreq() { return this->freq; }
        float getTime() { return this->time; }
        float getDuration() { return this->duration; }
        float getAmp() { return this->amp; }
        float getAttack() { return this->attack; }
        float getRelease() { return this->release; }
        float getDecay() { return this->decay; }
        float getSustain() { return this->sustain; }
};

class Sequence {
    private:
        TimeSignature ts;
        std::vector<Note> notes;

    public:
        Sequence(TimeSignature ts) { this->ts = ts; }

        void add(Note n) { notes.push_back(n); }

    /**Add notes from the source sequence s,
        *but starting on the beat indicated by startBeat
        */
    void addSequence(Sequence *s, float startBeat, float ampMult = 1.0) {
        for(auto &note : *(s->getNotes())) { add(Note(note, startBeat, ampMult)); }
    }

    std::vector<Note> *getNotes() { return &notes; }
};

// We make an app.
class MyApp : public App {
    public:
        // GUI manager for SquareWave voices
        // The name provided determines the name of the directory
        // where the presets and sequences are stored
        SynthGUIManager<SineEnv> synthManager{"SineEnv"};

        // This function is called right after the window is created
        // It provides a grphics context to initialize ParameterGUI
        // It's also a good place to put things that should
        // happen once at startup.
        void onCreate() override {
            navControl().active(false); // Disable navigation via keyboard, since we
                                        // will be using keyboard for note triggering

            // Set sampling rate for Gamma objects from app's audio
            gam::sampleRate(audioIO().framesPerSecond());

            imguiInit();

            // Play example sequence. Comment this line to start from scratch
            // synthManager.synthSequencer().playSequence("synth1.synthSequence");
            synthManager.synthRecorder().verbose(true);
        }

        // The audio callback function. Called when audio hardware requires data
        void onSound(AudioIOData &io) override {
            // THIS THIS THIS is where Andres suggests scheduling new events
            // define a counter... when I get here add the number of samples in block
            // when you get to target number, inject new sequence...

            synthManager.render(io); // Render audio
        }

        void onAnimate(double dt) override {
            // The GUI is prepared here
            imguiBeginFrame();
            // Draw a window that contains the synth control panel
            synthManager.drawSynthControlPanel();
            imguiEndFrame();
        }

        // The graphics callback function.
        void onDraw(Graphics &g) override {
            g.clear();
            // Render the synth's graphics
            synthManager.render(g);

            // GUI is drawn here
            imguiDraw();
        }

        // Whenever a key is pressed, this function is called
        bool onKeyDown(Keyboard const &k) override {
            if (ParameterGUI::usingKeyboard()) { // Ignore keys if GUI is using keyboard
                return true;
            }
            switch (k.key()) {
                case 8: // Backspace to end sequence
                    synthManager.synthSequencer().setTime(0);
                    synthManager.synthSequencer().stopSequence();
                    return false;
                default: // Starts a new sequence and ending any currently playing sequences
                    synthManager.synthSequencer().setTime(0);
                    synthManager.synthSequencer().stopSequence();
                    playSequence(1.0);
                    return false;
            }
        }

        // Whenever a key is released this function is called
        bool onKeyUp(Keyboard const &k) override {
            // do nothing
        }

        void onExit() override { imguiShutdown(); }

        void playNote(float freq, float time, float duration = 0.5, float amp = 0.2, float attack = 0.1, float decay = 0.5) {
            SynthVoice *voice;

            voice = synthManager.synth().getVoice<SineEnv>();
            voice->setInternalParameterValue("amplitude", amp);
            voice->setInternalParameterValue("frequency", freq);
            voice->setInternalParameterValue("attackTime", 0.01);
            voice->setInternalParameterValue("releaseTime", 0.05);
            voice->setInternalParameterValue("pan", 0.0);

            synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
        }

        Sequence *sequence(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->addSequence(sequencePhrase1(offset), dashLength * 0, 0.5);
            result->addSequence(sequencePhrase2(offset), dashLength * 27, 0.5);
            result->addSequence(sequencePhrase3(offset), dashLength * 27 * 2, 0.5);
            result->addSequence(sequencePhrase4(offset), dashLength * 27 * 3, 0.5);
            result->addSequence(sequencePhrase5(offset), dashLength * 27 * 4, 0.5);
            result->addSequence(sequencePhrase6(offset), dashLength * 27 * 5, 0.5);
            result->addSequence(sequencePhrase7(offset), dashLength * 27 * 6, 0.5);
            result->addSequence(sequencePhrase8(offset), dashLength * 27 * 7, 0.5);
            result->addSequence(sequencePhrase9(offset), dashLength * 27 * 8, 0.5);
            result->addSequence(sequencePhrase10(offset), dashLength * 27 * 9, 0.5);
            result->addSequence(sequencePhrase11(offset), dashLength * 27 * 10, 0.5);
            result->addSequence(sequencePhrase12(offset), dashLength * 27 * 11, 0.5);
            result->addSequence(sequencePhrase13(offset), dashLength * 27 * 12, 0.5);
            result->addSequence(sequencePhrase14(offset), dashLength * 27 * 13, 0.5);
            result->addSequence(sequencePhrase15(offset), dashLength * 27 * 14, 0.5);
            result->addSequence(sequencePhrase16(offset), dashLength * 27 * 15, 0.5);
            result->addSequence(sequencePhrase17(offset), dashLength * 27 * 16, 0.5);
            result->addSequence(sequencePhrase18(offset), dashLength * 27 * 17, 0.5);

            return result;
        }

        Sequence *sequencePhrase1(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(D5 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 6, dottedHalfNote / 1000.f, amplitude, dottedHalfNote, dottedHalfNote));
            result->add(Note(C5 * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 0, wholeNote / 1000.f, amplitude, wholeNote, wholeNote));

            return result;
        }

        Sequence *sequencePhrase2(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(C5s * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 26, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4s * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 19, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 19, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase3(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(D5 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 20, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 23, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 23, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 23, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase4(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(C5s * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4s * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4s * offset, dashLength * 21, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4s * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase5(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(C5s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 19, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4 * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase6(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(D5 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 23, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase7(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(C5 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5 * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5 * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5 * offset, dashLength * 21, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C4 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3 * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3 * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2 * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase8(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(C5 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F5s * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B5 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A5 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(G5 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(G4 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C4s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3 * offset, dashLength * 19, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(G3 * offset, dashLength * 26, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }
  
        Sequence *sequencePhrase9(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(E5 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F5s * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F4s * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 23, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 20, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase10(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(F5s * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F5s * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B5 * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B5 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A5 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(G5 * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(G4 * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 21, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 21, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 21, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase11(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(F5s * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A5s * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F5s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F4s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3 * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3 * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A2s * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase12(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(E5 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F5s * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F4s * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 23, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3 * offset, dashLength * 23, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F2s * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F2s * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F2s * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 23, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase13(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(E5 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B5 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 21, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 21, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F2s * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F2s * offset, dashLength * 3, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 21, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F2s * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase14(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(E5 * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F5s * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F4s * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 19, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 19, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F2s * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F2s * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2s * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2s * offset, dashLength * 19, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F2s * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F2s * offset, dashLength * 25, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase15(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(D6 * offset, dashLength * 20, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E5 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B5 * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 20, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B5 * offset, dashLength * 23, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 1, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(E4 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 4, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 23, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 20, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(F3s * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 20, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 5, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 20, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase16(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(D6 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C6s * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C6s * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 6, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A5s * offset, dashLength * 21, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 24, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A4s * offset, dashLength * 21, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C4s * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 18, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 0, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 15, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 17, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase17(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(D6 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B5 * offset, dashLength * 19, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D4 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B4 * offset, dashLength * 19, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 9, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 12, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            result->add(Note(B3 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            result->add(Note(D3 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            result->add(Note(B2 * offset, dashLength * 16, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 22, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        Sequence *sequencePhrase18(float offset = 1.0) {
            TimeSignature t;
            Sequence *result = new Sequence(t);

            result->add(Note(D6 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D6 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C6s * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 2, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D5 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C5s * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C4s * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B3 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(A3s * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(D3 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(C3s * offset, dashLength * 14, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 7, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 8, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 10, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 11, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));
            result->add(Note(B2 * offset, dashLength * 13, quarterNote / 1000.f, amplitude, quarterNote, quarterNote));

            return result;
        }

        void playSequence(Sequence *s, float bpm) {
            float secondsPerBeat = 60.0f / bpm;

            std::vector<Note> *notes = s->getNotes();

            for (auto &note : *notes)
            {
            playNote(
                note.getFreq(),
                note.getTime() * secondsPerBeat,
                note.getDuration() * secondsPerBeat,
                note.getAmp(),
                note.getAttack(),
                note.getDecay());
            }
        }

        void playSequence(float offset = 1.0, float bpm = 77.0) {
            Sequence *mySequence = sequence(offset);
            playSequence(mySequence, bpm);
        }
};

int main() {
    // Create app instance
    MyApp app;
    // Set up audio
    app.configureAudio(48000., 512, 2, 0);
    app.start();
    return 0;
}
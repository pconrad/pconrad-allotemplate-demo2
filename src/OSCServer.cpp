/*
Allocore Example: OSC Server

Description:
This is a simple OSC server that listens for packets with the address "/test"
and containing a string and int.

You should run the OSC client example AFTER running this program.

Author:
Lance Putnam, Oct. 2014
*/

#include <iostream>
#include <string>
#include "al/app/al_App.hpp"
using namespace al;

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

#include <iostream>
using std::cout;
using std::endl;

#include "SineEnv.hpp"

// App has osc::PacketHandler as base class
struct MyApp : public App
{

    SynthGUIManager<SineEnv> synthManager{"SineEnv"};

    // can give params in ctor
    // osc::Recv server {16447, "", 0.05};

    // or open later with `open` interface (at onCreate in this example)
    osc::Recv server;


    // This function is called right after the window is created
    // It provides a grphics context to initialize ParameterGUI
    // It's also a good place to put things that should
    // happen once at startup.
    void onCreate() override
    {
        navControl().active(false); // Disable navigation via keyboard, since we
                                    // will be using keyboard for note triggering

        // Set sampling rate for Gamma objects from app's audio
        gam::sampleRate(audioIO().framesPerSecond());

        imguiInit();

        // Play example sequence. Comment this line to start from scratch
        // synthManager.synthSequencer().playSequence("synth1.synthSequence");
        synthManager.synthRecorder().verbose(true);

        // Print out our IP address
        // std::cout << "SERVER: My IP is " << Socket::hostIP() << "\n";

        // port, address, timeout
        // "" as address for localhost
        server.open(16447, "localhost", 0.05);

        // Register ourself (osc::PacketHandler) with the server so onMessage
        // gets called.
        server.handler(oscDomain()->handler());

        // Start a thread to handle incoming packets
        server.start();

    }

    // The audio callback function. Called when audio hardware requires data
    void onSound(AudioIOData &io) override
    {
        // THIS THIS THIS is where Andres suggests scheduling new events
        // define a counter... when I get here add the number of samples in block
        // when you get to target number, inject new sequence...

        synthManager.render(io); // Render audio
    }

    void onAnimate(double dt) override
    {
        // The GUI is prepared here
        imguiBeginFrame();
        // Draw a window that contains the synth control panel
        synthManager.drawSynthControlPanel();
        imguiEndFrame();
    }

    // The graphics callback function.
    void onDraw(Graphics &g) override
    {
        g.clear();
        // Render the synth's graphics
        synthManager.render(g);

        // GUI is drawn here
        imguiDraw();
    }

    // This gets called whenever we receive a packet
    void onMessage(osc::Message &m) override
    {
        m.print();
        // Check that the address and tags match what we expect
        if (m.addressPattern() == "/test" && m.typeTags() == "si")
        {
            // Extract the data out of the packet
            std::string str;
            int val;
            m >> str >> val;

            // Print out the extracted packet data
            std::cout << "SERVER: recv " << str << " " << val << std::endl;
        }
    }

    // Whenever a key is pressed, this function is called
    bool onKeyDown(Keyboard const &k) override
    {
        if (ParameterGUI::usingKeyboard())
        { // Ignore keys if GUI is using
          // keyboard
            return true;
        }
        if (k.shift())
        {
            // If shift pressed then keyboard sets preset
            int presetNumber = asciiToIndex(k.key());
            synthManager.recallPreset(presetNumber);
        }
        else
        {
            // Otherwise trigger note for polyphonic synth
            int midiNote = asciiToMIDI(k.key());
            if (midiNote > 0)
            {
                synthManager.voice()->setInternalParameterValue(
                    "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
                synthManager.triggerOn(midiNote);
            }
        }
        return true;
    }
    // Whenever a key is released this function is called
    bool onKeyUp(Keyboard const &k) override
    {
        int midiNote = asciiToMIDI(k.key());
        if (midiNote > 0)
        {
            synthManager.triggerOff(midiNote);
        }
        return true;
    }

    void onExit() override { imguiShutdown(); }
};

int main()
{
    cout << __FILE__ << endl;
    MyApp().start();
}

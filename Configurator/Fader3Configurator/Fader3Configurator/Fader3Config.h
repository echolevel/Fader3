#pragma once

#include <iostream>
#include <cstdlib>
#include <vector>
#define __WINDOWS_MM__
#include "RtMidi.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <string>

#define LINEBUFFERMAX 192
#define LINESMAX 16

struct Fader3Settings {
    int chan[3] = { 1, 1, 1 };
    int ccNum[3] = { 1, 14, 15 };
	bool f14bit[3] = { true, true, true };
	int smooth14bit = 2;
	int smooth7bit = 4;
	int thresh14bit = 20;
	int thresh7bit = 2;	

    std::vector<unsigned char> getSysexBytes()
    {
        std::vector<unsigned char> message;
        
        message.push_back(0xF0); // systex start byte
        message.push_back(0x7D); // manufacturer ID

        message.push_back(0x00); // Fader3 validation byte
        message.push_back(0x0B); // Fader3 validation byte
        
        message.push_back(chan[0]-1);
        message.push_back(chan[1]-1);
        message.push_back(chan[2]-1);
        message.push_back(ccNum[0]);
        message.push_back(ccNum[1]);
        message.push_back(ccNum[2]);
        message.push_back(f14bit[0]);
        message.push_back(f14bit[1]);
        message.push_back(f14bit[2]);                        
        message.push_back(smooth14bit);        
        message.push_back(smooth7bit);        
        message.push_back(thresh14bit);        
        message.push_back(thresh7bit);
        message.push_back(true); // debug flag - not currently exposed
        
        message.push_back(0xF7); // Sysex end byte
        

        return message;
    }
};

class Fader3 {

public:
    Fader3()
    {};

    Fader3Settings *settings = new Fader3Settings();

    std::vector<std::string> MidiInNames;
    std::vector<std::string> MidiOutNames;
    std::vector<std::string> MidiLogs;

    unsigned int MidiInIndex = 0;
    unsigned int MidiOutIndex = 0;

    std::vector<unsigned char> IncomingMidiMessage;
    int MidiInNBytes;
    double stamp;

    // Cache previous channel and CC num to detect 14-bit messages
    int lastChannel = 0;
    int lastCCnum = 0;
    int lastCCvalue = 0;

    bool scrollToBottom = false;

    void SendMessageOnPort(const std::vector<unsigned char>* OutMsg, RtMidiOut* OutInstance)
    {
        if (OutInstance)
        {
            if (OutInstance->isPortOpen())
            {
                try
                {
                    OutInstance->sendMessage(OutMsg);
                }
                catch (RtMidiError& error)
                {
                    std::cerr << "RtMidiError: ";
                    error.printMessage();
                    return;
                }

            }
        }
    }

    // Add a log line to the log output text view
    void Log(const char* fmt){     
        std::time_t now = std::time(nullptr);
        std::tm localTime;
        localtime_s(&localTime, &now);

        std::ostringstream timestamp;
        timestamp << "[" << std::setfill('0') << std::setw(2) << localTime.tm_hour << ":"
                  << std::setfill('0') << std::setw(2) << localTime.tm_min << ":" 
                  << std::setfill('0') << std::setw(2) << localTime.tm_sec << "] ";
        
        std::string timestampedFmt = timestamp.str() + fmt;
        const char* finalFMt = timestampedFmt.c_str();

        char buffer[LINEBUFFERMAX];
        snprintf(buffer, sizeof(buffer), finalFMt);
        buffer[sizeof(buffer) - 1] = 0; // Ensure null-termination
        MidiLogs.push_back(buffer);
        scrollToBottom = true;

        // Lose surplus lines
        if (MidiLogs.size() > LINESMAX)
        {
            MidiLogs.erase(MidiLogs.begin(), MidiLogs.begin() + (MidiLogs.size() - LINESMAX));
        }
    }

};


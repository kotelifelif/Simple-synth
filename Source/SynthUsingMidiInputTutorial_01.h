/*
  ==============================================================================

   This file is part of the JUCE tutorials.
   Copyright (c) 2020 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             SynthUsingMidiInputTutorial
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Synthesiser with midi input.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2019, linux_make

 type:             Component
 mainClass:        MainContentComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/


#pragma once

enum class WaveForm {
    Sine,
    Square,
    Saw,
    Triangle
};

//==============================================================================
struct SineWaveSound   : public juce::SynthesiserSound
{
    SineWaveSound() {}

    bool appliesToNote    (int) override        { return true; }
    bool appliesToChannel (int) override        { return true; }
};

//==============================================================================
struct SineWaveVoice : public juce::SynthesiserVoice
{
    SineWaveVoice() = default;

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*> (sound) != nullptr;
    }

    void startNote(int midiNoteNumber,
        float velocity,
        juce::SynthesiserSound*,
        int) override
    {
        currentAngle = 0.0;
        tailOff = 0.0;

        level = static_cast<double> (velocity) * 0.15;

        const auto frequency =
            juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

        const auto sampleRate = getSampleRate();

        if (sampleRate > 0.0)
        {
            angleDelta = frequency
                * juce::MathConstants<double>::twoPi
                / sampleRate;
        }
        else
        {
            angleDelta = 0.0;
        }
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (tailOff == 0.0)
                tailOff = 1.0;
        }
        else
        {
            clearCurrentNote();
            angleDelta = 0.0;
            tailOff = 0.0;
        }
    }

    void pitchWheelMoved(int) override {}

    void controllerMoved(int, int) override {}

    //==========================================================================
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
        int startSample,
        int numSamples) override
    {
        if (angleDelta == 0.0)
            return;

        while (numSamples-- > 0)
        {
            float currentSample = getWaveSample(currentAngle);

            if (tailOff > 0.0)
            {
                currentSample *= static_cast<float> (tailOff);

                tailOff *= releaseRate;

                if (tailOff <= 0.005)
                {
                    clearCurrentNote();
                    angleDelta = 0.0;
                    tailOff = 0.0;
                    break;
                }
            }

            for (int channel = 0;
                channel < outputBuffer.getNumChannels();
                ++channel)
            {
                outputBuffer.addSample(
                    channel, startSample, currentSample);
            }

            currentAngle += angleDelta;

            if (currentAngle >= juce::MathConstants<double>::twoPi)
                currentAngle = std::fmod(
                    currentAngle,
                    juce::MathConstants<double>::twoPi);

            ++startSample;
        }
    }

    //==========================================================================
    void setTailOffSeconds(double seconds)
    {
        const auto sampleRate = getSampleRate();

        if (sampleRate > 0.0 && seconds > 0.0)
        {
            releaseRate = std::exp(
                std::log(0.005) / (seconds * sampleRate));
        }
        else
        {
            releaseRate = 0.0;
        }
    }

    void setWaveForm(WaveForm newForm)
    {
        form = newForm;
    }

private:
    //==========================================================================
    float getWaveSample(double angle) const
    {
        const auto amplitude = static_cast<float> (level);
        const auto twoPi = juce::MathConstants<double>::twoPi;

        switch (form)
        {
        case WaveForm::Sine:
            return amplitude * static_cast<float> (
                std::sin(angle));

        case WaveForm::Square:
            return amplitude * (
                std::sin(angle) >= 0.0 ? 1.0f : -1.0f);

        case WaveForm::Saw:
        {
            const auto phase = angle / twoPi;

            return amplitude * static_cast<float> (
                2.0 * phase - 1.0);
        }

        case WaveForm::Triangle:
            return amplitude * static_cast<float> (
                (2.0 / juce::MathConstants<double>::pi)
                * std::asin(std::sin(angle)));
        }

        return 0.0f;
    }

    double currentAngle = 0.0;
    double angleDelta = 0.0;
    double level = 0.0;
    double tailOff = 0.0;

    double releaseRate = 0.999;

    WaveForm form = WaveForm::Sine;
};

//==============================================================================
class SynthAudioSource   : public juce::AudioSource
{
public:
    SynthAudioSource (juce::MidiKeyboardState& keyState)
        : keyboardState (keyState)
    {
        for (auto i = 0; i < 4; ++i)                // [1]
            synth.addVoice (new SineWaveVoice());

        synth.addSound (new SineWaveSound());       // [2]
    }

    void setUsingSineWaveSound()
    {
        synth.clearSounds();
    }

    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        synth.setCurrentPlaybackSampleRate (sampleRate); // [3]
        midiCollector.reset(sampleRate);
    }

    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();
        juce::MidiBuffer incomingMidi;
        midiCollector.removeNextBlockOfMessages(
            incomingMidi,
            bufferToFill.numSamples
        ); // [11]
        keyboardState.processNextMidiBuffer(
            incomingMidi,
            bufferToFill.startSample,
            bufferToFill.numSamples,
            true
        );
        synth.renderNextBlock(
            *bufferToFill.buffer,
            incomingMidi,
            bufferToFill.startSample,
            bufferToFill.numSamples
        );
    }

    void setTailOffSeconds(double seconds) {
        int numVoices = synth.getNumVoices();
        for (int i = 0; i < numVoices; ++i) {
            auto voice = dynamic_cast<SineWaveVoice*>(synth.getVoice(i));
            voice->setTailOffSeconds(seconds);
        }
    }

    void setWaveForm(WaveForm form) {
        int numVoices = synth.getNumVoices();
        for (int i = 0; i < numVoices; ++i) {
            auto voice = dynamic_cast<SineWaveVoice*>(synth.getVoice(i));
            voice->setWaveForm(form);
        }
    }

    juce::MidiMessageCollector* getMidiCollector() {
        return &midiCollector;
    }

private:
    juce::MidiMessageCollector midiCollector;
    juce::MidiKeyboardState& keyboardState;
    juce::Synthesiser synth;
};

//==============================================================================
class MainContentComponent   : public juce::AudioAppComponent,
                               private juce::Timer
{
public:
    MainContentComponent()
        : synthAudioSource  (keyboardState),
          keyboardComponent (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        addAndMakeVisible (keyboardComponent);

        addAndMakeVisible(midiInputListLabel);
        midiInputListLabel.setText("MIDI Input:", juce::dontSendNotification);
        midiInputListLabel.attachToComponent(&midiInputList, true);
        auto midiInputs = juce::MidiInput::getAvailableDevices();
        addAndMakeVisible(midiInputList);
        midiInputList.setTextWhenNoChoicesAvailable("No MIDI Inputs Enabled");
        juce::StringArray midiInputNames;
        for (auto input : midiInputs)
            midiInputNames.add(input.name);
        midiInputList.addItemList(midiInputNames, 1);
        midiInputList.onChange = [this] { setMidiInput(midiInputList.getSelectedItemIndex()); };
        for (auto input : midiInputs)
        {
            if (deviceManager.isMidiInputDeviceEnabled(input.identifier))
            {
                setMidiInput(midiInputs.indexOf(input));
                break;
            }
        }
        if (midiInputList.getSelectedId() == 0)
            setMidiInput(0);

        addAndMakeVisible(tailOffSlider);
        tailOffSlider.setRange(0.0, 10.0, 1.0);
        tailOffSlider.onValueChange = [this] { setTailOffSeconds(); };
        addAndMakeVisible(tailOffLabel);
        tailOffLabel.setText("Tail off in seconds", juce::dontSendNotification);
        tailOffLabel.attachToComponent(&tailOffSlider, true);

        addAndMakeVisible(waveFormList);
        waveFormList.addItem("Sine", 1);
        waveFormList.addItem("Square", 2);
        waveFormList.addItem("Saw", 3);
        waveFormList.addItem("Triangle", 4);
        waveFormList.setSelectedItemIndex(0, juce::dontSendNotification);
        waveFormList.onChange = [this] { setWaveForm(); };
        
        addAndMakeVisible(waveFormLabel);
        waveFormLabel.setText("Choose wave form", juce::dontSendNotification);
        waveFormLabel.attachToComponent(&waveFormList, true);

        setAudioChannels (0, 2);

        setSize (600, 160);
        startTimer (400);
    }

    ~MainContentComponent() override
    {
        shutdownAudio();
    }

    void resized() override
    {
        midiInputList.setBounds(200, 10, getWidth() - 210, 20);
        tailOffSlider.setBounds(200, 40, getWidth() - 210, 20);
        waveFormList.setBounds(200, 70, getWidth() - 210, 20);
        keyboardComponent.setBounds(10, 100, getWidth() - 20, getHeight() - 100);
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        synthAudioSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        synthAudioSource.getNextAudioBlock (bufferToFill);
    }

    void releaseResources() override
    {
        synthAudioSource.releaseResources();
    }

private:
    void timerCallback() override
    {
        keyboardComponent.grabKeyboardFocus();
        stopTimer();
    }

    void setMidiInput(int index)
    {
        auto list = juce::MidiInput::getAvailableDevices();
        deviceManager.removeMidiInputDeviceCallback(list[lastInputIndex].identifier,
            synthAudioSource.getMidiCollector()); // [12]
        auto newInput = list[index];
        if (!deviceManager.isMidiInputDeviceEnabled(newInput.identifier))
            deviceManager.setMidiInputDeviceEnabled(newInput.identifier, true);
        deviceManager.addMidiInputDeviceCallback(newInput.identifier, synthAudioSource.getMidiCollector()); // [13]
        midiInputList.setSelectedId(index + 1, juce::dontSendNotification);
        lastInputIndex = index;
    }

    void setTailOffSeconds() {
        synthAudioSource.setTailOffSeconds(tailOffSlider.getValue());
    }


    void setWaveForm() {
        synthAudioSource.setWaveForm(static_cast<WaveForm>(waveFormList.getSelectedItemIndex()));
    }

    //==========================================================================
    juce::MidiKeyboardState keyboardState;
    SynthAudioSource synthAudioSource;
    juce::MidiKeyboardComponent keyboardComponent;
    juce::ComboBox midiInputList;
    juce::Label midiInputListLabel;
    juce::Label tailOffLabel;
    juce::Slider tailOffSlider;
    juce::ComboBox waveFormList;
    juce::Label waveFormLabel;
    int lastInputIndex = 0;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

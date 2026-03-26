#pragma once

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
class ADSRGraphComponent : public juce::Component,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    ADSRGraphComponent(juce::AudioProcessorValueTreeState& apvts,
                       std::atomic<bool>& mutedFlag)
        : apvts(apvts), muted(mutedFlag)
    {
        attack  = apvts.getRawParameterValue("attack")->load();
        decay   = apvts.getRawParameterValue("decay")->load();
        sustain = apvts.getRawParameterValue("sustain")->load();
        release = apvts.getRawParameterValue("release")->load();

        apvts.addParameterListener("attack",  this);
        apvts.addParameterListener("decay",   this);
        apvts.addParameterListener("sustain", this);
        apvts.addParameterListener("release", this);

        muteButton.setButtonText("MUTE");
        muteButton.setClickingTogglesState(true);
        muteButton.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff333333));
        muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffcc2222));
        muteButton.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xff888888));
        muteButton.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
        muteButton.onClick = [this]()
        {
            muted.store(muteButton.getToggleState());
        };
        addAndMakeVisible(muteButton);
    }

    ~ADSRGraphComponent() override
    {
        apvts.removeParameterListener("attack",  this);
        apvts.removeParameterListener("decay",   this);
        apvts.removeParameterListener("sustain", this);
        apvts.removeParameterListener("release", this);
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds();

        // Background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillAll();

        // Graph area (leave 14px at bottom for A/D/S/R labels)
        auto ga  = b.reduced(10, 8).toFloat();
        float w  = ga.getWidth();
        float h  = ga.getHeight() - 14.0f;
        float gx = ga.getX();
        float gy = ga.getY();
        float segW = w / 4.0f;

        auto hn = computeHandles(gx, gy, w, h);

        // Envelope fill
        juce::Path fill;
        fill.startNewSubPath(gx, gy + h);
        fill.lineTo(hn.ax, hn.ay);
        fill.lineTo(hn.dx, hn.dy);
        fill.lineTo(hn.sx2, hn.dy);
        fill.lineTo(hn.rx, gy + h);
        fill.closeSubPath();
        g.setColour(juce::Colour(0x2244aaff));
        g.fillPath(fill);

        // Envelope line
        juce::Path line;
        line.startNewSubPath(gx, gy + h);
        line.lineTo(hn.ax, hn.ay);
        line.lineTo(hn.dx, hn.dy);
        line.lineTo(hn.sx2, hn.dy);
        line.lineTo(hn.rx, gy + h);
        g.setColour(juce::Colour(0xff44aaff));
        g.strokePath(line, juce::PathStrokeType(2.0f, juce::PathStrokeType::mitered,
                                                 juce::PathStrokeType::rounded));

        // Handles
        const float hr = 5.0f;
        auto drawHandle = [&](float hx, float hy, bool active)
        {
            g.setColour(active ? juce::Colours::white : juce::Colour(0xffcccccc));
            g.fillEllipse(hx - hr, hy - hr, hr * 2.0f, hr * 2.0f);
            g.setColour(juce::Colour(0xff44aaff));
            g.drawEllipse(hx - hr, hy - hr, hr * 2.0f, hr * 2.0f, 1.5f);
        };
        drawHandle(hn.ax, hn.ay,   draggedHandle == 0);
        drawHandle(hn.dx, hn.dy,   draggedHandle == 1);
        drawHandle(hn.rx, gy + h,  draggedHandle == 2);

        // Zone labels
        g.setColour(juce::Colour(0xff888888));
        g.setFont(11.0f);
        float labelY = gy + h + 1.0f;
        g.drawText("A", juce::Rectangle<float>(gx,          labelY, segW, 13.0f), juce::Justification::centred);
        g.drawText("D", juce::Rectangle<float>(gx + segW,   labelY, segW, 13.0f), juce::Justification::centred);
        g.drawText("S", juce::Rectangle<float>(gx + 2*segW, labelY, segW, 13.0f), juce::Justification::centred);
        g.drawText("R", juce::Rectangle<float>(gx + 3*segW, labelY, segW, 13.0f), juce::Justification::centred);
    }

    void resized() override
    {
        muteButton.setBounds(getWidth() - 62, 6, 54, 24);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        draggedHandle = getHandleAt(e.position);
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (draggedHandle == -1) return;

        auto [gx, gy, w, h, segW] = graphMetrics();
        float px = e.position.x;
        float py = e.position.y;

        auto setParam = [&](const juce::String& id, float val)
        {
            if (auto* p = apvts.getParameter(id))
                p->setValueNotifyingHost(p->convertTo0to1(val));
        };

        if (draggedHandle == 0) // Attack — X only
        {
            float norm = juce::jlimit(0.0f, 1.0f, (px - gx) / segW);
            setParam("attack", 0.001f + norm * (1.0f - 0.001f));
        }
        else if (draggedHandle == 1) // Decay X + Sustain Y
        {
            float normX = juce::jlimit(0.0f, 1.0f, (px - gx - segW) / segW);
            setParam("decay", 0.001f + normX * (2.0f - 0.001f));
            float normY = juce::jlimit(0.0f, 1.0f, 1.0f - (py - gy) / h);
            setParam("sustain", normY);
        }
        else if (draggedHandle == 2) // Release — X only
        {
            float normX = juce::jlimit(0.0f, 1.0f, (px - gx - 3.0f * segW) / segW);
            setParam("release", 0.001f + normX * (5.0f - 0.001f));
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        draggedHandle = -1;
        repaint();
    }

private:
    struct Metrics { float gx, gy, w, h, segW; };

    Metrics graphMetrics() const
    {
        auto b = getLocalBounds();
        auto ga  = b.reduced(10, 8).toFloat();
        float w  = ga.getWidth();
        float h  = ga.getHeight() - 14.0f;
        return { ga.getX(), ga.getY(), w, h, w / 4.0f };
    }

    struct Handles { float ax, ay, dx, dy, sx2, rx; };

    Handles computeHandles(float gx, float gy, float w, float h) const
    {
        float segW = w / 4.0f;
        float aN = (attack  - 0.001f) / (1.0f - 0.001f);
        float dN = (decay   - 0.001f) / (2.0f - 0.001f);
        float rN = (release - 0.001f) / (5.0f - 0.001f);
        return {
            gx + segW * aN,                   // ax
            gy,                                // ay (attack peak always at top)
            gx + segW + segW * dN,             // dx
            gy + h * (1.0f - sustain),         // dy (sustain level)
            gx + 3.0f * segW,                  // sx2 (sustain end)
            gx + 3.0f * segW + segW * rN       // rx
        };
    }

    int getHandleAt(juce::Point<float> pos)
    {
        auto [gx, gy, w, h, segW] = graphMetrics();
        auto hn = computeHandles(gx, gy, w, h);
        const float threshold = 12.0f;
        if (juce::Point<float>(hn.ax, hn.ay ).getDistanceFrom(pos) < threshold) return 0;
        if (juce::Point<float>(hn.dx, hn.dy ).getDistanceFrom(pos) < threshold) return 1;
        if (juce::Point<float>(hn.rx, gy + h).getDistanceFrom(pos) < threshold) return 2;
        return -1;
    }

    void parameterChanged(const juce::String& paramID, float newValue) override
    {
        if      (paramID == "attack")  attack  = newValue;
        else if (paramID == "decay")   decay   = newValue;
        else if (paramID == "sustain") sustain = newValue;
        else if (paramID == "release") release = newValue;

        juce::MessageManager::callAsync(
            [safeThis = juce::Component::SafePointer<ADSRGraphComponent>(this)]()
            {
                if (safeThis) safeThis->repaint();
            });
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<bool>& muted;
    float attack = 0.01f, decay = 0.1f, sustain = 0.8f, release = 0.3f;
    int draggedHandle = -1;
    juce::TextButton muteButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ADSRGraphComponent)
};

//==============================================================================
class LFOComponent : public juce::Component
{
public:
    explicit LFOComponent(juce::AudioProcessorValueTreeState& apvts) : apvts(apvts)
    {
        // On/off toggle button
        enabledButton.setButtonText("ON");
        enabledButton.setClickingTogglesState(true);
        enabledButton.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff333333));
        enabledButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2277cc));
        enabledButton.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xff888888));
        enabledButton.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
        addAndMakeVisible(enabledButton);
        enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "lfoEnabled", enabledButton);

        // SLOW/FAST latch button
        latchButton.setButtonText("SLOW");
        latchButton.setClickingTogglesState(true);
        latchButton.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff333333));
        latchButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff224488));
        latchButton.setColour(juce::TextButton::textColourOffId,  juce::Colours::white);
        latchButton.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
        latchButton.onClick = [this]()
        {
            bool fast = latchButton.getToggleState();
            latchButton.setButtonText(fast ? "FAST" : "SLOW");
            setSpeedRange(!fast);
        };
        addAndMakeVisible(latchButton);

        // Knob setup helper
        auto setupKnob = [&](juce::Slider& s, juce::Label& l, const juce::String& text)
        {
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
            addAndMakeVisible(s);
            l.setText(text, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setColour(juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible(l);
        };

        setupKnob(depthKnob, depthLabel, "Depth");
        setupKnob(speedKnob, speedLabel, "Speed");

        // Mode knob — 3 discrete states
        modeKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        modeKnob.setRange(0.0, 4.0, 1.0);
        modeKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
        modeKnob.textFromValueFunction = [](double v) -> juce::String {
            switch ((int)std::round(v)) {
                case 1:  return "×2";
                case 2:  return "×3";
                case 3:  return "×4";
                case 4:  return "FUND.";
                default: return "ALL";
            }
        };
        modeKnob.setNumDecimalPlacesToDisplay(0);
        addAndMakeVisible(modeKnob);
        modeLabel.setText("Mode", juce::dontSendNotification);
        modeLabel.setJustificationType(juce::Justification::centred);
        modeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(modeLabel);

        depthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "lfoDepth", depthKnob);
        speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "lfoSpeed", speedKnob);
        modeAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "lfoMode",  modeKnob);

        // Apply SLOW range on startup (attachment has set the full range; override it)
        setSpeedRange(true);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillAll();

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("HARMONIC LFO", getLocalBounds().removeFromTop(22).toFloat(),
                   juce::Justification::centred);
    }

    void resized() override
    {
        auto area     = getLocalBounds();
        auto titleBar = area.removeFromTop(22);

        // ON button on the right of the title bar
        enabledButton.setBounds(titleBar.removeFromRight(44).reduced(3, 4));

        int knobSize = juce::jmin(area.getHeight() - 20, 90);
        int latchH   = 18;
        int labelH   = 18;
        int gap      = 30;
        int totalW   = knobSize * 3 + gap * 2;
        // Extra height for latch button above speed knob
        auto block   = area.withSizeKeepingCentre(totalW, latchH + labelH + knobSize);

        // Latch + speed column bounds (middle column)
        auto depthCol = block.removeFromLeft(knobSize);
        block.removeFromLeft(gap);
        auto speedCol = block.removeFromLeft(knobSize);
        block.removeFromLeft(gap);
        auto modeCol  = block;

        // Depth column: label then knob (leave top latchH empty to align knobs)
        depthCol.removeFromTop(latchH);
        depthLabel.setBounds(depthCol.removeFromTop(labelH));
        depthKnob.setBounds(depthCol);

        // Speed column: latch button, then label, then knob
        latchButton.setBounds(speedCol.removeFromTop(latchH).reduced(2, 1));
        speedLabel.setBounds(speedCol.removeFromTop(labelH));
        speedKnob.setBounds(speedCol);

        // Mode column: same offset as depth
        modeCol.removeFromTop(latchH);
        modeLabel.setBounds(modeCol.removeFromTop(labelH));
        modeKnob.setBounds(modeCol);
    }

private:
    void setSpeedRange(bool slow)
    {
        float newMin = slow ? 0.001f : 0.1f;
        float newMax = slow ? 0.1f   : 0.5f;
        float skew   = slow ? 3.0f   : 1.5f;

        speedKnob.setNormalisableRange({ newMin, newMax, 0.0001f, skew });

        float clamped = juce::jlimit(newMin, newMax, (float)speedKnob.getValue());
        if (auto* p = apvts.getParameter("lfoSpeed"))
            p->setValueNotifyingHost(p->convertTo0to1(clamped));
    }

    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton enabledButton;
    juce::TextButton latchButton;
    juce::Slider     depthKnob, speedKnob, modeKnob;
    juce::Label      depthLabel, speedLabel, modeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOComponent)
};

//==============================================================================
class HammondComponent : public juce::Component
{
public:
    explicit HammondComponent(juce::AudioProcessorValueTreeState& apvts) : apvts(apvts)
    {
        auto setupSlider = [&](juce::Slider& s, juce::Label& l, const juce::String& text)
        {
            s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 55, 20);
            addAndMakeVisible(s);
            l.setText(text, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centredRight);
            l.setColour(juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible(l);
        };

        setupSlider(octaveUpSlider,        octaveUpLabel,        "+1 Octave");
        setupSlider(thirdHarmonicSlider,   thirdHarmonicLabel,   "Oct + Fifth (3rd)");
        setupSlider(twoOctavesUpSlider,    twoOctavesUpLabel,    "+2 Octaves");
        setupSlider(fifthHarmonicSlider,   fifthHarmonicLabel,   "2 Oct + Third (5th)");
        setupSlider(sixthHarmonicSlider,   sixthHarmonicLabel,   "2 Oct + Fifth (6th)");
        setupSlider(threeOctavesUpSlider,  threeOctavesUpLabel,  "+3 Octaves");
        setupSlider(fifthSlider,           fifthLabel,           "Quint (5th)");
        setupSlider(subOctaveSlider,       subOctaveLabel,       "Sub Octave");

        octaveUpAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hammondOctaveUp",       octaveUpSlider);
        thirdHarmonicAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hammondThirdHarmonic",  thirdHarmonicSlider);
        twoOctavesUpAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hammondTwoOctavesUp",   twoOctavesUpSlider);
        fifthHarmonicAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hammondFifthHarmonic",  fifthHarmonicSlider);
        sixthHarmonicAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hammondSixthHarmonic",  sixthHarmonicSlider);
        threeOctavesUpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hammondThreeOctavesUp", threeOctavesUpSlider);
        fifthAttachment          = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hammondFifth",          fifthSlider);
        subOctaveAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hammondSubOctave",      subOctaveSlider);

        auto setupChimeBtn = [&](juce::TextButton& btn, const juce::String& text)
        {
            btn.setButtonText(text);
            btn.setClickingTogglesState(true);
            btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff333333));
            btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2277cc));
            btn.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xff888888));
            btn.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
            addAndMakeVisible(btn);
        };

        setupChimeBtn(chime2Button, "2nd");
        setupChimeBtn(chime3Button, "3rd");

        chime2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "hammondChime2", chime2Button);
        chime3Attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "hammondChime3", chime3Button);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillAll();
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("HAMMOND", getLocalBounds().removeFromTop(22).toFloat(),
                   juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop(22);
        auto inner = area.reduced(20, 0);

        auto placeRow = [&](juce::Label& lbl, juce::Slider& s)
        {
            auto row = inner.removeFromTop(28);
            lbl.setBounds(row.removeFromLeft(100));
            s.setBounds(row);
            inner.removeFromTop(8);
        };

        placeRow(octaveUpLabel,       octaveUpSlider);
        placeRow(thirdHarmonicLabel,  thirdHarmonicSlider);
        placeRow(twoOctavesUpLabel,   twoOctavesUpSlider);
        placeRow(fifthHarmonicLabel,  fifthHarmonicSlider);
        placeRow(sixthHarmonicLabel,  sixthHarmonicSlider);
        placeRow(threeOctavesUpLabel, threeOctavesUpSlider);
        placeRow(fifthLabel,          fifthSlider);
        placeRow(subOctaveLabel,      subOctaveSlider);

        // Chime buttons row
        inner.removeFromTop(8);
        auto btnRow = inner.removeFromTop(28);
        chime2Button.setBounds(btnRow.removeFromLeft(70).reduced(2, 2));
        btnRow.removeFromLeft(10);
        chime3Button.setBounds(btnRow.removeFromLeft(70).reduced(2, 2));
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Slider fifthSlider, subOctaveSlider;
    juce::Slider octaveUpSlider, thirdHarmonicSlider, twoOctavesUpSlider;
    juce::Slider fifthHarmonicSlider, sixthHarmonicSlider, threeOctavesUpSlider;
    juce::Label  fifthLabel, subOctaveLabel;
    juce::Label  octaveUpLabel, thirdHarmonicLabel, twoOctavesUpLabel;
    juce::Label  fifthHarmonicLabel, sixthHarmonicLabel, threeOctavesUpLabel;
    juce::TextButton chime2Button, chime3Button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fifthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subOctaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveUpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thirdHarmonicAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> twoOctavesUpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fifthHarmonicAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sixthHarmonicAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threeOctavesUpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chime2Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chime3Attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HammondComponent)
};

//==============================================================================
class RotaryComponent : public juce::Component
{
public:
    explicit RotaryComponent(juce::AudioProcessorValueTreeState& apvts) : apvts(apvts)
    {
        auto setupBtn = [&](juce::TextButton& btn, const juce::String& text,
                            juce::Colour onColour)
        {
            btn.setButtonText(text);
            btn.setClickingTogglesState(true);
            btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff333333));
            btn.setColour(juce::TextButton::buttonOnColourId, onColour);
            btn.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xff888888));
            btn.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
            addAndMakeVisible(btn);
        };

        setupBtn(onButton,    "ON",   juce::Colour(0xff2277cc));
        setupBtn(speedButton, "SLOW", juce::Colour(0xffaa4400));

        onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "rotaryEnabled", onButton);
        speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "rotaryFast", speedButton);

        // Sync text with restored parameter state
        speedButton.setButtonText(speedButton.getToggleState() ? "FAST" : "SLOW");
        speedButton.onClick = [this]()
        {
            speedButton.setButtonText(speedButton.getToggleState() ? "FAST" : "SLOW");
        };
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillAll();
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("ROTARY (LESLIE)", getLocalBounds().removeFromTop(22).toFloat(),
                   juce::Justification::centred);

        // Description labels
        g.setColour(juce::Colour(0xff888888));
        g.setFont(11.0f);
        auto area = getLocalBounds().reduced(20);
        area.removeFromTop(60);
        g.drawText("Horn: slow 0.8 Hz  /  fast 6.7 Hz", area.removeFromTop(16).toFloat(),
                   juce::Justification::centred);
        g.drawText("Drum: slow 0.6 Hz  /  fast 5.7 Hz", area.removeFromTop(16).toFloat(),
                   juce::Justification::centred);
    }

    void resized() override
    {
        auto area   = getLocalBounds();
        area.removeFromTop(22);
        auto row    = area.withSizeKeepingCentre(220, 36).translated(0, 6);
        onButton.setBounds   (row.removeFromLeft(100).reduced(4));
        speedButton.setBounds(row.reduced(4));
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton onButton, speedButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> speedAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryComponent)
};

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AudioPluginAudioProcessor& processorRef;

    // Keyboard
    juce::MidiKeyboardComponent keyboardComponent;
    int keyboardHeight = 80;

    // Waveform selector
    juce::Slider waveformSlider;
    juce::Label  waveformLabel;

    // Harmonics control
    juce::Slider harmonicsSlider;
    juce::Label  harmonicsLabel;

    // Subharmonics control
    juce::Slider subharmonicsSlider;
    juce::Label  subharmonicsLabel;

    // Bottom panel tabs
    ADSRGraphComponent      adsrGraph;
    LFOComponent            lfoComponent;
    HammondComponent        hammondComponent;
    RotaryComponent         rotaryComponent;
    juce::TabbedComponent   tabs { juce::TabbedButtonBar::TabsAtTop };

    // APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> waveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonicsAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subharmonicsAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};

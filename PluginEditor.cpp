#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      keyboardComponent(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      adsrGraph(p.getValueTreeState(), p.muted, [&p]() { return p.getActiveVoiceCount(); }),
      lfoComponent(p.getValueTreeState()),
      hammondComponent(p.getValueTreeState()),
      rotaryComponent(p.getValueTreeState()),
      airComponent(p.getValueTreeState()),
      ladderComponent(p.getValueTreeState()),
      jiComponent(p.getValueTreeState())
{
    // ==================== WAVEFORM SELECTOR ====================
    waveformLabel.setText("Waveform", juce::dontSendNotification);
    waveformLabel.setJustificationType(juce::Justification::centredRight);
    waveformLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(waveformLabel);

    waveformSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    waveformSlider.setRange(0.0, 2.0, 1.0);
    waveformSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 80, 20);
    addAndMakeVisible(waveformSlider);

    waveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(), "waveform", waveformSlider);

    // Harmonics
    harmonicsLabel.setText("Harmonics", juce::dontSendNotification);
    harmonicsLabel.setJustificationType(juce::Justification::centredRight);
    harmonicsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(harmonicsLabel);

    harmonicsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    harmonicsSlider.setRange(1.0, 16.0, 0.01);
    harmonicsSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    addAndMakeVisible(harmonicsSlider);

    harmonicsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(), "harmonics", harmonicsSlider);

    // Subharmonics
    subharmonicsLabel.setText("Subharmonics", juce::dontSendNotification);
    subharmonicsLabel.setJustificationType(juce::Justification::centredRight);
    subharmonicsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(subharmonicsLabel);

    subharmonicsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    subharmonicsSlider.setRange(0.0, 8.0, 0.01);
    subharmonicsSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    addAndMakeVisible(subharmonicsSlider);

    subharmonicsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(), "subharmonics", subharmonicsSlider);

    // Tabs
    tabs.setColour(juce::TabbedComponent::backgroundColourId,   juce::Colour(0xff1a1a1a));
    tabs.setColour(juce::TabbedComponent::outlineColourId,       juce::Colour(0xff3a3a3a));
    tabs.setTabBarDepth(28);
    tabs.addTab("ENVELOPE",     juce::Colour(0xff2a2a2a), &adsrGraph,        false);
    tabs.addTab("HARMONIC LFO", juce::Colour(0xff2a2a2a), &lfoComponent,     false);
    tabs.addTab("HAMMOND",      juce::Colour(0xff2a2a2a), &hammondComponent,  false);
    tabs.addTab("ROTARY",       juce::Colour(0xff2a2a2a), &rotaryComponent,   false);
    tabs.addTab("AIR",          juce::Colour(0xff2a2a2a), &airComponent,      false);
    tabs.addTab("LADDER",       juce::Colour(0xff2a2a2a), &ladderComponent,   false);
    tabs.addTab("TUNING",       juce::Colour(0xff2a2a2a), &jiComponent,       false);
    addAndMakeVisible(tabs);

    // Keyboard
    addAndMakeVisible(keyboardComponent);

    setSize(800, 620);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Keyboard — fixed at bottom
    keyboardComponent.setBounds(0, getHeight() - keyboardHeight, getWidth(), keyboardHeight);
    bounds.removeFromBottom(keyboardHeight);

    // Helper: place a label + slider row
    auto placeRow = [&](int height, juce::Label& label, juce::Slider& slider) {
        auto row = bounds.removeFromTop(height);
        label.setBounds(row.removeFromLeft(100).reduced(2, 5));
        slider.setBounds(row.reduced(2, 5));
    };

    placeRow(35, waveformLabel,     waveformSlider);
    placeRow(35, harmonicsLabel,    harmonicsSlider);
    placeRow(35, subharmonicsLabel, subharmonicsSlider);

    // Tabbed panel — takes all remaining space
    tabs.setBounds(bounds.reduced(10, 5));
}

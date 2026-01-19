#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      keyboardComponent(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    // ==================== WAVEFORM SELECTOR ====================
    waveformLabel.setText("Waveform", juce::dontSendNotification);
    waveformLabel.setJustificationType(juce::Justification::centredRight);
    waveformLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(waveformLabel);

    waveformSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    waveformSlider.setRange(0.0, 3.0, 1.0);
    waveformSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 80, 20);
    addAndMakeVisible(waveformSlider);

    waveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(),
        "waveform",
        waveformSlider);

    // Harmonics
    harmonicsLabel.setText("Harmonics", juce::dontSendNotification);
    harmonicsLabel.setJustificationType(juce::Justification::centredRight);
    harmonicsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(harmonicsLabel);

    harmonicsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    harmonicsSlider.setRange(0.0, 16.0, 1.0);  // 1-16, step by 1
    harmonicsSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    addAndMakeVisible(harmonicsSlider);

    harmonicsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(),
        "harmonics",
        harmonicsSlider);

    // Subharmonics
    subharmonicsLabel.setText("Subharmonics", juce::dontSendNotification);
    subharmonicsLabel.setJustificationType(juce::Justification::centredRight);
    subharmonicsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(subharmonicsLabel);

    subharmonicsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    subharmonicsSlider.setRange(0.0, 8.0, 1.0);  // 0-8, step by 1
    subharmonicsSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    addAndMakeVisible(subharmonicsSlider);

    subharmonicsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(),
        "subharmonics",
        subharmonicsSlider);

    // ADSR
    adsrTitleLabel.setText("ENVELOPE (ADSR)", juce::dontSendNotification);
    adsrTitleLabel.setJustificationType(juce::Justification::centred);
    adsrTitleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    adsrTitleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    addAndMakeVisible(adsrTitleLabel);

    attackLabel.setText("Attack", juce::dontSendNotification);
    attackLabel.setJustificationType(juce::Justification::centredTop);
    attackLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(attackLabel);

    attackSlider.setSliderStyle(juce::Slider::LinearVertical);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    attackSlider.setTextValueSuffix(" s");
    addAndMakeVisible(attackSlider);

    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(),
        "attack",
        attackSlider);

    decayLabel.setText("Decay", juce::dontSendNotification);
    decayLabel.setJustificationType(juce::Justification::centredTop);
    decayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(decayLabel);

    decaySlider.setSliderStyle(juce::Slider::LinearVertical);
    decaySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    decaySlider.setTextValueSuffix(" s");
    addAndMakeVisible(decaySlider);

    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(),
        "decay",
        decaySlider);

    sustainLabel.setText("Sustain", juce::dontSendNotification);
    sustainLabel.setJustificationType(juce::Justification::centredTop);
    sustainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(sustainLabel);

    sustainSlider.setSliderStyle(juce::Slider::LinearVertical);
    sustainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(sustainSlider);

    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(),
        "sustain",
        sustainSlider);

    releaseLabel.setText("Release", juce::dontSendNotification);
    releaseLabel.setJustificationType(juce::Justification::centredTop);
    releaseLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(releaseLabel);

    releaseSlider.setSliderStyle(juce::Slider::LinearVertical);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    releaseSlider.setTextValueSuffix(" s");
    addAndMakeVisible(releaseSlider);

    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(),
        "release",
        releaseSlider);

    // keyboard
    addAndMakeVisible(keyboardComponent);

    setSize(800, 500);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    // Attachments are automatically cleaned up by unique_ptr
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff2a2a2a));

    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(10, 170, getWidth() - 20, 240);

    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRect(10, 170, getWidth() - 20, 240, 2);
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // WAVEFORM
    auto waveformArea = bounds.removeFromTop(60);
    waveformArea.removeFromTop(10);

    auto waveformRow = waveformArea.removeFromTop(30);
    waveformLabel.setBounds(waveformRow.removeFromLeft(100).reduced(5));
    waveformSlider.setBounds(waveformRow.reduced(5));

    // HARMONICS
    auto harmonicsArea = bounds.removeFromTop(50);
    harmonicsArea.removeFromTop(5);

    auto harmonicsRow = harmonicsArea.removeFromTop(30);
    harmonicsLabel.setBounds(harmonicsRow.removeFromLeft(100).reduced(5));
    harmonicsSlider.setBounds(harmonicsRow.reduced(5));

    // SUBHARMONICS
    auto subharmonicsArea = bounds.removeFromTop(50);
    subharmonicsArea.removeFromTop(5);

    auto subharmonicsRow = subharmonicsArea.removeFromTop(30);
    subharmonicsLabel.setBounds(subharmonicsRow.removeFromLeft(100).reduced(5));
    subharmonicsSlider.setBounds(subharmonicsRow.reduced(5));

    // ADSR
    auto adsrArea = bounds.removeFromTop(250);
    adsrArea.reduce(20, 10);

    adsrTitleLabel.setBounds(adsrArea.removeFromTop(25));
    adsrArea.removeFromTop(5);

    auto sliderWidth = (adsrArea.getWidth() - 30) / 4;

    auto attackArea = adsrArea.removeFromLeft(sliderWidth);
    attackLabel.setBounds(attackArea.removeFromTop(20));
    attackSlider.setBounds(attackArea.reduced(5));

    adsrArea.removeFromLeft(10);

    auto decayArea = adsrArea.removeFromLeft(sliderWidth);
    decayLabel.setBounds(decayArea.removeFromTop(20));
    decaySlider.setBounds(decayArea.reduced(5));

    adsrArea.removeFromLeft(10);

    auto sustainArea = adsrArea.removeFromLeft(sliderWidth);
    sustainLabel.setBounds(sustainArea.removeFromTop(20));
    sustainSlider.setBounds(sustainArea.reduced(5));

    adsrArea.removeFromLeft(10);

    auto releaseArea = adsrArea.removeFromLeft(sliderWidth);
    releaseLabel.setBounds(releaseArea.removeFromTop(20));
    releaseSlider.setBounds(releaseArea.reduced(5));

    // KEYS
    keyboardComponent.setBounds(
        0,
        getHeight() - keyboardHeight,
        getWidth(),
        keyboardHeight);
}
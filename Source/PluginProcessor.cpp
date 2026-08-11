#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr size_t kMaxLoadBytes = 32u * 1024u * 1024u; // 32 MB cap

// Parameter IDs
// PluginProcessor.h など
static inline constexpr int kParamVersion = 1;

static inline const juce::ParameterID pSourceMode { "sourceMode", kParamVersion };
static inline const juce::ParameterID pSeed       { "seed",       kParamVersion };

static inline const juce::ParameterID pTickRate   { "tickRate",   kParamVersion };
static inline const juce::ParameterID pReadRate   { "readRate",   kParamVersion };
static inline const juce::ParameterID pFrameSize  { "frameSize",  kParamVersion };
static inline const juce::ParameterID pCorrupt    { "corrupt",    kParamVersion };

static inline const juce::ParameterID pFaultRate  { "faultRate",  kParamVersion };
static inline const juce::ParameterID pFaultLen   { "faultLen",   kParamVersion };
static inline const juce::ParameterID pSeekRange  { "seekRange",  kParamVersion };

static inline const juce::ParameterID pWDropout   { "wDropout",   kParamVersion };
static inline const juce::ParameterID pWRepeat    { "wRepeat",    kParamVersion };
static inline const juce::ParameterID pWSkip      { "wSkip",      kParamVersion };
static inline const juce::ParameterID pWDesync    { "wDesync",    kParamVersion };
static inline const juce::ParameterID pWBitslip   { "wBitslip",   kParamVersion };
static inline const juce::ParameterID pWStall     { "wStall",     kParamVersion };

static inline const juce::ParameterID pRawMix     { "rawMix",     kParamVersion };
static inline const juce::ParameterID pCombMix    { "combMix",    kParamVersion };
static inline const juce::ParameterID pCombDelayMs{ "combDelayMs",kParamVersion };
static inline const juce::ParameterID pCombFb     { "combFb",     kParamVersion };

static inline const juce::ParameterID pDrive      { "drive",      kParamVersion };
static inline const juce::ParameterID pOutGainDb  { "outGainDb",  kParamVersion };
static inline const juce::ParameterID pDcHpHz     { "dcHpHz",     kParamVersion };
}

BinaryGlitchAudioProcessor::BinaryGlitchAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Seed internal generator
    const auto s = static_cast<uint32_t>(*apvts.getRawParameterValue(pSeed.getParamID()));
    seed.store(s);
    lfsr.seed(s);
    fault.reset(s ^ 0xA5A5A5A5u);

    for (auto& v : voices)
        v = {};
}

const juce::String BinaryGlitchAudioProcessor::getName() const { return "BinaryGlitch"; }
bool BinaryGlitchAudioProcessor::acceptsMidi() const { return false; }
bool BinaryGlitchAudioProcessor::producesMidi() const { return false; }
bool BinaryGlitchAudioProcessor::isMidiEffect() const { return false; }
double BinaryGlitchAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int BinaryGlitchAudioProcessor::getNumPrograms() { return 1; }
int BinaryGlitchAudioProcessor::getCurrentProgram() { return 0; }
void BinaryGlitchAudioProcessor::setCurrentProgram (int) {}
const juce::String BinaryGlitchAudioProcessor::getProgramName (int) { return {}; }
void BinaryGlitchAudioProcessor::changeProgramName (int, const juce::String&) {}

bool BinaryGlitchAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* BinaryGlitchAudioProcessor::createEditor() { return new BinaryGlitchAudioProcessorEditor (*this); }

bool BinaryGlitchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn  = layouts.getMainInputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    // allow synth-like usage by ignoring input mismatch, but we will process input if present
    if (! mainIn.isDisabled() && mainIn != mainOut)
        return false;

    return true;
}

void BinaryGlitchAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    tickPhaseSamples = 0.0;

    comb.prepare(sr, getTotalNumOutputChannels(), 50.0f);
    comb.reset();

    for (int ch = 0; ch < 2; ++ch)
    {
        hpf[ch].reset();
        if (hpfCoefficients[ch] == nullptr)
            hpfCoefficients[ch] = new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        hpf[ch].coefficients = hpfCoefficients[ch];
    }
    currentHpHz = -1.0f;

    // Start in a stable place
    readPos = 0;
    boundary = 0;

    const auto s = static_cast<uint32_t>(*apvts.getRawParameterValue(pSeed.getParamID()));
    seed.store(s);
    lfsr.seed(s);
    fault.reset(s ^ 0xA5A5A5A5u);
    modulationRng.seed(s ^ 0x6D2B79F5u);

    internalByteData.resize(1u << 20);
    refillInternalSource(s);
    updateDerived();
}

void BinaryGlitchAudioProcessor::releaseResources() {}

static inline float dbToLin (float db) noexcept
{
    return std::pow(10.0f, db / 20.0f);
}

void BinaryGlitchAudioProcessor::updateDerived()
{
    tickRateHz = juce::jlimit(200.0, 8000.0, (double) *apvts.getRawParameterValue(pTickRate.getParamID()));
    tickIntervalSamples = juce::jmax(1.0, sr / tickRateHz);

    combMix = *apvts.getRawParameterValue(pCombMix.getParamID());
    comb.setDelayMs(*apvts.getRawParameterValue(pCombDelayMs.getParamID()));
    comb.setFeedback(*apvts.getRawParameterValue(pCombFb.getParamID()));

    bg::FaultWeights w;
    w.dropout = *apvts.getRawParameterValue(pWDropout.getParamID());
    w.repeat  = *apvts.getRawParameterValue(pWRepeat.getParamID());
    w.skip    = *apvts.getRawParameterValue(pWSkip.getParamID());
    w.desync  = *apvts.getRawParameterValue(pWDesync.getParamID());
    w.bitslip = *apvts.getRawParameterValue(pWBitslip.getParamID());
    w.stall   = *apvts.getRawParameterValue(pWStall.getParamID());
    fault.setWeights(w);

    const auto requestedSeed = static_cast<uint32_t>(*apvts.getRawParameterValue(pSeed.getParamID()));
    if (requestedSeed != internalSeed)
    {
        seed.store(requestedSeed);
        lfsr.seed(requestedSeed);
        fault.reset(requestedSeed ^ 0xA5A5A5A5u);
        modulationRng.seed(requestedSeed ^ 0x6D2B79F5u);
        refillInternalSource(requestedSeed);
    }

    const float hpHz = *apvts.getRawParameterValue(pDcHpHz.getParamID());
    updateHighPassCoefficients(hpHz);
}

void BinaryGlitchAudioProcessor::refillInternalSource (uint32_t newSeed) noexcept
{
    if (internalByteData.empty())
        return;

    internalSeed = newSeed;
    bg::Lfsr32 gen;
    gen.seed(newSeed);

    for (auto& b : internalByteData)
        b = gen.nextByte();
}

void BinaryGlitchAudioProcessor::updateHighPassCoefficients (float hpHz) noexcept
{
    const float clampedHz = juce::jlimit(5.0f, 200.0f, hpHz);
    if (std::abs(clampedHz - currentHpHz) < 0.001f)
        return;

    currentHpHz = clampedHz;

    const auto omega = juce::MathConstants<double>::twoPi * static_cast<double>(clampedHz) / juce::jmax(1.0, sr);
    const auto sinOmega = std::sin(omega);
    const auto cosOmega = std::cos(omega);
    const auto q = juce::MathConstants<double>::sqrt2 * 0.5;
    const auto alpha = sinOmega / (2.0 * q);

    const auto b0 = (1.0 + cosOmega) * 0.5;
    const auto b1 = -(1.0 + cosOmega);
    const auto b2 = (1.0 + cosOmega) * 0.5;
    const auto a0 = 1.0 + alpha;
    const auto a1 = -2.0 * cosOmega;
    const auto a2 = 1.0 - alpha;
    const auto a0Inv = 1.0 / a0;

    for (auto& coeffs : hpfCoefficients)
    {
        if (coeffs == nullptr)
            continue;

        auto* raw = coeffs->getRawCoefficients();
        raw[0] = static_cast<float>(b0 * a0Inv);
        raw[1] = static_cast<float>(b1 * a0Inv);
        raw[2] = static_cast<float>(b2 * a0Inv);
        raw[3] = static_cast<float>(a1 * a0Inv);
        raw[4] = static_cast<float>(a2 * a0Inv);
    }
}

uint8_t BinaryGlitchAudioProcessor::readByteWithFaults (const bg::ByteSource::ByteData& data, int64 pos) const
{
    if (data.empty())
        return 0;

    const auto n = static_cast<int64>(data.size());
    auto p = pos % n;
    if (p < 0) p += n;

    return data[(size_t) p];
}

float BinaryGlitchAudioProcessor::renderTickSample (int channel, uint8_t b0, uint8_t b1, bool gate)
{
    // Primary: simple phase-distortion sine with per-tick gate envelope
    auto& v = voices[juce::jlimit(0, 1, channel)];

    // Exponential-ish mapping for a digital-but-not-musical pitch range
    const float x = static_cast<float>(b0) / 255.0f; // 0..1
    const double f = 40.0 * std::pow(2.0, (double) (x * 8.0)); // ~40..10240Hz
    v.freq = juce::jlimit(20.0, 12000.0, f);

    const float pd = static_cast<float>(b1) / 255.0f; // 0..1

    // envelope
    const float atk = 0.35f;
    const float rel = 0.08f;
    if (gate)
        v.env = juce::jmin(1.0f, v.env + atk);
    else
        v.env = juce::jmax(0.0f, v.env - rel);

    // oscillator (evaluated per sample later; tick sets target, but for simplicity we render at audio rate below)
    // This function returns the per-sample oscillator value based on current voice state.

    const double inc = juce::MathConstants<double>::twoPi * v.freq / sr;
    v.phase += inc;
    if (v.phase > juce::MathConstants<double>::twoPi)
        v.phase -= juce::MathConstants<double>::twoPi;

    const double base = v.phase;
    const double warped = base + (double) (pd * 1.6f) * std::sin(base * 2.0);
    const float osc = (float) std::sin(warped);

    return osc * v.env;
}

void BinaryGlitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    updateDerived();

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Select active byte data
    std::shared_ptr<const bg::ByteSource::ByteData> fileData = byteSource.getData();

    const int mode = (int) *apvts.getRawParameterValue(pSourceMode.getParamID());
    const auto* active = (mode == 1 && fileData != nullptr && ! fileData->empty()) ? fileData.get() : &internalByteData;

    if (active == nullptr || active->empty())
    {
        buffer.clear();
        return;
    }

    const auto& data = *active;

    const float readRateBytes = *apvts.getRawParameterValue(pReadRate.getParamID());
    const int frameSize = (int) *apvts.getRawParameterValue(pFrameSize.getParamID());
    const float corrupt = *apvts.getRawParameterValue(pCorrupt.getParamID());

    const float faultRate = *apvts.getRawParameterValue(pFaultRate.getParamID());
    const int faultLenTicks = (int) *apvts.getRawParameterValue(pFaultLen.getParamID());
    const int seekRange = (int) *apvts.getRawParameterValue(pSeekRange.getParamID());

    const float rawMix = *apvts.getRawParameterValue(pRawMix.getParamID());
    const float drive = *apvts.getRawParameterValue(pDrive.getParamID());
    const float outGain = dbToLin(*apvts.getRawParameterValue(pOutGainDb.getParamID()));

    const int numSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Tick scheduling
        tickPhaseSamples += 1.0;
        if (tickPhaseSamples >= tickIntervalSamples)
        {
            tickPhaseSamples -= tickIntervalSamples;

            // Fault update (one per tick)
            fault.tick(tickRateHz, faultRate, faultLenTicks, seekRange, readPos);

            // Advance transport (bytes per tick)
            const auto ft = fault.getCurrentFault();

            if (ft == bg::FaultType::stall)
            {
                // no movement
            }
            else if (ft == bg::FaultType::skip)
            {
                // occasional aggressive forward jump
                readPos += (int64) (readRateBytes * 1.0f);
                if (modulationRng.nextFloat01() < 0.25f)
                    readPos += modulationRng.nextInt(-seekRange, seekRange - 1);
            }
            else
            {
                readPos += (int64) juce::jmax(1.0f, readRateBytes);
            }

            // keep boundary wandering slightly
            boundary = (boundary + modulationRng.nextInt(-1, 1)) & 1;
        }

        // Frame header derived corruption (gives repeating structure)
        int64 pos = readPos;

        const int64 fs = (int64) juce::jmax(64, frameSize);
        const int64 frameStart = (pos / fs) * fs;
        const uint8_t h0 = readByteWithFaults(data, frameStart);
        const uint8_t h1 = readByteWithFaults(data, frameStart + 1);

        const uint8_t xorMask = (uint8_t) (h0 * (uint8_t) juce::jlimit(0, 255, (int) (corrupt * 255.0f)));
        const int rot = (int) (h1 & 7);

        // Read bytes (stereo de-correlation by small offset)
        const int64 stereoOffset = 17;

        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        {
            const int64 chPos = pos + (ch == 0 ? 0 : stereoOffset);

            // Apply repeat window
            const auto ft = fault.getCurrentFault();
            int64 readP = chPos;

            if (ft == bg::FaultType::repeat)
            {
                const int span = juce::jmax(16, seekRange);
                const int64 base = fault.getRepeatBasePos();
                const int64 rel = readP - base;
                readP = base + (rel % span);
            }

            if (ft == bg::FaultType::desync)
                readP += fault.getDesyncOffsetBytes();

            // Byte 0/1
            uint8_t b0 = readByteWithFaults(data, readP + boundary);
            uint8_t b1 = readByteWithFaults(data, readP + boundary + 1);

            // corruption
            b0 ^= xorMask;
            b1 ^= (uint8_t) (xorMask ^ 0x5A);

            if (rot != 0)
            {
                b0 = (uint8_t) ((b0 << rot) | (b0 >> (8 - rot)));
                b1 = (uint8_t) ((b1 >> rot) | (b1 << (8 - rot)));
            }

            if (ft == bg::FaultType::bitslip)
            {
                const int slip = fault.getBitSlip();
                const int sAbs = std::abs(slip);
                if (sAbs != 0)
                {
                    if (slip > 0)
                        b0 = (uint8_t) ((b0 << sAbs) | (b0 >> (8 - sAbs)));
                    else
                        b0 = (uint8_t) ((b0 >> sAbs) | (b0 << (8 - sAbs)));
                }
            }

            // gate logic: fault dropout forces gate off
            const bool gate = (ft != bg::FaultType::dropout) && (b1 > 96);

            // RAW: 8-bit unsigned to -1..1
            float raw = ((float) b0 / 127.5f) - 1.0f;

            // Synthesis: structured layer
            float synth = renderTickSample(ch, b0, b1, gate);

            float y = rawMix * raw + (1.0f - rawMix) * synth;

            // Thin comb (structure enhancer)
            if (combMix > 0.0001f)
            {
                const float c = comb.process(ch, y);
                y = y + combMix * c;
            }

            // Soft clip / drive
            y = std::tanh(y * drive) * outGain;

            // DC / HPF
            if (ch < 2)
                y = hpf[ch].processSample(y);

            buffer.setSample(ch, sample, y);
        }

        comb.advance();
    }
}

void BinaryGlitchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (lastFile.existsAsFile())
        state.setProperty("lastFile", lastFile.getFullPathName(), nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BinaryGlitchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr)
        return;

    if (xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));

        const auto p = apvts.state.getProperty("lastFile").toString();
        if (p.isNotEmpty())
            lastFile = juce::File(p);

        const auto s = static_cast<uint32_t>(*apvts.getRawParameterValue(pSeed.getParamID()));
        seed.store(s);
        lfsr.seed(s);
        fault.reset(s ^ 0xA5A5A5A5u);
    }
}

bool BinaryGlitchAudioProcessor::loadBinaryFile (const juce::File& file, juce::String* outError)
{
    juce::String err;
    auto bytes = bg::ByteSource::loadFileToMemory(file, kMaxLoadBytes, &err);
    if (bytes == nullptr || bytes->empty())
    {
        if (outError) *outError = err.isNotEmpty() ? err : "Failed to load file.";
        return false;
    }

    byteSource.setData(bytes);
    lastFile = file;
    return true;
}

void BinaryGlitchAudioProcessor::clearLoadedFile()
{
    byteSource.clear();
    lastFile = juce::File{};
}

juce::File BinaryGlitchAudioProcessor::getLastLoadedFile() const
{
    return lastFile;
}

juce::AudioProcessorValueTreeState::ParameterLayout BinaryGlitchAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(pSourceMode, "Source", juce::StringArray{ "Internal", "File" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(pSeed, "Seed", 1, 999999, 12345));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(pTickRate, "Tick Rate (Hz)", juce::NormalisableRange<float>(200.0f, 8000.0f, 1.0f, 0.35f), 1500.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pReadRate, "Read Rate (bytes/tick)", juce::NormalisableRange<float>(0.25f, 32.0f, 0.01f, 0.5f), 2.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(pFrameSize, "Frame Size (bytes)", 64, 8192, 512));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pCorrupt, "Header Corruption", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0005f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(pFaultRate, "Fault Rate (starts/sec)", juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f, 0.5f), 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(pFaultLen, "Fault Length (ticks)", 1, 2000, 220));
    params.push_back(std::make_unique<juce::AudioParameterInt>(pSeekRange, "Seek Range (bytes)", 16, 65536, 2048));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(pWDropout, "W:Dropout", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.20f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pWRepeat,  "W:Repeat",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pWSkip,    "W:Skip",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.20f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pWDesync,  "W:Desync",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.10f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pWBitslip, "W:Bitslip", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.03f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pWStall,   "W:Stall",   juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.02f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(pRawMix, "Raw Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0005f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pCombMix, "Comb Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0005f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pCombDelayMs, "Comb Delay (ms)", juce::NormalisableRange<float>(1.0f, 50.0f, 0.01f, 0.4f), 12.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pCombFb, "Comb Feedback", juce::NormalisableRange<float>(0.0f, 0.95f, 0.0005f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(pDrive, "Drive", juce::NormalisableRange<float>(0.5f, 8.0f, 0.0005f, 0.4f), 2.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pOutGainDb, "Output Gain (dB)", juce::NormalisableRange<float>(-24.0f, 6.0f, 0.01f), -6.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(pDcHpHz, "DC/HPF (Hz)", juce::NormalisableRange<float>(5.0f, 200.0f, 0.01f, 0.5f), 30.0f));

    return { params.begin(), params.end() };
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BinaryGlitchAudioProcessor();
}

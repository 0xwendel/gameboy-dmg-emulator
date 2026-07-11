#pragma once

#include <cstdint>
#include <vector>

// APU DMG fiel: 4 canais, frame sequencer no falling-edge do bit 12 do DIV,
// length/envelope/sweep, DAC, pan/volume NR50/NR51, high-pass e buffer 44.1 kHz.
class APU {
public:
    APU();

    void reset();
    void writeRegister(uint16_t address, uint8_t value);
    uint8_t readRegister(uint16_t address) const;

    // Um T-cycle: edge do DIV (FS) + timers de canal + geração de sample.
    void tickTCycle(uint16_t divBefore, uint16_t divAfter);

    // Escrita em FF04 (DIV=0): pode gerar falling-edge do bit 12.
    void onDivReset(uint16_t divBefore);

    size_t popSamples(int16_t* out, size_t maxFrames);
    size_t samplesAvailable() const { return m_sampleBuffer.size() / 2; }
    void clearSampleBuffer() { m_sampleBuffer.clear(); }

    void setEnabled(bool enabled) { m_outputEnabled = enabled; }
    bool enabled() const { return m_outputEnabled; }
    void setVolume(float volume) { m_volume = volume; }

    bool powerOn() const { return (m_nr52 & 0x80) != 0; }

    void serialize(std::vector<uint8_t>& out) const;
    bool deserialize(const uint8_t*& ptr, const uint8_t* end);

    static constexpr int kSampleRate = 44100;

private:
    struct SquareChannel {
        bool enabled = false;
        bool dacEnabled = false;
        uint8_t duty = 0;
        uint8_t length = 0;
        bool lengthEnabled = false;
        uint8_t volume = 0;
        uint8_t envelopePeriod = 0;
        uint8_t envelopeTimer = 0;
        bool envelopeIncrease = false;
        bool envelopeRunning = false;
        uint16_t frequency = 0;
        uint16_t freqTimer = 0;
        uint8_t dutyStep = 0;
        uint8_t nr0 = 0, nr1 = 0, nr2 = 0, nr3 = 0, nr4 = 0;

        bool hasSweep = false;
        uint8_t sweepPeriod = 0;
        uint8_t sweepTimer = 0;
        bool sweepDecrease = false;
        uint8_t sweepShift = 0;
        uint16_t shadowFreq = 0;
        bool sweepEnabled = false;
        bool sweepNegateUsed = false;

        void trigger(bool isCh1);
        void clockLength();
        void clockEnvelope();
        void clockSweep();
        uint16_t calcSweep() const;
        uint8_t digitalOutput() const;
        void tickTimer();
        uint16_t period() const;
    };

    struct WaveChannel {
        bool enabled = false;
        bool dacEnabled = false;
        uint16_t length = 0;
        bool lengthEnabled = false;
        uint8_t volumeCode = 0;
        uint16_t frequency = 0;
        uint16_t freqTimer = 0;
        uint8_t position = 0;
        uint8_t waveRam[16]{};
        uint8_t nr0 = 0, nr1 = 0, nr2 = 0, nr3 = 0, nr4 = 0;
        uint8_t currentSample = 0;

        void trigger();
        void clockLength();
        uint8_t digitalOutput() const;
        void tickTimer();
        uint16_t period() const;
    };

    struct NoiseChannel {
        bool enabled = false;
        bool dacEnabled = false;
        uint8_t length = 0;
        bool lengthEnabled = false;
        uint8_t volume = 0;
        uint8_t envelopePeriod = 0;
        uint8_t envelopeTimer = 0;
        bool envelopeIncrease = false;
        bool envelopeRunning = false;
        uint8_t clockShift = 0;
        bool widthMode = false;
        uint8_t divisorCode = 0;
        uint16_t lfsr = 0x7FFF;
        uint16_t freqTimer = 0;
        uint8_t nr1 = 0, nr2 = 0, nr3 = 0, nr4 = 0;

        void trigger();
        void clockLength();
        void clockEnvelope();
        uint8_t digitalOutput() const;
        void tickTimer();
        uint16_t period() const;
    };

    void clockFrameSequencer();
    void maybeExtraLengthClock(bool wasLengthEnabled, bool nowLengthEnabled, SquareChannel& ch);
    void maybeExtraLengthClock(bool wasLengthEnabled, bool nowLengthEnabled, WaveChannel& ch);
    void maybeExtraLengthClock(bool wasLengthEnabled, bool nowLengthEnabled, NoiseChannel& ch);
    void mixSample();
    void powerOff();
    void updateNr52Status();

    SquareChannel m_ch1;
    SquareChannel m_ch2;
    WaveChannel m_ch3;
    NoiseChannel m_ch4;

    uint8_t m_nr50 = 0x77;
    uint8_t m_nr51 = 0xF3;
    uint8_t m_nr52 = 0xF1;

    bool m_outputEnabled = true; // mute do host (não desliga hardware)
    float m_volume = 0.6f;

    uint8_t m_frameSeqStep = 0;

    // Sample generation: 4194304 / 44100 ≈ 95.108
    double m_sampleTimer = 0.0;
    static constexpr double kCyclesPerSample = 4194304.0 / kSampleRate;

    // High-pass (capacitor) por canal estereo — remove DC do DAC
    double m_hpLeft = 0.0;
    double m_hpRight = 0.0;
    // ~0.999958 @ 44100 Hz (constante típica de emuladores DMG)
    static constexpr double kHighPass = 0.999958;

    std::vector<int16_t> m_sampleBuffer;
};

#pragma once

#include <cstdint>
#include <vector>

class APU {
public:
    APU();

    void reset();
    void writeRegister(uint16_t address, uint8_t value);
    uint8_t readRegister(uint16_t address) const;

    void tickTCycle(uint16_t divBefore, uint16_t divAfter);
    void onDivReset(uint16_t divBefore);

    size_t popSamples(int16_t* out, size_t maxFrames);
    size_t samplesAvailable() const;
    void clearSampleBuffer();

    void setEnabled(bool enabled) { m_outputEnabled = enabled; }
    bool enabled() const { return m_outputEnabled; }
    void setVolume(float volume);

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
        uint32_t freqTimer = 0;
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
        uint32_t period() const;
    };

    struct WaveChannel {
        bool enabled = false;
        bool dacEnabled = false;
        uint16_t length = 0;
        bool lengthEnabled = false;
        uint8_t volumeCode = 0;
        uint16_t frequency = 0;
        uint32_t freqTimer = 0;
        uint8_t position = 0;
        uint8_t waveRam[16]{};
        uint8_t nr0 = 0, nr1 = 0, nr2 = 0, nr3 = 0, nr4 = 0;
        uint8_t currentSample = 0;

        void trigger();
        void clockLength();
        uint8_t digitalOutput() const;
        void tickTimer();
        uint32_t period() const;
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
        uint32_t freqTimer = 0;
        uint8_t nr1 = 0, nr2 = 0, nr3 = 0, nr4 = 0;

        void trigger();
        void clockLength();
        void clockEnvelope();
        uint8_t digitalOutput() const;
        void tickTimer();
        uint32_t period() const;
    };

    void clockFrameSequencer();
    void maybeExtraLengthClock(bool wasLengthEnabled, bool nowLengthEnabled, SquareChannel& ch);
    void maybeExtraLengthClock(bool wasLengthEnabled, bool nowLengthEnabled, WaveChannel& ch);
    void maybeExtraLengthClock(bool wasLengthEnabled, bool nowLengthEnabled, NoiseChannel& ch);
    void mixSample();
    void powerOff();
    void updateNr52Status();
    void pushSample(int16_t l, int16_t r);

    SquareChannel m_ch1;
    SquareChannel m_ch2;
    WaveChannel m_ch3;
    NoiseChannel m_ch4;

    uint8_t m_nr50 = 0x77;
    uint8_t m_nr51 = 0xF3;
    uint8_t m_nr52 = 0xF1;

    bool m_outputEnabled = true;
    float m_volume = 0.45f;

    uint8_t m_frameSeqStep = 0;

    double m_sampleTimer = 0.0;
    static constexpr double kCyclesPerSample = 4194304.0 / kSampleRate;

    double m_capLeft = 0.0;
    double m_capRight = 0.0;
    static constexpr double kCapCharge = 0.999958;

    static constexpr size_t kRingCapacity = 44100;
    std::vector<int16_t> m_ring;
    size_t m_ringRead = 0;
    size_t m_ringWrite = 0;
    size_t m_ringFrames = 0;
};

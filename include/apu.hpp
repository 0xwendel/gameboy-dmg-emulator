#pragma once

#include <cstdint>
#include <vector>

// APU DMG simplificada: 4 canais + frame sequencer + buffer de samples.
class APU {
public:
    APU();

    void reset();
    void writeRegister(uint16_t address, uint8_t value);
    uint8_t readRegister(uint16_t address) const;

    // Avança em T-cycles (dots). Gera samples internos em ~44100 Hz.
    void tick(uint32_t tCycles);

    // Copia samples pendentes para o host (int16 mono intercalado L/R se stereo).
    // Retorna quantos frames stereo (pares L,R) foram escritos.
    size_t popSamples(int16_t* out, size_t maxFrames);

    void setEnabled(bool enabled) { m_masterEnabled = enabled; }
    bool enabled() const { return m_masterEnabled; }
    void setVolume(float volume) { m_volume = volume; }

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
        uint16_t frequency = 0;
        uint16_t freqTimer = 0;
        uint8_t dutyStep = 0;
        uint8_t nr0 = 0, nr1 = 0, nr2 = 0, nr3 = 0, nr4 = 0;
        // Sweep (ch1 only)
        bool hasSweep = false;
        uint8_t sweepPeriod = 0;
        uint8_t sweepTimer = 0;
        bool sweepDecrease = false;
        uint8_t sweepShift = 0;
        uint16_t shadowFreq = 0;
        bool sweepEnabled = false;

        void trigger(bool isCh1);
        void clockLength();
        void clockEnvelope();
        void clockSweep();
        float sample() const;
        void tickTimer(uint32_t tCycles);
    };

    struct WaveChannel {
        bool enabled = false;
        bool dacEnabled = false;
        uint16_t length = 0;
        bool lengthEnabled = false;
        uint8_t volumeCode = 0; // 0=mute,1=100%,2=50%,3=25%
        uint16_t frequency = 0;
        uint16_t freqTimer = 0;
        uint8_t position = 0;
        uint8_t waveRam[16]{};
        uint8_t nr0 = 0, nr1 = 0, nr2 = 0, nr3 = 0, nr4 = 0;
        uint8_t currentSample = 0;

        void trigger();
        void clockLength();
        float sample() const;
        void tickTimer(uint32_t tCycles);
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
        uint8_t clockShift = 0;
        bool widthMode = false;
        uint8_t divisorCode = 0;
        uint16_t lfsr = 0x7FFF;
        uint16_t freqTimer = 0;
        uint8_t nr1 = 0, nr2 = 0, nr3 = 0, nr4 = 0;

        void trigger();
        void clockLength();
        void clockEnvelope();
        float sample() const;
        void tickTimer(uint32_t tCycles);
    };

    void clockFrameSequencer();
    void mixSample();

    SquareChannel m_ch1;
    SquareChannel m_ch2;
    WaveChannel m_ch3;
    NoiseChannel m_ch4;

    uint8_t m_nr50 = 0x77;
    uint8_t m_nr51 = 0xF3;
    uint8_t m_nr52 = 0xF1;

    bool m_masterEnabled = true;
    float m_volume = 0.35f;

    uint32_t m_frameSeqTimer = 0;
    uint8_t m_frameSeqStep = 0;

    // Sample generation: 4194304 / 44100 ≈ 95.108
    double m_sampleTimer = 0.0;
    static constexpr double kCyclesPerSample = 4194304.0 / kSampleRate;

    std::vector<int16_t> m_sampleBuffer; // interleaved stereo
};

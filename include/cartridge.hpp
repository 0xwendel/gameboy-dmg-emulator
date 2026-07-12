#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

class Cartridge {
public:
    enum class MbcType {
        None,
        MBC1,
        MBC2,
        MBC3,
        MBC5,
        Unknown
    };

    Cartridge() = default;

    bool load(const std::vector<uint8_t>& romData);
    bool loadFromFile(const std::string& path);

    uint8_t read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);

    void updateRtcWallClock();

    bool hasBattery() const { return m_hasBattery; }
    bool hasRtc() const { return m_hasRtc; }
    bool saveBattery(const std::string& path) const;
    bool loadBattery(const std::string& path);

    MbcType type() const { return m_type; }
    const std::string& title() const { return m_title; }
    const std::string& romPath() const { return m_romPath; }
    std::string defaultSavePath() const;

private:
    struct RtcRegs {
        uint8_t s = 0;
        uint8_t m = 0;
        uint8_t h = 0;
        uint8_t dl = 0;
        uint8_t dh = 0;
    };

    void parseHeader();
    uint32_t romBankCount() const;
    uint32_t ramBankCount() const;
    uint32_t mapRomAddress(uint16_t address) const;
    uint32_t mapRamAddress(uint16_t address) const;

    void latchRtc();
    void advanceRtcSeconds(uint64_t seconds);
    uint8_t readRtc(uint8_t reg) const;
    void writeRtc(uint8_t reg, uint8_t value);

    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_ram;
    std::string m_title;
    std::string m_romPath;

    MbcType m_type = MbcType::None;
    bool m_hasBattery = false;
    bool m_hasRtc = false;
    bool m_ramEnabled = false;

    uint8_t m_romBankLower = 1;
    uint8_t m_romBankUpper = 0;
    uint8_t m_bankingMode = 0;

    uint8_t m_romBank = 1;
    uint8_t m_ramBank = 0;

    uint16_t m_romBank5 = 1;
    uint8_t m_ramBank5 = 0;

    RtcRegs m_rtc{};
    RtcRegs m_rtcLatched{};
    uint8_t m_rtcLatchData = 0xFF;
    bool m_rtcLatchedReady = false;
    std::time_t m_rtcLastSync = 0;
};

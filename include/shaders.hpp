#pragma once

#include "raylib.h"

enum class ScreenShaderId : int {
    None = 0,
    Scanlines,
    LcdGrid,
    LcdMatrix,
    Crt,
    SoftGlow,
    Count
};

enum class CrtPreset : int {
    Clean = 0,
    Realistic,
    Broken,
    Count
};

const char* ScreenShaderName(ScreenShaderId id);
int ScreenShaderCount();

const char* CrtPresetName(CrtPreset preset);
int CrtPresetCount();

class ScreenShaders {
public:
    ScreenShaders() = default;
    ~ScreenShaders();

    ScreenShaders(const ScreenShaders&) = delete;
    ScreenShaders& operator=(const ScreenShaders&) = delete;

    bool load();
    void unload();

    void setActive(ScreenShaderId id);
    void setActiveIndex(int index);
    ScreenShaderId active() const { return m_active; }
    int activeIndex() const { return static_cast<int>(m_active); }

    void cycleNext();
    void cyclePrev();

    void setIntensity(float intensity);
    float intensity() const { return m_intensity; }

    void setCrtPreset(CrtPreset preset);
    CrtPreset crtPreset() const { return m_crtPreset; }
    void setCrtPresetIndex(int index);
    int crtPresetIndex() const { return static_cast<int>(m_crtPreset); }

    void draw(const Texture2D& texture, Rectangle src, Rectangle dest,
              float timeSeconds);

private:
    Shader m_shaders[static_cast<int>(ScreenShaderId::Count)]{};
    bool m_loaded[static_cast<int>(ScreenShaderId::Count)]{};
    int m_locResolution[static_cast<int>(ScreenShaderId::Count)]{};
    int m_locTime[static_cast<int>(ScreenShaderId::Count)]{};
    int m_locIntensity[static_cast<int>(ScreenShaderId::Count)]{};
    ScreenShaderId m_active = ScreenShaderId::None;
    bool m_ready = false;

    float m_intensity = 0.85f;
    CrtPreset m_crtPreset = CrtPreset::Broken;

    Shader m_blurShader{};
    Shader m_crtComposite{};
    bool m_blurLoaded = false;
    bool m_crtCompositeLoaded = false;
    int m_locBlurResolution = -1;
    int m_locBlurDirection = -1;
    int m_locBlurThreshold = -1;
    int m_locBlurExtract = -1;

    int m_locCrtResolution = -1;
    int m_locCrtTime = -1;
    int m_locCrtIntensity = -1;
    int m_locCrtBloomAmount = -1;
    int m_locCrtBarrel = -1;
    int m_locCrtScan = -1;
    int m_locCrtMask = -1;
    int m_locCrtCa = -1;
    int m_locCrtVignette = -1;
    int m_locCrtCorner = -1;
    int m_locCrtFlicker = -1;
    int m_locCrtNoise = -1;
    int m_locCrtRoll = -1;
    int m_locCrtWobble = -1;
    int m_locCrtBloomTex = -1;

    RenderTexture2D m_blurH{};
    RenderTexture2D m_blurV{};
    int m_fboW = 0;
    int m_fboH = 0;
    bool m_fboReady = false;

    bool loadOne(ScreenShaderId id, const char* fsCode);
    bool loadCrtPipeline();
    void unloadCrtPipeline();
    bool ensureCrtTargets(int width, int height);
    void drawCrt(const Texture2D& texture, Rectangle src, Rectangle dest,
                 float timeSeconds);
    void drawSimple(const Texture2D& texture, Rectangle src, Rectangle dest,
                    float timeSeconds);
};

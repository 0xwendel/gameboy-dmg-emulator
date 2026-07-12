#pragma once

#include "raylib.h"

#include <string>

enum class ScreenShaderId : int {
    None = 0,
    Scanlines,
    LcdGrid,
    LcdMatrix,
    Crt,
    SoftGlow,
    Count
};

const char* ScreenShaderName(ScreenShaderId id);
int ScreenShaderCount();

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

    bool loadOne(ScreenShaderId id, const char* fsCode);
};

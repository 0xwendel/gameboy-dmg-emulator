#include "shaders.hpp"

#include <algorithm>
#include <cstring>

namespace {

constexpr const char* kDefaultVs = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
out vec2 fragTexCoord;
out vec4 fragColor;
uniform mat4 mvp;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

constexpr const char* kFsNone = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
void main() {
    finalColor = texture(texture0, fragTexCoord) * fragColor;
}
)";

constexpr const char* kFsScanlines = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 resolution;
uniform float intensity;
void main() {
    vec4 tex = texture(texture0, fragTexCoord) * fragColor;
    float y = fragTexCoord.y * max(resolution.y, 1.0);
    float line = mod(floor(y), 2.0);
    float wave = 0.85 + 0.15 * sin(y * 3.14159265);
    float dark = mix(1.0, mix(wave, 0.72, line), clamp(intensity, 0.0, 1.0));
    finalColor = vec4(tex.rgb * dark, tex.a);
}
)";

constexpr const char* kFsLcdGrid = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float intensity;
void main() {
    vec4 tex = texture(texture0, fragTexCoord) * fragColor;
    vec2 gb = fragTexCoord * vec2(160.0, 144.0);
    vec2 f = fract(gb);
    float edge = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
    float grid = smoothstep(0.0, 0.12, edge);
    float mixAmt = mix(1.0, grid * 0.55 + 0.45, clamp(intensity, 0.0, 1.0));
    finalColor = vec4(tex.rgb * mixAmt, tex.a);
}
)";

constexpr const char* kFsLcdMatrix = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 resolution;
uniform float intensity;
void main() {
    vec4 tex = texture(texture0, fragTexCoord) * fragColor;
    vec2 gb = fragTexCoord * vec2(160.0, 144.0);
    vec2 f = fract(gb);
    float third = 1.0 / 3.0;
    vec3 mask = vec3(0.15);
    if (f.x < third) mask.r = 1.0;
    else if (f.x < 2.0 * third) mask.g = 1.0;
    else mask.b = 1.0;
    if (f.y < 0.08 || f.y > 0.92) mask *= 0.35;
    vec3 rgb = tex.rgb * mix(vec3(1.0), mask, clamp(intensity, 0.0, 1.0) * 0.85);
    float lum = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
    rgb = mix(vec3(lum), rgb, 0.85);
    finalColor = vec4(rgb, tex.a);
}
)";

constexpr const char* kFsCrt = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 resolution;
uniform float intensity;
void main() {
    vec2 uv = fragTexCoord;
    vec2 cc = uv - 0.5;
    float dist = dot(cc, cc);
    float barrel = 0.18 * clamp(intensity, 0.0, 1.0);
    uv = uv + cc * dist * barrel;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 tex = texture(texture0, uv) * fragColor;
    float y = uv.y * max(resolution.y, 1.0);
    float scan = 0.88 + 0.12 * sin(y * 3.14159265);
    float line = mod(floor(y), 2.0);
    scan = mix(scan, scan * 0.78, line);

    float vig = 1.0 - dist * (1.2 * intensity);
    vig = clamp(vig, 0.55, 1.0);

    float ab = 0.0015 * intensity;
    float r = texture(texture0, uv + vec2(ab, 0.0)).r;
    float b = texture(texture0, uv - vec2(ab, 0.0)).b;
    vec3 rgb = vec3(r, tex.g, b) * scan * vig;

    finalColor = vec4(rgb, tex.a);
}
)";

constexpr const char* kFsSoftGlow = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 resolution;
uniform float intensity;
void main() {
    vec2 px = 1.0 / max(resolution, vec2(1.0));
    vec4 c = texture(texture0, fragTexCoord) * 4.0;
    c += texture(texture0, fragTexCoord + vec2( px.x, 0.0));
    c += texture(texture0, fragTexCoord + vec2(-px.x, 0.0));
    c += texture(texture0, fragTexCoord + vec2(0.0,  px.y));
    c += texture(texture0, fragTexCoord + vec2(0.0, -px.y));
    c += texture(texture0, fragTexCoord + vec2( px.x,  px.y)) * 0.5;
    c += texture(texture0, fragTexCoord + vec2(-px.x,  px.y)) * 0.5;
    c += texture(texture0, fragTexCoord + vec2( px.x, -px.y)) * 0.5;
    c += texture(texture0, fragTexCoord + vec2(-px.x, -px.y)) * 0.5;
    c /= 10.0;
    vec4 sharp = texture(texture0, fragTexCoord);
    float t = clamp(intensity, 0.0, 1.0) * 0.65;
    finalColor = mix(sharp, c, t) * fragColor;
}
)";

const char* fsFor(ScreenShaderId id) {
    switch (id) {
        case ScreenShaderId::None: return kFsNone;
        case ScreenShaderId::Scanlines: return kFsScanlines;
        case ScreenShaderId::LcdGrid: return kFsLcdGrid;
        case ScreenShaderId::LcdMatrix: return kFsLcdMatrix;
        case ScreenShaderId::Crt: return kFsCrt;
        case ScreenShaderId::SoftGlow: return kFsSoftGlow;
        default: return kFsNone;
    }
}

} // namespace

const char* ScreenShaderName(ScreenShaderId id) {
    switch (id) {
        case ScreenShaderId::None: return "None";
        case ScreenShaderId::Scanlines: return "Scanlines";
        case ScreenShaderId::LcdGrid: return "LCD Grid";
        case ScreenShaderId::LcdMatrix: return "LCD Matrix";
        case ScreenShaderId::Crt: return "CRT";
        case ScreenShaderId::SoftGlow: return "Soft Glow";
        default: return "None";
    }
}

int ScreenShaderCount() {
    return static_cast<int>(ScreenShaderId::Count);
}

ScreenShaders::~ScreenShaders() {
    unload();
}

bool ScreenShaders::loadOne(ScreenShaderId id, const char* fsCode) {
    const int i = static_cast<int>(id);
    Shader sh = LoadShaderFromMemory(kDefaultVs, fsCode);
    if (sh.id == 0) {
        m_loaded[i] = false;
        return false;
    }
    m_shaders[i] = sh;
    m_loaded[i] = true;
    m_locResolution[i] = GetShaderLocation(sh, "resolution");
    m_locTime[i] = GetShaderLocation(sh, "time");
    m_locIntensity[i] = GetShaderLocation(sh, "intensity");
    return true;
}

bool ScreenShaders::load() {
    unload();
    bool any = false;
    m_loaded[static_cast<int>(ScreenShaderId::None)] = true;
    any = true;

    for (int i = 1; i < static_cast<int>(ScreenShaderId::Count); ++i) {
        const auto id = static_cast<ScreenShaderId>(i);
        if (loadOne(id, fsFor(id))) any = true;
    }
    m_ready = any;
    m_active = ScreenShaderId::None;
    return m_ready;
}

void ScreenShaders::unload() {
    for (int i = 0; i < static_cast<int>(ScreenShaderId::Count); ++i) {
        if (i != static_cast<int>(ScreenShaderId::None) && m_loaded[i] && m_shaders[i].id != 0) {
            UnloadShader(m_shaders[i]);
        }
        m_shaders[i] = {};
        m_loaded[i] = false;
        m_locResolution[i] = -1;
        m_locTime[i] = -1;
        m_locIntensity[i] = -1;
    }
    m_ready = false;
    m_active = ScreenShaderId::None;
}

void ScreenShaders::setActive(ScreenShaderId id) {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(ScreenShaderId::Count)) return;
    if (id != ScreenShaderId::None && !m_loaded[i]) return;
    m_active = id;
}

void ScreenShaders::setActiveIndex(int index) {
    if (index < 0) index = 0;
    if (index >= ScreenShaderCount()) index = ScreenShaderCount() - 1;
    setActive(static_cast<ScreenShaderId>(index));
}

void ScreenShaders::cycleNext() {
    int i = static_cast<int>(m_active);
    for (int n = 0; n < ScreenShaderCount(); ++n) {
        i = (i + 1) % ScreenShaderCount();
        if (i == 0 || m_loaded[i]) {
            m_active = static_cast<ScreenShaderId>(i);
            return;
        }
    }
}

void ScreenShaders::cyclePrev() {
    int i = static_cast<int>(m_active);
    for (int n = 0; n < ScreenShaderCount(); ++n) {
        i = (i - 1 + ScreenShaderCount()) % ScreenShaderCount();
        if (i == 0 || m_loaded[i]) {
            m_active = static_cast<ScreenShaderId>(i);
            return;
        }
    }
}

void ScreenShaders::draw(const Texture2D& texture, Rectangle src, Rectangle dest,
                         float timeSeconds) {
    if (m_active == ScreenShaderId::None || !m_ready) {
        DrawTexturePro(texture, src, dest, Vector2{0, 0}, 0.0f, WHITE);
        return;
    }

    const int i = static_cast<int>(m_active);
    if (!m_loaded[i]) {
        DrawTexturePro(texture, src, dest, Vector2{0, 0}, 0.0f, WHITE);
        return;
    }

    Shader& sh = m_shaders[i];
    const float res[2] = { dest.width, dest.height };
    const float intensity = 0.85f;
    if (m_locResolution[i] >= 0) {
        SetShaderValue(sh, m_locResolution[i], res, SHADER_UNIFORM_VEC2);
    }
    if (m_locTime[i] >= 0) {
        SetShaderValue(sh, m_locTime[i], &timeSeconds, SHADER_UNIFORM_FLOAT);
    }
    if (m_locIntensity[i] >= 0) {
        SetShaderValue(sh, m_locIntensity[i], &intensity, SHADER_UNIFORM_FLOAT);
    }

    BeginShaderMode(sh);
    DrawTexturePro(texture, src, dest, Vector2{0, 0}, 0.0f, WHITE);
    EndShaderMode();
}

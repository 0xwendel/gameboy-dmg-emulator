#include "shaders.hpp"

#include <algorithm>
#include <cmath>
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

// Fallback single-pass CRT if multi-pass FBOs fail.
constexpr const char* kFsCrtFallback = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 resolution;
uniform float intensity;
uniform float time;
void main() {
    float I = clamp(intensity, 0.0, 1.0);
    vec2 uv = fragTexCoord;
    vec2 cc = uv - 0.5;
    float dist = dot(cc, cc);
    uv = uv + cc * dist * (0.22 * I);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float ab = 0.0025 * I;
    float r = texture(texture0, uv + vec2(ab, 0.0)).r;
    float g = texture(texture0, uv).g;
    float b = texture(texture0, uv - vec2(ab, 0.0)).b;
    vec3 rgb = vec3(r, g, b);
    float y = uv.y * max(resolution.y, 1.0);
    float scan = 0.78 + 0.22 * sin(y * 3.14159265);
    float vig = clamp(1.0 - dist * (1.35 * I), 0.4, 1.0);
    float flick = 1.0 - 0.06 * I * sin(time * 62.0);
    float n = fract(sin(dot(uv * resolution + time * 40.0, vec2(12.9898, 78.233))) * 43758.5453);
    rgb = rgb * scan * vig * flick + (n - 0.5) * 0.04 * I;
    finalColor = vec4(rgb * fragColor.rgb, fragColor.a);
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

// Separable blur: optional bright-pass extract on first pass (extract=1).
constexpr const char* kFsBlur = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 resolution;
uniform vec2 direction;
uniform float threshold;
uniform float extract;
void main() {
    vec2 px = direction / max(resolution, vec2(1.0));
    float w0 = 0.227027;
    float w1 = 0.1945946;
    float w2 = 0.1216216;
    float w3 = 0.054054;
    float w4 = 0.016216;
    vec3 c = texture(texture0, fragTexCoord).rgb * w0;
    c += texture(texture0, fragTexCoord + px * 1.0).rgb * w1;
    c += texture(texture0, fragTexCoord - px * 1.0).rgb * w1;
    c += texture(texture0, fragTexCoord + px * 2.0).rgb * w2;
    c += texture(texture0, fragTexCoord - px * 2.0).rgb * w2;
    c += texture(texture0, fragTexCoord + px * 3.0).rgb * w3;
    c += texture(texture0, fragTexCoord - px * 3.0).rgb * w3;
    c += texture(texture0, fragTexCoord + px * 4.0).rgb * w4;
    c += texture(texture0, fragTexCoord - px * 4.0).rgb * w4;
    if (extract > 0.5) {
        float lum = dot(c, vec3(0.299, 0.587, 0.114));
        float soft = max(lum - threshold, 0.0) / max(1.0 - threshold, 0.001);
        c *= clamp(soft, 0.0, 1.0);
    }
    finalColor = vec4(c, 1.0) * fragColor;
}
)";

constexpr const char* kFsCrtComposite = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform sampler2D texture1;
uniform vec2 resolution;
uniform float time;
uniform float intensity;
uniform float bloomAmount;
uniform float barrel;
uniform float scanStrength;
uniform float maskStrength;
uniform float caAmount;
uniform float vignetteStrength;
uniform float cornerRadius;
uniform float flickerAmount;
uniform float noiseAmount;
uniform float rollAmount;
uniform float wobbleAmount;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

vec2 curveUV(vec2 uv, float amount) {
    vec2 cc = uv - 0.5;
    float r2 = dot(cc, cc);
    // Barrel + mild higher-order pincushion
    uv = uv + cc * (r2 * amount + r2 * r2 * amount * 0.35);
    return uv;
}

float roundedCornerMask(vec2 uv, float radius) {
    vec2 p = abs(uv - 0.5) * 2.0;
    float edge = length(max(p + radius - 1.0, 0.0));
    return 1.0 - smoothstep(0.0, radius * 1.2 + 0.001, edge);
}

void main() {
    float I = clamp(intensity, 0.0, 1.0);
    vec2 uv = fragTexCoord;

    // Horizontal wobble / jitter (vintage unstable H-sync)
    float wob = sin(time * 7.3 + uv.y * 40.0) * 0.0018
              + sin(time * 23.1 + uv.y * 120.0) * 0.0007;
    float jitter = (hash21(vec2(floor(time * 24.0), floor(uv.y * resolution.y))) - 0.5) * 0.004;
    uv.x += (wob + jitter) * wobbleAmount * I;

    // Occasional vertical hold drift
    uv.y += sin(time * 0.7) * 0.0012 * wobbleAmount * I;

    uv = curveUV(uv, barrel * I);

    if (uv.x < -0.02 || uv.x > 1.02 || uv.y < -0.02 || uv.y > 1.02) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Radial chromatic aberration (stronger at edges)
    vec2 cc = uv - 0.5;
    float dist = length(cc);
    vec2 caDir = (dist > 1e-5) ? normalize(cc) : vec2(1.0, 0.0);
    float ca = caAmount * I * (0.35 + dist * dist * 2.2);
    float r = texture(texture0, uv + caDir * ca).r;
    float g = texture(texture0, uv).g;
    float b = texture(texture0, uv - caDir * ca).b;
    vec3 col = vec3(r, g, b);

    // Beam-profile scanlines (thicker on bright pixels)
    float lum = max(dot(col, vec3(0.299, 0.587, 0.114)), 0.0);
    float scanY = uv.y * max(resolution.y, 1.0);
    float beam = fract(scanY);
    float sigma = mix(0.28, 0.48, clamp(lum, 0.0, 1.0));
    float beamProf = exp(-0.5 * pow((beam - 0.5) / sigma, 2.0));
    float scan = mix(1.0, 0.42 + 0.58 * beamProf, scanStrength * I);
    col *= scan;

    // RGB aperture grille / slot mask
    float maskX = fragTexCoord.x * max(resolution.x, 1.0);
    float triad = mod(floor(maskX), 3.0);
    vec3 mask = vec3(0.22);
    if (triad < 0.5) mask.r = 1.0;
    else if (triad < 1.5) mask.g = 1.0;
    else mask.b = 1.0;
    // Horizontal slot gaps
    float slot = step(0.12, fract(scanY * 0.5));
    mask *= mix(0.55, 1.0, slot);
    col *= mix(vec3(1.0), mask, maskStrength * I * 0.9);

    // Phosphor bloom / halation (RenderTexture is Y-flipped in OpenGL)
    vec3 bloom = texture(texture1, vec2(fragTexCoord.x, 1.0 - fragTexCoord.y)).rgb;
    vec3 phosphorTint = vec3(0.95, 1.05, 0.92);
    col += bloom * phosphorTint * bloomAmount * I;

    // Rolling interference bar
    float rollPos = fract(time * 0.085);
    float roll = smoothstep(0.0, 0.04, abs(fract(uv.y - rollPos + 0.5) - 0.5));
    roll = 1.0 - (1.0 - roll) * rollAmount * I * 0.55;
    col *= roll;
    col += vec3(0.04, 0.045, 0.05) * (1.0 - roll) * rollAmount * I;

    // 60 Hz flicker
    float flick = 1.0 - flickerAmount * I * (0.5 + 0.5 * sin(time * 62.832));
    col *= flick;

    // Grain / static
    float n1 = hash21(uv * resolution + floor(time * 48.0));
    float n2 = hash21(uv * resolution * 1.7 - floor(time * 31.0));
    float grain = (n1 * 2.0 - 1.0) * 0.065 + (n2 * 2.0 - 1.0) * 0.03;
    col += grain * noiseAmount * I;
    // Colored snow flecks when noise is high
    if (n1 > 0.992) {
        col += vec3(0.35, 0.35, 0.4) * noiseAmount * I;
    }

    // Vignette + rounded glass corners
    float vig = 1.0 - dist * dist * (vignetteStrength * I * 1.6);
    vig = clamp(vig, 0.25, 1.0);
    float corner = roundedCornerMask(uv, mix(0.02, 0.12, cornerRadius * I));
    col *= vig * corner;

    // CRT-ish gamma / black crush
    col = max(col, 0.0);
    col = pow(col, vec3(1.08));
    col = col * 1.08 - 0.02 * I;
    col = clamp(col, 0.0, 1.0);

    // Outside warped tube: pure black
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        col *= smoothstep(0.04, 0.0, max(max(-uv.x, uv.x - 1.0), max(-uv.y, uv.y - 1.0)));
    }

    finalColor = vec4(col * fragColor.rgb, fragColor.a);
}
)";

const char* fsFor(ScreenShaderId id) {
    switch (id) {
        case ScreenShaderId::None: return kFsNone;
        case ScreenShaderId::Scanlines: return kFsScanlines;
        case ScreenShaderId::LcdGrid: return kFsLcdGrid;
        case ScreenShaderId::LcdMatrix: return kFsLcdMatrix;
        case ScreenShaderId::Crt: return kFsCrtFallback;
        case ScreenShaderId::SoftGlow: return kFsSoftGlow;
        default: return kFsNone;
    }
}

struct CrtWeights {
    float bloom = 0.55f;
    float barrel = 0.20f;
    float scan = 0.85f;
    float mask = 0.70f;
    float ca = 0.0045f;
    float vignette = 0.95f;
    float corner = 0.85f;
    float flicker = 0.12f;
    float noise = 0.55f;
    float roll = 0.65f;
    float wobble = 0.70f;
    float blurThreshold = 0.35f;
};

CrtWeights weightsFor(CrtPreset preset) {
    CrtWeights w;
    switch (preset) {
        case CrtPreset::Clean:
            w.bloom = 0.28f;
            w.barrel = 0.14f;
            w.scan = 0.55f;
            w.mask = 0.40f;
            w.ca = 0.0020f;
            w.vignette = 0.70f;
            w.corner = 0.60f;
            w.flicker = 0.0f;
            w.noise = 0.0f;
            w.roll = 0.0f;
            w.wobble = 0.0f;
            w.blurThreshold = 0.45f;
            break;
        case CrtPreset::Realistic:
            w.bloom = 0.48f;
            w.barrel = 0.18f;
            w.scan = 0.78f;
            w.mask = 0.62f;
            w.ca = 0.0035f;
            w.vignette = 0.90f;
            w.corner = 0.80f;
            w.flicker = 0.05f;
            w.noise = 0.22f;
            w.roll = 0.25f;
            w.wobble = 0.20f;
            w.blurThreshold = 0.38f;
            break;
        case CrtPreset::Broken:
        default:
            w.bloom = 0.72f;
            w.barrel = 0.24f;
            w.scan = 0.92f;
            w.mask = 0.85f;
            w.ca = 0.0060f;
            w.vignette = 1.05f;
            w.corner = 0.95f;
            w.flicker = 0.18f;
            w.noise = 0.85f;
            w.roll = 0.90f;
            w.wobble = 1.0f;
            w.blurThreshold = 0.28f;
            break;
    }
    return w;
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

const char* CrtPresetName(CrtPreset preset) {
    switch (preset) {
        case CrtPreset::Clean: return "Clean";
        case CrtPreset::Realistic: return "Realistic";
        case CrtPreset::Broken: return "Broken";
        default: return "Broken";
    }
}

int CrtPresetCount() {
    return static_cast<int>(CrtPreset::Count);
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

bool ScreenShaders::loadCrtPipeline() {
    unloadCrtPipeline();

    m_blurShader = LoadShaderFromMemory(kDefaultVs, kFsBlur);
    m_blurLoaded = (m_blurShader.id != 0);
    if (m_blurLoaded) {
        m_locBlurResolution = GetShaderLocation(m_blurShader, "resolution");
        m_locBlurDirection = GetShaderLocation(m_blurShader, "direction");
        m_locBlurThreshold = GetShaderLocation(m_blurShader, "threshold");
        m_locBlurExtract = GetShaderLocation(m_blurShader, "extract");
    }

    m_crtComposite = LoadShaderFromMemory(kDefaultVs, kFsCrtComposite);
    m_crtCompositeLoaded = (m_crtComposite.id != 0);
    if (m_crtCompositeLoaded) {
        m_locCrtResolution = GetShaderLocation(m_crtComposite, "resolution");
        m_locCrtTime = GetShaderLocation(m_crtComposite, "time");
        m_locCrtIntensity = GetShaderLocation(m_crtComposite, "intensity");
        m_locCrtBloomAmount = GetShaderLocation(m_crtComposite, "bloomAmount");
        m_locCrtBarrel = GetShaderLocation(m_crtComposite, "barrel");
        m_locCrtScan = GetShaderLocation(m_crtComposite, "scanStrength");
        m_locCrtMask = GetShaderLocation(m_crtComposite, "maskStrength");
        m_locCrtCa = GetShaderLocation(m_crtComposite, "caAmount");
        m_locCrtVignette = GetShaderLocation(m_crtComposite, "vignetteStrength");
        m_locCrtCorner = GetShaderLocation(m_crtComposite, "cornerRadius");
        m_locCrtFlicker = GetShaderLocation(m_crtComposite, "flickerAmount");
        m_locCrtNoise = GetShaderLocation(m_crtComposite, "noiseAmount");
        m_locCrtRoll = GetShaderLocation(m_crtComposite, "rollAmount");
        m_locCrtWobble = GetShaderLocation(m_crtComposite, "wobbleAmount");
        m_locCrtBloomTex = GetShaderLocation(m_crtComposite, "texture1");
    }

    return m_blurLoaded && m_crtCompositeLoaded;
}

void ScreenShaders::unloadCrtPipeline() {
    if (m_fboReady) {
        if (m_blurH.id != 0) UnloadRenderTexture(m_blurH);
        if (m_blurV.id != 0) UnloadRenderTexture(m_blurV);
        m_blurH = {};
        m_blurV = {};
        m_fboW = 0;
        m_fboH = 0;
        m_fboReady = false;
    }
    if (m_blurLoaded && m_blurShader.id != 0) {
        UnloadShader(m_blurShader);
    }
    if (m_crtCompositeLoaded && m_crtComposite.id != 0) {
        UnloadShader(m_crtComposite);
    }
    m_blurShader = {};
    m_crtComposite = {};
    m_blurLoaded = false;
    m_crtCompositeLoaded = false;
    m_locBlurResolution = m_locBlurDirection = m_locBlurThreshold = m_locBlurExtract = -1;
    m_locCrtResolution = m_locCrtTime = m_locCrtIntensity = m_locCrtBloomAmount = -1;
    m_locCrtBarrel = m_locCrtScan = m_locCrtMask = m_locCrtCa = -1;
    m_locCrtVignette = m_locCrtCorner = m_locCrtFlicker = m_locCrtNoise = -1;
    m_locCrtRoll = m_locCrtWobble = m_locCrtBloomTex = -1;
}

bool ScreenShaders::ensureCrtTargets(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (m_fboReady && m_fboW == width && m_fboH == height) {
        return true;
    }

    if (m_fboReady) {
        if (m_blurH.id != 0) UnloadRenderTexture(m_blurH);
        if (m_blurV.id != 0) UnloadRenderTexture(m_blurV);
        m_blurH = {};
        m_blurV = {};
        m_fboReady = false;
    }

    m_blurH = LoadRenderTexture(width, height);
    m_blurV = LoadRenderTexture(width, height);
    if (m_blurH.id == 0 || m_blurV.id == 0) {
        if (m_blurH.id != 0) UnloadRenderTexture(m_blurH);
        if (m_blurV.id != 0) UnloadRenderTexture(m_blurV);
        m_blurH = {};
        m_blurV = {};
        return false;
    }

    SetTextureFilter(m_blurH.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(m_blurV.texture, TEXTURE_FILTER_BILINEAR);
    m_fboW = width;
    m_fboH = height;
    m_fboReady = true;
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

    loadCrtPipeline();

    m_ready = any;
    m_active = ScreenShaderId::None;
    return m_ready;
}

void ScreenShaders::unload() {
    unloadCrtPipeline();
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
    if (id != ScreenShaderId::None && !m_loaded[i] && id != ScreenShaderId::Crt) return;
    if (id == ScreenShaderId::Crt && !m_loaded[i] && !m_crtCompositeLoaded) return;
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
        if (i == 0 || m_loaded[i] ||
            (static_cast<ScreenShaderId>(i) == ScreenShaderId::Crt && m_crtCompositeLoaded)) {
            m_active = static_cast<ScreenShaderId>(i);
            return;
        }
    }
}

void ScreenShaders::cyclePrev() {
    int i = static_cast<int>(m_active);
    for (int n = 0; n < ScreenShaderCount(); ++n) {
        i = (i - 1 + ScreenShaderCount()) % ScreenShaderCount();
        if (i == 0 || m_loaded[i] ||
            (static_cast<ScreenShaderId>(i) == ScreenShaderId::Crt && m_crtCompositeLoaded)) {
            m_active = static_cast<ScreenShaderId>(i);
            return;
        }
    }
}

void ScreenShaders::setIntensity(float intensity) {
    m_intensity = std::clamp(intensity, 0.0f, 1.0f);
}

void ScreenShaders::setCrtPreset(CrtPreset preset) {
    const int i = static_cast<int>(preset);
    if (i < 0 || i >= static_cast<int>(CrtPreset::Count)) return;
    m_crtPreset = preset;
}

void ScreenShaders::setCrtPresetIndex(int index) {
    if (index < 0) index = 0;
    if (index >= CrtPresetCount()) index = CrtPresetCount() - 1;
    m_crtPreset = static_cast<CrtPreset>(index);
}

void ScreenShaders::drawSimple(const Texture2D& texture, Rectangle src, Rectangle dest,
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
    const float intensity = m_intensity;
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

void ScreenShaders::drawCrt(const Texture2D& texture, Rectangle src, Rectangle dest,
                            float timeSeconds) {
    const int tw = static_cast<int>(std::lround(dest.width));
    const int th = static_cast<int>(std::lround(dest.height));
    if (!m_blurLoaded || !m_crtCompositeLoaded || !ensureCrtTargets(tw, th)) {
        drawSimple(texture, src, dest, timeSeconds);
        return;
    }

    const CrtWeights w = weightsFor(m_crtPreset);
    const float res[2] = {
        static_cast<float>(m_fboW),
        static_cast<float>(m_fboH)
    };

    const Rectangle fboDest{
        0.0f, 0.0f,
        static_cast<float>(m_fboW),
        static_cast<float>(m_fboH)
    };
    const Rectangle srcUp{ src.x, src.y, src.width, src.height };

    BeginTextureMode(m_blurH);
    ClearBackground(BLACK);
    const float dirH[2] = { 1.0f, 0.0f };
    const float extractOn = 1.0f;
    if (m_locBlurResolution >= 0) {
        SetShaderValue(m_blurShader, m_locBlurResolution, res, SHADER_UNIFORM_VEC2);
    }
    if (m_locBlurDirection >= 0) {
        SetShaderValue(m_blurShader, m_locBlurDirection, dirH, SHADER_UNIFORM_VEC2);
    }
    if (m_locBlurThreshold >= 0) {
        SetShaderValue(m_blurShader, m_locBlurThreshold, &w.blurThreshold, SHADER_UNIFORM_FLOAT);
    }
    if (m_locBlurExtract >= 0) {
        SetShaderValue(m_blurShader, m_locBlurExtract, &extractOn, SHADER_UNIFORM_FLOAT);
    }
    BeginShaderMode(m_blurShader);
    DrawTexturePro(texture, srcUp, fboDest, Vector2{0, 0}, 0.0f, WHITE);
    EndShaderMode();
    EndTextureMode();

    // Pass 2: vertical blur blurH -> blurV (no extract)
    BeginTextureMode(m_blurV);
    ClearBackground(BLACK);
    const float dirV[2] = { 0.0f, 1.0f };
    const float extractOff = 0.0f;
    if (m_locBlurDirection >= 0) {
        SetShaderValue(m_blurShader, m_locBlurDirection, dirV, SHADER_UNIFORM_VEC2);
    }
    if (m_locBlurExtract >= 0) {
        SetShaderValue(m_blurShader, m_locBlurExtract, &extractOff, SHADER_UNIFORM_FLOAT);
    }
    // Sample blurH.texture: raylib stores upside-down; flip source height.
    const Rectangle blurSrc{
        0.0f, 0.0f,
        static_cast<float>(m_blurH.texture.width),
        -static_cast<float>(m_blurH.texture.height)
    };
    BeginShaderMode(m_blurShader);
    DrawTexturePro(m_blurH.texture, blurSrc, fboDest, Vector2{0, 0}, 0.0f, WHITE);
    EndShaderMode();
    EndTextureMode();

    // Pass 3: composite to backbuffer
    const float intensity = m_intensity;
    const float screenRes[2] = { dest.width, dest.height };
    if (m_locCrtResolution >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtResolution, screenRes, SHADER_UNIFORM_VEC2);
    }
    if (m_locCrtTime >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtTime, &timeSeconds, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtIntensity >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtIntensity, &intensity, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtBloomAmount >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtBloomAmount, &w.bloom, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtBarrel >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtBarrel, &w.barrel, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtScan >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtScan, &w.scan, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtMask >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtMask, &w.mask, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtCa >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtCa, &w.ca, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtVignette >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtVignette, &w.vignette, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtCorner >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtCorner, &w.corner, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtFlicker >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtFlicker, &w.flicker, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtNoise >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtNoise, &w.noise, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtRoll >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtRoll, &w.roll, SHADER_UNIFORM_FLOAT);
    }
    if (m_locCrtWobble >= 0) {
        SetShaderValue(m_crtComposite, m_locCrtWobble, &w.wobble, SHADER_UNIFORM_FLOAT);
    }

    // Bind bloom (blurV) as texture1. raylib sets texture0 from DrawTexturePro.
    if (m_locCrtBloomTex >= 0) {
        SetShaderValueTexture(m_crtComposite, m_locCrtBloomTex, m_blurV.texture);
    }

    // For composite, sample bloom with same UV space as dest. blurV is flipped in GPU
    // memory; when used as secondary sampler in shader, raylib's SetShaderValueTexture
    // does not flip — sample with Y flip in shader via fragTexCoord mapping.
    // We instead draw bloom into blurV already compensating flips so texture1 UV matches
    // screen UV. After two flips (H write + V write from flipped H), blurV should match
    // upright screen space when sampled with standard fragTexCoord.

    BeginShaderMode(m_crtComposite);
    DrawTexturePro(texture, src, dest, Vector2{0, 0}, 0.0f, WHITE);
    EndShaderMode();
}

void ScreenShaders::draw(const Texture2D& texture, Rectangle src, Rectangle dest,
                         float timeSeconds) {
    if (m_active == ScreenShaderId::Crt) {
        drawCrt(texture, src, dest, timeSeconds);
        return;
    }
    drawSimple(texture, src, dest, timeSeconds);
}

#include "intro/IntroAnimation.hpp"
#include <imgui.h>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <numeric>

namespace astrocore {

// ── Font ─────────────────────────────────────────────────────────────────────
// 5-wide × 7-tall bitmap, bit 4 = left, bit 0 = right.
// Letter order: A  S  T  R  O  D  E  X
static const uint8_t kFont[8][7] = {
    {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}, // A
    {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}, // S
    {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}, // T
    {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}, // R
    {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}, // O
    {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110}, // D
    {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}, // E
    {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}, // X
};

// ── rng helpers (seeded in initParticles) ────────────────────────────────────
static float frand() { return (float)std::rand() / (float)RAND_MAX; }
static float frandr(float lo, float hi) { return lo + frand() * (hi - lo); }

// ── initParticles ─────────────────────────────────────────────────────────────

void IntroAnimation::initParticles(float W, float H) {
    std::srand(42);
    m_particles.clear();
    m_W = W; m_H = H;

    // Grid layout for font
    const float gridStep = 13.f;   // centre-to-centre spacing between font pixels
    const float lGap     = 16.f;   // extra gap between letters
    const float lW       = 5.f * gridStep;
    const float lH       = 7.f * gridStep;
    const float totalW   = 8.f * lW + 7.f * lGap;

    const float sx = W * 0.5f - totalW * 0.5f;
    const float sy = H * 0.42f - lH * 0.5f;

    m_centerX = W * 0.5f;
    m_centerY = sy + lH * 0.5f;
    m_titleW  = totalW;
    m_titleH  = lH;

    // ── Letter particles ──────────────────────────────────────────────────────
    for (int li = 0; li < 8; ++li) {
        float lx = sx + li * (lW + lGap);
        for (int row = 0; row < 7; ++row)
            for (int bit = 0; bit < 5; ++bit) {
                if (!((kFont[li][row] >> (4 - bit)) & 1)) continue;

                float tx = lx + bit * gridStep + gridStep * 0.5f;
                float ty = sy + row * gridStep + gridStep * 0.5f;

                float angle = frand() * 6.2832f;
                float dist  = frandr(250.f, 600.f);

                Particle p;
                p.x = W * 0.5f + cosf(angle) * dist;
                p.y = H * 0.5f + sinf(angle) * dist;
                p.tx = tx;  p.ty = ty;
                p.vx = 0.f; p.vy = 0.f;
                p.phase        = frand() * 6.2832f;
                p.twinkleSpeed = frandr(1.8f, 4.5f);
                p.sparklePhase = frand() * 6.2832f;
                p.baseSize     = frandr(2.0f, 4.5f);
                p.alpha        = 0.f;
                p.isLetter     = true;
                p.goToBorder   = false;  // assigned at scatter time
                p.letterIdx    = li;
                p.row          = row;
                p.col          = bit;

                float t = frand();
                if      (t < 0.25f) { p.r = 0.88f; p.g = 0.95f; p.b = 1.00f; }  // near-white cool blue
                else if (t < 0.55f) { p.r = 0.55f; p.g = 0.82f; p.b = 1.00f; }  // atmosphere rim blue
                else if (t < 0.80f) { p.r = 0.32f; p.g = 0.62f; p.b = 0.95f; }  // deeper atmospheric
                else                { p.r = 0.70f; p.g = 0.88f; p.b = 1.00f; }  // light sky blue

                m_particles.push_back(p);
            }
    }
    buildConstellations();

    // ── Debris cloud around the title ─────────────────────────────────────────
    const int numDebris = 220;
    for (int i = 0; i < numDebris; ++i) {
        float angle  = frand() * 6.2832f;
        float radius = frandr(20.f, 240.f);
        // Slightly elliptical cloud matching the aspect of the title
        float tx = m_centerX + cosf(angle) * radius * 2.2f;
        float ty = m_centerY + sinf(angle) * radius * 0.9f;

        // Scatter start
        float sa = frand() * 6.2832f;
        float sd = frandr(350.f, 700.f);

        Particle p;
        p.x = W * 0.5f + cosf(sa) * sd;
        p.y = H * 0.5f + sinf(sa) * sd;
        p.tx = tx;  p.ty = ty;
        p.vx = 0.f; p.vy = 0.f;
        p.phase        = frand() * 6.2832f;
        p.twinkleSpeed = frandr(0.6f, 2.8f);
        p.sparklePhase = frand() * 6.2832f;
        p.baseSize     = frandr(0.8f, 3.0f);
        p.alpha        = 0.f;
        p.isLetter     = false;
        p.goToBorder   = false;  // assigned at scatter time
        // Slow random drift — used during Idle; overwritten at scatter trigger
        float driftAngle = frand() * 6.2832f;
        float driftSpeed = frandr(10.f, 35.f);
        p.vx = cosf(driftAngle) * driftSpeed;
        p.vy = sinf(driftAngle) * driftSpeed;
        p.letterIdx    = -1;
        p.row          = 0;
        p.col          = 0;

        // Debris — deep space blue matching atmosphere glow
        float t = frand();
        if      (t < 0.40f) { p.r = 0.12f; p.g = 0.28f; p.b = 0.78f; }  // deep navy
        else if (t < 0.70f) { p.r = 0.25f; p.g = 0.50f; p.b = 0.90f; }  // mid atmospheric
        else if (t < 0.90f) { p.r = 0.40f; p.g = 0.68f; p.b = 0.96f; }  // lighter atmospheric
        else                { p.r = 0.75f; p.g = 0.90f; p.b = 1.00f; }  // rare bright rim flash

        m_particles.push_back(p);
    }
}

// ── buildConstellations ───────────────────────────────────────────────────────
// Connect horizontally or vertically adjacent lit pixels within the same letter.

void IntroAnimation::buildConstellations() {
    m_lines.clear();
    const int n = (int)m_particles.size();
    for (int i = 0; i < n; ++i) {
        const auto& a = m_particles[(size_t)i];
        if (!a.isLetter) continue;
        for (int j = i + 1; j < n; ++j) {
            const auto& b = m_particles[(size_t)j];
            if (!b.isLetter) continue;
            if (a.letterIdx != b.letterIdx) continue;
            int dr = std::abs(a.row - b.row);
            int dc = std::abs(a.col - b.col);
            // 4-connectivity only (H + V neighbours)
            if ((dr == 1 && dc == 0) || (dr == 0 && dc == 1)) {
                ConstellationLine line;
                line.pi           = i;
                line.pj           = j;
                line.phase        = frand() * 6.2832f;
                // Very slow flicker: 0.15–0.55 Hz  →  × 2π ≈ 0.94–3.46 rad/s
                line.flickerSpeed = frandr(0.94f, 3.46f);
                m_lines.push_back(line);
            }
        }
    }
}

// ── assignBorderTargets ───────────────────────────────────────────────────────
// Distribute all particles evenly along the UI panel border rectangle.
// Particles are sorted by their current scatter angle so nearby particles
// end up at nearby border positions — keeps the lines looking coherent.

void IntroAnimation::assignBorderTargets() {
    // Must match UIManager::render() exactly:
    //   SetNextWindowPos  (10, 10)   ImGuiCond_Always
    //   SetNextWindowSize (340, 720) ImGuiCond_FirstUseEver
    //   WindowRounding = 10 px  WindowBorderSize = 1 px
    m_borderX = 10.f;
    m_borderY = 10.f;
    m_borderW = 340.f;
    m_borderH = 720.f;

    // Tag particles: left screen-half → border, right screen-half → scatter+fade
    for (auto& p : m_particles)
        p.goToBorder = (p.x < m_W * 0.5f);

    // Collect border-bound indices only
    std::vector<int> borderIdx;
    borderIdx.reserve(m_particles.size());
    for (int i = 0; i < (int)m_particles.size(); ++i)
        if (m_particles[(size_t)i].goToBorder)
            borderIdx.push_back(i);

    if (borderIdx.empty()) return;

    // Sort by scatter angle so nearby particles land at nearby border positions
    std::sort(borderIdx.begin(), borderIdx.end(), [&](int a, int b) {
        float aa = atan2f(m_particles[(size_t)a].vy, m_particles[(size_t)a].vx);
        float ba = atan2f(m_particles[(size_t)b].vy, m_particles[(size_t)b].vx);
        return aa < ba;
    });

    // Walk a rounded-rect perimeter (r matches UIManager WindowRounding = 10 px).
    // We build a list of sample points then assign one per border particle.
    const float r  = 10.f;   // must match style.WindowRounding
    const float x0 = m_borderX,            y0 = m_borderY;
    const float x1 = m_borderX + m_borderW, y1 = m_borderY + m_borderH;

    // Straight-edge lengths (between arc endpoints)
    const float sTop    = (x1 - r) - (x0 + r);
    const float sRight  = (y1 - r) - (y0 + r);
    const float sBottom = sTop;
    const float sLeft   = sRight;
    const float arcLen  = r * 1.5708f; // r * π/2
    const float total   = sTop + arcLen + sRight + arcLen + sBottom + arcLen + sLeft + arcLen;

    const int   nb = (int)borderIdx.size();
    for (int i = 0; i < nb; ++i) {
        float t  = (float)i / (float)nb * total;
        float px, py;

        // Top edge  (left→right)
        if (t < sTop) {
            px = x0 + r + t;  py = y0;
        }
        // Top-right arc
        else if ((t -= sTop) < arcLen) {
            float a = -1.5708f + (t / arcLen) * 1.5708f;   // -π/2 → 0
            px = (x1 - r) + cosf(a) * r;  py = (y0 + r) + sinf(a) * r;
        }
        // Right edge  (top→bottom)
        else if ((t -= arcLen) < sRight) {
            px = x1;  py = y0 + r + t;
        }
        // Bottom-right arc
        else if ((t -= sRight) < arcLen) {
            float a = 0.f + (t / arcLen) * 1.5708f;        // 0 → π/2
            px = (x1 - r) + cosf(a) * r;  py = (y1 - r) + sinf(a) * r;
        }
        // Bottom edge  (right→left)
        else if ((t -= arcLen) < sBottom) {
            px = x1 - r - t;  py = y1;
        }
        // Bottom-left arc
        else if ((t -= sBottom) < arcLen) {
            float a = 1.5708f + (t / arcLen) * 1.5708f;    // π/2 → π
            px = (x0 + r) + cosf(a) * r;  py = (y1 - r) + sinf(a) * r;
        }
        // Left edge  (bottom→top)
        else if ((t -= arcLen) < sLeft) {
            px = x0;  py = y1 - r - t;
        }
        // Top-left arc
        else {
            t -= sLeft;
            float a = 3.14159f + (t / arcLen) * 1.5708f;   // π → 3π/2
            px = (x0 + r) + cosf(a) * r;  py = (y0 + r) + sinf(a) * r;
        }

        m_particles[(size_t)borderIdx[(size_t)i]].tx = px;
        m_particles[(size_t)borderIdx[(size_t)i]].ty = py;
    }
}

// ── drawTitleGlow ─────────────────────────────────────────────────────────────
// Layered ambient halo centred on the whole ASTRODEX title block.

void IntroAnimation::drawTitleGlow(ImDrawList* dl) {
    // Only shown while the title is on screen; hide during and after explosion
    if (m_phase == IntroPhase::ExplodeOut    ||
        m_phase == IntroPhase::BorderAssemble ||
        m_phase == IntroPhase::BorderFade     ||
        m_phase == IntroPhase::Done) return;

    // Fade in alongside the particles; use first letter-particle alpha as proxy
    float masterAlpha = 0.f;
    for (const auto& p : m_particles)
        if (p.isLetter) { masterAlpha = p.alpha; break; }
    if (masterAlpha <= 0.01f) return;

    // Fade out immediately when START is pressed (over 0.5 s of scatter time)
    if (m_phase == IntroPhase::Scatter)
        masterAlpha *= std::max(0.f, 1.f - m_scatterTime / 0.5f);
    if (masterAlpha <= 0.01f) return;

    // Gentle slow pulse
    float pulse = 0.82f + 0.18f * sinf(m_time * 0.9f);

    const float cx = m_centerX;
    const float cy = m_centerY;

    // Half-extents of the tight title rect, with per-layer padding
    const float hw = m_titleW * 0.5f;
    const float hh = m_titleH * 0.5f;
    const float rounding = 18.f;

    struct Layer { float pad; int a; };
    // Outermost layers: very wide, very transparent — bloom
    // Inner layers: tight, slightly brighter
    static constexpr Layer kLayers[] = {
        { 120.f,  6 },
        {  85.f, 10 },
        {  60.f, 16 },
        {  42.f, 22 },
        {  28.f, 30 },
        {  16.f, 38 },
        {   8.f, 28 },   // tightest ring slightly dimmer so it doesn't overpower text
    };

    for (const auto& L : kLayers) {
        int alpha = (int)(L.a * pulse * masterAlpha);
        if (alpha <= 0) continue;
        ImU32 col = IM_COL32(20, 80, 200, alpha);
        dl->AddRectFilled(
            { cx - hw - L.pad, cy - hh - L.pad },
            { cx + hw + L.pad, cy + hh + L.pad },
            col, rounding + L.pad * 0.3f);
    }
}

// ── drawConstellations ────────────────────────────────────────────────────────

void IntroAnimation::drawConstellations(ImDrawList* dl) {
    // Constellation lines look good during Assemble/Idle/Scatter/BorderAssemble.
    // Skip during BorderFade — particles are now just dots outlining the panel.
    if (m_phase == IntroPhase::BorderFade) return;

    for (const auto& ln : m_lines) {
        const auto& pa = m_particles[(size_t)ln.pi];
        const auto& pb = m_particles[(size_t)ln.pj];

        // Line visibility follows the minimum alpha of its two endpoints
        float endpointAlpha = std::min(pa.alpha, pb.alpha);
        if (endpointAlpha <= 0.02f) continue;

        // Flicker: sin oscillation — line is mostly visible, occasionally drops out.
        // Using a power to spend more time near the bright peak.
        float raw  = sinf(m_time * ln.flickerSpeed + ln.phase);
        float vis  = raw * raw * raw * raw;        // [0..1], near 0 most of the time → inverted:
        vis        = 0.15f + 0.85f * (0.5f + 0.5f * raw); // smoother: mostly bright, dips low rarely

        float lineAlpha = endpointAlpha * vis;
        if (lineAlpha <= 0.02f) continue;

        // Colour: blend between the two endpoint colours
        float mr = (pa.r + pb.r) * 0.5f;
        float mg = (pa.g + pb.g) * 0.5f;
        float mb = (pa.b + pb.b) * 0.5f;

        // Outermost halo — very wide, very soft
        ImU32 haloCol = IM_COL32(
            (int)(mr * 60),
            (int)(mg * 120),
            (int)(mb * 200),
            (int)(lineAlpha * 18));
        dl->AddLine({ pa.x, pa.y }, { pb.x, pb.y }, haloCol, 8.0f);

        // Glow pass — wider, dimmer
        ImU32 glowCol = IM_COL32(
            (int)(mr * 120),
            (int)(mg * 180),
            (int)(mb * 255),
            (int)(lineAlpha * 55));
        dl->AddLine({ pa.x, pa.y }, { pb.x, pb.y }, glowCol, 3.5f);

        // Core line — thin, brighter
        ImU32 lineCol = IM_COL32(
            (int)(mr * 210),
            (int)(mg * 235),
            (int)(mb * 255),
            (int)(lineAlpha * 110));
        dl->AddLine({ pa.x, pa.y }, { pb.x, pb.y }, lineCol, 1.2f);
    }
}

// ── getUIAlpha ────────────────────────────────────────────────────────────────

float IntroAnimation::getUIAlpha() const {
    // The intro no longer transitions into the planet UI panel.
    // The galaxy screen handles its own fade-in via Application.
    return 0.f;
}

// ── syncBorderToWindow ────────────────────────────────────────────────────────
// Called from Application once the real ImGui window pos/size is known.
// Translates every border-particle target by the delta so they sit exactly
// on the panel's actual rendered border.

void IntroAnimation::syncBorderToWindow(float x, float y, float w, float h) {
    const float dx = x - m_borderX;
    const float dy = y - m_borderY;
    if (std::fabsf(dx) < 0.5f && std::fabsf(dy) < 0.5f) return; // already aligned

    for (auto& p : m_particles) {
        if (!p.goToBorder) continue;
        p.tx += dx;
        p.ty += dy;
        // Nudge current position toward the correction so the spring
        // closes the gap quickly without a jarring instant jump
        p.x  += dx;
        p.y  += dy;
    }
    m_borderX = x;
    m_borderY = y;
    m_borderW = w;
    m_borderH = h;
}

// ── update ────────────────────────────────────────────────────────────────────

void IntroAnimation::update(float dt) {
    m_time += dt;
    if (!m_initialized) return;

    switch (m_phase) {

    case IntroPhase::Assemble: {
        // Spring coefficient ramps up over time so assembly accelerates
        float spring = 4.5f + m_time * 3.f;
        bool allDone = true;
        for (auto& p : m_particles) {
            p.x    += (p.tx - p.x) * spring * dt;
            p.y    += (p.ty - p.y) * spring * dt;
            p.alpha = std::min(p.alpha + dt * 1.5f, 1.0f);
            float dist = (p.tx - p.x) * (p.tx - p.x) + (p.ty - p.y) * (p.ty - p.y);
            if (dist > 4.f) allDone = false;
        }
        if (allDone || m_time > 3.0f) {
            for (auto& p : m_particles) { p.x = p.tx; p.y = p.ty; p.alpha = 1.f; }
            m_phase = IntroPhase::Idle;
        }
        break;
    }

    case IntroPhase::Idle: {
        m_btnAlpha = std::min(m_btnAlpha + dt * 2.0f, 1.0f);
        const float pad = 30.f;
        for (auto& p : m_particles) {
            if (!p.isLetter) {
                // Free drift with screen-wrap
                p.x += p.vx * dt;
                p.y += p.vy * dt;
                if (p.x < -pad)        p.x += m_W + pad * 2.f;
                else if (p.x > m_W + pad) p.x -= m_W + pad * 2.f;
                if (p.y < -pad)        p.y += m_H + pad * 2.f;
                else if (p.y > m_H + pad) p.y -= m_H + pad * 2.f;
            }
        }
        break;
    }

    case IntroPhase::Scatter: {
        m_scatterTime += dt;
        m_btnAlpha = std::max(m_btnAlpha - dt * 6.f, 0.f);
        for (auto& p : m_particles) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
        }
        // After the initial burst, switch to ExplodeOut — all particles fly
        // off-screen to the sides, then the galaxy fades in.
        if (m_scatterTime > 0.55f) {
            m_borderTime = 0.f;
            m_phase = IntroPhase::ExplodeOut;
        }
        break;
    }

    case IntroPhase::ExplodeOut: {
        m_borderTime += dt;
        const float margin = 80.f;
        bool anyVisible = false;
        for (auto& p : m_particles) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            bool off = (p.x < -margin || p.x > m_W + margin ||
                        p.y < -margin || p.y > m_H + margin);
            if (off)
                p.alpha = std::max(p.alpha - dt * 2.5f, 0.f);
            if (p.alpha > 0.01f) anyVisible = true;
        }
        if (!anyVisible || m_borderTime > 1.8f)
            m_phase = IntroPhase::Done;
        break;
    }

    case IntroPhase::BorderAssemble: {
        m_borderTime += dt;
        const float spring = 5.f + m_borderTime * 3.5f;
        const float margin = 40.f;
        bool allBorderDone = true;
        for (auto& p : m_particles) {
            if (p.goToBorder) {
                // Spring toward border target; keep visible
                p.x += (p.tx - p.x) * spring * dt;
                p.y += (p.ty - p.y) * spring * dt;
                p.alpha = std::min(p.alpha + dt * 2.5f, 1.f);
                float d2 = (p.tx - p.x)*(p.tx - p.x) + (p.ty - p.y)*(p.ty - p.y);
                if (d2 > 6.f) allBorderDone = false;
            } else {
                // Right-side: keep flying, fade only once off-screen
                p.x += p.vx * dt;
                p.y += p.vy * dt;
                bool off = (p.x < -margin || p.x > m_W + margin ||
                            p.y < -margin || p.y > m_H + margin);
                if (off) p.alpha = std::max(p.alpha - dt * 4.f, 0.f);
            }
        }
        if (allBorderDone || m_borderTime > 2.2f) {
            for (auto& p : m_particles) if (p.goToBorder) { p.x = p.tx; p.y = p.ty; }
            m_borderTime = 0.f;
            m_phase = IntroPhase::BorderFade;
        }
        break;
    }

    case IntroPhase::BorderFade: {
        m_borderTime += dt;
        // Border particles fade only after the UI has had 0.6 s to appear
        const float fadeDelay = 0.6f;
        if (m_borderTime > fadeDelay) {
            float fadeRate = dt * 0.9f;
            for (auto& p : m_particles)
                if (p.goToBorder)
                    p.alpha = std::max(p.alpha - fadeRate, 0.f);
        }
        if (m_borderTime > 2.4f) m_phase = IntroPhase::Done;
        break;
    }

    case IntroPhase::Done:
        break;
    }
}

// ── drawParticles ─────────────────────────────────────────────────────────────

void IntroAnimation::drawParticles(ImDrawList* dl) {
    for (const auto& p : m_particles) {
        if (p.alpha <= 0.01f) continue;

        float tw, sz, alp;

        if (!p.isLetter) {
            // Debris: dramatic blink — spends most time dim, occasionally fires a bright
            // sparkle spike.  Two superimposed sines: a slow "gate" that occasionally
            // opens, and a fast oscillation that creates the flicker inside that window.
            float gate  = sinf(m_time * p.twinkleSpeed * 0.18f + p.sparklePhase);
            float fast  = sinf(m_time * p.twinkleSpeed * 1.3f  + p.phase);
            // Gate^4: spends ~75 % of time near 0, spikes sharply to 1.
            float gateP = gate * gate * gate * gate;
            // Sparkle peak = gate × fast in positive region
            float spike = gateP * std::max(0.f, fast);
            tw  = 0.12f + 0.88f * spike;   // mostly dim, bursts bright
            sz  = p.baseSize * (0.7f + 0.6f * spike);
            alp = p.alpha * tw;

            // Sparkle burst glow — only drawn when the particle is actually sparkling
            if (spike > 0.35f) {
                float burstR = sz + 5.f + spike * 8.f;
                float bh     = burstR * 0.5f;
                ImU32 bc = IM_COL32(
                    (int)(p.r * 120),
                    (int)(p.g * 170),
                    (int)(p.b * 255),
                    (int)(spike * 70 * p.alpha));
                dl->AddRectFilled({ p.x - bh, p.y - bh }, { p.x + bh, p.y + bh }, bc);
            }
        } else {
            // Letter pixels: gentle steady twinkle (unchanged)
            tw  = 0.65f + 0.35f * sinf(m_time * p.twinkleSpeed + p.phase);
            sz  = p.baseSize * (0.85f + 0.15f * tw);
            alp = p.alpha * tw;
        }

        float half = sz * 0.5f;

        // Outer glow (larger, dimmer)
        if (sz > 1.8f) {
            float gs  = sz + 2.8f;
            float gh  = gs * 0.5f;
            ImU32 gc  = IM_COL32(
                (int)(p.r * 120),
                (int)(p.g * 160),
                (int)(p.b * 255),
                (int)(alp * 55));
            dl->AddRectFilled({ p.x - gh, p.y - gh }, { p.x + gh, p.y + gh }, gc);
        }

        // Core pixel
        ImU32 col = IM_COL32(
            (int)(p.r * 255),
            (int)(p.g * 255),
            (int)(p.b * 255),
            (int)(alp * 255));
        dl->AddRectFilled({ p.x - half, p.y - half }, { p.x + half, p.y + half }, col);
    }
}

// ── drawStartButton ───────────────────────────────────────────────────────────

void IntroAnimation::drawStartButton(ImDrawList* dl, float W, float H, float alpha) {
    if (alpha <= 0.01f) return;

    const float bw = 210.f, bh = 52.f;
    const float bx = floorf(W * 0.5f - bw * 0.5f);
    const float by = floorf(H * 0.62f);

    m_btnX = bx; m_btnY = by; m_btnW = bw; m_btnH = bh;

    const float blink = 0.55f + 0.45f * sinf(m_time * 3.5f);

    // Background — dark navy
    dl->AddRectFilled({ bx, by }, { bx + bw, by + bh },
        IM_COL32(5, 10, 40, (int)(180 * alpha)));

    // Animated border — atmosphere blue
    const float bt = 3.f;
    const ImU32 bc = IM_COL32(80, 150, 255, (int)(210 * blink * alpha));
    dl->AddRectFilled({ bx,           by           }, { bx + bw,      by + bt      }, bc);
    dl->AddRectFilled({ bx,           by + bh - bt }, { bx + bw,      by + bh      }, bc);
    dl->AddRectFilled({ bx,           by           }, { bx + bt,       by + bh      }, bc);
    dl->AddRectFilled({ bx + bw - bt, by           }, { bx + bw,      by + bh      }, bc);

    // Corner accents — bright atmosphere blue
    const float cs = 6.f;
    const ImU32 cc = IM_COL32(100, 180, 255, (int)(255 * alpha));
    dl->AddRectFilled({ bx,           by           }, { bx + cs, by + cs }, cc);
    dl->AddRectFilled({ bx + bw - cs, by           }, { bx + bw, by + cs }, cc);
    dl->AddRectFilled({ bx,           by + bh - cs }, { bx + cs, by + bh }, cc);
    dl->AddRectFilled({ bx + bw - cs, by + bh - cs }, { bx + bw, by + bh }, cc);

    // Centred text with glow
    const char*  txt  = "[ START ]";
    const ImVec2 tsz  = ImGui::CalcTextSize(txt);
    const float  tx   = bx + (bw - tsz.x) * 0.5f;
    const float  ty   = by + (bh - tsz.y) * 0.5f;
    for (int ox = -1; ox <= 1; ++ox)
        for (int oy = -1; oy <= 1; ++oy)
            if (ox || oy)
                dl->AddText({ tx + ox, ty + oy },
                    IM_COL32(40, 100, 220, (int)(80 * alpha)), txt);
    dl->AddText({ tx, ty },
        IM_COL32(160, 210, 255, (int)(255 * alpha)), txt);

    // Click detection
    if (m_phase == IntroPhase::Idle && alpha > 0.5f) {
        auto& io = ImGui::GetIO();
        if (io.MouseClicked[0]) {
            float mx = io.MousePos.x, my = io.MousePos.y;
            if (mx >= bx && mx <= bx + bw && my >= by && my <= by + bh) {
                // Assign outward scatter velocities
                for (auto& p : m_particles) {
                    float dx = p.x - m_centerX, dy = p.y - m_centerY;
                    float len = sqrtf(dx * dx + dy * dy) + 0.1f;
                    float spd = frandr(500.f, 1100.f);
                    p.vx = (dx / len) * spd;
                    p.vy = (dy / len) * spd;
                }
                m_phase = IntroPhase::Scatter;
            }
        }
    }
}

// ── drawScanlines ─────────────────────────────────────────────────────────────

void IntroAnimation::drawScanlines(ImDrawList* dl, float W, float H, float alpha) {
    if (alpha <= 0.01f) return;
    const ImU32 col = IM_COL32(0, 0, 0, (int)(28 * alpha));
    for (float y = 0.f; y < H; y += 4.f)
        dl->AddRectFilled({ 0.f, y }, { W, y + 1.5f }, col);
}

// ── render ────────────────────────────────────────────────────────────────────

void IntroAnimation::render(ImDrawList* dl, float W, float H) {
    if (!m_initialized) {
        initParticles(W, H);
        m_initialized = true;
    } else if (std::abs(W - m_W) > 1.f || std::abs(H - m_H) > 1.f) {
        // Window was resized - scale all positions proportionally
        float scaleX = W / m_W;
        float scaleY = H / m_H;
        for (auto& p : m_particles) {
            p.x *= scaleX;
            p.y *= scaleY;
            p.tx *= scaleX;
            p.ty *= scaleY;
        }
        m_centerX *= scaleX;
        m_centerY *= scaleY;
        m_titleW *= scaleX;
        m_titleH *= scaleY;
        m_borderX *= scaleX;
        m_borderY *= scaleY;
        m_borderW *= scaleX;
        m_borderH *= scaleY;
    }
    m_W = W; m_H = H;

    if (m_phase == IntroPhase::Done) return;

    drawTitleGlow(dl);
    drawConstellations(dl);
    drawParticles(dl);
    drawStartButton(dl, W, H, m_btnAlpha);
    drawScanlines(dl, W, H, 0.6f);
}

}  // namespace astrocore

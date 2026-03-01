#pragma once
#include <imgui.h>
#include <vector>

namespace astrocore {

enum class IntroPhase { Assemble, Idle, Scatter, ExplodeOut, BorderAssemble, BorderFade, Done };

struct Particle {
    float x, y;          // current position
    float tx, ty;        // target position
    float vx, vy;        // scatter velocity
    float phase;         // twinkle phase offset
    float twinkleSpeed;
    float sparklePhase;  // secondary phase for debris sparkle bursts
    float baseSize;
    float r, g, b;       // colour [0..1]
    float alpha;
    bool  isLetter;
    bool  goToBorder;    // true = assemble into UI border; false = scatter + fade
    int   letterIdx;     // which letter (-1 = debris)
    int   row, col;      // grid position within letter
};

struct ConstellationLine {
    int   pi, pj;        // indices into m_particles
    float phase;         // flicker phase
    float flickerSpeed;  // radians/s (slow = rare flicker)
};

class IntroAnimation {
public:
    IntroAnimation() = default;
    void  update(float dt);
    void  render(ImDrawList* dl, float W, float H);
    bool  isDone()      const { return m_phase == IntroPhase::Done; }
    // Returns 0 until the UI border is assembled, then ramps 0->1 over ~1 s
    float getUIAlpha()  const;
    // Called once the real ImGui window rect is known - snaps border particles
    // to match it exactly (handles any imgui.ini saved offset)
    void  syncBorderToWindow(float x, float y, float w, float h);

private:
    void initParticles        (float W, float H);
    void buildConstellations  ();
    void assignBorderTargets  ();
    void drawTitleGlow        (ImDrawList* dl);
    void drawConstellations   (ImDrawList* dl);
    void drawParticles        (ImDrawList* dl);
    void drawStartButton      (ImDrawList* dl, float W, float H, float alpha);
    void drawScanlines        (ImDrawList* dl, float W, float H, float alpha);

    IntroPhase            m_phase        = IntroPhase::Assemble;
    float                 m_time         = 0.f;
    float                 m_scatterTime  = 0.f;
    float                 m_btnAlpha     = 0.f;
    float                 m_btnX = 0.f, m_btnY = 0.f;
    float                 m_btnW = 0.f, m_btnH = 0.f;
    float                 m_centerX      = 0.f;   // title centre (for scatter)
    float                 m_centerY      = 0.f;
    float                 m_titleW       = 0.f;   // pixel dimensions of the title block
    float                 m_titleH       = 0.f;
    float                 m_W            = 0.f;   // screen size (for wrapping)
    float                 m_H            = 0.f;
    float                 m_borderX      = 0.f;   // UI panel border rect
    float                 m_borderY      = 0.f;
    float                 m_borderW      = 0.f;
    float                 m_borderH      = 0.f;
    float                 m_borderTime   = 0.f;   // time within BorderAssemble/BorderFade
    bool                          m_initialized  = false;
    std::vector<Particle>         m_particles;
    std::vector<ConstellationLine> m_lines;
};

}  // namespace astrocore

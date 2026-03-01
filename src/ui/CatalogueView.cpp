#include "ui/CatalogueView.hpp"
#include "data/ExoplanetData.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <spdlog/fmt/fmt.h>

namespace astrocore {

// ── Planet type color mapping (placeholder card backgrounds) ─────────────────
// Terrestrial: brown/rocky  |  Gas Giant: orange/Jupiter-like
// Ice Giant: blue/Neptune    |  Super-Earth: green/Earth-like
// Unknown: gray

static const ImU32 kColorTerrestrial = IM_COL32(139, 90, 60, 255);
static const ImU32 kColorGasGiant    = IM_COL32(255, 140, 60, 255);
static const ImU32 kColorIceGiant    = IM_COL32(100, 180, 255, 255);
static const ImU32 kColorSuperEarth  = IM_COL32(80, 200, 120, 255);
static const ImU32 kColorUnknown     = IM_COL32(120, 120, 120, 255);

// Card dimensions
static constexpr float kCardW      = 200.f;
static constexpr float kCardH      = 280.f;
static constexpr float kSpacing    = 16.f;
static constexpr float kCornerR    = 8.f;
static constexpr float kThumbH     = 160.f;  // Height of the color placeholder area
static constexpr float kPadInner   = 10.f;

// Sort mode display strings (for combo dropdown)
static const char* kSortLabels[] = {
    "Discovery Date (newest)",
    "Discovery Date (oldest)",
    "Mass (heaviest)",
    "Mass (lightest)",
    "Radius (largest)",
    "Radius (smallest)",
    "Name A-Z",
    "Name Z-A"
};

// ── Helpers ──────────────────────────────────────────────────────────────────

// Classify a planet_type string into a canonical category
static std::string canonicalType(const std::string& raw) {
    // Lowercase comparison
    std::string lower;
    lower.reserve(raw.size());
    for (char c : raw) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    if (lower.find("gas") != std::string::npos && lower.find("giant") != std::string::npos)
        return "Gas Giant";
    if (lower.find("ice") != std::string::npos && lower.find("giant") != std::string::npos)
        return "Ice Giant";
    if (lower.find("super") != std::string::npos)
        return "Super-Earth";
    if (lower.find("terrestrial") != std::string::npos || lower.find("rocky") != std::string::npos)
        return "Terrestrial";
    if (lower.find("neptune") != std::string::npos)
        return "Ice Giant";
    if (lower.find("jupiter") != std::string::npos)
        return "Gas Giant";
    return raw;
}

// Case-insensitive prefix match
static bool prefixMatch(const std::string& text, const char* prefix) {
    size_t plen = std::strlen(prefix);
    if (plen == 0) return true;
    if (text.size() < plen) return false;
    for (size_t i = 0; i < plen; ++i) {
        if (std::tolower(static_cast<unsigned char>(text[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i])))
            return false;
    }
    return true;
}

// ── CatalogueView implementation ─────────────────────────────────────────────

CatalogueView::CatalogueView() = default;

void CatalogueView::init() {
    m_needsRefilter = true;
}

ImU32 CatalogueView::planetTypeColor(const std::string& type) {
    std::string canon = canonicalType(type);
    if (canon == "Terrestrial")  return kColorTerrestrial;
    if (canon == "Gas Giant")    return kColorGasGiant;
    if (canon == "Ice Giant")    return kColorIceGiant;
    if (canon == "Super-Earth")  return kColorSuperEarth;
    return kColorUnknown;
}

bool CatalogueView::isInHabitableZone(const ExoplanetData& planet) {
    if (!planet.semi_major_axis_au.hasValue())
        return false;
    if (!planet.host_star.luminosity_solar.hasValue())
        return false;

    double lum = planet.host_star.luminosity_solar.value;
    if (lum <= 0.0) return false;

    double sqrtLum = std::sqrt(lum);
    double inner = sqrtLum * constants::HZ_INNER_FACTOR;
    double outer = sqrtLum * constants::HZ_OUTER_FACTOR;
    double sma = planet.semi_major_axis_au.value;

    return (sma >= inner && sma <= outer);
}

void CatalogueView::setPlanetCallback(std::function<void(const std::string&)> cb) {
    m_planetCallback = std::move(cb);
}

void CatalogueView::setLoadingProgress(int current, int total) {
    m_loadingCurrent = current;
    m_loadingTotal = total;
}

// ── Main render entry point ─────────────────────────────────────────────────

void CatalogueView::setThumbnail(const std::string& name, ImTextureID texID) {
    m_thumbnails[name] = texID;
}

void CatalogueView::render(const std::vector<ExoplanetData>& data, float W, float H, float dt) {
    // Reset hovered card each frame; it will be set during renderCardGrid
    m_hoveredCardIdx = -1;
    m_hoveredPlanetName.clear();
    // Full-screen catalogue window (no title bar, no move)
    ImGui::SetNextWindowPos({0.f, 0.f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({W, H}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16.f, 12.f});

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration    |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (!ImGui::Begin("##catalogue_view", nullptr, kFlags)) {
        ImGui::PopStyleVar();
        ImGui::End();
        return;
    }
    ImGui::PopStyleVar();

    // ── Title ────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.88f, 1.0f, 1.0f));
    ImGui::Text("Exoplanet Catalogue");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("(%d planets)", static_cast<int>(data.size()));
    ImGui::Spacing();

    // ── Top bar: search + filters + sort ─────────────────────────────────
    renderSearchBar(data);
    ImGui::SameLine();
    renderSortControls();
    ImGui::Spacing();
    renderFilters();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Loading progress ─────────────────────────────────────────────────
    if (m_loadingTotal > 0 && m_loadingCurrent < m_loadingTotal) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 0.85f));
        auto loadText = fmt::format("Loading... {}/{} planets", m_loadingCurrent, m_loadingTotal);
        float textW = ImGui::CalcTextSize(loadText.c_str()).x;
        float availW = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((availW - textW) * 0.5f + ImGui::GetCursorPosX());
        ImGui::Text("%s", loadText.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ── Apply filters/sort if needed ─────────────────────────────────────
    if (m_needsRefilter) {
        applyFiltersAndSort(data);
        m_needsRefilter = false;
    }

    // ── Card grid ────────────────────────────────────────────────────────
    renderCardGrid(data, W, H);

    ImGui::End();
}

// ── Search bar ──────────────────────────────────────────────────────────────

void CatalogueView::renderSearchBar(const std::vector<ExoplanetData>& data) {
    ImGui::SetNextItemWidth(220.f);
    bool changed = ImGui::InputTextWithHint("##cat_search", "Search planets...",
                                             m_searchBuf, sizeof(m_searchBuf));
    if (changed) {
        m_needsRefilter = true;
    }

    // Autocomplete dropdown
    if (std::strlen(m_searchBuf) > 0 && ImGui::IsItemActive()) {
        // Collect matching names (up to 8 suggestions)
        std::vector<const ExoplanetData*> matches;
        for (const auto& planet : data) {
            if (prefixMatch(planet.name, m_searchBuf)) {
                matches.push_back(&planet);
                if (matches.size() >= 8) break;
            }
        }

        if (!matches.empty()) {
            ImGui::SetNextWindowPos(
                {ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y});
            ImGui::SetNextWindowSize({220.f, 0.f});
            if (ImGui::Begin("##cat_ac", nullptr,
                             ImGuiWindowFlags_NoDecoration   |
                             ImGuiWindowFlags_NoMove         |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_Tooltip)) {
                for (const auto* p : matches) {
                    bool selected = ImGui::Selectable(p->name.c_str());
                    if (selected) {
                        // Copy name into search buffer
                        std::strncpy(m_searchBuf, p->name.c_str(), sizeof(m_searchBuf) - 1);
                        m_searchBuf[sizeof(m_searchBuf) - 1] = '\0';
                        m_needsRefilter = true;
                    }
                }
            }
            ImGui::End();
        }
    }
}

// ── Filter chips ────────────────────────────────────────────────────────────

void CatalogueView::renderFilters() {
    // Type filter chips in a horizontal row
    auto chipButton = [this](const char* label, PlanetTypeFilter type) {
        bool active = (m_typeFilter == type);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.50f, 0.80f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.55f, 0.85f, 0.90f));
        }
        if (ImGui::SmallButton(label)) {
            m_typeFilter = type;
            m_needsRefilter = true;
        }
        if (active) {
            ImGui::PopStyleColor(2);
        }
        ImGui::SameLine();
    };

    chipButton("All", PlanetTypeFilter::All);
    chipButton("Terrestrial", PlanetTypeFilter::Terrestrial);
    chipButton("Gas Giant", PlanetTypeFilter::GasGiant);
    chipButton("Ice Giant", PlanetTypeFilter::IceGiant);
    chipButton("Super-Earth", PlanetTypeFilter::SuperEarth);

    // Habitable Zone toggle (checkbox style)
    {
        bool active = m_habitableZoneFilter;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.35f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.60f, 0.40f, 0.90f));
        }
        if (ImGui::SmallButton(m_habitableZoneFilter ? "[x] Habitable Zone" : "[ ] Habitable Zone")) {
            m_habitableZoneFilter = !m_habitableZoneFilter;
            m_needsRefilter = true;
        }
        if (active) {
            ImGui::PopStyleColor(2);
        }
    }
}

// ── Sort controls ───────────────────────────────────────────────────────────

void CatalogueView::renderSortControls() {
    ImGui::Text("Sort:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.f);
    int sortIdx = static_cast<int>(m_sortMode);
    if (ImGui::Combo("##cat_sort", &sortIdx, kSortLabels,
                      IM_ARRAYSIZE(kSortLabels))) {
        m_sortMode = static_cast<CatalogueSortMode>(sortIdx);
        m_needsRefilter = true;
    }
}

// ── Filter and sort logic ───────────────────────────────────────────────────

void CatalogueView::applyFiltersAndSort(const std::vector<ExoplanetData>& data) {
    m_filteredIndices.clear();
    m_filteredIndices.reserve(data.size());

    for (int i = 0; i < static_cast<int>(data.size()); ++i) {
        const auto& planet = data[i];

        // Text search filter
        if (std::strlen(m_searchBuf) > 0) {
            if (!prefixMatch(planet.name, m_searchBuf))
                continue;
        }

        // Planet type filter
        if (m_typeFilter != PlanetTypeFilter::All) {
            std::string canon = planet.planet_type.hasValue()
                ? canonicalType(planet.planet_type.value)
                : "";

            switch (m_typeFilter) {
            case PlanetTypeFilter::Terrestrial:
                if (canon != "Terrestrial") continue;
                break;
            case PlanetTypeFilter::GasGiant:
                if (canon != "Gas Giant") continue;
                break;
            case PlanetTypeFilter::IceGiant:
                if (canon != "Ice Giant") continue;
                break;
            case PlanetTypeFilter::SuperEarth:
                if (canon != "Super-Earth") continue;
                break;
            default: break;
            }
        }

        // Habitable zone filter
        if (m_habitableZoneFilter) {
            if (!isInHabitableZone(planet))
                continue;
        }

        m_filteredIndices.push_back(i);
    }

    // Sort
    std::sort(m_filteredIndices.begin(), m_filteredIndices.end(),
        [&data, this](int a, int b) {
            const auto& pa = data[a];
            const auto& pb = data[b];

            switch (m_sortMode) {
            case CatalogueSortMode::DiscoveryDateDesc:
                return pa.discovery_year > pb.discovery_year;
            case CatalogueSortMode::DiscoveryDateAsc:
                return pa.discovery_year < pb.discovery_year;
            case CatalogueSortMode::MassDesc: {
                double ma = pa.mass_earth.hasValue() ? pa.mass_earth.value : -1.0;
                double mb = pb.mass_earth.hasValue() ? pb.mass_earth.value : -1.0;
                return ma > mb;
            }
            case CatalogueSortMode::MassAsc: {
                double ma = pa.mass_earth.hasValue() ? pa.mass_earth.value : 1e18;
                double mb = pb.mass_earth.hasValue() ? pb.mass_earth.value : 1e18;
                return ma < mb;
            }
            case CatalogueSortMode::RadiusDesc: {
                double ra = pa.radius_earth.hasValue() ? pa.radius_earth.value : -1.0;
                double rb = pb.radius_earth.hasValue() ? pb.radius_earth.value : -1.0;
                return ra > rb;
            }
            case CatalogueSortMode::RadiusAsc: {
                double ra = pa.radius_earth.hasValue() ? pa.radius_earth.value : 1e18;
                double rb = pb.radius_earth.hasValue() ? pb.radius_earth.value : 1e18;
                return ra < rb;
            }
            case CatalogueSortMode::NameAZ:
                return pa.name < pb.name;
            case CatalogueSortMode::NameZA:
                return pa.name > pb.name;
            }
            return false;
        });
}

// ── Card grid rendering ─────────────────────────────────────────────────────

void CatalogueView::renderCardGrid(const std::vector<ExoplanetData>& data,
                                     float /*W*/, float /*H*/) {
    if (m_filteredIndices.empty()) {
        ImGui::TextDisabled("No planets match current filters.");
        return;
    }

    // Scrollable child region for the card grid
    if (!ImGui::BeginChild("##card_grid", {0.f, 0.f}, false,
                           ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        ImGui::EndChild();
        return;
    }

    float availW = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, static_cast<int>((availW + kSpacing) / (kCardW + kSpacing)));

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int idx = 0; idx < static_cast<int>(m_filteredIndices.size()); ++idx) {
        int dataIdx = m_filteredIndices[idx];
        const auto& planet = data[dataIdx];
        int col = idx % cols;

        ImGui::PushID(idx);

        // Position management: new row
        if (col == 0 && idx > 0) {
            ImGui::Dummy({0.f, kSpacing});
        }

        ImVec2 cursorStart = ImGui::GetCursorScreenPos();

        // Card background (dark semi-transparent)
        ImVec2 cardMin = cursorStart;
        ImVec2 cardMax = {cursorStart.x + kCardW, cursorStart.y + kCardH};
        dl->AddRectFilled(cardMin, cardMax, IM_COL32(25, 25, 32, 220), kCornerR);

        // Card border (subtle)
        dl->AddRect(cardMin, cardMax, IM_COL32(60, 65, 80, 120), kCornerR, 0, 1.0f);

        // ── Thumbnail or colored placeholder rectangle ─────────────────
        std::string typeStr = planet.planet_type.hasValue() ? planet.planet_type.value : "";
        ImVec2 thumbMin = {cursorStart.x + kPadInner, cursorStart.y + kPadInner};
        ImVec2 thumbMax = {cursorStart.x + kCardW - kPadInner,
                           cursorStart.y + kPadInner + kThumbH};

        // Check if we have a rendered thumbnail for this planet
        auto thumbIt = m_thumbnails.find(planet.name);
        if (thumbIt != m_thumbnails.end() && thumbIt->second != 0) {
            // Draw the thumbnail image
            ImVec2 thumbSize = {thumbMax.x - thumbMin.x, thumbMax.y - thumbMin.y};
            ImGui::SetCursorScreenPos(thumbMin);
            ImGui::Image(thumbIt->second, thumbSize);
        } else {
            // Fallback: colored placeholder rectangle
            ImU32 typeColor = planetTypeColor(typeStr);
            dl->AddRectFilled(thumbMin, thumbMax, typeColor, 4.f);

            // Type label centered on the placeholder
            if (!typeStr.empty()) {
                std::string canon = canonicalType(typeStr);
                ImVec2 labelSz = ImGui::CalcTextSize(canon.c_str());
                float labelX = thumbMin.x + (thumbMax.x - thumbMin.x - labelSz.x) * 0.5f;
                float labelY = thumbMin.y + (thumbMax.y - thumbMin.y - labelSz.y) * 0.5f;
                dl->AddText({labelX, labelY}, IM_COL32(255, 255, 255, 200), canon.c_str());
            }
        }

        // ── Text area below placeholder ─────────────────────────────────
        float textY = cursorStart.y + kPadInner + kThumbH + 8.f;

        // Planet name (truncate if too long)
        {
            std::string displayName = planet.name;
            float maxTextW = kCardW - 2.f * kPadInner;
            ImVec2 nameSz = ImGui::CalcTextSize(displayName.c_str());
            if (nameSz.x > maxTextW) {
                // Truncate with ellipsis
                while (displayName.size() > 3) {
                    displayName.pop_back();
                    auto testName = displayName + "...";
                    if (ImGui::CalcTextSize(testName.c_str()).x <= maxTextW) {
                        displayName = testName;
                        break;
                    }
                }
            }
            dl->AddText({cursorStart.x + kPadInner, textY},
                        IM_COL32(230, 230, 240, 255), displayName.c_str());
            textY += ImGui::GetTextLineHeight() + 2.f;
        }

        // Discovery year
        if (planet.discovery_year > 0) {
            auto yearStr = fmt::format("{}", planet.discovery_year);
            dl->AddText({cursorStart.x + kPadInner, textY},
                        IM_COL32(160, 165, 185, 200), yearStr.c_str());
            textY += ImGui::GetTextLineHeight() + 2.f;
        }

        // Key stat: mass or radius (whichever is available)
        if (planet.mass_earth.hasValue()) {
            auto massStr = fmt::format("{:.1f} M\xe2\x8a\x95", planet.mass_earth.value);
            dl->AddText({cursorStart.x + kPadInner, textY},
                        IM_COL32(140, 180, 220, 200), massStr.c_str());
            textY += ImGui::GetTextLineHeight() + 2.f;
        } else if (planet.radius_earth.hasValue()) {
            auto radStr = fmt::format("{:.1f} R\xe2\x8a\x95", planet.radius_earth.value);
            dl->AddText({cursorStart.x + kPadInner, textY},
                        IM_COL32(140, 180, 220, 200), radStr.c_str());
            textY += ImGui::GetTextLineHeight() + 2.f;
        } else {
            // Data incomplete indicator
            dl->AddText({cursorStart.x + kPadInner, textY},
                        IM_COL32(200, 120, 80, 180), "Data incomplete");
        }

        // ── Click detection (invisible button covering entire card) ─────
        ImGui::SetCursorScreenPos(cursorStart);
        if (ImGui::InvisibleButton("##card", {kCardW, kCardH})) {
            onCardClicked(dataIdx, data);
        }

        // Hover highlight and thumbnail hover tracking
        if (ImGui::IsItemHovered()) {
            dl->AddRect(cardMin, cardMax, IM_COL32(100, 180, 255, 180), kCornerR, 0, 2.0f);
            ImGui::SetTooltip("%s", planet.name.c_str());
            m_hoveredCardIdx = dataIdx;
            m_hoveredPlanetName = planet.name;
        }

        ImGui::PopID();

        // Same-line for grid columns
        if (col < cols - 1) {
            ImGui::SameLine(0.f, kSpacing);
        }
    }

    ImGui::EndChild();
}

// ── Card click handler ──────────────────────────────────────────────────────

void CatalogueView::onCardClicked(int idx, const std::vector<ExoplanetData>& data) {
    if (idx < 0 || idx >= static_cast<int>(data.size())) return;
    if (m_planetCallback) {
        m_planetCallback(data[idx].name);
    }
}

}  // namespace astrocore

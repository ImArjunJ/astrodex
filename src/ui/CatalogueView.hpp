#pragma once

#include <imgui.h>
#include <vector>
#include <string>
#include <functional>
#include <set>
#include <unordered_map>

namespace astrocore {

struct ExoplanetData;

// Sort modes for the catalogue
enum class CatalogueSortMode {
    DiscoveryDateDesc,
    DiscoveryDateAsc,
    MassDesc,
    MassAsc,
    RadiusDesc,
    RadiusAsc,
    NameAZ,
    NameZA
};

// Planet type filter
enum class PlanetTypeFilter {
    All,
    Terrestrial,
    GasGiant,
    IceGiant,
    SuperEarth
};

// ── CatalogueView ─────────────────────────────────────────────────────────────
// Renders a Pokedex-style card grid of exoplanets with filtering, sorting,
// and search. Each card shows a colored placeholder rectangle based on planet
// type, the planet name, discovery year, and a key stat. Clicking a card
// triggers a callback to load that planet in the main renderer.
class CatalogueView {
public:
    CatalogueView();

    void init();
    void render(const std::vector<ExoplanetData>& data, float W, float H, float dt = 0.f);
    void setPlanetCallback(std::function<void(const std::string&)> cb);
    void setLoadingProgress(int current, int total);

    // Thumbnail management
    void setThumbnail(const std::string& name, ImTextureID texID);
    int getHoveredCardIndex() const { return m_hoveredCardIdx; }
    std::string getHoveredPlanetName() const { return m_hoveredPlanetName; }

private:
    void renderSearchBar(const std::vector<ExoplanetData>& data);
    void renderFilters();
    void renderSortControls();
    void renderCardGrid(const std::vector<ExoplanetData>& data, float W, float H);
    void applyFiltersAndSort(const std::vector<ExoplanetData>& data);
    void onCardClicked(int idx, const std::vector<ExoplanetData>& data);

    // Returns true if the planet is in the habitable zone
    static bool isInHabitableZone(const ExoplanetData& planet);

    // Returns an ImU32 color for a planet type string
    static ImU32 planetTypeColor(const std::string& type);

    // Filter state
    PlanetTypeFilter m_typeFilter = PlanetTypeFilter::All;
    bool m_habitableZoneFilter = false;

    // Sort state
    CatalogueSortMode m_sortMode = CatalogueSortMode::DiscoveryDateDesc;

    // Search state
    char m_searchBuf[256] = {};
    bool m_acOpen = false;

    // Filtered and sorted indices into the data vector
    std::vector<int> m_filteredIndices;
    bool m_needsRefilter = true;

    // Loading progress
    int m_loadingCurrent = 0;
    int m_loadingTotal = 0;

    // Planet click callback
    std::function<void(const std::string&)> m_planetCallback;

    // Thumbnail state
    std::unordered_map<std::string, ImTextureID> m_thumbnails;
    int m_hoveredCardIdx = -1;
    std::string m_hoveredPlanetName;
    float m_hoverTime = 0.f;
};

}  // namespace astrocore

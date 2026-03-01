#pragma once

#include <string>
#include <vector>
#include "data/ExoplanetData.hpp"

namespace astrocore {

// Reference exoplanet data for comparison plots
struct ReferenceExoplanet {
    std::string name;
    double mass_earth = 0.0;
    double radius_earth = 0.0;
    double equilibrium_temp_k = 0.0;
    double orbital_period_days = 0.0;
    double semi_major_axis_au = 0.0;
    double insolation_flux = 1.0;
    double earth_similarity_index = 0.0;
    std::string planet_type;
    bool is_reference = false;  // True for Earth, Venus, Mars, etc.
};

// Chart type for sub-tab selection
enum class ChartType {
    MassRadius,
    HabitableZone,
    OrbitalParams,
    Temperature,
    Atmosphere,
    Comparison
};

// Data visualization class for exoplanet charts
class DataVisualization {
public:
    DataVisualization();
    ~DataVisualization();

    // Initialize ImPlot context
    void init();

    // Cleanup
    void shutdown();

    // Set the current planet to visualize
    void setCurrentPlanet(const ExoplanetData& data);
    void clearCurrentPlanet();

    // Add reference planets for comparison
    void addReferencePlanet(const ReferenceExoplanet& planet);
    void clearReferencePlanets();
    void loadDefaultReferences();

    // Render the GRAPHS tab content
    void renderGraphsTab();

    // Apply theme (call when theme changes)
    void applyTheme(bool darkMode);

private:
    // Individual chart renderers
    void renderMassRadiusDiagram();
    void renderHabitableZonePlot();
    void renderOrbitalParametersChart();
    void renderTemperatureHistogram();
    void renderAtmosphereChart();
    void renderComparisonChart();

    // Helper to render chart sub-tabs
    void renderChartTabs();

    // Helper for parsing atmosphere JSON
    struct AtmosphereComponent {
        std::string gas;
        double percentage;
    };
    std::vector<AtmosphereComponent> parseAtmosphere(const std::string& json);

    // Current planet data
    ExoplanetData m_currentPlanet;
    bool m_hasCurrentPlanet = false;

    // Reference planets for comparison
    std::vector<ReferenceExoplanet> m_referencePlanets;

    // Selected chart type
    ChartType m_selectedChart = ChartType::MassRadius;

    // Display options
    bool m_showUncertainties = true;
    bool m_showDataSources = true;
    bool m_highlightCurrent = true;

    // Initialized flag
    bool m_initialized = false;
    bool m_darkMode = true;
};

}  // namespace astrocore

#include "ui/DataVisualization.hpp"
#include "core/Logger.hpp"

#include <imgui.h>
#include <implot.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>

namespace astrocore {

// Composition curve parameters for Mass-Radius diagram
namespace composition {
    // Pure iron composition: R = 0.77 * M^0.27
    inline double ironRadius(double mass) { return 0.77 * std::pow(mass, 0.27); }
    // Earth-like rocky: R = M^0.27
    inline double earthLikeRadius(double mass) { return std::pow(mass, 0.27); }
    // Pure water ice: R = 1.24 * M^0.27
    inline double waterRadius(double mass) { return 1.24 * std::pow(mass, 0.27); }
    // H/He envelope (mini-Neptune): R = 2.5 * M^0.20
    inline double gasEnvelopeRadius(double mass) { return 2.5 * std::pow(mass, 0.20); }
}

DataVisualization::DataVisualization() = default;

DataVisualization::~DataVisualization() {
    shutdown();
}

void DataVisualization::init() {
    if (m_initialized) return;

    ImPlot::CreateContext();

    // Configure default plot style
    ImPlotStyle& style = ImPlot::GetStyle();
    style.PlotPadding = ImVec2(10, 10);
    style.LabelPadding = ImVec2(5, 5);
    style.LegendPadding = ImVec2(5, 5);
    style.PlotMinSize = ImVec2(200, 150);

    loadDefaultReferences();
    m_initialized = true;
    LOG_INFO("DataVisualization initialized with ImPlot");
}

void DataVisualization::shutdown() {
    if (!m_initialized) return;
    ImPlot::DestroyContext();
    m_initialized = false;
}

void DataVisualization::setCurrentPlanet(const ExoplanetData& data) {
    m_currentPlanet = data;
    m_hasCurrentPlanet = true;
}

void DataVisualization::clearCurrentPlanet() {
    m_currentPlanet = ExoplanetData{};
    m_hasCurrentPlanet = false;
}

void DataVisualization::addReferencePlanet(const ReferenceExoplanet& planet) {
    m_referencePlanets.push_back(planet);
}

void DataVisualization::clearReferencePlanets() {
    m_referencePlanets.clear();
}

void DataVisualization::loadDefaultReferences() {
    clearReferencePlanets();

    // Solar system references
    addReferencePlanet({"Earth", 1.0, 1.0, 255, 365.25, 1.0, 1.0, 1.0, "Rocky", true});
    addReferencePlanet({"Venus", 0.815, 0.949, 737, 224.7, 0.723, 1.91, 0.44, "Rocky", true});
    addReferencePlanet({"Mars", 0.107, 0.532, 210, 687.0, 1.524, 0.43, 0.70, "Rocky", true});
    addReferencePlanet({"Mercury", 0.055, 0.383, 440, 88.0, 0.387, 6.67, 0.56, "Rocky", true});

    // Notable exoplanets
    addReferencePlanet({"Kepler-442 b", 2.34, 1.34, 233, 112.3, 0.409, 0.70, 0.84, "Super-Earth", false});
    addReferencePlanet({"TRAPPIST-1 e", 0.772, 0.918, 251, 6.1, 0.029, 0.66, 0.85, "Rocky", false});
    addReferencePlanet({"TRAPPIST-1 f", 0.934, 1.045, 219, 9.2, 0.038, 0.38, 0.68, "Rocky", false});
    addReferencePlanet({"Proxima Cen b", 1.07, 1.08, 234, 11.2, 0.049, 0.65, 0.87, "Rocky", false});
    addReferencePlanet({"Kepler-452 b", 5.0, 1.63, 265, 384.8, 1.046, 1.10, 0.83, "Super-Earth", false});
    addReferencePlanet({"LHS 1140 b", 6.98, 1.73, 235, 24.7, 0.094, 0.46, 0.74, "Super-Earth", false});
    addReferencePlanet({"K2-18 b", 8.63, 2.61, 284, 32.9, 0.143, 1.11, 0.0, "Mini-Neptune", false});
    addReferencePlanet({"HD 40307 g", 7.09, 1.8, 210, 197.8, 0.600, 0.68, 0.0, "Super-Earth", false});
}

void DataVisualization::applyTheme(bool darkMode) {
    m_darkMode = darkMode;

    if (!m_initialized) return;

    ImPlotStyle& style = ImPlot::GetStyle();

    if (darkMode) {
        // Dark theme colors
        style.Colors[ImPlotCol_PlotBg] = ImVec4(0.08f, 0.12f, 0.18f, 0.95f);
        style.Colors[ImPlotCol_PlotBorder] = ImVec4(0.3f, 0.5f, 0.7f, 0.3f);
        style.Colors[ImPlotCol_LegendBg] = ImVec4(0.05f, 0.08f, 0.12f, 0.90f);
        style.Colors[ImPlotCol_LegendBorder] = ImVec4(0.3f, 0.5f, 0.7f, 0.3f);
        style.Colors[ImPlotCol_LegendText] = ImVec4(0.9f, 0.95f, 1.0f, 1.0f);
        style.Colors[ImPlotCol_AxisText] = ImVec4(0.8f, 0.9f, 1.0f, 1.0f);
        style.Colors[ImPlotCol_AxisGrid] = ImVec4(0.3f, 0.5f, 0.7f, 0.2f);
        style.Colors[ImPlotCol_TitleText] = ImVec4(0.9f, 0.95f, 1.0f, 1.0f);
    } else {
        // Light theme colors
        style.Colors[ImPlotCol_PlotBg] = ImVec4(0.95f, 0.96f, 0.98f, 0.95f);
        style.Colors[ImPlotCol_PlotBorder] = ImVec4(0.2f, 0.3f, 0.5f, 0.3f);
        style.Colors[ImPlotCol_LegendBg] = ImVec4(0.98f, 0.98f, 0.99f, 0.90f);
        style.Colors[ImPlotCol_LegendBorder] = ImVec4(0.2f, 0.3f, 0.5f, 0.3f);
        style.Colors[ImPlotCol_LegendText] = ImVec4(0.1f, 0.1f, 0.15f, 1.0f);
        style.Colors[ImPlotCol_AxisText] = ImVec4(0.15f, 0.2f, 0.3f, 1.0f);
        style.Colors[ImPlotCol_AxisGrid] = ImVec4(0.2f, 0.3f, 0.5f, 0.15f);
        style.Colors[ImPlotCol_TitleText] = ImVec4(0.1f, 0.15f, 0.2f, 1.0f);
    }
}

void DataVisualization::renderGraphsTab() {
    if (!m_initialized) {
        ImGui::TextDisabled("Visualization not initialized");
        return;
    }

    ImGui::Spacing();

    // Chart type sub-tabs
    renderChartTabs();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Render selected chart
    switch (m_selectedChart) {
        case ChartType::MassRadius:
            renderMassRadiusDiagram();
            break;
        case ChartType::HabitableZone:
            renderHabitableZonePlot();
            break;
        case ChartType::OrbitalParams:
            renderOrbitalParametersChart();
            break;
        case ChartType::Temperature:
            renderTemperatureHistogram();
            break;
        case ChartType::Atmosphere:
            renderAtmosphereChart();
            break;
        case ChartType::Comparison:
            renderComparisonChart();
            break;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Options checkboxes
    ImGui::Checkbox("Uncertainties", &m_showUncertainties);
    ImGui::SameLine();
    ImGui::Checkbox("Data Source", &m_showDataSources);
    ImGui::SameLine();
    ImGui::Checkbox("Highlight", &m_highlightCurrent);
}

void DataVisualization::renderChartTabs() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));

    struct TabInfo {
        const char* label;
        ChartType type;
    };

    TabInfo tabs[] = {
        {"Mass-Rad", ChartType::MassRadius},
        {"Hab Zone", ChartType::HabitableZone},
        {"Orbital", ChartType::OrbitalParams},
        {"Temp", ChartType::Temperature},
        {"Atmos", ChartType::Atmosphere},
        {"Compare", ChartType::Comparison}
    };

    for (int i = 0; i < 6; ++i) {
        if (i > 0) ImGui::SameLine();

        bool selected = (m_selectedChart == tabs[i].type);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 0.7f));
        }

        if (ImGui::SmallButton(tabs[i].label)) {
            m_selectedChart = tabs[i].type;
        }

        if (selected) {
            ImGui::PopStyleColor();
        }
    }

    ImGui::PopStyleVar();
}

void DataVisualization::renderMassRadiusDiagram() {
    ImGui::TextDisabled("Mass-Radius Diagram");

    float availHeight = std::max(150.0f, ImGui::GetContentRegionAvail().y - 30.0f);

    if (ImPlot::BeginPlot("##MassRadius", ImVec2(-1, availHeight),
                          ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Mass (M Earth)", "Radius (R Earth)",
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoGridLines,
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoGridLines);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
        ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.01, 1000.0, ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.1, 20.0, ImPlotCond_Once);

        // Draw composition curves
        const int nPoints = 50;
        double masses[nPoints], ironR[nPoints], earthR[nPoints], waterR[nPoints], gasR[nPoints];

        for (int i = 0; i < nPoints; ++i) {
            double m = 0.01 * std::pow(100000.0, static_cast<double>(i) / (nPoints - 1));
            masses[i] = m;
            ironR[i] = composition::ironRadius(m);
            earthR[i] = composition::earthLikeRadius(m);
            waterR[i] = composition::waterRadius(m);
            gasR[i] = composition::gasEnvelopeRadius(m);
        }

        ImPlot::SetNextLineStyle(ImVec4(0.6f, 0.3f, 0.2f, 0.6f), 1.5f);
        ImPlot::PlotLine("Pure Iron", masses, ironR, nPoints);

        ImPlot::SetNextLineStyle(ImVec4(0.4f, 0.6f, 0.3f, 0.6f), 1.5f);
        ImPlot::PlotLine("Earth-like", masses, earthR, nPoints);

        ImPlot::SetNextLineStyle(ImVec4(0.3f, 0.5f, 0.8f, 0.6f), 1.5f);
        ImPlot::PlotLine("Pure Water", masses, waterR, nPoints);

        ImPlot::SetNextLineStyle(ImVec4(0.8f, 0.7f, 0.3f, 0.6f), 1.5f);
        ImPlot::PlotLine("H/He Envelope", masses, gasR, nPoints);

        // Plot reference planets
        for (const auto& ref : m_referencePlanets) {
            if (ref.mass_earth > 0 && ref.radius_earth > 0) {
                ImVec4 color;
                float size;

                if (ref.is_reference) {
                    color = ImVec4(1.0f, 1.0f, 1.0f, 0.9f);
                    size = 6.0f;
                } else {
                    color = ImVec4(0.5f, 0.8f, 0.5f, 0.7f);
                    size = 4.0f;
                }

                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, size, color, 1.0f, color);
                double m = ref.mass_earth, r = ref.radius_earth;
                ImPlot::PlotScatter(ref.name.c_str(), &m, &r, 1);
            }
        }

        // Plot current planet (highlighted)
        if (m_hasCurrentPlanet && m_highlightCurrent) {
            const auto& d = m_currentPlanet;
            if (d.mass_earth.hasValue() && d.radius_earth.hasValue()) {
                double m = d.mass_earth.value;
                double r = d.radius_earth.value;

                // Color based on data source
                ImVec4 color;
                if (m_showDataSources && d.mass_earth.isAIInferred()) {
                    color = ImVec4(0.4f, 0.85f, 1.0f, 1.0f);  // Cyan for AI
                } else {
                    color = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);  // Orange for current
                }

                ImPlot::SetNextMarkerStyle(ImPlotMarker_Diamond, 10.0f, color, 2.0f, color);
                ImPlot::PlotScatter(d.name.c_str(), &m, &r, 1);

                // Error bars if showing uncertainties
                if (m_showUncertainties) {
                    if (d.mass_earth.uncertainty.has_value()) {
                        double err = d.mass_earth.uncertainty.value();
                        ImPlot::SetNextErrorBarStyle(color, 1.5f);
                        ImPlot::PlotErrorBars("##mass_err", &m, &r, &err, 1);
                    }
                    if (d.radius_earth.uncertainty.has_value()) {
                        double err = d.radius_earth.uncertainty.value();
                        ImPlot::SetNextErrorBarStyle(color, 1.5f);
                        ImPlot::PlotErrorBars("##radius_err", &m, &r, &err, 1, ImPlotErrorBarsFlags_Horizontal);
                    }
                }
            }
        }

        ImPlot::EndPlot();
    }
}

void DataVisualization::renderHabitableZonePlot() {
    ImGui::TextDisabled("Habitable Zone Analysis");

    float availHeight = std::max(150.0f, ImGui::GetContentRegionAvail().y - 30.0f);

    if (ImPlot::BeginPlot("##HabZone", ImVec2(-1, availHeight), ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Insolation (S Earth)", "Eq. Temperature (K)",
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert,
                          ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.1, 100.0, ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 100.0, 500.0, ImPlotCond_Once);

        // Draw habitable zone bands
        // Conservative HZ: 0.95 - 1.37 S_Earth (Kopparapu et al. 2013)
        double consLeft[] = {0.95, 0.95};
        double consRight[] = {1.37, 1.37};
        double consY[] = {100.0, 500.0};
        ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
        ImPlot::SetNextFillStyle(ImVec4(0.2f, 0.8f, 0.3f, 0.25f));
        ImPlot::PlotShaded("Conservative HZ", consLeft, consRight, consY, 2);
        ImPlot::PopStyleVar();

        // Optimistic HZ: 0.38 - 1.76 S_Earth
        double optLeft[] = {0.38, 0.38};
        double optRight[] = {1.76, 1.76};
        ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.15f);
        ImPlot::SetNextFillStyle(ImVec4(0.8f, 0.8f, 0.2f, 0.15f));
        ImPlot::PlotShaded("Optimistic HZ", optLeft, optRight, consY, 2);
        ImPlot::PopStyleVar();

        // Plot reference planets
        for (const auto& ref : m_referencePlanets) {
            if (ref.insolation_flux > 0 && ref.equilibrium_temp_k > 0) {
                ImVec4 color;
                float size;

                if (ref.is_reference) {
                    color = ImVec4(1.0f, 1.0f, 1.0f, 0.9f);
                    size = 6.0f;
                } else {
                    color = ImVec4(0.5f, 0.8f, 0.5f, 0.7f);
                    size = 4.0f;
                }

                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, size, color, 1.0f, color);
                double insol = ref.insolation_flux, temp = ref.equilibrium_temp_k;
                ImPlot::PlotScatter(ref.name.c_str(), &insol, &temp, 1);
            }
        }

        // Plot current planet
        if (m_hasCurrentPlanet && m_highlightCurrent) {
            const auto& d = m_currentPlanet;
            if (d.insolation_flux.hasValue() && d.equilibrium_temp_k.hasValue()) {
                double insol = d.insolation_flux.value;
                double temp = d.equilibrium_temp_k.value;

                ImVec4 color = m_showDataSources && d.insolation_flux.isAIInferred()
                    ? ImVec4(0.4f, 0.85f, 1.0f, 1.0f)
                    : ImVec4(1.0f, 0.6f, 0.2f, 1.0f);

                ImPlot::SetNextMarkerStyle(ImPlotMarker_Diamond, 10.0f, color, 2.0f, color);
                ImPlot::PlotScatter(d.name.c_str(), &insol, &temp, 1);
            }
        }

        ImPlot::EndPlot();
    }

    // Show ESI if available
    if (m_hasCurrentPlanet && m_currentPlanet.earth_similarity_index.hasValue()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Earth Similarity Index: ");
        ImGui::SameLine();
        ImGui::Text("%.2f", m_currentPlanet.earth_similarity_index.value);
    }
}

void DataVisualization::renderOrbitalParametersChart() {
    ImGui::TextDisabled("Orbital Parameters");

    float availHeight = std::max(150.0f, ImGui::GetContentRegionAvail().y - 30.0f);

    if (ImPlot::BeginPlot("##Orbital", ImVec2(-1, availHeight), ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Semi-Major Axis (AU)", "Orbital Period (days)",
                          ImPlotAxisFlags_AutoFit,
                          ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
        ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.001, 100.0, ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.1, 100000.0, ImPlotCond_Once);

        // Kepler's third law reference line (for solar-mass star)
        const int nPoints = 50;
        double sma[nPoints], period[nPoints];
        for (int i = 0; i < nPoints; ++i) {
            double a = 0.001 * std::pow(100000.0, static_cast<double>(i) / (nPoints - 1));
            sma[i] = a;
            period[i] = 365.25 * std::pow(a, 1.5);  // P = a^1.5 years (Kepler)
        }

        ImPlot::SetNextLineStyle(ImVec4(0.5f, 0.5f, 0.5f, 0.4f), 1.0f);
        ImPlot::PlotLine("Kepler (1 M_sun)", sma, period, nPoints);

        // Plot reference planets
        for (const auto& ref : m_referencePlanets) {
            if (ref.semi_major_axis_au > 0 && ref.orbital_period_days > 0) {
                ImVec4 color = ref.is_reference
                    ? ImVec4(1.0f, 1.0f, 1.0f, 0.9f)
                    : ImVec4(0.5f, 0.8f, 0.5f, 0.7f);
                float size = ref.is_reference ? 6.0f : 4.0f;

                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, size, color, 1.0f, color);
                double a = ref.semi_major_axis_au, p = ref.orbital_period_days;
                ImPlot::PlotScatter(ref.name.c_str(), &a, &p, 1);
            }
        }

        // Plot current planet
        if (m_hasCurrentPlanet && m_highlightCurrent) {
            const auto& d = m_currentPlanet;
            if (d.semi_major_axis_au.hasValue() && d.orbital_period_days.hasValue()) {
                double a = d.semi_major_axis_au.value;
                double p = d.orbital_period_days.value;

                ImVec4 color = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Diamond, 10.0f, color, 2.0f, color);
                ImPlot::PlotScatter(d.name.c_str(), &a, &p, 1);
            }
        }

        ImPlot::EndPlot();
    }

    // Show eccentricity if available
    if (m_hasCurrentPlanet && m_currentPlanet.eccentricity.hasValue()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Eccentricity: ");
        ImGui::SameLine();
        ImGui::Text("%.3f", m_currentPlanet.eccentricity.value);
    }
}

void DataVisualization::renderTemperatureHistogram() {
    ImGui::TextDisabled("Temperature Distribution");

    // Create histogram bins of known exoplanet temperatures
    // (Reference data for context)
    struct TempBin {
        double center;
        int count;
    };

    // Simulated distribution based on known exoplanet population
    TempBin bins[] = {
        {150, 45}, {200, 89}, {250, 156}, {300, 312}, {350, 287},
        {400, 198}, {500, 245}, {600, 312}, {700, 198}, {800, 234},
        {900, 178}, {1000, 256}, {1200, 345}, {1400, 267}, {1600, 189},
        {1800, 145}, {2000, 98}, {2200, 67}, {2500, 45}
    };
    const int nBins = sizeof(bins) / sizeof(bins[0]);

    double centers[nBins], counts[nBins];
    for (int i = 0; i < nBins; ++i) {
        centers[i] = bins[i].center;
        counts[i] = static_cast<double>(bins[i].count);
    }

    float availHeight = std::max(150.0f, ImGui::GetContentRegionAvail().y - 30.0f);

    if (ImPlot::BeginPlot("##TempHist", ImVec2(-1, availHeight), ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Eq. Temperature (K)", "Count",
                          ImPlotAxisFlags_AutoFit,
                          ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, 100.0, 3000.0, ImPlotCond_Once);

        // Plot histogram bars
        ImPlot::SetNextFillStyle(ImVec4(0.3f, 0.5f, 0.8f, 0.6f));
        ImPlot::PlotBars("Known Exoplanets", centers, counts, nBins, 80.0);

        // Mark habitable temperature range
        double habRange[] = {273 - 40, 273 + 50};  // ~233K to 323K
        ImPlot::SetNextLineStyle(ImVec4(0.2f, 0.8f, 0.3f, 0.8f), 2.0f);
        ImPlot::PlotInfLines("Habitable Range", habRange, 2);

        // Mark current planet temperature
        if (m_hasCurrentPlanet && m_highlightCurrent) {
            const auto& d = m_currentPlanet;
            if (d.equilibrium_temp_k.hasValue()) {
                double temp = d.equilibrium_temp_k.value;
                ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), 3.0f);
                ImPlot::PlotInfLines(d.name.c_str(), &temp, 1);
            }
        }

        ImPlot::EndPlot();
    }

    // Legend
    ImGui::Spacing();
    ImGui::TextDisabled("Green lines: Habitable range (233-323 K)");
    if (m_hasCurrentPlanet && m_currentPlanet.equilibrium_temp_k.hasValue()) {
        ImGui::TextDisabled("Orange line: Current planet (%.0f K)",
                           m_currentPlanet.equilibrium_temp_k.value);
    }
}

std::vector<DataVisualization::AtmosphereComponent>
DataVisualization::parseAtmosphere(const std::string& jsonStr) {
    std::vector<AtmosphereComponent> components;

    if (jsonStr.empty()) return components;

    try {
        auto j = nlohmann::json::parse(jsonStr);
        if (j.is_object()) {
            for (auto& [key, value] : j.items()) {
                if (value.is_number()) {
                    components.push_back({key, value.get<double>()});
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_WARN("Failed to parse atmosphere JSON: {}", e.what());
    }

    // Sort by percentage descending
    std::sort(components.begin(), components.end(),
              [](const auto& a, const auto& b) { return a.percentage > b.percentage; });

    return components;
}

void DataVisualization::renderAtmosphereChart() {
    ImGui::TextDisabled("Atmospheric Composition");

    if (!m_hasCurrentPlanet || !m_currentPlanet.atmosphere_composition.hasValue()) {
        ImGui::Spacing();
        ImGui::TextDisabled("No atmosphere data available");

        // Show detected molecules if available
        ImGui::Spacing();
        ImGui::TextDisabled("Surface Pressure: ");
        ImGui::SameLine();
        if (m_hasCurrentPlanet && m_currentPlanet.surface_pressure_atm.hasValue()) {
            ImGui::Text("%.2f atm", m_currentPlanet.surface_pressure_atm.value);
            if (m_currentPlanet.surface_pressure_atm.isAIInferred()) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.75f, 1.0f, 0.90f));
                ImGui::Text(" AI");
                ImGui::PopStyleColor();
            }
        } else {
            ImGui::Text("--");
        }
        return;
    }

    auto components = parseAtmosphere(m_currentPlanet.atmosphere_composition.value);
    if (components.empty()) {
        ImGui::TextDisabled("Could not parse atmosphere data");
        return;
    }

    float availHeight = std::max(120.0f, ImGui::GetContentRegionAvail().y - 60.0f);

    if (ImPlot::BeginPlot("##Atmosphere", ImVec2(-1, availHeight),
                          ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxes("Percentage (%)", nullptr,
                          ImPlotAxisFlags_AutoFit,
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 100.0, ImPlotCond_Always);

        // Gas colors
        auto getGasColor = [](const std::string& gas) -> ImVec4 {
            if (gas == "N2" || gas == "nitrogen") return ImVec4(0.7f, 0.7f, 0.9f, 0.8f);
            if (gas == "O2" || gas == "oxygen") return ImVec4(0.3f, 0.6f, 1.0f, 0.8f);
            if (gas == "CO2" || gas == "carbon_dioxide") return ImVec4(0.9f, 0.5f, 0.2f, 0.8f);
            if (gas == "H2O" || gas == "water") return ImVec4(0.3f, 0.8f, 0.9f, 0.8f);
            if (gas == "H2" || gas == "hydrogen") return ImVec4(0.4f, 0.9f, 0.9f, 0.8f);
            if (gas == "He" || gas == "helium") return ImVec4(0.9f, 0.9f, 0.5f, 0.8f);
            if (gas == "CH4" || gas == "methane") return ImVec4(0.6f, 0.4f, 0.2f, 0.8f);
            if (gas == "Ar" || gas == "argon") return ImVec4(0.6f, 0.3f, 0.7f, 0.8f);
            return ImVec4(0.5f, 0.5f, 0.5f, 0.8f);
        };

        // Plot horizontal bars for each gas
        for (size_t i = 0; i < components.size() && i < 8; ++i) {
            const auto& comp = components[i];
            double y = static_cast<double>(i);
            double pct = comp.percentage;

            ImPlot::SetNextFillStyle(getGasColor(comp.gas));
            ImPlot::PlotBars(comp.gas.c_str(), &pct, 1, 0.6, y, ImPlotBarsFlags_Horizontal);
        }

        ImPlot::EndPlot();
    }

    // Legend below chart
    ImGui::Spacing();
    for (size_t i = 0; i < components.size() && i < 6; ++i) {
        if (i > 0) ImGui::SameLine(0, 15);
        const auto& comp = components[i];
        ImGui::TextDisabled("%s: %.1f%%", comp.gas.c_str(), comp.percentage);
    }

    // Data source indicator
    if (m_showDataSources && m_currentPlanet.atmosphere_composition.isAIInferred()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.75f, 1.0f, 0.90f));
        ImGui::TextDisabled("Data source: AI-inferred");
        ImGui::PopStyleColor();
    }
}

void DataVisualization::renderComparisonChart() {
    ImGui::TextDisabled("Comparative Analysis");

    if (!m_hasCurrentPlanet) {
        ImGui::Spacing();
        ImGui::TextDisabled("Select a planet to compare");
        return;
    }

    // Find similar planets from reference list
    std::vector<const ReferenceExoplanet*> similar;
    const auto& d = m_currentPlanet;

    for (const auto& ref : m_referencePlanets) {
        // Similarity criteria
        bool typeSimilar = d.planet_type.hasValue() &&
                          (ref.planet_type == d.planet_type.value ||
                           (ref.planet_type == "Rocky" && d.planet_type.value == "Super-Earth") ||
                           (ref.planet_type == "Super-Earth" && d.planet_type.value == "Rocky"));

        bool massSimilar = d.mass_earth.hasValue() && ref.mass_earth > 0 &&
                          std::abs(std::log10(d.mass_earth.value) - std::log10(ref.mass_earth)) < 0.5;

        if (typeSimilar || massSimilar) {
            similar.push_back(&ref);
        }
    }

    // Limit to top 5
    if (similar.size() > 5) similar.resize(5);

    // Create radar/bar comparison
    float availHeight = std::max(120.0f, ImGui::GetContentRegionAvail().y - 80.0f);

    if (ImPlot::BeginPlot("##Compare", ImVec2(-1, availHeight), ImPlotFlags_NoMouseText)) {
        // Normalize parameters for comparison
        std::vector<std::string> labels = {"Mass", "Radius", "Temp", "ESI"};
        ImPlot::SetupAxes(nullptr, nullptr,
                          ImPlotAxisFlags_AutoFit,
                          ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 2.0, ImPlotCond_Once);

        const int nParams = 4;
        double positions[nParams] = {0, 1, 2, 3};

        // Current planet bars
        if (d.mass_earth.hasValue() && d.radius_earth.hasValue()) {
            double currentVals[nParams];
            currentVals[0] = d.mass_earth.hasValue() ? std::min(2.0, d.mass_earth.value / 5.0) : 0;
            currentVals[1] = d.radius_earth.hasValue() ? std::min(2.0, d.radius_earth.value / 2.0) : 0;
            currentVals[2] = d.equilibrium_temp_k.hasValue() ? std::min(2.0, d.equilibrium_temp_k.value / 300.0) : 0;
            currentVals[3] = d.earth_similarity_index.hasValue() ? d.earth_similarity_index.value * 2.0 : 0;

            ImPlot::SetNextFillStyle(ImVec4(1.0f, 0.6f, 0.2f, 0.8f));
            ImPlot::PlotBars(d.name.c_str(), positions, currentVals, nParams, 0.15);
        }

        // Similar planets bars
        double offset = 0.18;
        for (size_t i = 0; i < similar.size(); ++i) {
            const auto* ref = similar[i];
            double refVals[nParams];
            refVals[0] = std::min(2.0, ref->mass_earth / 5.0);
            refVals[1] = std::min(2.0, ref->radius_earth / 2.0);
            refVals[2] = std::min(2.0, ref->equilibrium_temp_k / 300.0);
            refVals[3] = ref->earth_similarity_index * 2.0;

            double offsetPos[nParams];
            for (int j = 0; j < nParams; ++j) {
                offsetPos[j] = positions[j] + offset * static_cast<double>(i + 1);
            }

            ImVec4 color = ref->is_reference
                ? ImVec4(0.9f, 0.9f, 0.9f, 0.7f)
                : ImVec4(0.5f, 0.8f, 0.5f, 0.7f);

            ImPlot::SetNextFillStyle(color);
            ImPlot::PlotBars(ref->name.c_str(), offsetPos, refVals, nParams, 0.15);
        }

        ImPlot::EndPlot();
    }

    // Axis labels
    ImGui::Spacing();
    ImGui::TextDisabled("0: Mass (x5 M_E)  1: Radius (x2 R_E)  2: Temp (x300 K)  3: ESI (x0.5)");

    // Similar planets list
    if (!similar.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Similar planets:");
        for (size_t i = 0; i < similar.size(); ++i) {
            ImGui::SameLine();
            ImGui::Text("%s%s", similar[i]->name.c_str(), i < similar.size() - 1 ? "," : "");
        }
    }
}

}  // namespace astrocore

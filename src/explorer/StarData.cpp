#include "explorer/StarData.hpp"
#include "core/Logger.hpp"

#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace astrocore {

// ── Fast CSV field access without allocating strings ─────────────────────────

static float parseFloat(const char* s, size_t len, float def) {
    if (len == 0) return def;
    // strtof needs null-terminated string; use a small stack buffer
    char buf[64];
    size_t n = std::min(len, size_t(63));
    std::memcpy(buf, s, n);
    buf[n] = '\0';
    char* end;
    float val = std::strtof(buf, &end);
    return (end == buf) ? def : val;
}

// ── Header parsing (only done once, fine to allocate) ────────────────────────

static int findColumn(const char* data, size_t headerEnd, const char* name) {
    size_t nameLen = std::strlen(name);
    size_t pos = 0;
    int col = 0;
    while (pos < headerEnd) {
        size_t fStart = pos, fLen = 0;
        if (data[pos] == '"') {
            fStart = pos + 1;
            pos++;
            while (pos < headerEnd && data[pos] != '"') pos++;
            fLen = pos - fStart;
            if (pos < headerEnd) pos++; // skip closing quote
            if (pos < headerEnd && data[pos] == ',') pos++;
        } else {
            while (pos < headerEnd && data[pos] != ',') pos++;
            fLen = pos - fStart;
            if (pos < headerEnd) pos++; // skip comma
        }
        if (fLen == nameLen && std::memcmp(data + fStart, name, nameLen) == 0) {
            return col;
        }
        col++;
    }
    return -1;
}

// Ballesteros (2012) approximation: B-V color index to RGB
glm::vec3 StarData::bvToRGB(float bv) {
    bv = std::clamp(bv, -0.4f, 2.0f);

    float t = 4600.0f * (1.0f / (0.92f * bv + 1.7f) + 1.0f / (0.92f * bv + 0.62f));

    float x, y;
    if (t <= 4000.0f) {
        x = -0.2661239e9f / (t*t*t) - 0.2343589e6f / (t*t) + 0.8776956e3f / t + 0.179910f;
    } else {
        x = -3.0258469e9f / (t*t*t) + 2.1070379e6f / (t*t) + 0.2226347e3f / t + 0.240390f;
    }

    if (t <= 2222.0f) {
        y = -1.1063814f * x * x * x - 1.34811020f * x * x + 2.18555832f * x - 0.20219683f;
    } else if (t <= 4000.0f) {
        y = -0.9549476f * x * x * x - 1.37418593f * x * x + 2.09137015f * x - 0.16748867f;
    } else {
        y = 3.0817580f * x * x * x - 5.87338670f * x * x + 3.75112997f * x - 0.37001483f;
    }

    float Y = 1.0f;
    float X = (Y / y) * x;
    float Z = (Y / y) * (1.0f - x - y);

    float r =  3.2406f * X - 1.5372f * Y - 0.4986f * Z;
    float g = -0.9689f * X + 1.8758f * Y + 0.0415f * Z;
    float b =  0.0557f * X - 0.2040f * Y + 1.0570f * Z;

    r = std::max(0.0f, r);
    g = std::max(0.0f, g);
    b = std::max(0.0f, b);
    float mx = std::max({r, g, b, 0.001f});
    return glm::vec3(r / mx, g / mx, b / mx);
}

bool StarData::load(const std::string& csvPath) {
    // Read entire file into memory at once
    std::ifstream file(csvPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open star catalog: {}", csvPath);
        return false;
    }

    auto fileSize = file.tellg();
    file.seekg(0);
    std::vector<char> fileData(static_cast<size_t>(fileSize));
    file.read(fileData.data(), fileSize);
    file.close();

    const char* data = fileData.data();
    size_t len = fileData.size();

    // Find end of header line
    size_t headerEnd = 0;
    while (headerEnd < len && data[headerEnd] != '\n' && data[headerEnd] != '\r') headerEnd++;

    // Find column indices from header
    int colX    = findColumn(data, headerEnd, "x");
    int colY    = findColumn(data, headerEnd, "y");
    int colZ    = findColumn(data, headerEnd, "z");
    int colMag  = findColumn(data, headerEnd, "mag");
    int colCI   = findColumn(data, headerEnd, "ci");
    int colDist = findColumn(data, headerEnd, "dist");
    int colName = findColumn(data, headerEnd, "proper");
    int colSpect = findColumn(data, headerEnd, "spect");
    int colCon  = findColumn(data, headerEnd, "con");

    if (colX < 0 || colY < 0 || colZ < 0 || colMag < 0) {
        LOG_ERROR("Star catalog missing required columns (x, y, z, mag)");
        return false;
    }

    // Find max column we need so we know how many fields to scan per line
    int maxCol = std::max({colX, colY, colZ, colMag, colCI, colDist, colName, colSpect, colCon});

    m_vertices.clear();
    m_records.clear();
    m_vertices.reserve(120000);
    m_records.reserve(120000);

    // Skip header line(s)
    size_t pos = headerEnd;
    while (pos < len && (data[pos] == '\n' || data[pos] == '\r')) pos++;

    // Parse each data line
    while (pos < len) {
        size_t lineStart = pos;

        // Find end of this line
        size_t lineEnd = pos;
        while (lineEnd < len && data[lineEnd] != '\n' && data[lineEnd] != '\r') lineEnd++;

        size_t lineLen = lineEnd - lineStart;
        if (lineLen == 0) {
            pos = lineEnd;
            while (pos < len && (data[pos] == '\n' || data[pos] == '\r')) pos++;
            continue;
        }

        // Extract needed fields by column index — scan fields sequentially
        size_t fieldPos = lineStart;
        float xv = 0, yv = 0, zv = 0, mag = 20.0f, ci = 0.65f, dist = 0.0f;
        const char* namePtr = nullptr; size_t nameLen = 0;
        const char* spectPtr = nullptr; size_t spectLen = 0;
        const char* conPtr = nullptr; size_t conLen = 0;

        for (int col = 0; col <= maxCol && fieldPos < lineEnd; col++) {
            // Find this field's start and extent
            size_t fStart = fieldPos;
            size_t fLen = 0;

            if (fieldPos < lineEnd && data[fieldPos] == '"') {
                fStart = fieldPos + 1;
                fieldPos++;
                while (fieldPos < lineEnd && data[fieldPos] != '"') fieldPos++;
                fLen = fieldPos - fStart;
                if (fieldPos < lineEnd) fieldPos++; // skip closing quote
                if (fieldPos < lineEnd && data[fieldPos] == ',') fieldPos++;
            } else {
                fStart = fieldPos;
                while (fieldPos < lineEnd && data[fieldPos] != ',') fieldPos++;
                fLen = fieldPos - fStart;
                if (fieldPos < lineEnd) fieldPos++; // skip comma
            }

            if (col == colX)    xv   = parseFloat(data + fStart, fLen, 0.0f);
            else if (col == colY)    yv   = parseFloat(data + fStart, fLen, 0.0f);
            else if (col == colZ)    zv   = parseFloat(data + fStart, fLen, 0.0f);
            else if (col == colMag)  mag  = parseFloat(data + fStart, fLen, 20.0f);
            else if (col == colCI)   ci   = parseFloat(data + fStart, fLen, 0.65f);
            else if (col == colDist) dist = parseFloat(data + fStart, fLen, 0.0f);
            else if (col == colName)  { namePtr = data + fStart; nameLen = fLen; }
            else if (col == colSpect) { spectPtr = data + fStart; spectLen = fLen; }
            else if (col == colCon)   { conPtr = data + fStart; conLen = fLen; }
        }

        glm::vec3 position(xv, yv, zv);

        if (dist >= 99999.0f) {
            float l = glm::length(position);
            if (l > 0.001f) position = glm::normalize(position) * 10000.0f;
        }

        StarVertex sv;
        sv.position  = position;
        sv.magnitude = mag;
        sv.color     = bvToRGB(ci);
        m_vertices.push_back(sv);

        StarRecord rec;
        rec.magnitude = mag;
        rec.distance  = dist;
        rec.position  = position;
        // Only allocate strings for non-empty fields
        if (nameLen > 0)  rec.name = std::string(namePtr, nameLen);
        if (spectLen > 0) rec.spectralType = std::string(spectPtr, spectLen);
        if (conLen > 0)   rec.constellation = std::string(conPtr, conLen);
        m_records.push_back(std::move(rec));

        // Advance past line ending
        pos = lineEnd;
        while (pos < len && (data[pos] == '\n' || data[pos] == '\r')) pos++;
    }

    LOG_INFO("Loaded {} stars from {}", m_vertices.size(), csvPath);
    return true;
}

StarInfo StarData::findNearest(const glm::vec3& pos) const {
    StarInfo info{};
    float bestDist2 = 1e30f;

    for (size_t i = 0; i < m_records.size(); i++) {
        glm::vec3 d = m_records[i].position - pos;
        float dist2 = glm::dot(d, d);
        if (dist2 < bestDist2) {
            bestDist2 = dist2;
            const auto& rec = m_records[i];
            info.name          = rec.name;
            info.spectralType  = rec.spectralType;
            info.constellation = rec.constellation;
            info.magnitude     = rec.magnitude;
            info.distance      = std::sqrt(dist2);
            info.position      = rec.position;
        }
    }

    return info;
}

} // namespace astrocore

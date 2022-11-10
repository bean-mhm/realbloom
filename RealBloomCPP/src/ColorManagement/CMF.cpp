#include "CMF.h"

std::shared_ptr<CmfTable> CmfTable::load(const std::string& filename)
{
    try
    {
        // Make a CMF table
        std::shared_ptr<CmfTable> table = std::make_shared<CmfTable>();

        // Read the CSV file
        rapidcsv::Document doc(filename, rapidcsv::LabelParams(-1, -1));

        if (doc.GetColumnCount() < 4)
            throw std::exception("At least 4 columns are required (wavelength, X, Y, Z).");

        // Get the wavelength info
        std::vector<float> wavelengths = doc.GetColumn<float>(0);

        // This is kind of arbitrary but what would you use
        // a CMF table with less than 10 entries for
        if (wavelengths.size() < 10)
            throw std::exception(
                stringFormat("At least 10 entries are needed! (%u)", wavelengths.size()).c_str()
            );

        table->m_count = wavelengths.size();
        table->m_start = wavelengths[0];
        table->m_end = wavelengths[wavelengths.size() - 1];
        table->m_step = wavelengths[1] - wavelengths[0];

        // start + (step * (count - 1)) == end
        if (fabsf((table->m_start + (table->m_step * (float)(table->m_count - 1))) - table->m_end) >= table->m_step)
            throw std::exception(stringFormat(
                "Wavelength entries must be equally distanced from each other. "
                "(%u, %.3f, %.3f, %.3f)",
                table->m_count,
                table->m_start,
                table->m_end,
                table->m_step
            ).c_str());

        // Get XYZ values
        table->m_valuesX = doc.GetColumn<float>(1);
        table->m_valuesY = doc.GetColumn<float>(2);
        table->m_valuesZ = doc.GetColumn<float>(3);

        // Check if they all have the same number of entries
        // Pretty long conditions in this function, eh
        if (!((table->m_valuesX.size() == table->m_count) &&
            (table->m_valuesY.size() == table->m_count) &&
            (table->m_valuesZ.size() == table->m_count)))
        {
            throw std::exception(stringFormat(
                "The first 4 columns must have the same number of entries. "
                "(%u, %u, %u, %u)",
                table->m_count,
                table->m_valuesX.size(),
                table->m_valuesY.size(),
                table->m_valuesZ.size()
            ).c_str());
        }

        return table;
    } catch (const std::exception& e)
    {
        throw std::exception(printErr(__FUNCTION__, e.what()).c_str());
    }
}

inline float lerp(float a, float b, float t)
{
    return a + ((b - a) * t);
}

std::array<float, 3> CmfTable::sample(float wavelength) const
{
    // Out of range values

    if (wavelength < m_start)
        return { m_valuesX[0], m_valuesY[0], m_valuesZ[0] };

    if (wavelength > m_end)
        return { m_valuesX[m_count - 1], m_valuesY[m_count - 1], m_valuesZ[m_count - 1] };

    // How far along the range
    float pos = (wavelength - m_start) / (m_end - m_start);

    // Fuzzy index
    float fidx = pos * (float)(m_count - 1);

    // Integer index
    size_t idx = (size_t)floorf(fidx);

    // Fractional part
    float offset = fidx - (float)idx;

    // If it's the last entry
    if (idx >= (m_count - 1))
        return { m_valuesX[m_count - 1], m_valuesY[m_count - 1], m_valuesZ[m_count - 1] };

    // Sample linearly from idx to idx+1
    float smpX = lerp(m_valuesX[idx], m_valuesX[idx + 1], offset);
    float smpY = lerp(m_valuesY[idx], m_valuesY[idx + 1], offset);
    float smpZ = lerp(m_valuesZ[idx], m_valuesZ[idx + 1], offset);

    return { smpX, smpY, smpZ };
}

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <memory>

#include "rapidcsv/rapidcsv.h"

#include "../Utils/Misc.h"

class CmfTable
{
private:
    size_t m_count = 0;
    float m_start = 0;
    float m_end = 0;
    float m_step = 0;

    std::vector<float> m_valuesX;
    std::vector<float> m_valuesY;
    std::vector<float> m_valuesZ;

public:
    size_t getCount() const;
    float getStart() const;
    float getEnd() const;
    float getStep() const;
    std::array<float, 3> sample(float wavelength) const;

    static std::shared_ptr<CmfTable> load(const std::string& filename);
};

class CMF
{};

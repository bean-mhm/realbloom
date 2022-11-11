#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <filesystem>

#include "rapidcsv/rapidcsv.h"

#include "../Utils/Misc.h"

constexpr const char* CMF_DIR = "cmf";

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
    float getRange() const;
    float getStep() const;
    std::array<float, 3> sample(float wavelength) const;

    CmfTable(const std::string& filename);
};

struct CmfTableInfo
{
    std::string name;
    std::string path;

    CmfTableInfo(const std::string& name = "", const std::string& path = "");
};

class CMF
{
private:
    struct CmfVars
    {
        std::vector<CmfTableInfo> tables;
        std::shared_ptr<CmfTable> activeTable = nullptr;
        CmfTableInfo activeTableInfo;
        std::string activeTableDetails = "";
    };
    static CmfVars* S_VARS;
    static SimpleState S_STATE;

    static void retrieveTables();

public:
    CMF() = delete;
    CMF(const CMF&) = delete;
    CMF& operator= (const CMF&) = delete;

    static bool init();
    static void cleanUp();

    static const std::vector<CmfTableInfo>& getAvailableTables();

    static std::shared_ptr<CmfTable> getActiveTable();
    static CmfTableInfo getActiveTableInfo();
    static std::string getActiveTableDetails();
    static void setActiveTable(const CmfTableInfo& tableInfo);

    static bool hasTable();
    static bool ok();
    static std::string getError();
};

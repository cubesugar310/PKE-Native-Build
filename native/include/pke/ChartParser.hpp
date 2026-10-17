#pragma once
#include "pke/Chart.hpp"
#include <filesystem>

namespace pke {
enum class NoteMappingMode { PsychSection, AbsoluteNoteHalf };
struct ChartParserOptions { NoteMappingMode mappingMode{NoteMappingMode::PsychSection}; };
Chart ParseChart(const std::filesystem::path& jsonPath, ChartParserOptions options = {});
}

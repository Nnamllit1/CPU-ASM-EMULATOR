#pragma once

#include <vector>

namespace console {

std::vector<float> resampleAudioForSpeed(const std::vector<float>& samples, double speed);

} // namespace console

#include "playback.h"

#include <algorithm>
#include <cmath>

namespace console {

std::vector<float> resampleAudioForSpeed(const std::vector<float>& samples, double speed) {
	if (samples.empty() || !std::isfinite(speed) || speed <= 0.0) return {};
	if (speed == 1.0) return samples;
	const size_t outputSize = std::max<size_t>(1, static_cast<size_t>(std::llround(samples.size() / speed)));
	std::vector<float> output(outputSize);
	for (size_t index = 0; index < output.size(); ++index) {
		const double sourcePosition = std::min<double>(index * speed, samples.size() - 1);
		const size_t first = static_cast<size_t>(sourcePosition);
		const size_t second = std::min(first + 1, samples.size() - 1);
		const float fraction = static_cast<float>(sourcePosition - first);
		output[index] = samples[first] + (samples[second] - samples[first]) * fraction;
	}
	return output;
}

} // namespace console

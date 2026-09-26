#include "game_face/core/blendshapes.h"

#include "game_face/core/face_tracker.h"

#include <cmath>

namespace game_face {

std::optional<std::size_t> blendshapeIndex(std::string_view name)
{
    for (std::size_t i = 0; i < kBlendshapeNames.size(); ++i) {
        if (kBlendshapeNames[i] == name)
            return i;
    }
    return std::nullopt;
}

BlendshapeScores toScores(const FaceFrame& frame)
{
    BlendshapeScores scores{};
    for (std::size_t i = 0; i < frame.blendshapes.size(); ++i) {
        const auto& shape = frame.blendshapes[i];
        // MediaPipe returns them in table order; fall back to a lookup if not.
        std::optional<std::size_t> index;
        if (i < kBlendshapeNames.size() && kBlendshapeNames[i] == shape.name)
            index = i;
        else
            index = blendshapeIndex(shape.name);
        if (index)
            scores[*index] = std::floor(shape.score * 100.0 + 0.5);  // JS Math.round
    }
    return scores;
}

} // namespace game_face

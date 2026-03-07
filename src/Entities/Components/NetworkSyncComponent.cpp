// src/Entities/Components/NetworkSyncComponent.cpp

#include "NetworkSyncComponent.h"
#include <nlohmann/json.hpp>

// ---------------------------------------------------------------------------
// pushSnapshot
// ---------------------------------------------------------------------------

void NetworkSyncComponent::pushSnapshot(const Network::TransformSnapshot& snapshot) {
    // Discard out-of-order packets (sequence numbers must be monotonically
    // increasing; duplicates and late arrivals are silently dropped).
    if (!buffer_.empty() && snapshot.sequenceNumber <= buffer_.back().sequenceNumber) {
        return;
    }

    buffer_.push_back(snapshot);

    // Clamp the buffer to maxBufferSize_ to prevent memory bloat.
    while (buffer_.size() > maxBufferSize_) {
        buffer_.pop_front();
    }

    // The first time we accumulate two snapshots we synchronise our playback
    // clock to the server timeline.
    if (!started_ && buffer_.size() >= 2) {
        renderTime_ = buffer_.back().timestamp;
        started_    = true;
    }
}

// ---------------------------------------------------------------------------
// initFromJson — data-driven component initialisation
// ---------------------------------------------------------------------------

void NetworkSyncComponent::initFromJson(const nlohmann::json& j) {
    if (j.contains("interpolation_delay"))
        interpolationDelay_ = j["interpolation_delay"].get<float>();
    if (j.contains("max_buffer_size"))
        maxBufferSize_ = static_cast<std::size_t>(j["max_buffer_size"].get<int>());
}

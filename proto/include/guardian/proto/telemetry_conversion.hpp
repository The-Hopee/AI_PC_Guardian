#pragma once

#include <optional>

#include <guardian/model/telemetry_event.hpp>
#include <guardian/v1/telemetry.pb.h>

namespace guardian::proto {

std::optional<::guardian::v1::TelemetryEvent> to_proto(
    const ::guardian::model::TelemetryEvent& event);

std::optional<::guardian::model::TelemetryEvent> from_proto(
    const ::guardian::v1::TelemetryEvent& message);

}  // namespace guardian::proto

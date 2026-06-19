#include "app/state.h"

MonotubState g;

const char* fanReasonName(FanReason reason) {
  switch (reason) {
    case FanReason::Manual:
      return "manual";
    case FanReason::Vent:
      return "vent";
    case FanReason::Off:
    default:
      return "off";
  }
}

const char* mistReasonName(MistReason reason) {
  switch (reason) {
    case MistReason::HumidityAutomation:
      return "humidity_automation";
    case MistReason::Manual:
    default:
      return "manual";
  }
}

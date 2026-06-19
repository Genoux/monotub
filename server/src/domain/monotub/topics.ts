export const TOPIC_PREFIX = 'monotub';

export const monotubTopics = {
  connected: `${TOPIC_PREFIX}/connected`,
  fanState: `${TOPIC_PREFIX}/fan/state`,
  mistState: `${TOPIC_PREFIX}/mist/state`,
  status: `${TOPIC_PREFIX}/status`,
  statusRequest: `${TOPIC_PREFIX}/status/request`,
  relayResync: `${TOPIC_PREFIX}/relay/resync`,
  fanSet: `${TOPIC_PREFIX}/fan/set`,
  mistSet: `${TOPIC_PREFIX}/mist/set`,
  automationEnabledSet: `${TOPIC_PREFIX}/automation/enabled/set`,
  humidityDryBelowSet: `${TOPIC_PREFIX}/humidity/dry_below/set`,
  humidityBandLowSet: `${TOPIC_PREFIX}/humidity/band_low/set`,
  humidityBandHighSet: `${TOPIC_PREFIX}/humidity/band_high/set`,
  humidityWetAboveSet: `${TOPIC_PREFIX}/humidity/wet_above/set`,
  bandHoldSet: `${TOPIC_PREFIX}/humidity/band_hold_seconds/set`,
  ventOnSecondsSet: `${TOPIC_PREFIX}/fan/vent_on_seconds/set`,
  ventOffSecondsSet: `${TOPIC_PREFIX}/fan/vent_off_seconds/set`,
  simulationEnabledSet: `${TOPIC_PREFIX}/simulation/enabled/set`,
  simulationHumiditySet: `${TOPIC_PREFIX}/simulation/humidity/set`,
  log: `${TOPIC_PREFIX}/log`,
  subscribeAll: `${TOPIC_PREFIX}/#`,
} as const;

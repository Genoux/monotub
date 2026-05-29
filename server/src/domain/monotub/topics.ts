export const TOPIC_PREFIX = 'monotub';

export const monotubTopics = {
  connected: `${TOPIC_PREFIX}/connected`,
  fanState: `${TOPIC_PREFIX}/fan/state`,
  mistState: `${TOPIC_PREFIX}/mist/state`,
  status: `${TOPIC_PREFIX}/status`,
  routineSet: `${TOPIC_PREFIX}/routine/set`,
  fanSet: `${TOPIC_PREFIX}/fan/set`,
  onSecondsSet: `${TOPIC_PREFIX}/fan/on_seconds/set`,
  offSecondsSet: `${TOPIC_PREFIX}/fan/off_seconds/set`,
  mistSet: `${TOPIC_PREFIX}/mist/set`,
  automationEnabledSet: `${TOPIC_PREFIX}/automation/enabled/set`,
  humidityLowSet: `${TOPIC_PREFIX}/humidity/low_threshold/set`,
  humidityHighSet: `${TOPIC_PREFIX}/humidity/high_threshold/set`,
  humidityLowPersistSet: `${TOPIC_PREFIX}/humidity/low_persist_seconds/set`,
  humidityHighPersistSet: `${TOPIC_PREFIX}/humidity/high_persist_seconds/set`,
  mistPulseSet: `${TOPIC_PREFIX}/mist/pulse_seconds/set`,
  correctiveFanSet: `${TOPIC_PREFIX}/fan/corrective_seconds/set`,
  mistCooldownSet: `${TOPIC_PREFIX}/mist/cooldown_seconds/set`,
  fanCooldownSet: `${TOPIC_PREFIX}/fan/cooldown_seconds/set`,
  log: `${TOPIC_PREFIX}/log`,
  subscribeAll: `${TOPIC_PREFIX}/#`,
} as const;

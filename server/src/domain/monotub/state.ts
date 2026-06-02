import type { MonotubState } from './types';

const MAX_LOG_ENTRIES = 50;

export const createInitialState = (): MonotubState => ({
  connected: null,
  fan: null,
  mist: null,
  status: null,
  logs: [],
  desired: {
    routine: null,
    manual_fan: null,
    on_seconds: null,
    off_seconds: null,
    mist: null,
    automation_enabled: null,
    humidity_low_threshold: null,
    humidity_high_threshold: null,
    humidity_low_persist_seconds: null,
    humidity_high_persist_seconds: null,
    mist_pulse_seconds: null,
    corrective_fan_seconds: null,
    mist_cooldown_seconds: null,
    fan_cooldown_seconds: null,
    updatedAt: null,
  },
  lastSeenAt: null,
  lastTransitionAt: null,
  nextTransitionAt: null,
});

const syncEmptyDesiredFromStatus = (state: MonotubState): void => {
  if (!state.status) return;

  state.desired = {
    routine: state.desired.routine ?? state.status.routine,
    manual_fan: state.desired.manual_fan ?? state.status.manual_fan ?? null,
    on_seconds: state.desired.on_seconds ?? state.status.on_seconds,
    off_seconds: state.desired.off_seconds ?? state.status.off_seconds,
    mist:
      state.status.automation_enabled === true
        ? state.status.automation_mist ?? state.status.mist ?? null
        : state.desired.mist ??
          state.status.manual_mist ??
          state.status.mist ??
          null,
    automation_enabled:
      state.desired.automation_enabled ??
      state.status.automation_enabled ??
      null,
    humidity_low_threshold:
      state.desired.humidity_low_threshold ??
      state.status.humidity_low_threshold ??
      null,
    humidity_high_threshold:
      state.desired.humidity_high_threshold ??
      state.status.humidity_high_threshold ??
      null,
    humidity_low_persist_seconds:
      state.desired.humidity_low_persist_seconds ??
      state.status.humidity_low_persist_seconds ??
      null,
    humidity_high_persist_seconds:
      state.desired.humidity_high_persist_seconds ??
      state.status.humidity_high_persist_seconds ??
      null,
    mist_pulse_seconds:
      state.desired.mist_pulse_seconds ?? state.status.mist_pulse_seconds ?? null,
    corrective_fan_seconds:
      state.desired.corrective_fan_seconds ??
      state.status.corrective_fan_seconds ??
      null,
    mist_cooldown_seconds:
      state.desired.mist_cooldown_seconds ??
      state.status.mist_cooldown_seconds ??
      null,
    fan_cooldown_seconds:
      state.desired.fan_cooldown_seconds ??
      state.status.fan_cooldown_seconds ??
      null,
    updatedAt: state.desired.updatedAt,
  };
};

export const setDesiredRoutine = (
  state: MonotubState,
  routine: boolean,
  updatedAt = new Date().toISOString(),
): void => {
  state.desired.routine = routine;
  state.desired.updatedAt = updatedAt;
};

export const setDesiredManualFan = (
  state: MonotubState,
  manualFan: boolean,
  updatedAt = new Date().toISOString(),
): void => {
  state.desired.manual_fan = manualFan;
  state.desired.updatedAt = updatedAt;
};

export const setDesiredOnSeconds = (
  state: MonotubState,
  seconds: number,
  updatedAt = new Date().toISOString(),
): void => {
  state.desired.on_seconds = seconds;
  state.desired.updatedAt = updatedAt;
};

export const setDesiredOffSeconds = (
  state: MonotubState,
  seconds: number,
  updatedAt = new Date().toISOString(),
): void => {
  state.desired.off_seconds = seconds;
  state.desired.updatedAt = updatedAt;
};

export const setDesiredMist = (
  state: MonotubState,
  mist: boolean,
  updatedAt = new Date().toISOString(),
): void => {
  state.desired.mist = mist;
  state.desired.updatedAt = updatedAt;
};

type AutomationDesiredPatch = Partial<
  Pick<
    MonotubState['desired'],
    | 'automation_enabled'
    | 'humidity_low_threshold'
    | 'humidity_high_threshold'
    | 'humidity_low_persist_seconds'
    | 'humidity_high_persist_seconds'
    | 'mist_pulse_seconds'
    | 'corrective_fan_seconds'
    | 'mist_cooldown_seconds'
    | 'fan_cooldown_seconds'
  >
>;

export const setDesiredAutomationSettings = (
  state: MonotubState,
  patch: AutomationDesiredPatch,
  updatedAt = new Date().toISOString(),
): void => {
  state.desired = {
    ...state.desired,
    ...patch,
    updatedAt,
  };
};

/** Align countdown with ESP32 schedule timer (not stale fan-edge timestamps). */
export const syncScheduleTransitions = (
  state: MonotubState,
  receivedAt: string,
): void => {
  const status = state.status;
  if (!status?.routine || status.routine_paused) {
    state.nextTransitionAt = null;
    return;
  }
  if (status.next_scheduled_fae_s == null) {
    return;
  }

  const remainingSec = status.next_scheduled_fae_s;
  const phaseOn = status.scheduled_fae_on ?? state.fan === 'ON';
  const phaseSec = phaseOn ? status.on_seconds : status.off_seconds;
  const received = new Date(receivedAt);
  const elapsedSec = Math.max(0, phaseSec - remainingSec);

  state.nextTransitionAt = new Date(
    received.getTime() + remainingSec * 1000,
  ).toISOString();
  state.lastTransitionAt = new Date(
    received.getTime() - elapsedSec * 1000,
  ).toISOString();
};

export const recomputeNextTransition = (
  state: MonotubState,
  receivedAt?: string,
): void => {
  if (!state.status?.routine || state.status.routine_paused) {
    state.nextTransitionAt = null;
    return;
  }
  if (
    state.status.next_scheduled_fae_s != null &&
    receivedAt != null
  ) {
    syncScheduleTransitions(state, receivedAt);
    return;
  }

  if (!state.lastTransitionAt || !state.fan) {
    state.nextTransitionAt = null;
    return;
  }
  const durationSec =
    state.fan === 'ON' ? state.status.on_seconds : state.status.off_seconds;
  const next = new Date(
    new Date(state.lastTransitionAt).getTime() + durationSec * 1000,
  );
  state.nextTransitionAt = next.toISOString();
};

export const applyMqttMessage = (
  state: MonotubState,
  topic: string,
  message: string,
  receivedAt: string,
  topics: {
    connected: string;
    fanState: string;
    mistState: string;
    status: string;
    log: string;
  },
): void => {
  state.lastSeenAt = receivedAt;

  switch (topic) {
    case topics.connected:
      state.connected = message as MonotubState['connected'];
      break;
    case topics.fanState: {
      const newFan = message as MonotubState['fan'];
      if (!state.status?.routine && newFan !== state.fan) {
        state.lastTransitionAt = receivedAt;
      }
      state.fan = newFan;
      recomputeNextTransition(state, receivedAt);
      break;
    }
    case topics.mistState:
      state.mist = message as MonotubState['mist'];
      break;
    case topics.log:
      state.logs = [
        { message, receivedAt },
        ...state.logs,
      ].slice(0, MAX_LOG_ENTRIES);
      break;
    case topics.status: {
      let parsed: MonotubState['status'];
      try {
        parsed = JSON.parse(message) as MonotubState['status'];
      } catch {
        console.error('[status] invalid JSON (truncated MQTT payload?)');
        break;
      }
      state.status = parsed;
      if (parsed?.mist != null) {
        state.mist = parsed.mist ? 'ON' : 'OFF';
      }
      if (parsed?.fan != null) {
        const newFan = parsed.fan ? 'ON' : 'OFF';
        if (!parsed.routine && newFan !== state.fan) {
          state.lastTransitionAt = receivedAt;
        }
        state.fan = newFan;
      }
      try {
        syncEmptyDesiredFromStatus(state);
        syncScheduleTransitions(state, receivedAt);
        recomputeNextTransition(state, receivedAt);
      } catch (err) {
        console.error('[status] schedule sync failed:', err);
      }
      break;
    }
  }
};

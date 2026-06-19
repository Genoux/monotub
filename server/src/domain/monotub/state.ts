import type { MonotubState } from './types';

const MAX_LOG_ENTRIES = 50;

export const createInitialState = (): MonotubState => ({
  connected: null,
  fan: null,
  mist: null,
  status: null,
  logs: [],
  desired: {
    manual_fan: null,
    mist: null,
    automation_enabled: null,
    rh_dry_below: null,
    rh_band_low: null,
    rh_band_high: null,
    rh_wet_above: null,
    band_hold_seconds: null,
    vent_on_seconds: null,
    vent_off_seconds: null,
    updatedAt: null,
  },
  lastSeenAt: null,
  lastTransitionAt: null,
  nextTransitionAt: null,
});

const syncEmptyDesiredFromStatus = (state: MonotubState): void => {
  if (!state.status) return;

  state.desired = {
    manual_fan: state.desired.manual_fan ?? state.status.manual_fan ?? null,
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
    rh_dry_below:
      state.desired.rh_dry_below ?? state.status.rh_dry_below ?? null,
    rh_band_low:
      state.desired.rh_band_low ?? state.status.rh_band_low ?? null,
    rh_band_high:
      state.desired.rh_band_high ?? state.status.rh_band_high ?? null,
    rh_wet_above:
      state.desired.rh_wet_above ?? state.status.rh_wet_above ?? null,
    band_hold_seconds:
      state.desired.band_hold_seconds ?? state.status.band_hold_seconds ?? null,
    vent_on_seconds:
      state.desired.vent_on_seconds ?? state.status.vent_on_seconds ?? null,
    vent_off_seconds:
      state.desired.vent_off_seconds ?? state.status.vent_off_seconds ?? null,
    updatedAt: state.desired.updatedAt,
  };
};

export const setDesiredManualFan = (
  state: MonotubState,
  manualFan: boolean,
  updatedAt = new Date().toISOString(),
): void => {
  state.desired.manual_fan = manualFan;
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
    | 'rh_dry_below'
    | 'rh_band_low'
    | 'rh_band_high'
    | 'rh_wet_above'
    | 'band_hold_seconds'
    | 'vent_on_seconds'
    | 'vent_off_seconds'
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

const dutyPhaseSeconds = (status: NonNullable<MonotubState['status']>, on: boolean): number => {
  if (on) return status.vent_on_seconds ?? 5;
  return status.vent_off_seconds ?? 90;
};

/** Align countdown with the ESP32 duty-cycle timer (not stale fan-edge timestamps). */
export const syncScheduleTransitions = (
  state: MonotubState,
  receivedAt: string,
): void => {
  const status = state.status;
  const remainingSec = status?.next_duty_toggle_s;
  if (!status || remainingSec == null || remainingSec <= 0) {
    state.nextTransitionAt = null;
    return;
  }

  const phaseOn = status.duty_fan_on ?? state.fan === 'ON';
  const phaseSec = dutyPhaseSeconds(status, phaseOn);
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
  const status = state.status;
  if (!status) {
    state.nextTransitionAt = null;
    return;
  }
  if (status.next_duty_toggle_s != null && receivedAt != null) {
    syncScheduleTransitions(state, receivedAt);
    return;
  }

  if (!status.duty_fan_on || !state.lastTransitionAt || !state.fan) {
    state.nextTransitionAt = null;
    return;
  }
  const durationSec = dutyPhaseSeconds(status, state.fan === 'ON');
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
      if (!state.status?.automation_enabled && newFan !== state.fan) {
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
        if (!parsed.automation_enabled && newFan !== state.fan) {
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

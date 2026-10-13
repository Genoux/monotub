export type FanReason = 'off' | 'manual' | 'fae' | 'vent';

export type MistReason = 'manual' | 'humidity_automation';

export type HumidityBand =
  | 'unknown'
  | 'too_dry'
  | 'recovering'
  | 'healthy'
  | 'drying'
  | 'too_wet';

export type FanStatus = {
  fan: boolean;
  mist?: boolean;
  manual_fan?: boolean;
  manual_mist?: boolean;
  automation_mist?: boolean;
  fan_reason?: FanReason;
  mist_reason?: MistReason;
  uptime_s: number;
  rssi: number;
  ip?: string;
  humidity_percent?: number;
  temperature_c?: number;
  environment_ok?: boolean;
  smoothed_humidity_percent?: number;
  automation_enabled?: boolean;
  simulation_enabled?: boolean;
  simulated_humidity_percent?: number;
  // Band model.
  rh_dry_below?: number;
  rh_band_low?: number;
  rh_band_high?: number;
  rh_wet_above?: number;
  band_hold_seconds?: number;
  current_band?: HumidityBand;
  band_hold_remaining_s?: number;
  // Fan cycle.
  vent_on_seconds?: number;
  vent_off_seconds?: number;
  duty_fan_on?: boolean;
  next_duty_toggle_s?: number;
};

export type DesiredFanState = {
  manual_fan: boolean | null;
  mist: boolean | null;
  automation_enabled: boolean | null;
  rh_dry_below: number | null;
  rh_band_low: number | null;
  rh_band_high: number | null;
  rh_wet_above: number | null;
  band_hold_seconds: number | null;
  vent_on_seconds: number | null;
  vent_off_seconds: number | null;
  updatedAt: string | null;
};

export type LogEntry = {
  message: string;
  receivedAt: string;
};

export type MonotubState = {
  connected: 'online' | 'offline' | null;
  fan: 'ON' | 'OFF' | null;
  mist: 'ON' | 'OFF' | null;
  status: FanStatus | null;
  desired: DesiredFanState;
  logs: LogEntry[];
  lastSeenAt: string | null;
  lastTransitionAt: string | null;
  nextTransitionAt: string | null;
};

export type MonotubStateView = MonotubState & {
  /** A non-retained message arrived within OFFLINE_AFTER_SECONDS and no LWT. */
  online: boolean;
  /** Seconds since the last message from the board, null if none yet. */
  age_s: number | null;
};

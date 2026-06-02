export type FanReason =
  | 'off'
  | 'manual'
  | 'scheduled_fae'
  | 'humidity_correction';

export type MistReason = 'manual' | 'humidity_automation';

export type FanStatus = {
  on_seconds: number;
  off_seconds: number;
  routine: boolean;
  routine_paused?: boolean;
  routine_pause_reason?:
    | 'none'
    | 'unknown'
    | 'automation_off'
    | 'humidity_low'
    | 'humidity_high';
  fan: boolean;
  manual_fan?: boolean;
  manual_mist?: boolean;
  automation_mist?: boolean;
  mist?: boolean;
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
  fan_reason?: FanReason;
  mist_reason?: MistReason;
  humidity_low_threshold?: number;
  humidity_high_threshold?: number;
  humidity_soft_low_threshold?: number;
  humidity_soft_high_threshold?: number;
  humidity_hard_high_threshold?: number;
  humidity_low_persist_seconds?: number;
  humidity_high_persist_seconds?: number;
  mist_pulse_seconds?: number;
  corrective_fan_seconds?: number;
  mist_cooldown_seconds?: number;
  fan_cooldown_seconds?: number;
  mist_cooldown_remaining_s?: number;
  fan_cooldown_remaining_s?: number;
  humidity_high_persist_remaining_s?: number;
  mist_pulse_remaining_s?: number;
  corrective_fan_remaining_s?: number;
  scheduled_fae_on_seconds?: number;
  scheduled_fae_off_seconds?: number;
  scheduled_fae_on?: boolean;
  next_scheduled_fae_s?: number;
};

export type DesiredFanState = {
  routine: boolean | null;
  manual_fan: boolean | null;
  on_seconds: number | null;
  off_seconds: number | null;
  mist: boolean | null;
  automation_enabled: boolean | null;
  humidity_low_threshold: number | null;
  humidity_high_threshold: number | null;
  humidity_low_persist_seconds: number | null;
  humidity_high_persist_seconds: number | null;
  mist_pulse_seconds: number | null;
  corrective_fan_seconds: number | null;
  mist_cooldown_seconds: number | null;
  fan_cooldown_seconds: number | null;
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

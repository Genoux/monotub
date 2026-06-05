import { monotubTopics } from '../domain/monotub/topics';
import type { FanStatus } from '../domain/monotub/types';

const QUIET_TOPICS = new Set<string>([monotubTopics.statusRequest]);

let lastFanState: string | null = null;
let lastMistState: string | null = null;
let lastStatusSummary: string | null = null;

const statusSummary = (status: FanStatus): string => {
	const rh = status.smoothed_humidity_percent ?? status.humidity_percent;
	const rhText = rh == null ? 'RH —' : `RH ${rh.toFixed(1)}%`;
	const routine = status.routine ? 'routine ON' : 'routine OFF';
	const paused = status.routine_paused ? ` paused(${status.routine_pause_reason})` : '';
	return [
		rhText,
		`fan ${status.fan ? 'ON' : 'OFF'}(${status.fan_reason ?? '?'})`,
		`mist ${status.mist ? 'ON' : 'OFF'}`,
		routine + paused,
		status.simulation_enabled ? `SIM ${status.simulated_humidity_percent?.toFixed(0) ?? '?'}%` : '',
	]
		.filter(Boolean)
		.join(' · ');
};

export const shouldLogMqttSend = (topic: string): boolean =>
	!QUIET_TOPICS.has(topic);

export const logMqttSend = (topic: string, message: string): void => {
	if (shouldLogMqttSend(topic)) {
		console.log(`[send] ${topic} = ${message}`);
	}
};

export const logMqttRecv = (topic: string, message: string): void => {
	if (QUIET_TOPICS.has(topic)) return;

	if (topic === monotubTopics.fanState) {
		if (message === lastFanState) return;
		lastFanState = message;
		console.log(`[recv] fan ${message}`);
		return;
	}

	if (topic === monotubTopics.mistState) {
		if (message === lastMistState) return;
		lastMistState = message;
		console.log(`[recv] mist ${message}`);
		return;
	}

	if (topic === monotubTopics.status) {
		let parsed: FanStatus;
		try {
			parsed = JSON.parse(message) as FanStatus;
		} catch {
			console.log('[recv] status (invalid JSON)');
			return;
		}
		const summary = statusSummary(parsed);
		if (summary === lastStatusSummary) return;
		lastStatusSummary = summary;
		console.log(`[status] ${summary}`);
		return;
	}

	if (topic === monotubTopics.log) {
		console.log(`[esp32] ${message}`);
		return;
	}

	if (topic === monotubTopics.connected) {
		console.log(`[recv] device ${message}`);
		return;
	}

	console.log(`[recv] ${topic} = ${message}`);
};

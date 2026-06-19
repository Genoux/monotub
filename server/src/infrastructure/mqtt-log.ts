import { monotubTopics } from '../domain/monotub/topics';
import type { FanStatus } from '../domain/monotub/types';

const QUIET_TOPICS = new Set<string>([monotubTopics.statusRequest]);

// Track the last "interesting state" — only log when one of these changes.
let lastBand: string | null = null;
let lastFanReason: string | null = null;
let lastMist: boolean | null = null;
let lastFanState: string | null = null;
let lastMistState: string | null = null;

const stateKey = (status: FanStatus): string =>
	[
		status.current_band ?? '?',
		status.fan_reason ?? '?',
		String(status.mist ?? false),
	].join('|');

const rhText = (status: FanStatus): string => {
	const rh = status.smoothed_humidity_percent ?? status.humidity_percent;
	return rh == null ? 'RH —' : `RH ${rh.toFixed(1)}%`;
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
		console.log(`[fan] ${message}`);
		return;
	}

	if (topic === monotubTopics.mistState) {
		if (message === lastMistState) return;
		lastMistState = message;
		console.log(`[mist] ${message}`);
		return;
	}

	if (topic === monotubTopics.status) {
		let status: FanStatus;
		try {
			status = JSON.parse(message) as FanStatus;
		} catch {
			console.log('[status] invalid JSON');
			return;
		}

		const key = stateKey(status);
		const bandChanged = status.current_band !== lastBand;
		const fanChanged = status.fan_reason !== lastFanReason;
		const mistChanged = status.mist !== lastMist;

		if (!bandChanged && !fanChanged && !mistChanged) return;

		lastBand = status.current_band ?? null;
		lastFanReason = status.fan_reason ?? null;
		lastMist = status.mist ?? null;

		const parts = [rhText(status), `band ${status.current_band ?? '?'}`];
		if (fanChanged) parts.push(`fan ${status.fan ? 'ON' : 'OFF'}(${status.fan_reason ?? '?'})`);
		if (mistChanged) parts.push(`mist ${status.mist ? 'ON' : 'OFF'}`);
		if (status.simulation_enabled) parts.push(`SIM`);

		console.log(`[status] ${parts.join(' · ')}`);
		return;
	}

	if (topic === monotubTopics.log) {
		console.log(`[esp32] ${message}`);
		return;
	}

	if (topic === monotubTopics.connected) {
		console.log(`[device] ${message}`);
		return;
	}

	console.log(`[recv] ${topic} = ${message}`);
};

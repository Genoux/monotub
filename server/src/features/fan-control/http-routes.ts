import type { Express, Request, Response } from "express";
import {
	setDesiredAutomationSettings,
	setDesiredManualFan,
	setDesiredMist,
	setDesiredOffSeconds,
	setDesiredOnSeconds,
	setDesiredRoutine,
} from "../../domain/monotub/state";
import { monotubTopics } from "../../domain/monotub/topics";
import type { MonotubState } from "../../domain/monotub/types";

type FanControlHttpDeps = {
	brokerUrl: string;
	getState: () => MonotubState;
	publish: (topic: string, message: string) => void;
};

const automationFieldTopics = {
	automation_enabled: monotubTopics.automationEnabledSet,
	humidity_low_threshold: monotubTopics.humidityLowSet,
	humidity_high_threshold: monotubTopics.humidityHighSet,
	humidity_low_persist_seconds: monotubTopics.humidityLowPersistSet,
	humidity_high_persist_seconds: monotubTopics.humidityHighPersistSet,
	mist_pulse_seconds: monotubTopics.mistPulseSet,
	corrective_fan_seconds: monotubTopics.correctiveFanSet,
	mist_cooldown_seconds: monotubTopics.mistCooldownSet,
	fan_cooldown_seconds: monotubTopics.fanCooldownSet,
} as const;

type AutomationField = keyof typeof automationFieldTopics;

const numberFields = new Set<AutomationField>([
	"humidity_low_threshold",
	"humidity_high_threshold",
	"humidity_low_persist_seconds",
	"humidity_high_persist_seconds",
	"mist_pulse_seconds",
	"corrective_fan_seconds",
	"mist_cooldown_seconds",
	"fan_cooldown_seconds",
]);

const validateAutomationPatch = (
	body: unknown,
): Partial<Pick<MonotubState["desired"], AutomationField>> | null => {
	if (!body || typeof body !== "object") return null;

	const patch: Partial<Pick<MonotubState["desired"], AutomationField>> = {};
	for (const field of Object.keys(automationFieldTopics) as AutomationField[]) {
		if (!(field in body)) continue;

		const value = (body as Record<string, unknown>)[field];
		if (field === "automation_enabled") {
			if (typeof value !== "boolean") return null;
			patch.automation_enabled = value;
			continue;
		}

		if (!numberFields.has(field)) return null;
		const numberValue = Number(value);
		if (!Number.isFinite(numberValue) || numberValue <= 0) return null;
		patch[field] = numberValue;
	}
	return patch;
};

export const registerFanControlHttpRoutes = (
	app: Express,
	{ brokerUrl, getState, publish }: FanControlHttpDeps,
): void => {
	app.get("/api", (_req: Request, res: Response) => {
		res.json({
			service: "monotub-mqtt-server",
			broker: brokerUrl,
			state: getState(),
		});
	});

	app.get("/state", (_req: Request, res: Response) => {
		res.json(getState());
	});

	app.post("/sync", (_req: Request, res: Response) => {
		publish(monotubTopics.statusRequest, String(Date.now()));
		res.json({ ok: true });
	});

	app.post("/relay/resync", (_req: Request, res: Response) => {
		publish(monotubTopics.relayResync, String(Date.now()));
		res.json({ ok: true });
	});

	app.post("/simulation", (req: Request, res: Response) => {
		const enabled = req.body?.enabled;
		if (typeof enabled !== "boolean") {
			res.status(400).json({ error: "body.enabled must be boolean" });
			return;
		}
		publish(monotubTopics.simulationEnabledSet, enabled ? "ON" : "OFF");
		res.json({ ok: true });
	});

	app.post("/simulation/humidity", (req: Request, res: Response) => {
		const humidity = Number(req.body?.humidity);
		if (!Number.isFinite(humidity) || humidity <= 0 || humidity > 100) {
			res.status(400).json({ error: "body.humidity must be 0-100" });
			return;
		}
		publish(monotubTopics.simulationHumiditySet, String(humidity));
		res.json({ ok: true });
	});

	app.post("/routine", (req: Request, res: Response) => {
		const enabled = req.body?.enabled;
		if (typeof enabled !== "boolean") {
			res.status(400).json({ error: "body.enabled must be boolean" });
			return;
		}
		publish(monotubTopics.routineSet, enabled ? "ON" : "OFF");
		setDesiredRoutine(getState(), enabled);
		res.json({ ok: true });
	});

	app.post("/fan", (req: Request, res: Response) => {
		if (getState().status?.automation_enabled === true) {
			res.status(409).json({ error: "manual fan control requires automation off" });
			return;
		}
		const enabled = req.body?.enabled;
		if (typeof enabled !== "boolean") {
			res.status(400).json({ error: "body.enabled must be boolean" });
			return;
		}
		publish(monotubTopics.fanSet, enabled ? "ON" : "OFF");
		setDesiredManualFan(getState(), enabled);
		res.json({ ok: true });
	});

	app.post("/fan/toggle", (_req: Request, res: Response) => {
		const state = getState();
		if (state.status?.automation_enabled === true) {
			res.status(409).json({ error: "manual fan control requires automation off" });
			return;
		}
		const current =
			state.desired.manual_fan ??
			state.status?.manual_fan ??
			state.status?.fan ??
			false;
		const next = !current;
		publish(monotubTopics.fanSet, "TOGGLE");
		setDesiredManualFan(state, next);
		res.json({ ok: true, fan: next });
	});

	app.post("/on-seconds", (req: Request, res: Response) => {
		const seconds = Number(req.body?.seconds);
		if (!Number.isFinite(seconds) || seconds <= 0) {
			res.status(400).json({ error: "body.seconds must be > 0" });
			return;
		}
		const roundedSeconds = Math.round(seconds);
		publish(monotubTopics.onSecondsSet, String(roundedSeconds));
		setDesiredOnSeconds(getState(), roundedSeconds);
		res.json({ ok: true });
	});

	app.post("/off-seconds", (req: Request, res: Response) => {
		const seconds = Number(req.body?.seconds);
		if (!Number.isFinite(seconds) || seconds <= 0) {
			res.status(400).json({ error: "body.seconds must be > 0" });
			return;
		}
		const roundedSeconds = Math.round(seconds);
		publish(monotubTopics.offSecondsSet, String(roundedSeconds));
		setDesiredOffSeconds(getState(), roundedSeconds);
		res.json({ ok: true });
	});

	app.post("/mist", (req: Request, res: Response) => {
		const enabled = req.body?.enabled;
		if (typeof enabled !== "boolean") {
			res.status(400).json({ error: "body.enabled must be boolean" });
			return;
		}
		publish(monotubTopics.mistSet, enabled ? "ON" : "OFF");
		setDesiredMist(getState(), enabled);
		res.json({ ok: true });
	});

	app.post("/mist/toggle", (_req: Request, res: Response) => {
		const state = getState();
		const current =
			state.desired.mist ??
			state.status?.manual_mist ??
			state.status?.mist ??
			true;
		const next = !current;
		publish(monotubTopics.mistSet, "TOGGLE");
		setDesiredMist(state, next);
		res.json({ ok: true, mist: next });
	});

	app.post("/automation-settings", (req: Request, res: Response) => {
		const patch = validateAutomationPatch(req.body);
		if (!patch || Object.keys(patch).length === 0) {
			res.status(400).json({
				error: "body must include at least one valid automation setting",
			});
			return;
		}

		(Object.entries(patch) as [AutomationField, boolean | number][]).forEach(
			([field, value]) => {
				const message =
					typeof value === "boolean" ? (value ? "ON" : "OFF") : String(value);
				publish(automationFieldTopics[field], message);
			},
		);
		setDesiredAutomationSettings(getState(), patch);
		res.json({ ok: true });
	});
};

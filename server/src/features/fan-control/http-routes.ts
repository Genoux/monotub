import type { Express, Request, Response } from "express";
import {
	setDesiredAutomationSettings,
	setDesiredManualFan,
	setDesiredMist,
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
	rh_dry_below: monotubTopics.humidityDryBelowSet,
	rh_band_low: monotubTopics.humidityBandLowSet,
	rh_band_high: monotubTopics.humidityBandHighSet,
	rh_wet_above: monotubTopics.humidityWetAboveSet,
	band_hold_seconds: monotubTopics.bandHoldSet,
	vent_on_seconds: monotubTopics.ventOnSecondsSet,
	vent_off_seconds: monotubTopics.ventOffSecondsSet,
} as const;

type AutomationField = keyof typeof automationFieldTopics;

const numberFields = new Set<AutomationField>([
	"rh_dry_below",
	"rh_band_low",
	"rh_band_high",
	"rh_wet_above",
	"band_hold_seconds",
	"vent_on_seconds",
	"vent_off_seconds",
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

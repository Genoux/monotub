import { bridgeApiUrl } from "./bridge-url";
import type { AutomationSettings, MonotubState } from "../types";

const jsonHeaders = { "Content-Type": "application/json" };

export const fetchMonotubState = async (): Promise<MonotubState | null> => {
  try {
    const response = await fetch(bridgeApiUrl("/state"), { cache: "no-store" });
    if (!response.ok) return null;
    return (await response.json()) as MonotubState;
  } catch {
    return null;
  }
};

export const setRoutineEnabled = async (enabled: boolean): Promise<void> => {
  const response = await fetch(bridgeApiUrl("/routine"), {
    method: "POST",
    headers: jsonHeaders,
    body: JSON.stringify({ enabled }),
  });
  if (!response.ok) {
    const error = await response.json().catch(() => ({}));
    throw new Error(
      (error as { error?: string }).error ?? response.statusText,
    );
  }
};

export const setMistEnabled = async (enabled: boolean): Promise<void> => {
  const response = await fetch(bridgeApiUrl("/mist"), {
    method: "POST",
    headers: jsonHeaders,
    body: JSON.stringify({ enabled }),
  });
  if (!response.ok) {
    const error = await response.json().catch(() => ({}));
    throw new Error(
      (error as { error?: string }).error ?? response.statusText,
    );
  }
};

export const setOnSeconds = async (seconds: number): Promise<void> => {
  const response = await fetch(bridgeApiUrl("/on-seconds"), {
    method: "POST",
    headers: jsonHeaders,
    body: JSON.stringify({ seconds }),
  });
  if (!response.ok) {
    const error = await response.json().catch(() => ({}));
    throw new Error(
      (error as { error?: string }).error ?? response.statusText,
    );
  }
};

export const setOffSeconds = async (seconds: number): Promise<void> => {
  const response = await fetch(bridgeApiUrl("/off-seconds"), {
    method: "POST",
    headers: jsonHeaders,
    body: JSON.stringify({ seconds }),
  });
  if (!response.ok) {
    const error = await response.json().catch(() => ({}));
    throw new Error(
      (error as { error?: string }).error ?? response.statusText,
    );
  }
};

export const setAutomationSettings = async (
  settings: AutomationSettings,
): Promise<void> => {
  const response = await fetch(bridgeApiUrl("/automation-settings"), {
    method: "POST",
    headers: jsonHeaders,
    body: JSON.stringify(settings),
  });
  if (!response.ok) {
    const error = await response.json().catch(() => ({}));
    throw new Error(
      (error as { error?: string }).error ?? response.statusText,
    );
  }
};

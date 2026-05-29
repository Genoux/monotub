"use client";

import { useCallback, useEffect, useState } from "react";
import {
  fetchMonotubState,
  setAutomationSettings,
  setMistEnabled,
  setOffSeconds,
  setOnSeconds,
  setRoutineEnabled,
} from "../api/fan-control-api";
import { formatRemaining } from "../lib/format";
import type { MonotubState } from "../types";

const POLL_INTERVAL_MS = 2000;
const COUNTDOWN_TICK_MS = 100;

export type CountdownSnapshot = {
  label: string;
  progressPercent: number;
  fanPhase: "on" | "off" | null;
};

export const useFanState = () => {
  const [state, setState] = useState<MonotubState | null>(null);
  const [unreachable, setUnreachable] = useState(false);
  const [actionError, setActionError] = useState<string | null>(null);
  const [countdown, setCountdown] = useState<CountdownSnapshot>({
    label: "—",
    progressPercent: 0,
    fanPhase: null,
  });

  const refresh = useCallback(async () => {
    const next = await fetchMonotubState();
    if (!next) {
      setUnreachable(true);
      return;
    }
    setUnreachable(false);
    setState(next);
  }, []);

  useEffect(() => {
    const initial = setTimeout(() => void refresh(), 0);
    const interval = setInterval(refresh, POLL_INTERVAL_MS);
    return () => {
      clearTimeout(initial);
      clearInterval(interval);
    };
  }, [refresh]);

  useEffect(() => {
    const tick = () => {
      if (!state?.status?.routine) {
        setCountdown({
          label: state?.status?.routine === false ? "paused" : "—",
          progressPercent: 0,
          fanPhase: null,
        });
        return;
      }
      const phaseOn =
        state.status.scheduled_fae_on ?? state.fan === "ON";

      if (state.nextTransitionAt && state.lastTransitionAt) {
        const now = Date.now();
        const end = new Date(state.nextTransitionAt).getTime();
        const start = new Date(state.lastTransitionAt).getTime();
        const total = Math.max(1, end - start);
        const remaining = Math.max(0, end - now);
        const elapsed = Math.max(0, Math.min(total, now - start));

        setCountdown({
          label: formatRemaining(remaining),
          progressPercent: (elapsed / total) * 100,
          fanPhase: phaseOn ? "on" : "off",
        });
        return;
      }

      const remainingSec = state.status.next_scheduled_fae_s;
      if (remainingSec != null) {
        const duration = phaseOn
          ? state.status.on_seconds
          : state.status.off_seconds;
        const total = Math.max(1, duration * 1000);
        const remaining = remainingSec * 1000;
        const elapsed = Math.max(0, total - remaining);

        setCountdown({
          label: formatRemaining(remaining),
          progressPercent: (elapsed / total) * 100,
          fanPhase: phaseOn ? "on" : "off",
        });
        return;
      }

      setCountdown({ label: "—", progressPercent: 0, fanPhase: null });
    };

    tick();
    const interval = setInterval(tick, COUNTDOWN_TICK_MS);
    return () => clearInterval(interval);
  }, [state]);

  const runAction = useCallback(
    async (action: () => Promise<void>) => {
      setActionError(null);
      try {
        await action();
        await refresh();
      } catch (error) {
        setActionError(
          error instanceof Error ? error.message : "Request failed",
        );
      }
    },
    [refresh],
  );

  const [isStale, setIsStale] = useState(false);

  useEffect(() => {
    const updateStale = () => {
      if (!state?.lastSeenAt) {
        setIsStale(true);
        return;
      }
      setIsStale(Date.now() - new Date(state.lastSeenAt).getTime() > 60_000);
    };
    updateStale();
    const interval = setInterval(updateStale, 5000);
    return () => clearInterval(interval);
  }, [state?.lastSeenAt]);

  return {
    state,
    unreachable,
    isStale,
    actionError,
    countdown,
    refresh,
    enableRoutine: () => runAction(() => setRoutineEnabled(true)),
    disableRoutine: () => runAction(() => setRoutineEnabled(false)),
    enableMist: () => runAction(() => setMistEnabled(true)),
    disableMist: () => runAction(() => setMistEnabled(false)),
    updateOnSeconds: (seconds: number) =>
      runAction(() => setOnSeconds(seconds)),
    updateOffSeconds: (seconds: number) =>
      runAction(() => setOffSeconds(seconds)),
    updateAutomationSettings: (settings: Parameters<typeof setAutomationSettings>[0]) =>
      runAction(() => setAutomationSettings(settings)),
  };
};

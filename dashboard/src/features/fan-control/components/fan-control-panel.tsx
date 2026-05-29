"use client";

import { type ReactNode, useId, useState } from "react";
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Progress } from "@/components/ui/progress";
import { Separator } from "@/components/ui/separator";
import { cn } from "@/lib/utils";
import { useFanState } from "../hooks/use-fan-state";
import { formatUptime, relativeTime } from "../lib/format";
import type { AutomationSettings } from "../types";
import { StatusBadge } from "./status-badge";

const StatusRow = ({
  label,
  children,
}: {
  label: string;
  children: ReactNode;
}) => (
  <div className="flex items-center justify-between gap-3 border-t border-border py-2 first:border-t-0 first:pt-0">
    <span className="font-mono text-xs uppercase tracking-wide text-muted-foreground">
      {label}
    </span>
    <div className="font-mono text-sm tabular-nums">{children}</div>
  </div>
);

const Field = ({
  label,
  inputId,
  children,
}: {
  label: string;
  inputId: string;
  children: ReactNode;
}) => (
  <div className="space-y-2">
    <label
      htmlFor={inputId}
      className="font-mono text-xs uppercase tracking-wide text-muted-foreground"
    >
      {label}
    </label>
    <div className="flex gap-2">{children}</div>
  </div>
);

type AutomationNumberField = {
  key: Exclude<keyof AutomationSettings, "automation_enabled">;
  label: string;
  step: number;
};

const automationNumberFields: AutomationNumberField[] = [
  { key: "humidity_low_threshold", label: "RH low trigger (%)", step: 0.1 },
  { key: "humidity_high_threshold", label: "RH high trigger (%)", step: 0.1 },
  { key: "humidity_low_persist_seconds", label: "Low RH persist (s)", step: 1 },
  { key: "humidity_high_persist_seconds", label: "High RH persist (s)", step: 1 },
  { key: "mist_pulse_seconds", label: "Mist pulse (s)", step: 1 },
  { key: "corrective_fan_seconds", label: "Fan correction (s)", step: 1 },
  { key: "mist_cooldown_seconds", label: "Mist cooldown (s)", step: 1 },
  { key: "fan_cooldown_seconds", label: "Fan cooldown (s)", step: 1 },
];

export const FanControlPanel = () => {
  const {
    state,
    unreachable,
    isStale,
    actionError,
    countdown,
    enableRoutine,
    disableRoutine,
    enableMist,
    disableMist,
    updateOnSeconds,
    updateOffSeconds,
    updateAutomationSettings,
  } = useFanState();

  const onSecondsId = useId();
  const offSecondsId = useId();
  const automationId = useId();

  const [onSecondsDraft, setOnSecondsDraft] = useState<string | null>(null);
  const [offSecondsDraft, setOffSecondsDraft] = useState<string | null>(null);
  const [automationDrafts, setAutomationDrafts] = useState<
    Partial<Record<AutomationNumberField["key"], string>>
  >({});

  const onSecondsInput =
    onSecondsDraft ??
    (state?.status?.on_seconds != null ? String(state.status.on_seconds) : "");
  const offSecondsInput =
    offSecondsDraft ??
    (state?.status?.off_seconds != null ? String(state.status.off_seconds) : "");

  const online = state?.connected === "online";
  const fanOn = state?.fan === "ON" || state?.status?.fan === true;
  const mistOn = state?.mist === "ON" || state?.status?.mist === true;
  const routineOn = state?.status?.routine === true;
  const automationOn = state?.status?.automation_enabled === true;
  const fanReason = state?.status?.fan_reason?.replaceAll("_", " ") ?? "unknown";
  const mistReason =
    state?.status?.mist_reason?.replaceAll("_", " ") ?? "unknown";

  const automationValue = (key: AutomationNumberField["key"]) =>
    automationDrafts[key] ??
    (state?.status?.[key] != null ? String(state.status[key]) : "");

  const updateAutomationDraft = (
    key: AutomationNumberField["key"],
    value: string,
  ) =>
    setAutomationDrafts((drafts) => ({
      ...drafts,
      [key]: value,
    }));

  const submitAutomationField = async (key: AutomationNumberField["key"]) => {
    await updateAutomationSettings({ [key]: Number(automationValue(key)) });
    setAutomationDrafts((drafts) => ({
      ...drafts,
      [key]: undefined,
    }));
  };

  return (
    <div className="mx-auto flex w-full max-w-xl flex-col gap-4">
      <header className="space-y-1">
        <h1 className="font-mono text-xl font-semibold tracking-tight">
          Fan control
        </h1>
        <p
          className={cn(
            "font-mono text-xs text-muted-foreground",
            isStale && "text-[var(--status-offline)]",
          )}
        >
          {unreachable
            ? "Bridge unreachable"
            : `Last message: ${relativeTime(state?.lastSeenAt ?? null)}`}
        </p>
      </header>

      {actionError ? (
        <p className="border border-[var(--status-off)]/40 bg-[var(--status-off)]/10 px-3 py-2 font-mono text-xs text-[var(--status-off)]">
          {actionError}
        </p>
      ) : null}

      <Card className="rounded-none border-border bg-card shadow-none">
        <CardHeader className="border-b border-border pb-3">
          <CardTitle className="font-mono text-sm uppercase tracking-widest">
            Device status
          </CardTitle>
          <CardDescription className="font-mono text-xs">
            Live telemetry from ESP32
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-0 pt-4">
          <StatusRow label="Device">
            <StatusBadge
              label={online ? "online" : (state?.connected ?? "unknown")}
              variant={online ? "on" : "offline"}
            />
          </StatusRow>
          <StatusRow label="Fan">
            <StatusBadge
              label={fanOn ? "on" : "off"}
              variant={fanOn ? "on" : "off"}
            />
          </StatusRow>
          <StatusRow label="Mist">
            <StatusBadge
              label={mistOn ? "on" : "off"}
              variant={mistOn ? "on" : "off"}
            />
          </StatusRow>
          <StatusRow label="Routine">
            <StatusBadge
              label={routineOn ? "on" : "off"}
              variant={routineOn ? "on" : "off"}
            />
          </StatusRow>
          <StatusRow label="Humidity">
            {state?.status?.humidity_percent != null
              ? `${state.status.humidity_percent.toFixed(1)}%`
              : "—"}
          </StatusRow>
          <StatusRow label="Temperature">
            {state?.status?.temperature_c != null
              ? `${state.status.temperature_c.toFixed(1)} °C`
              : "—"}
          </StatusRow>
          <StatusRow label="Sensor">
            <StatusBadge
              label={
                state?.status?.environment_ok === false
                  ? "error"
                  : state?.status?.environment_ok
                    ? "ok"
                    : "unknown"
              }
              variant={
                state?.status?.environment_ok === false
                  ? "off"
                  : state?.status?.environment_ok
                    ? "on"
                    : "unknown"
              }
            />
          </StatusRow>
          <StatusRow label="Automation">
            <StatusBadge
              label={
                state?.status?.automation_enabled == null
                  ? "unknown"
                  : automationOn
                    ? "enabled"
                    : "disabled"
              }
              variant={
                state?.status?.automation_enabled == null
                  ? "unknown"
                  : automationOn
                    ? "on"
                    : "off"
              }
            />
          </StatusRow>
          <StatusRow label="Fan reason">{fanReason}</StatusRow>
          <StatusRow label="Mist reason">{mistReason}</StatusRow>
          <StatusRow label="RH band">
            {state?.status?.humidity_low_threshold != null &&
              state.status.humidity_high_threshold != null
              ? `${state.status.humidity_low_threshold.toFixed(1)}–${state.status.humidity_high_threshold.toFixed(1)}%`
              : "—"}
          </StatusRow>
          <StatusRow label="Cooldowns">
            fan {state?.status?.fan_cooldown_remaining_s ?? "—"}s / mist{" "}
            {state?.status?.mist_cooldown_remaining_s ?? "—"}s
          </StatusRow>
          <StatusRow label="Uptime">
            {formatUptime(state?.status?.uptime_s)}
          </StatusRow>
          <StatusRow label="Signal">
            {state?.status?.rssi != null
              ? `${state.status.rssi} dBm`
              : "—"}
          </StatusRow>
          <StatusRow label="Next change">{countdown.label}</StatusRow>
          <Progress
            value={countdown.progressPercent}
            className={cn(
              "mt-3 h-1 rounded-none bg-border",
              countdown.fanPhase === "on" &&
              "[&>[data-slot=progress-indicator]]:bg-[var(--status-on)]",
              countdown.fanPhase === "off" &&
              "[&>[data-slot=progress-indicator]]:bg-[var(--status-off)]",
            )}
          />
        </CardContent>
      </Card>

      <Card className="rounded-none border-border bg-card shadow-none">
        <CardHeader className="border-b border-border pb-3">
          <CardTitle className="font-mono text-sm uppercase tracking-widest">
            Controls
          </CardTitle>
        </CardHeader>
        <CardContent className="space-y-4 pt-4">
          <div className="grid grid-cols-2 gap-2">
            <Button
              className="rounded-none font-mono text-xs uppercase"
              onClick={enableRoutine}
            >
              Routine on
            </Button>
            <Button
              className="rounded-none font-mono text-xs uppercase"
              variant="destructive"
              onClick={disableRoutine}
            >
              Routine off
            </Button>
          </div>

          <div className="grid grid-cols-2 gap-2">
            <Button
              className="rounded-none font-mono text-xs uppercase"
              onClick={enableMist}
            >
              Mist on
            </Button>
            <Button
              className="rounded-none font-mono text-xs uppercase"
              variant="destructive"
              onClick={disableMist}
            >
              Mist off
            </Button>
          </div>

          <Separator />

          <Field label="Fan on duration (seconds)" inputId={onSecondsId}>
            <Input
              id={onSecondsId}
              type="number"
              min={1}
              step={1}
              value={onSecondsInput}
              onChange={(event) => setOnSecondsDraft(event.target.value)}
              className="rounded-none font-mono"
            />
            <Button
              className="rounded-none font-mono text-xs uppercase"
              onClick={async () => {
                await updateOnSeconds(Number(onSecondsInput));
                setOnSecondsDraft(null);
              }}
            >
              Set
            </Button>
          </Field>

          <Field label="Fan off duration (seconds)" inputId={offSecondsId}>
            <Input
              id={offSecondsId}
              type="number"
              min={1}
              step={1}
              value={offSecondsInput}
              onChange={(event) => setOffSecondsDraft(event.target.value)}
              className="rounded-none font-mono"
            />
            <Button
              className="rounded-none font-mono text-xs uppercase"
              onClick={async () => {
                await updateOffSeconds(Number(offSecondsInput));
                setOffSecondsDraft(null);
              }}
            >
              Set
            </Button>
          </Field>
        </CardContent>
      </Card>

      <Card className="rounded-none border-border bg-card shadow-none">
        <CardHeader className="border-b border-border pb-3">
          <CardTitle className="font-mono text-sm uppercase tracking-widest">
            Automation
          </CardTitle>
          <CardDescription className="font-mono text-xs">
            ESP32-local RH control settings
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-4 pt-4">
          <div className="grid grid-cols-2 gap-2">
            <Button
              className="rounded-none font-mono text-xs uppercase"
              onClick={() =>
                updateAutomationSettings({ automation_enabled: true })
              }
            >
              Auto on
            </Button>
            <Button
              className="rounded-none font-mono text-xs uppercase"
              variant="destructive"
              onClick={() =>
                updateAutomationSettings({ automation_enabled: false })
              }
            >
              Auto off
            </Button>
          </div>

          <div className="grid gap-4">
            {automationNumberFields.map((field) => {
              const inputId = `${automationId}-${field.key}`;
              const value = automationValue(field.key);

              return (
                <Field key={field.key} label={field.label} inputId={inputId}>
                  <Input
                    id={inputId}
                    type="number"
                    min={0}
                    step={field.step}
                    value={value}
                    onChange={(event) =>
                      updateAutomationDraft(field.key, event.target.value)
                    }
                    className="rounded-none font-mono"
                  />
                  <Button
                    className="rounded-none font-mono text-xs uppercase"
                    onClick={() => submitAutomationField(field.key)}
                  >
                    Set
                  </Button>
                </Field>
              );
            })}
          </div>
        </CardContent>
      </Card>

      <Card className="rounded-none border-border bg-card shadow-none">
        <CardHeader className="border-b border-border pb-3">
          <CardTitle className="font-mono text-sm uppercase tracking-widest">
            Events
          </CardTitle>
          <CardDescription className="font-mono text-xs">
            ESP32 boot &amp; relay log
          </CardDescription>
        </CardHeader>
        <CardContent className="pt-4">
          <div className="flex h-64 flex-col overflow-y-auto">
            {!state?.logs?.length ? (
              <p className="font-mono text-xs text-muted-foreground">No events yet</p>
            ) : (
              state.logs.map((entry, i) => (
                <div key={i} className="flex items-baseline gap-3 border-t border-border py-1.5 first:border-t-0 first:pt-0">
                  <span className="shrink-0 font-mono text-[10px] tabular-nums text-muted-foreground">
                    {new Date(entry.receivedAt).toLocaleTimeString()}
                  </span>
                  <span className="break-all font-mono text-xs">{entry.message}</span>
                </div>
              ))
            )}
          </div>
        </CardContent>
      </Card>

      <p className="text-center font-mono text-[10px] uppercase tracking-widest text-muted-foreground">
        Auto-refresh every 2s
      </p>
    </div>
  );
};

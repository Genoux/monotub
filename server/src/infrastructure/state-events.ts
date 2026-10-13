import { toStateView } from '../domain/monotub/state';
import type { MonotubState } from '../domain/monotub/types';

type Listener = () => void;

export type StateEvents = {
  emit: () => void;
  subscribe: (listener: Listener) => () => void;
};

const ONLINE_CHECK_INTERVAL_MS = 5_000;

export const createStateEvents = (): StateEvents => {
  const listeners = new Set<Listener>();

  return {
    emit: () => {
      for (const listener of listeners) listener();
    },
    subscribe: (listener) => {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
  };
};

/**
 * Going offline is the absence of messages, so nothing else would emit it.
 * Poll the derived flag and emit when it flips.
 */
export const watchOnline = (
  state: MonotubState,
  events: StateEvents,
): (() => void) => {
  let lastOnline = toStateView(state).online;
  const timer = setInterval(() => {
    const online = toStateView(state).online;
    if (online === lastOnline) return;
    lastOnline = online;
    events.emit();
  }, ONLINE_CHECK_INTERVAL_MS);
  return () => clearInterval(timer);
};

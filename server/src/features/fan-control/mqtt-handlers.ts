import { applyMqttMessage } from '../../domain/monotub/state';
import { monotubTopics } from '../../domain/monotub/topics';
import { logMqttRecv } from '../../infrastructure/mqtt-log';
import type { MonotubState } from '../../domain/monotub/types';

export const handleFanControlMqttMessage = (
  state: MonotubState,
  topic: string,
  payload: Buffer,
  retained = false,
): void => {
  const message = payload.toString();
  const receivedAt = new Date().toISOString();
  logMqttRecv(topic, message);
  applyMqttMessage(state, topic, message, receivedAt, {
    connected: monotubTopics.connected,
    fanState: monotubTopics.fanState,
    mistState: monotubTopics.mistState,
    status: monotubTopics.status,
    log: monotubTopics.log,
  }, retained);
};

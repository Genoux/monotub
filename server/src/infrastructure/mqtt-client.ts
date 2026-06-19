import mqtt, { type MqttClient } from 'mqtt';
import { handleFanControlMqttMessage } from '../features/fan-control/mqtt-handlers';
import { logMqttSend } from './mqtt-log';
import { monotubTopics } from '../domain/monotub/topics';
import type { MonotubState } from '../domain/monotub/types';

type MqttBridge = {
  client: MqttClient;
  publish: (topic: string, message: string) => void;
  disconnect: () => void;
};

export const connectMqttBridge = (
  brokerUrl: string,
  state: MonotubState,
): MqttBridge => {
  const client = mqtt.connect(brokerUrl);

  client.on('connect', () => {
    console.log(`[mqtt] connected to ${brokerUrl}`);
    client.subscribe(monotubTopics.subscribeAll, { qos: 1 }, (err) => {
      if (err) console.error('[mqtt] subscribe failed:', err.message);
      else console.log(`[mqtt] subscribed to ${monotubTopics.subscribeAll}`);
    });
  });

  client.on('reconnect', () => console.log('[mqtt] reconnecting...'));
  client.on('error', (err) =>
    console.error(
      '[mqtt] error:',
      err.message || (err as NodeJS.ErrnoException).code || err,
    ),
  );

  client.on('message', (topic, payload) => {
    handleFanControlMqttMessage(state, topic, payload);
  });

  const publish = (topic: string, message: string) => {
    // Command topics must not be retained — on ESP32 reconnect the broker
    // would replay every /set message and pulse latching relays again.
    client.publish(topic, message, { retain: false, qos: 1 });
    logMqttSend(topic, message);
  };

  const disconnect = () => {
    console.log('\n[mqtt] disconnecting...');
    client.end(false, {}, () => process.exit(0));
  };

  return { client, publish, disconnect };
};

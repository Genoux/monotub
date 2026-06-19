import { createInitialState } from './domain/monotub/state';
import { createExpressApp } from './infrastructure/express-app';
import { connectMqttBridge } from './infrastructure/mqtt-client';

const BROKER_URL = process.env.MQTT_URL ?? 'mqtt://localhost:1883';
const HTTP_PORT = Number(process.env.HTTP_PORT ?? 8001);

const state = createInitialState();
const { publish, disconnect } = connectMqttBridge(BROKER_URL, state);
const app = createExpressApp({ brokerUrl: BROKER_URL, state, publish });

app.listen(HTTP_PORT, () => {
  console.log(`[http] listening on http://localhost:${HTTP_PORT}`);
});

process.on('SIGINT', disconnect);
process.on('SIGTERM', disconnect);

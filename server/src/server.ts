import { createInitialState } from './domain/monotub/state';
import { createExpressApp } from './infrastructure/express-app';
import { connectMqttBridge } from './infrastructure/mqtt-client';
import { createStateEvents, watchOnline } from './infrastructure/state-events';

const BROKER_URL = process.env.MQTT_URL ?? 'mqtt://localhost:1883';
const HTTP_PORT = Number(process.env.HTTP_PORT ?? 8001);

const state = createInitialState();
const events = createStateEvents();
const { publish, disconnect } = connectMqttBridge(BROKER_URL, state, events.emit);
watchOnline(state, events);
const app = createExpressApp({ brokerUrl: BROKER_URL, state, publish, events });

app.listen(HTTP_PORT, () => {
  console.log(`[http] listening on http://localhost:${HTTP_PORT}`);
});

process.on('SIGINT', disconnect);
process.on('SIGTERM', disconnect);

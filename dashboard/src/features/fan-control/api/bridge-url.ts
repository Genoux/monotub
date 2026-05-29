const bridgeUrl = process.env.NEXT_PUBLIC_MQTT_BRIDGE_URL ?? "http://localhost:8001";

export const bridgeApiUrl = (path: string) =>
  `${bridgeUrl.replace(/\/$/, "")}${path.startsWith("/") ? path : `/${path}`}`;

import express, { type Express } from 'express';
import { registerFanControlHttpRoutes } from '../features/fan-control/http-routes';
import type { MonotubState } from '../domain/monotub/types';
import type { StateEvents } from './state-events';

type CreateExpressAppOptions = {
  brokerUrl: string;
  state: MonotubState;
  publish: (topic: string, message: string) => void;
  events: StateEvents;
};

export const createExpressApp = ({
  brokerUrl,
  state,
  publish,
  events,
}: CreateExpressAppOptions): Express => {
  const app = express();

  app.use(express.json());

  registerFanControlHttpRoutes(app, {
    brokerUrl,
    getState: () => state,
    publish,
    events,
  });

  return app;
};

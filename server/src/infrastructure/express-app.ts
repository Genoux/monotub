import express, { type Express } from 'express';
import { registerFanControlHttpRoutes } from '../features/fan-control/http-routes';
import type { MonotubState } from '../domain/monotub/types';

type CreateExpressAppOptions = {
  brokerUrl: string;
  state: MonotubState;
  publish: (topic: string, message: string) => void;
};

export const createExpressApp = ({
  brokerUrl,
  state,
  publish,
}: CreateExpressAppOptions): Express => {
  const app = express();

  app.use(express.json());

  registerFanControlHttpRoutes(app, {
    brokerUrl,
    getState: () => state,
    publish,
  });

  return app;
};

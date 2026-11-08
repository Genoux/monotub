import path from 'node:path';
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
  app.use(express.static(path.join(import.meta.dir, '../../public')));

  registerFanControlHttpRoutes(app, {
    brokerUrl,
    getState: () => state,
    publish,
  });

  return app;
};

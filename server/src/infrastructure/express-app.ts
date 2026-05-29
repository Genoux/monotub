import express, { type Express } from 'express';
import { registerFanControlHttpRoutes } from '../features/fan-control/http-routes';
import type { MonotubState } from '../domain/monotub/types';

type CreateExpressAppOptions = {
  brokerUrl: string;
  state: MonotubState;
  publish: (topic: string, message: string) => void;
};

const corsOrigin = process.env.CORS_ORIGIN ?? 'http://localhost:3000';

export const createExpressApp = ({
  brokerUrl,
  state,
  publish,
}: CreateExpressAppOptions): Express => {
  const app = express();

  app.use((req, res, next) => {
    res.setHeader('Access-Control-Allow-Origin', corsOrigin);
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
    if (req.method === 'OPTIONS') {
      res.sendStatus(204);
      return;
    }
    next();
  });

  app.use(express.json());

  registerFanControlHttpRoutes(app, {
    brokerUrl,
    getState: () => state,
    publish,
  });

  return app;
};

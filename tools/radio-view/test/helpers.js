import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

const repo = (p) => readFileSync(fileURLToPath(new URL(`../../../${p}`, import.meta.url)), 'utf8');
export const MODEL_YML = repo('radio/edgetx/sdcard/MODELS/model42.yml');
export const CHANNEL_MAP_H = repo('lib/rideon_core/src/ChannelMap.h');
export const CONFIG_H = repo('include/config.h');

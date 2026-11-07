// The repo's own files, bundled at build time (Vite ?raw imports), so the view always shows
// the model and the channel map of the commit it was built from.
import modelYml from '../../../../radio/edgetx/sdcard/MODELS/model42.yml?raw';
import channelMapH from '../../../../lib/rideon_core/src/ChannelMap.h?raw';
import configH from '../../../../include/config.h?raw';
import { channelNumbers, configValues } from './repo.js';
import { load } from './model.js';

export const REPO_YML = modelYml;
export const CH = channelNumbers(channelMapH);
export const LIMITS = configValues(configH);
export const REPO = load(modelYml);

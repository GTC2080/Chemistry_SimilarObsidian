import { LazyStore } from "@tauri-apps/plugin-store";
import { SETTINGS_STORE_NAME } from "../components/settings/settingsTypes";

export const settingsStore = new LazyStore(SETTINGS_STORE_NAME);

export async function persistStoreValues(values: Record<string, unknown>) {
  await Promise.all(
    Object.entries(values).map(([key, value]) => settingsStore.set(key, value)),
  );
  await settingsStore.save();
}


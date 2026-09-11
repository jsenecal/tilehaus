import { slugToGlyph } from "./icon_catalog";

// Resolve a tile icon glyph for a Home Assistant entity, staying within the
// glyphs the firmware bundles. Order: the entity's own mdi: icon when bundled,
// then a domain/device_class default, else "" (leave it to the picker).
//
// Every slug below is verified present in common/assets/icon_glyphs.yaml; if a
// firmware font trim ever drops one, slugToGlyph returns undefined and we fall
// back to "" rather than emitting a glyph the panel can't draw.

interface EntityMetaLike {
  icon: string;
  deviceClass: string;
}

const DOMAIN_SLUG: Readonly<Record<string, string>> = {
  light: "lightbulb",
  switch: "power",
  input_boolean: "power",
  fan: "fan",
  lock: "lock",
  cover: "window-shutter",
  climate: "thermostat",
  media_player: "speaker",
  camera: "cctv",
  vacuum: "robot-vacuum",
  scene: "palette",
  person: "account",
  device_tracker: "account",
  sun: "white-balance-sunny",
  weather: "weather-cloudy",
  alarm_control_panel: "shield-home",
  number: "gauge",
  input_number: "gauge",
  sensor: "gauge",
  binary_sensor: "circle-outline",
  button: "circle-outline",
  input_button: "circle-outline",
  select: "circle-outline",
  input_select: "circle-outline",
  automation: "check-circle",
  script: "check-circle",
};

const SENSOR_CLASS_SLUG: Readonly<Record<string, string>> = {
  temperature: "thermometer",
  humidity: "water-percent",
  battery: "battery",
  power: "flash",
  energy: "flash",
  current: "flash",
  voltage: "flash",
};

const BINARY_CLASS_SLUG: Readonly<Record<string, string>> = {
  motion: "motion-sensor",
  occupancy: "motion-sensor",
  presence: "motion-sensor",
  door: "door",
  window: "window-shutter",
  opening: "door",
  garage_door: "garage",
  moisture: "water",
  smoke: "fire",
  heat: "fire",
  gas: "fire",
};

const COVER_CLASS_SLUG: Readonly<Record<string, string>> = {
  garage: "garage",
  door: "garage",
  blind: "blinds",
  shade: "blinds",
  shutter: "window-shutter",
  curtain: "curtains",
  window: "window-shutter",
};

export function iconGlyphForEntity(meta: EntityMetaLike, entityId: string): string {
  // 1. The entity's own mdi icon, if the firmware bundles it.
  const m = /^mdi:([a-z0-9-]+)$/.exec(meta.icon.trim());
  if (m) {
    const g = slugToGlyph(m[1]!);
    if (g !== undefined) return g;
  }
  // 2. Domain (refined by device_class) default.
  const domain = entityId.split(".")[0] ?? "";
  let slug: string | undefined;
  if (domain === "sensor") slug = SENSOR_CLASS_SLUG[meta.deviceClass];
  else if (domain === "binary_sensor") slug = BINARY_CLASS_SLUG[meta.deviceClass];
  else if (domain === "cover") slug = COVER_CLASS_SLUG[meta.deviceClass];
  if (slug === undefined) slug = DOMAIN_SLUG[domain];
  if (slug === undefined) return "";
  return slugToGlyph(slug) ?? "";
}

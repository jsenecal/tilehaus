import { iconGlyphForEntity } from "../../web/icon_from_entity";
import { slugToGlyph } from "../../web/icon_catalog";

function assert(cond: boolean, message: string): void {
  if (!cond) throw new Error(message);
}

export function runIconFromEntityTests(): void {
  const bulb = slugToGlyph("lightbulb")!;
  const gauge = slugToGlyph("gauge")!;
  const thermometer = slugToGlyph("thermometer")!;
  const motion = slugToGlyph("motion-sensor")!;
  const shutter = slugToGlyph("window-shutter")!;
  const garage = slugToGlyph("garage")!;

  // 1. HA's own mdi icon wins when the bundle has it.
  assert(
    iconGlyphForEntity({ icon: "mdi:lightbulb", deviceClass: "" }, "switch.x") === bulb,
    "bundled mdi icon used verbatim",
  );
  // An mdi icon the bundle lacks falls through to the domain default.
  assert(
    iconGlyphForEntity({ icon: "mdi:some-icon-not-bundled", deviceClass: "" }, "sensor.x") === gauge,
    "unbundled mdi icon falls back to domain",
  );

  // 2. Domain defaults.
  assert(iconGlyphForEntity({ icon: "", deviceClass: "" }, "light.x") === bulb, "light domain default");
  assert(iconGlyphForEntity({ icon: "", deviceClass: "" }, "sensor.x") === gauge, "sensor domain default");

  // 2b. device_class refinements.
  assert(
    iconGlyphForEntity({ icon: "", deviceClass: "temperature" }, "sensor.x") === thermometer,
    "temperature sensor",
  );
  assert(
    iconGlyphForEntity({ icon: "", deviceClass: "motion" }, "binary_sensor.x") === motion,
    "motion binary_sensor",
  );
  assert(
    iconGlyphForEntity({ icon: "", deviceClass: "" }, "cover.x") === shutter,
    "cover default",
  );
  assert(
    iconGlyphForEntity({ icon: "", deviceClass: "garage" }, "cover.x") === garage,
    "garage cover",
  );

  // 3. Unknown domain, no icon → empty (leave it to the picker).
  assert(iconGlyphForEntity({ icon: "", deviceClass: "" }, "wibble.x") === "", "unknown domain empty");
  assert(iconGlyphForEntity({ icon: "", deviceClass: "" }, "") === "", "empty entity id empty");
}

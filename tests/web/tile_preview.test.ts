import { PANEL, isBright, splitValue, tileLook } from "../../web/tile_preview";
import { panelGrid } from "../../web/grid_view";
import { defaultCard } from "../../web/editor_state";

function assert(cond: boolean, message: string): void {
  if (!cond) throw new Error(message);
}

const ent = (state: string, attributes: Record<string, unknown> = {}) => ({ state, unit: "", attributes });

export function runTilePreviewTests(): void {
  // The firmware's needs_dark_ink() threshold.
  assert(isBright(PANEL.lime) && !isBright(PANEL.tileBg) && isBright(0xffffff) && !isBright(PANEL.fan), "brightness rule");

  // Toggle: lime when on, charcoal when off; the deck accent and a custom colour win.
  {
    const t = { ...defaultCard(2), entity: "light.a" };
    assert(tileLook(t, ent("on"), -1).fill === PANEL.lime, "on is lime");
    assert(tileLook(t, ent("off"), -1).fill === PANEL.tileBg, "off is charcoal");
    assert(tileLook(t, ent("on"), 0x3b9be3).fill === 0x3b9be3, "deck accent");
    assert(tileLook({ ...t, activeColor: 0xff0000 }, ent("on"), 0x3b9be3).fill === 0xff0000, "tile colour beats accent");
    assert(tileLook({ ...t, transparent: true }, ent("on"), -1).fill === null, "transparent");
  }
  // Card-specific colours, as the firmware paints them.
  assert(tileLook({ ...defaultCard(10), entity: "lock.f" }, ent("locked"), -1).fill === PANEL.good, "lock locked");
  assert(tileLook({ ...defaultCard(10), entity: "lock.f" }, ent("unlocked"), -1).fill === PANEL.alert, "lock unlocked");
  assert(tileLook({ ...defaultCard(12), entity: "climate.c" }, ent("heat", { hvac_action: "heating" }), -1).fill === PANEL.warm, "climate heating");
  assert(tileLook({ ...defaultCard(12), entity: "climate.c" }, ent("heat", { hvac_action: "idle" }), -1).fill === PANEL.tileBg, "climate idle");
  assert(tileLook({ ...defaultCard(17), entity: "fan.f" }, ent("on"), -1).fill === PANEL.fan, "fan");
  assert(tileLook({ ...defaultCard(18), entity: "alarm.a" }, ent("armed_away"), -1).fill === PANEL.armed, "alarm armed");
  assert(tileLook({ ...defaultCard(23), entity: "script.s" }, ent("on"), -1).fill === PANEL.lime, "running script");
  assert(tileLook({ ...defaultCard(23), entity: "scene.s" }, ent("on"), -1).fill === PANEL.tileBg, "a scene never tints");
  assert(tileLook({ ...defaultCard(11) }, undefined, -1).fill === null, "header sits on the page");
  // Follow colour: the light's own colour.
  {
    const t = { ...defaultCard(4), entity: "light.a", followColor: true };
    assert(tileLook(t, ent("on", { rgb_color: [255, 128, 0] }), -1).fill === 0xff8000, "follow colour");
  }

  // Values split like the firmware's split_sensor_value.
  {
    const a = splitValue("23.46", "°C");
    assert(a.value === "23.5" && a.unit === "°C" && a.numeric, "number + unit");
    assert(splitValue("139", "").value === "139", "whole number bare");
    assert(!splitValue("Heating", "").numeric && splitValue("Heating", "").value === "Heating", "text state");
    assert(splitValue("unavailable", "%").value === "—", "unavailable");
  }

  // The firmware's grid arithmetic: the office deck (12 x 7 on 1024 x 600).
  {
    const g = panelGrid(1024, 600, 12, 7);
    assert(g.cellW === 73 && g.cellH === 72, `office cells ${g.cellW}x${g.cellH}`);
    const l = panelGrid(1024, 600, 10, 6);
    assert(l.cellW === 90 && l.cellH === 86, `lobby cells ${l.cellW}x${l.cellH}`);
  }
}

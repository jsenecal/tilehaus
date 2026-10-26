import { type DeckCard } from "./model/deck";
import { ICON_CATALOG, glyphToEntry } from "./icon_catalog";
import { TIME_ZONES } from "./time_zones";

// Drawing a tile the way the panel does, for the configurator's preview. Every
// size here is the panel's own pixel value (inset, icon, font sizes) and every
// colour the firmware's palette (firmware/native/palette.h, theme.h): the
// preview is built at the panel's resolution and scaled as a whole, so these
// match the glass one to one.
//
// tileLook() decides colours and is pure (host-tested); renderTileFace()
// builds the DOM.

export const PANEL = {
  tileBg: 0x313131,
  lime: 0xb5e853,
  ink: 0xffffff,
  inkOnLight: 0x1a1a1a,
  inkDim: 0xb0b0b0,
  inkFaint: 0x909090,
  modalWell: 0x1e1e1e,
  good: 0x2e7d32,
  alert: 0xc62828,
  warm: 0xc24e1e,
  armed: 0xb23b3b,
  fan: 0x2c6e8a,
  wled: 0x5a3e8a,
  warn: 0xd99a1e,
  page: 0xe9e6e0,
  pageInk: 0x141414,
  pageMuted: 0x5c5955,
  nightPage: 0x0e0e0e,
  nightInk: 0xf2f0ec,
  nightMuted: 0x8e8b86,
} as const;

export const TILE_INSET = 16;
const ICON_PX = 46;
const NAME_PX = 22;
const NAME_TIGHT_PX = 17;  // tiles narrower than TIGHT_W, as on the panel
const TIGHT_W = 200;
const CAPTION_PX = 14;

/** What the preview knows about a tile's entity (from the HA client). */
export interface EntityView {
  state: string;
  unit: string;
  attributes: Record<string, unknown>;
}

export const hex = (rgb: number): string => "#" + (rgb & 0xffffff).toString(16).padStart(6, "0");

/** The firmware's needs_dark_ink(): a fill bright enough for dark text. */
export function isBright(rgb: number): boolean {
  const r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;
  return (r * 299 + g * 587 + b * 114) / 1000 > 150;
}

export interface TileLook {
  fill: number | null;  // null = transparent (on the page)
  active: boolean;
}

const num = (v: unknown): number | null => (typeof v === "number" && Number.isFinite(v) ? v : null);

// The tile's fill for its state, per card type, as the firmware paints it.
export function tileLook(card: DeckCard, ent: EntityView | undefined, accent: number): TileLook {
  const on = card.activeColor >= 0 ? card.activeColor : accent >= 0 ? accent : PANEL.lime;
  const off = card.inactiveColor >= 0 ? card.inactiveColor : PANEL.tileBg;
  const own = (fallback: number): number => (card.activeColor >= 0 ? card.activeColor : fallback);
  if (card.transparent || card.type === 11) return { fill: null, active: false };
  const st = ent?.state ?? "";
  switch (card.type) {
    case 2: case 6: case 14: case 21:  // Toggle, Presence, Door/Window, Page
      return st === "on" ? { fill: on, active: true } : { fill: off, active: false };
    case 4: {  // Light (control): follow colour tints with the light's own colour
      if (st !== "on") return { fill: off, active: false };
      const rgb = ent?.attributes["rgb_color"];
      if (card.followColor && Array.isArray(rgb) && rgb.length === 3) {
        const [r, g, b] = rgb.map((x) => Math.max(0, Math.min(255, Number(x) || 0)));
        return { fill: (r! << 16) | (g! << 8) | b!, active: true };
      }
      return { fill: on, active: true };
    }
    case 13: return st === "on" ? { fill: own(PANEL.wled), active: true } : { fill: off, active: false };
    case 17: return st === "on" ? { fill: own(PANEL.fan), active: true } : { fill: off, active: false };
    case 10:  // Lock: always tinted, secure or not
      if (ent === undefined) return { fill: off, active: false };
      return st === "locked"
        ? { fill: own(PANEL.good), active: true }
        : { fill: card.inactiveColor >= 0 ? card.inactiveColor : PANEL.alert, active: false };
    case 12:  // Climate: warm while actually heating
      return ent?.attributes["hvac_action"] === "heating"
        ? { fill: own(PANEL.warm), active: true } : { fill: off, active: false };
    case 18:  // Alarm
      return /^(armed|arming|pending|triggered)/.test(st)
        ? { fill: own(PANEL.armed), active: true } : { fill: off, active: false };
    case 23:  // Action: a script tints while it runs
      return card.entity.startsWith("script.") && st === "on"
        ? { fill: on, active: true } : { fill: off, active: false };
    case 24: return st === "active" ? { fill: on, active: true } : { fill: off, active: false };
    case 29: return st === "playing" ? { fill: on, active: true } : { fill: off, active: false };
    default: return { fill: off, active: false };
  }
}

// ---------------------------------------------------------------------------

/** "23.5" + "°C", or a text state whole: the firmware's split_sensor_value. */
export function splitValue(state: string, unit: string): { value: string; unit: string; numeric: boolean } {
  if (state === "" || state === "unavailable" || state === "unknown") return { value: "—", unit: "", numeric: false };
  const n = Number(state);
  if (state.trim() !== "" && Number.isFinite(n)) {
    return { value: Number.isInteger(n) ? String(n) : n.toFixed(1), unit, numeric: true };
  }
  return { value: unit ? `${state} ${unit}` : state, unit: "", numeric: false };
}

const pathFor = (slug: string): string => ICON_CATALOG.find((e) => e.slug === slug)?.path ?? "";

function svgIcon(path: string, size: number, color: string): SVGSVGElement {
  const ns = "http://www.w3.org/2000/svg";
  const svg = document.createElementNS(ns, "svg");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("width", String(size));
  svg.setAttribute("height", String(size));
  const p = document.createElementNS(ns, "path");
  p.setAttribute("d", path);
  p.setAttribute("fill", color);
  svg.append(p);
  return svg;
}

let measureCtx: CanvasRenderingContext2D | null = null;
function textWidth(text: string, font: string): number {
  if (measureCtx === null) measureCtx = document.createElement("canvas").getContext("2d");
  if (measureCtx === null) return text.length * 20;
  measureCtx.font = font;
  return measureCtx.measureText(text).width;
}

interface FaceContext {
  w: number;           // tile size in panel px
  h: number;
  accent: number;
  night: boolean;      // the page behind transparent tiles
  now: Date;
}

/** Draw a tile's face into `el` (which the caller sized and filled). */
export function renderTileFace(el: HTMLElement, card: DeckCard, ent: EntityView | undefined, ctx: FaceContext): void {
  const look = tileLook(card, ent, ctx.accent);
  const pageInk = hex(ctx.night ? PANEL.nightInk : PANEL.pageInk);
  const pageMuted = hex(ctx.night ? PANEL.nightMuted : PANEL.pageMuted);
  const ink = look.fill === null ? pageInk : hex(isBright(look.fill) ? PANEL.inkOnLight : PANEL.ink);
  const dim = look.fill === null ? pageMuted : isBright(look.fill) ? "rgba(26,26,26,.7)" : hex(PANEL.inkDim);
  el.style.background = look.fill === null ? "transparent" : hex(look.fill);
  el.style.color = ink;

  const cw = ctx.w - 2 * TILE_INSET, ch = ctx.h - 2 * TILE_INSET;
  const named = !card.hideLabel && card.title !== "";
  const nameH = named ? 34 : 0;
  const tight = ctx.w < TIGHT_W;
  const add = (node: HTMLElement | SVGElement, x: number, y: number): void => {
    (node as HTMLElement).style.position = "absolute";
    (node as HTMLElement).style.left = `${x}px`;
    (node as HTMLElement).style.top = `${y}px`;
    el.append(node);
  };
  const div = (cls: string, text: string): HTMLElement => {
    const d = document.createElement("div");
    d.className = cls;
    d.textContent = text;
    return d;
  };
  const caption = (text: string, color = dim): void => {
    const c = div("pv-caption", text.toUpperCase());
    c.style.color = color;
    c.style.fontSize = `${CAPTION_PX}px`;
    add(c, TILE_INSET, TILE_INSET);
  };
  const icon = (glyph: string, fallbackSlug = ""): void => {
    const path = glyphToEntry(glyph)?.path ?? (fallbackSlug ? pathFor(fallbackSlug) : "");
    if (path) add(svgIcon(path, ICON_PX, ink), TILE_INSET, TILE_INSET);
  };
  const name = (): void => {
    if (!named) return;
    const n = div("pv-name", card.title);
    n.style.fontSize = `${tight ? NAME_TIGHT_PX : NAME_PX}px`;
    n.style.width = `${cw}px`;
    n.style.position = "absolute";
    n.style.left = `${TILE_INSET}px`;
    n.style.bottom = `${TILE_INSET}px`;
    el.append(n);
  };
  // The firmware's FitNumber: the number as large as the box allows, the unit
  // at ~42% beside it on the baseline.
  const bigNumber = (value: string, unit: string, bx: number, by: number, bw: number, bh: number, centre: boolean): void => {
    const unitRatio = 0.42;
    const ref = 100;
    const vw = textWidth(value, `800 ${ref}px "Inter Tight"`);
    const uw = unit ? textWidth(unit, `700 ${ref * unitRatio}px "Inter Tight"`) + 4 : 0;
    // 0.92: a margin for the canvas measuring in a fallback face before the
    // web font has loaded (the editor redraws once fonts are ready).
    const byWidth = vw + uw > 0 ? (bw / (vw + uw)) * ref * 0.92 : bh;
    const size = Math.max(14, Math.min(bh * 0.95, byWidth, 180));
    const box = document.createElement("div");
    box.className = "pv-number";
    box.style.width = `${bw}px`;
    box.style.height = `${bh}px`;
    box.style.justifyContent = centre ? "center" : "flex-start";
    const v = div("pv-value", value);
    v.style.fontSize = `${size}px`;
    box.append(v);
    if (unit) {
      const u = div("pv-unit", unit);
      u.style.fontSize = `${Math.max(12, size * unitRatio)}px`;
      box.append(u);
    }
    add(box, bx, by);
  };
  // A ring (the Timer) or a 240° dial open at the bottom (the Gauge): the
  // track drawn, and arc(fraction, colour) to draw the filled part.
  const ring = (open: boolean): { size: number; x: number; y: number; svg: SVGSVGElement; arc: (frac: number, color: string) => void } => {
    const roomy = cw >= 180 && ch >= 180;
    const size = Math.max(48, roomy ? Math.min(cw, ch - 22 - 34 - 16) : Math.min(cw, ch - nameH));
    const stroke = Math.max(6, size / (open ? 9 : 11));
    const r = (size - stroke) / 2, c = size / 2;
    const ns = "http://www.w3.org/2000/svg";
    const svg = document.createElementNS(ns, "svg");
    svg.setAttribute("width", String(size));
    svg.setAttribute("height", String(size));
    const arc = (from: number, sweep: number, color: string, opacity = 1): void => {
      if (sweep <= 0) return;
      const a0 = (from * Math.PI) / 180, a1 = ((from + sweep) * Math.PI) / 180;
      const p = document.createElementNS(ns, "path");
      const large = sweep > 180 ? 1 : 0;
      const full = sweep >= 359.9;
      p.setAttribute("d", full
        ? `M ${c} ${c - r} A ${r} ${r} 0 1 1 ${c - 0.01} ${c - r}`
        : `M ${c + r * Math.cos(a0)} ${c + r * Math.sin(a0)} A ${r} ${r} 0 ${large} 1 ${c + r * Math.cos(a1)} ${c + r * Math.sin(a1)}`);
      p.setAttribute("fill", "none");
      p.setAttribute("stroke", color);
      p.setAttribute("stroke-width", String(stroke));
      p.setAttribute("stroke-opacity", String(opacity));
      svg.append(p);
    };
    const start = open ? 150 : -90, span = open ? 240 : 360;
    arc(start, span, "#000", 0.3);
    return {
      size, x: TILE_INSET + (cw - size) / 2, y: TILE_INSET + (roomy ? 22 + 8 : 0), svg,
      arc: (frac: number, color: string) => { arc(start, span * Math.max(0, Math.min(1, frac)), color); },
    };
  };

  const st = ent?.state ?? "";
  switch (card.type) {
    case 0: {  // Blank: a heading on the page, or icon + title
      if (card.transparent && card.title !== "") {
        if (card.row === 0) {
          const t = div("pv-page-title", card.title);
          t.style.color = pageInk;
          add(t, TILE_INSET, Math.max(0, (ctx.h - 66) / 2));
          const bar = document.createElement("div");
          bar.className = "pv-bar";
          bar.style.background = pageInk;
          add(bar, TILE_INSET, Math.max(0, (ctx.h - 66) / 2) + 54);
        } else {
          const c = div("pv-caption", card.title.toUpperCase());
          c.style.color = pageMuted;
          c.style.position = "absolute";
          c.style.left = `${TILE_INSET}px`;
          c.style.bottom = "4px";
          el.append(c);
        }
        return;
      }
      icon(card.icon);
      name();
      return;
    }
    case 1: {  // Sensor
      const p = splitValue(st, ent?.unit ?? "");
      if (p.numeric) bigNumber(p.value, p.unit, TILE_INSET, TILE_INSET, cw, ch - nameH, false);
      else { const t = div("pv-text", p.value); t.style.width = `${cw}px`; add(t, TILE_INSET, TILE_INSET); }
      name();
      return;
    }
    case 3: case 15: {  // fill slider rising from the bottom
      let pct = 0;
      if (card.entity.startsWith("light.")) pct = st === "on" ? Math.round(((num(ent?.attributes["brightness"]) ?? 255) / 255) * 100) : 0;
      else {
        const v = Number(st), lo = num(ent?.attributes["min"]) ?? 0, hi = num(ent?.attributes["max"]) ?? 100;
        if (Number.isFinite(v) && hi > lo) pct = Math.round(((v - lo) / (hi - lo)) * 100);
      }
      const fill = document.createElement("div");
      fill.className = "pv-fill";
      const fc = card.activeColor >= 0 ? card.activeColor : ctx.accent >= 0 ? ctx.accent : PANEL.lime;
      fill.style.background = hex(fc);
      fill.style.height = `${Math.max(0, Math.min(100, pct))}%`;
      fill.style.bottom = "0";
      el.append(fill);
      if (pct >= 30) el.style.color = hex(isBright(fc) ? PANEL.inkOnLight : PANEL.ink);  // the name sits in the fill
      icon(card.icon);
      name();
      return;
    }
    case 5: {  // Cover: a shade hanging from the top
      const pos = num(ent?.attributes["current_position"]) ?? (st === "closed" ? 0 : st === "open" ? 100 : 0);
      const fill = document.createElement("div");
      fill.className = "pv-fill";
      const fc = ctx.accent >= 0 ? ctx.accent : PANEL.lime;
      fill.style.background = hex(fc);
      fill.style.height = `${100 - Math.max(0, Math.min(100, pos))}%`;
      fill.style.top = "0";
      el.append(fill);
      if (100 - pos >= 70) el.style.color = hex(isBright(fc) ? PANEL.inkOnLight : PANEL.ink);
      icon(card.icon, "blinds");
      name();
      return;
    }
    case 7: {  // Weather
      const t = num(ent?.attributes["temperature"]);
      if (t !== null) bigNumber(Number.isInteger(t) ? String(t) : t.toFixed(1), String(ent?.attributes["temperature_unit"] ?? "°"), TILE_INSET, TILE_INSET, cw, ch - nameH, false);
      if (st) { const n = div("pv-name", st.replace(/-/g, " ")); n.style.position = "absolute"; n.style.left = `${TILE_INSET}px`; n.style.bottom = `${TILE_INSET}px`; n.style.fontSize = `${tight ? NAME_TIGHT_PX : NAME_PX}px`; el.append(n); }
      return;
    }
    case 11: {  // Header: greeting left, clock right, on the page
      const g = div("pv-greeting", st || "Good evening");
      g.style.color = pageInk;
      add(g, TILE_INSET, TILE_INSET);
      const clock = div("pv-clock", ctx.now.toTimeString().slice(0, 5));
      clock.style.color = pageInk;
      clock.style.position = "absolute";
      clock.style.right = `${TILE_INSET}px`;
      clock.style.top = `${TILE_INSET}px`;
      el.append(clock);
      return;
    }
    case 16: case 18: {  // Option select / Alarm: the state, centred
      icon(card.icon, card.type === 18 ? "shield-home" : "");
      const t = div("pv-text pv-centre", card.type === 18 ? st.replace(/_/g, " ") : st);
      t.style.width = `${ctx.w}px`;
      add(t, 0, ctx.h / 2 - 14);
      name();
      return;
    }
    case 19: {  // Weather forecast
      const t = div("pv-text pv-centre", "Forecast");
      t.style.width = `${ctx.w}px`;
      t.style.color = dim;
      add(t, 0, ctx.h / 2 - 14);
      return;
    }
    case 22: icon(card.icon, "chevron-left"); name(); return;
    case 24: {  // Timer
      const dur = parseHms(String(ent?.attributes["duration"] ?? ""));
      let rem = st === "idle" ? dur : parseHms(String(ent?.attributes["remaining"] ?? ""));
      const fin = Date.parse(String(ent?.attributes["finishes_at"] ?? ""));
      if (st === "active" && Number.isFinite(fin)) rem = Math.max(0, Math.round((fin - ctx.now.getTime()) / 1000));
      const g = ring(false);
      g.arc(dur > 0 && rem >= 0 ? rem / dur : 0, ink);
      add(g.svg, g.x, g.y);
      const inner = g.size * 0.8;
      bigNumber(rem >= 0 ? countdown(rem) : "–", "", g.x + (g.size - inner * 0.74) / 2, g.y + (g.size - inner * 0.46) / 2, inner * 0.74, inner * 0.46, true);
      if (cw >= 180 && ch >= 180) caption(st === "active" ? "Running" : st === "paused" ? "Paused" : st === "idle" ? "Ready" : "");
      name();
      return;
    }
    case 25: {  // Garage door / gate
      icon(card.icon, ent?.attributes["device_class"] === "gate" ? (st === "open" ? "gate-open" : "gate") : (st === "open" ? "garage-open" : "garage"));
      const c = div("pv-caption", st ? st.toUpperCase() : "");
      c.style.color = st === "open" ? hex(PANEL.warn) : dim;
      add(c, TILE_INSET, ctx.h / 2 - 8);
      name();
      return;
    }
    case 26: {  // Gauge
      const p = splitValue(st, ent?.unit ?? "");
      const g = ring(true);
      const [lo, hi, warn] = gaugeScale(card.entity2, ent?.unit ?? "");
      const v = Number(st);
      const frac = Number.isFinite(v) && hi > lo ? Math.max(0, Math.min(1, (v - lo) / (hi - lo))) : 0;
      const warning = Number.isFinite(v) && warn !== null && (warn.below ? v < warn.at : v > warn.at);
      const fc = warning ? PANEL.alert : card.activeColor >= 0 ? card.activeColor : ctx.accent >= 0 ? ctx.accent : PANEL.lime;
      g.arc(frac, hex(fc));
      add(g.svg, g.x, g.y + g.size / 12);
      const inner = g.size * 0.78;
      bigNumber(p.value, p.unit, g.x + (g.size - inner * 0.78) / 2, g.y + g.size / 12 + (g.size - inner * 0.46) / 2, inner * 0.78, inner * 0.46, true);
      if (cw >= 180 && ch >= 180) caption(warning ? (warn?.below ? "Low" : "High") : `${lo}–${hi}${ent?.unit ? " " + ent.unit : ""}`, warning ? hex(PANEL.alert) : dim);
      name();
      return;
    }
    case 27: {  // Clock
      const zone = TIME_ZONES.find((z) => z.tz === card.entity2);
      const opts: Intl.DateTimeFormatOptions = zone?.iana ? { timeZone: zone.iana } : {};
      const hm = new Intl.DateTimeFormat("en-GB", { ...opts, hour: "2-digit", minute: "2-digit", hour12: false }).format(ctx.now);
      const day = new Intl.DateTimeFormat("en-GB", { ...opts, weekday: "short" }).format(ctx.now).toUpperCase();
      const date = new Intl.DateTimeFormat("en-GB", { ...opts, day: "numeric", month: "short" }).format(ctx.now).toUpperCase();
      caption(card.entity2 ? `${day} · ${offsetText(zone?.iana ?? "", ctx.now)}` : `${day} ${date}`);
      bigNumber(hm, "", TILE_INSET, TILE_INSET + 22 + 6, cw, ch - 22 - nameH - 12, false);
      name();
      return;
    }
    case 28: {  // Wi-Fi share
      caption("Guest Wi-Fi");
      const size = Math.max(48, Math.min(cw, ch - 22 - 6 - (named ? 40 : 0)));
      const qr = document.createElement("div");
      qr.className = "pv-qr";
      qr.style.width = qr.style.height = `${size}px`;
      add(qr, TILE_INSET + (cw - size) / 2, TILE_INSET + 28);
      name();
      return;
    }
    case 29: {  // Media: the firmware's three layouts, by content size
      const playing = st === "playing";
      const track = playing || st === "paused" || st === "buffering";
      const large = cw >= 200 && ch >= 240, wide = !large && cw >= 200 && ch >= 110;
      const app = String(ent?.attributes["app_name"] ?? "");
      const title = track ? String(ent?.attributes["media_title"] ?? "") || app : "";
      const artist = track ? String(ent?.attributes["media_artist"] ?? "") : "";
      const picture = track ? String(ent?.attributes["entity_picture"] ?? "") : "";
      const word = ({ playing: "Playing", paused: "Paused", buffering: "Loading", idle: "Idle", on: "Idle", off: "Off", standby: "Off" } as Record<string, string>)[st] ?? "Unavailable";
      const corner = 34, capH = 17;
      let fg = ink, fgDim = dim;
      // The cover fills the tile under a dark wash; only a full URL can load
      // here (a path on Home Assistant can't), and without one the tile keeps
      // its state colour.
      if (/^https?:\/\//.test(picture)) {
        el.style.background = `linear-gradient(rgba(0,0,0,.5), rgba(0,0,0,.5)), center / cover no-repeat url("${picture.replace(/"/g, "%22")}"), ${hex(PANEL.tileBg)}`;
        el.style.color = hex(PANEL.ink);
        fg = hex(PANEL.ink);
        fgDim = hex(PANEL.inkDim);
      }
      const cap = div("pv-caption", (!wide && !large || !named ? word : `${word} · ${card.title}`).toUpperCase());
      cap.style.cssText += `color:${fgDim};font-size:${CAPTION_PX}px;width:${cw - corner}px;overflow:hidden;white-space:nowrap;`;
      add(cap, TILE_INSET, TILE_INSET);
      const line = (text: string, y: number, w: number, px: number, weight: number, lines: number, color: string): void => {
        const d = div("pv-media-line", text);
        d.style.cssText += `width:${w}px;font:${weight} ${px}px/1.2 "Inter Tight",sans-serif;color:${color};` +
          `display:-webkit-box;-webkit-line-clamp:${lines};-webkit-box-orient:vertical;overflow:hidden;`;
        add(d, TILE_INSET, y);
      };
      const titlePx = large ? 34 : tight ? 17 : 22;
      const titleY = TILE_INSET + capH + 4;
      const titleH = Math.round(titlePx * 1.2) * (large ? 2 : 1);
      if (title) line(title, titleY, cw - corner, titlePx, large ? 800 : 700, large ? 2 : 1, fg);
      if (artist) line(artist, titleY + titleH, cw, large ? 22 : 17, 700, 1, fgDim);
      // The full-view arrows, in the top-right corner.
      const expand = (): void => {
        const p = pathFor("arrow-expand");
        if (p) { const g = svgIcon(p, 27, fg); g.style.opacity = "0.7"; add(g, ctx.w - 38, 11); }
      };
      if (!wide && !large) {
        if (!title) { const p = pathFor("speaker"); if (p) add(svgIcon(p, ICON_PX, fg), TILE_INSET, TILE_INSET + (ch - ICON_PX) / 2); }
        name();
        expand();
        return;
      }
      const dur = num(ent?.attributes["media_duration"]), pos = num(ent?.attributes["media_position"]);
      const fmt = (t: number): string => { t = Math.floor(t); const h = Math.floor(t / 3600), m = Math.floor(t / 60) % 60, s2 = String(t % 60).padStart(2, "0"); return h ? `${h}:${String(m).padStart(2, "0")}:${s2}` : `${m}:${s2}`; };
      const timeText = track && pos !== null ? fmt(pos) + (dur ? ` / ${fmt(dur)}` : "") : "";
      const bar = (x: number, y: number, w: number): void => {
        if (!track) return;
        const bb = document.createElement("div");
        const frac = pos !== null && dur ? Math.min(1, pos / dur) : 0;
        bb.style.cssText = `width:${w}px;height:4px;background:linear-gradient(90deg, ${fg} ${frac * 100}%, ${fg}4d ${frac * 100}%);`;
        add(bb, x, y);
      };
      const time = (x: number, y: number): void => {
        if (!timeText) return;
        const t = div("pv-caption", timeText);
        t.style.color = fg;
        t.style.fontSize = `${CAPTION_PX}px`;
        add(t, x, y);
      };
      const btn = (slug: string, x: number, y: number, size: number): void => {
        const p = pathFor(slug);
        if (p) add(svgIcon(p, ICON_PX, fg), x + (size - ICON_PX) / 2, y + (size - ICON_PX) / 2);
      };
      const playSlug = playing ? "pause" : "play";
      const bottom = TILE_INSET + ch;
      if (wide) {
        const wb = 48, g = 2, right = TILE_INSET + cw;
        btn("skip-next", right - wb, bottom - wb, wb);
        btn(playSlug, right - 2 * wb - g, bottom - wb, wb);
        btn("skip-previous", right - 3 * wb - 2 * g, bottom - wb, wb);
        const leftW = cw - 3 * wb - 2 * g - 16, barY = bottom - (wb - 4) / 2 - 4;
        bar(TILE_INSET, barY, leftW);
        time(TILE_INSET, barY + 4 - 8 - 17);
        expand();
        return;
      }
      const b = 56, volH = 46, mid = TILE_INSET + cw / 2;
      const vp = pathFor("volume-high");
      if (vp) add(svgIcon(vp, ICON_PX, fg), TILE_INSET, bottom - ICON_PX);
      const vol = num(ent?.attributes["volume_level"]) ?? 0;
      const v = document.createElement("div");
      v.style.cssText = `width:${cw - 60}px;height:8px;background:linear-gradient(90deg, ${fg} ${vol * 100}%, ${fg}4d ${vol * 100}%);`;
      add(v, TILE_INSET + 60, bottom - volH / 2 - 4);
      const btnY = bottom - volH - 8 - b;
      btn(playSlug, mid - b / 2, btnY, b);
      btn("skip-previous", mid - b / 2 - b - 24, btnY, b);
      btn("skip-next", mid + b / 2 + 24, btnY, b);
      const timeY = btnY - 10 - 17;
      time(TILE_INSET, timeY);
      bar(TILE_INSET, timeY - 6 - 4, cw);
      expand();
      return;
    }
    default:
      icon(card.icon, card.type === 9 || card.type === 23 ? "gesture-tap" : "");
      name();
  }
}

function parseHms(s: string): number {
  const m = /^(?:(\d+) days?, )?(\d+):(\d{2}):(\d{2})/.exec(s);
  if (!m) return -1;
  return ((Number(m[1] ?? 0) * 24 + Number(m[2])) * 60 + Number(m[3])) * 60 + Number(m[4]);
}

function countdown(s: number): string {
  const h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, sec = s % 60;
  const p = (n: number): string => String(n).padStart(2, "0");
  return h > 0 ? `${h}:${p(m)}:${p(sec)}` : `${m}:${p(sec)}`;
}

function offsetText(iana: string, now: Date): string {
  if (!iana) return "";
  const at = (tz?: string): number => {
    const parts = new Intl.DateTimeFormat("en-US", { timeZone: tz, hourCycle: "h23", year: "numeric", month: "numeric", day: "numeric", hour: "numeric", minute: "numeric" }).formatToParts(now);
    const g = (t: string): number => Number(parts.find((p) => p.type === t)?.value ?? 0);
    return Date.UTC(g("year"), g("month") - 1, g("day"), g("hour"), g("minute"));
  };
  const mins = Math.round((at(iana) - at(undefined)) / 60000);
  if (mins === 0) return "SAME TIME";
  const a = Math.abs(mins);
  return `${mins > 0 ? "+" : "-"}${a % 60 === 0 ? `${a / 60} H` : `${Math.floor(a / 60)}:${String(a % 60).padStart(2, "0")}`}`;
}

// The firmware's parse_gauge_scale, reduced to what the preview draws.
function gaugeScale(spec: string, unit: string): [number, number, { below: boolean; at: number } | null] {
  const defaults: Record<string, [number, number]> = { "%": [0, 100], "°C": [0, 40], "°F": [32, 104], ppm: [400, 2000], W: [0, 3000], kW: [0, 10] };
  let [lo, hi] = defaults[unit] ?? [0, 100];
  let warn: { below: boolean; at: number } | null = unit === "ppm" ? { below: false, at: 1000 } : null;
  const m = /^\s*(?:(-?[\d.]+)\.\.(-?[\d.]+))?\s*(?:([<>])(-?[\d.]+))?\s*$/.exec(spec);
  if (spec.trim() !== "" && m) {
    if (m[1] !== undefined && Number(m[2]) > Number(m[1])) { lo = Number(m[1]); hi = Number(m[2]); }
    if (m[3] !== undefined) warn = { below: m[3] === "<", at: Number(m[4]) };
  }
  return [lo, hi, warn];
}

import { type DeckCard } from "./model/deck";

export type FieldKey = "entity" | "entity2" | "title" | "iconAlt";

export interface FieldDef {
  key: FieldKey;
  label: string;
  hint: string;
  entity: boolean; // true = an entity id (typeahead, never name-autofilled); false = display label
}

// The text fields a card type exposes, in display order, and whether each holds
// an entity id or a plain display label. Most cards: one bound Entity + a Title
// label, with Entity 2 hidden. Header and Weather repurpose fields (verified in
// header_card.h / weather_card.h): Header reads all three as entity ids
// (greeting / subtitle / weather); Weather uses Entity 2 as a forecast-high
// helper and Icon-alt as the forecast-low helper (both input_numbers). Type
// numbers are the frozen wire enum (see card_types.ts).
export function cardFieldDefs(type: number): FieldDef[] {
  if (type === 11) return [ // Header
    { key: "entity",  label: "Greeting entity", entity: true, hint: "HA template sensor for the top greeting line (its text shows verbatim)." },
    { key: "title",   label: "Subtitle entity", entity: true, hint: "HA template sensor for the second line. Blank = no subtitle." },
    { key: "entity2", label: "Weather entity",  entity: true, hint: "Weather entity for the glyph + temperature." },
  ];
  if (type === 22) return [ // Back (pops the navigation stack)
    { key: "title", label: "Title", entity: false, hint: "Optional label under the back icon." },
  ];
  if (type === 21) return [ // Page (opens a page; optionally tracks an entity for state)
    { key: "entity",  label: "Tracked entity (optional)", entity: true,  hint: "HA entity for the tile's on/off tint + icon, like a Toggle. Blank = plain nav tile." },
    { key: "entity2", label: "Target page",               entity: false, hint: "The page this tile opens when tapped." },
    { key: "title",   label: "Title",                     entity: false, hint: "Name shown on the tile." },
  ];
  if (type === 7) return [ // Weather
    { key: "entity",  label: "Weather entity",       entity: true,  hint: "Weather entity this tile displays." },
    { key: "entity2", label: "Forecast-high helper", entity: true,  hint: "input_number holding today's forecast high (optional)." },
    { key: "iconAlt", label: "Forecast-low helper",  entity: true,  hint: "input_number holding today's forecast low (optional). Blank = derive a “…_low” id from the high helper." },
    { key: "title",   label: "Title",               entity: false, hint: "Name shown on the tile. Blank = no label." },
  ];
  return [
    { key: "entity", label: "Entity", entity: true,  hint: "Home Assistant entity id this tile binds to, e.g. light.office_desk_lamp. Pick one to auto-fill Title and Icon." },
    { key: "title",  label: "Title",  entity: false, hint: "Name shown on the tile. Blank = no label." },
  ];
}

// ---- Type-aware boolean flags -------------------------------------------

export type FlagKey =
  | "hideLabel" | "detail" | "followColor"
  | "showWeather" | "showClock" | "showHilo" | "transparent" | "tightMargins";

export interface FlagDef {
  key: FlagKey;
  label: string;
  hint: string;
  positive?: boolean; // shown checked when the field is true (default); false = the field itself is a "show" default-true
}

// Card type numbers (frozen wire enum, see card_types.ts).
const HIDE_LABEL_TYPES = new Set([1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 13, 15, 16, 17, 18, 22]);
const DETAIL_TYPES = new Set([2, 3, 5, 15, 17]);
const FOLLOW_COLOR_TYPES = new Set([3, 4]);
const HEADER_TYPE = 11;

// The boolean flags a card type actually honours, in display order. Every tile
// gets "Transparent background"; the rest depend on the type.
export function cardFlagDefs(type: number): FlagDef[] {
  const defs: FlagDef[] = [];
  if (HIDE_LABEL_TYPES.has(type)) {
    defs.push({ key: "hideLabel", label: "Hide label", hint: "Show only the icon, with no name on the tile." });
  }
  if (DETAIL_TYPES.has(type)) {
    defs.push({ key: "detail", label: "Detail chevron", hint: "Add a corner button that opens the entity's detail popup." });
  }
  if (FOLLOW_COLOR_TYPES.has(type)) {
    defs.push({ key: "followColor", label: "Follow colour", hint: "Tint the tile (and its detail sliders) to the light's actual colour." });
  }
  if (type === HEADER_TYPE) {
    defs.push({ key: "showWeather", label: "Show weather", hint: "Show the weather icon + temperature. Off = text-only header." });
    defs.push({ key: "showClock", label: "Show clock", hint: "Show the clock + date. Off = text-only header." });
    defs.push({ key: "showHilo", label: "Show hi/lo temps", hint: "Show today's high/low beside the weather temperature (from the forecast helpers)." });
  }
  defs.push({ key: "transparent", label: "Transparent background", hint: "Drop the tile's background — use a Blank or Header tile as a section header." });
  if (type === 0) {
    defs.push({ key: "tightMargins", label: "Reduced margins", hint: "Shrink the tile inset so the icon + text sit close to the edges." });
  }
  return defs;
}

export function flagValue(card: DeckCard, key: FlagKey): boolean {
  return card[key];
}

export function patchForFlag(key: FlagKey, value: boolean): Partial<DeckCard> {
  return { [key]: value } as Partial<DeckCard>;
}

export function patchForField(key: FieldKey, value: string): Partial<DeckCard> {
  return key === "entity" ? { entity: value }
    : key === "entity2" ? { entity2: value }
    : key === "iconAlt" ? { iconAlt: value }
    : { title: value };
}

export function fieldValue(card: DeckCard, key: FieldKey): string {
  return key === "entity" ? card.entity
    : key === "entity2" ? card.entity2
    : key === "iconAlt" ? card.iconAlt
    : card.title;
}

// Entity ids whose id STARTS WITH the query (case-insensitive), capped so the
// suggestion <datalist> never holds the whole 3000+ entity list. An empty query
// returns nothing (rather than everything).
export function matchEntities(entities: readonly string[], query: string, limit = 50): string[] {
  const q = query.trim().toLowerCase();
  if (q === "") return [];
  const out: string[] = [];
  for (const id of entities) {
    if (!id.toLowerCase().startsWith(q)) continue;
    out.push(id);
    if (out.length >= limit) break;
  }
  return out;
}

/** Versioned binary document for on-device tile-deck configuration. */
export const DECK_DOCUMENT_VERSION = 6;
export const DECK_HEADER_SIZE = 16;         // v1/v2 header
export const DECK_HEADER_SIZE_V3 = 20;      // v3+ appends a 4-byte deck accent
export const DECK_HEADER_SIZE_V5 = 20;      // v5 header (accent + page table)
export const DECK_HEADER_SIZE_V6 = 22;      // v6 appends gridCols/gridRows
export const DECK_ACCENT_DEFAULT = -1;      // -1 = use the built-in card colours
export const DECK_ALIGN_DEFAULT = 5;        // v4 align byte: halign|(valign<<2); 5 = centre/centre
export const DECK_GRID_COLS_DEFAULT = 10;
export const DECK_GRID_ROWS_DEFAULT = 6;
export const DECK_MAX_CARD_COUNT = 64;
export const DECK_MAX_STRING_BYTES = 63;
export const DECK_MAX_ICON_BYTES = 63;
export const DECK_MAX_TYPE = 22;

const MAGIC = [0x44, 0x45, 0x43, 0x4b]; // DECK

export interface DeckCard {
  type: number;
  entity: string;
  title: string;
  w: number;
  h: number;
  entity2: string;
  icon: string;
  iconAlt: string;
  activeColor: number;   // -1 = card default, else 0..0xFFFFFF
  inactiveColor: number;
  hideLabel: boolean;
  detail: boolean;
  followColor: boolean;
  showHilo: boolean;      // Header: show today's high/low beside the weather temp
  transparent: boolean;   // any tile: drop the background (section headers)
  showWeather: boolean;   // Header: show the weather cluster (default true)
  showClock: boolean;     // Header: show the clock + date (default true)
  tightMargins: boolean;  // Blank: shrink the tile inset (section headers)
  align: number;          // Blank text/icon alignment: halign|(valign<<2), each 0/1/2
  page: number;           // which page this card lives on (0 = Home)
  col: number;           // grid column 0..GRID_COLUMNS-1
  row: number;           // grid row, 0-based
}

/** A decoded deck plus its document-level settings. */
export interface DeckDocument {
  accent: number;   // deck accent colour, 0xRRGGBB, or -1 for the built-in defaults
  pages: string[];        // page names, index 0 = Home
  gridCols: number;
  gridRows: number;
  cards: DeckCard[];
}

export class DeckConfigError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "DeckConfigError";
  }
}

function fail(message: string): never { throw new DeckConfigError(message); }

function encodeString(value: string, label: string, maxLength: number): Uint8Array {
  if (typeof value !== "string") fail(`${label} must be a string`);
  const encoded = new TextEncoder().encode(value);
  if (encoded.length > maxLength) fail(`${label} exceeds ${maxLength} bytes`);
  return encoded;
}

function checkByte(value: number, label: string): number {
  if (!Number.isInteger(value) || value < 0 || value > 0xff) fail(`${label} must be 0..255`);
  return value;
}

function checkColor(value: number, label: string): number {
  if (value === -1) return -1;
  if (!Number.isInteger(value) || value < 0 || value > 0xffffff) fail(`${label} must be -1 or 0..0xFFFFFF`);
  return value;
}

function writeU16(out: Uint8Array, offset: number, value: number): void {
  out[offset] = value & 0xff;
  out[offset + 1] = (value >>> 8) & 0xff;
}

function writeU32(out: Uint8Array, offset: number, value: number): void {
  out[offset] = value & 0xff;
  out[offset + 1] = (value >>> 8) & 0xff;
  out[offset + 2] = (value >>> 16) & 0xff;
  out[offset + 3] = (value >>> 24) & 0xff;
}

function readU16(input: Uint8Array, offset: number): number {
  return input[offset]! | (input[offset + 1]! << 8);
}

function readU32(input: Uint8Array, offset: number): number {
  return (input[offset]! | (input[offset + 1]! << 8) | (input[offset + 2]! << 16) | (input[offset + 3]! << 24)) >>> 0;
}

function readI32(input: Uint8Array, offset: number): number {
  return input[offset]! | (input[offset + 1]! << 8) | (input[offset + 2]! << 16) | (input[offset + 3]! << 24);
}

function encodeCard(card: DeckCard): Uint8Array {
  if (!Number.isInteger(card.type) || card.type < 0 || card.type > DECK_MAX_TYPE) fail("card type out of range");
  const entity = encodeString(card.entity, "entity", DECK_MAX_STRING_BYTES);
  const title = encodeString(card.title, "title", DECK_MAX_STRING_BYTES);
  const entity2 = encodeString(card.entity2, "entity2", DECK_MAX_STRING_BYTES);
  const icon = encodeString(card.icon, "icon", DECK_MAX_ICON_BYTES);
  const iconAlt = encodeString(card.iconAlt, "iconAlt", DECK_MAX_ICON_BYTES);
  // showWeather/showClock default true and are stored inverted (set bit = hidden)
  // so a deck written before these flags existed decodes to "shown".
  const flags = (card.hideLabel ? 1 : 0) | (card.detail ? 2 : 0) | (card.followColor ? 4 : 0)
    | (card.showHilo ? 8 : 0) | (card.transparent ? 16 : 0)
    | (card.showWeather ? 0 : 32) | (card.showClock ? 0 : 64)
    | (card.tightMargins ? 128 : 0);
  const strings = [entity, title, entity2, icon, iconAlt];
  const body = new Uint8Array(16 + strings.reduce((n, s) => n + 1 + s.length, 0));
  body[0] = card.type;
  body[1] = checkByte(card.w, "w");
  body[2] = checkByte(card.h, "h");
  body[3] = flags;
  writeU32(body, 4, checkColor(card.activeColor, "activeColor") >>> 0);
  writeU32(body, 8, checkColor(card.inactiveColor, "inactiveColor") >>> 0);
  body[12] = checkByte(card.col, "col");
  body[13] = checkByte(card.row, "row");
  body[14] = checkByte(card.align, "align");
  body[15] = checkByte(card.page, "page");
  let offset = 16;
  for (const s of strings) {
    body[offset++] = s.length;
    body.set(s, offset);
    offset += s.length;
  }
  return body;
}

export function encodeDeck(
  cards: DeckCard[], accent: number = DECK_ACCENT_DEFAULT, pages: string[] = ["Home"],
  gridCols: number = DECK_GRID_COLS_DEFAULT, gridRows: number = DECK_GRID_ROWS_DEFAULT,
): Uint8Array {
  if (!Array.isArray(cards)) fail("deck must be an array");
  if (cards.length > DECK_MAX_CARD_COUNT) fail("deck contains too many cards");
  if (pages.length < 1 || pages.length > 255) fail("deck must have 1..255 pages");
  if (gridCols < 1 || gridCols > 40 || gridRows < 1 || gridRows > 40) fail("grid size out of range");
  const pageBodies = pages.map((p) => encodeString(p, "page name", DECK_MAX_STRING_BYTES));
  const bodies = cards.map(encodeCard);
  const pageBytes = pageBodies.reduce((n, s) => n + 1 + s.length, 0);
  const payloadLength = pageBytes + bodies.reduce((n, b) => n + b.length, 0);
  const out = new Uint8Array(DECK_HEADER_SIZE_V6 + payloadLength);
  out.set(MAGIC, 0);
  writeU16(out, 4, DECK_DOCUMENT_VERSION);
  writeU16(out, 6, DECK_HEADER_SIZE_V6);
  writeU32(out, 8, payloadLength);
  writeU16(out, 12, cards.length);
  out[14] = pages.length;
  out[15] = 0;
  writeU32(out, 16, checkColor(accent, "accent") >>> 0);
  out[20] = checkByte(gridCols, "gridCols");
  out[21] = checkByte(gridRows, "gridRows");
  let offset = DECK_HEADER_SIZE_V6;
  for (const s of pageBodies) { out[offset++] = s.length; out.set(s, offset); offset += s.length; }
  for (const b of bodies) { out.set(b, offset); offset += b.length; }
  return out;
}

function readString(input: Uint8Array, offset: number, label: string): { value: string; next: number } {
  if (offset >= input.length) fail(`truncated ${label} length`);
  const length = input[offset]!;
  const start = offset + 1;
  if (start + length > input.length) fail(`truncated ${label} body`);
  let value: string;
  try {
    value = new TextDecoder("utf-8", { fatal: true }).decode(input.slice(start, start + length));
  } catch {
    fail(`${label} is not valid UTF-8`);
  }
  return { value, next: start + length };
}

export function decodeDeck(input: Uint8Array): DeckCard[] {
  return decodeDeckDocument(input).cards;
}

export function decodeDeckDocument(input: Uint8Array): DeckDocument {
  if (!(input instanceof Uint8Array) || input.length < DECK_HEADER_SIZE ||
      MAGIC.some((value, index) => input[index] !== value)) {
    fail("invalid deck document header");
  }
  const version = readU16(input, 4);
  if (version !== 5 && version !== 6) fail("invalid deck document header");
  const headerSize = version === 6 ? DECK_HEADER_SIZE_V6 : DECK_HEADER_SIZE_V5;
  if (input.length < headerSize ||
      readU16(input, 6) !== headerSize ||
      input[15] !== 0 ||
      readU32(input, 8) !== input.length - headerSize) {
    fail("invalid deck document header");
  }
  const accent = readI32(input, 16);
  const pageCount = input[14]!;
  if (pageCount < 1) fail("invalid page count");
  const gridCols = version === 6 ? input[20]! : DECK_GRID_COLS_DEFAULT;
  const gridRows = version === 6 ? input[21]! : DECK_GRID_ROWS_DEFAULT;
  const cardCount = readU16(input, 12);
  if (cardCount > DECK_MAX_CARD_COUNT) fail("deck contains too many cards");
  let offset = headerSize;
  const pages: string[] = [];
  for (let i = 0; i < pageCount; i += 1) {
    const s = readString(input, offset, "page name"); offset = s.next; pages.push(s.value);
  }
  const cards: DeckCard[] = [];
  for (let index = 0; index < cardCount; index += 1) {
    if (offset + 16 > input.length) fail("truncated card body");
    const type = input[offset]!;
    if (type > DECK_MAX_TYPE) fail("unknown card type");
    const w = input[offset + 1]!, h = input[offset + 2]!, flags = input[offset + 3]!;
    const activeColor = readI32(input, offset + 4);
    const inactiveColor = readI32(input, offset + 8);
    const col = input[offset + 12]!, row = input[offset + 13]!, align = input[offset + 14]!, page = input[offset + 15]!;
    offset += 16;
    const entity = readString(input, offset, "entity"); offset = entity.next;
    const title = readString(input, offset, "title"); offset = title.next;
    const entity2 = readString(input, offset, "entity2"); offset = entity2.next;
    const icon = readString(input, offset, "icon"); offset = icon.next;
    const iconAlt = readString(input, offset, "iconAlt"); offset = iconAlt.next;
    cards.push({
      type, entity: entity.value, title: title.value, w, h, entity2: entity2.value,
      icon: icon.value, iconAlt: iconAlt.value, activeColor, inactiveColor,
      hideLabel: (flags & 1) !== 0, detail: (flags & 2) !== 0, followColor: (flags & 4) !== 0,
      showHilo: (flags & 8) !== 0, transparent: (flags & 16) !== 0,
      showWeather: (flags & 32) === 0, showClock: (flags & 64) === 0,
      tightMargins: (flags & 128) !== 0, align, page, col, row,
    });
  }
  if (offset !== input.length) fail("trailing bytes after deck records");
  return { accent, pages, gridCols, gridRows, cards };
}

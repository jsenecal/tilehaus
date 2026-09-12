import { decodeDeck, decodeDeckDocument, encodeDeck, DeckConfigError, DECK_MAX_CARD_COUNT, DECK_MAX_DOCUMENT_BYTES, type DeckCard } from "../../web/model/deck";
import { readFileSync } from "node:fs";

function deepEqual(actual: unknown, expected: unknown, message: string): void {
  const a = JSON.stringify(actual);
  const e = JSON.stringify(expected);
  if (a !== e) throw new Error(`${message}: expected ${e}, received ${a}`);
}

function assert(condition: boolean, message: string): void {
  if (!condition) throw new Error(message);
}

function throws(fn: () => void, message: string): void {
  try {
    fn();
  } catch (error) {
    if (error instanceof DeckConfigError) return;
    throw new Error(`${message}: threw non-DeckConfigError ${String(error)}`);
  }
  throw new Error(`${message}: expected a DeckConfigError`);
}

const BASIC: DeckCard[] = [
  {
    type: 4, entity: "light.desk", title: "Lamp", w: 2, h: 2, entity2: "",
    icon: "\u{F095F}", iconAlt: "\u{F1B1F}", activeColor: -1, inactiveColor: -1,
    hideLabel: false, detail: false, followColor: true, showHilo: false, transparent: false, showWeather: true, showClock: true, tightMargins: false, align: 5, page: 0, col: 0, row: 0,
  },
  {
    type: 2, entity: "switch.fan", title: "Fan", w: 2, h: 2, entity2: "",
    icon: "", iconAlt: "", activeColor: 0x2e7d32, inactiveColor: -1,
    hideLabel: true, detail: false, followColor: false, showHilo: false, transparent: false, showWeather: true, showClock: true, tightMargins: false, align: 5, page: 0, col: 3, row: 0,
  },
];

export function runDeckCodecTests(): void {
  // Round-trip
  deepEqual(decodeDeck(encodeDeck(BASIC)), BASIC, "basic round-trip");
  deepEqual(decodeDeck(encodeDeck([])), [], "empty deck round-trip");

  // Header integrity
  const good = encodeDeck(BASIC);
  const badMagic = good.slice(); badMagic[0] = 0x00;
  throws(() => decodeDeck(badMagic), "bad magic rejected");
  const badVersion = good.slice(); badVersion[4] = 0x04;
  throws(() => decodeDeck(badVersion), "pre-v5 rejected");
  throws(() => decodeDeck(good.slice(0, 8)), "truncated header rejected");
  throws(() => decodeDeck(good.slice(0, good.length - 1)), "truncated body rejected");

  // Field bounds
  throws(() => encodeDeck([{ ...BASIC[0]!, type: 99 }]), "unknown type rejected");
  throws(() => encodeDeck([{ ...BASIC[0]!, entity: "x".repeat(64) }]), "oversize string rejected");
  throws(() => encodeDeck(new Array(DECK_MAX_CARD_COUNT + 1).fill(BASIC[0]!)), "too many cards rejected");

  // A full-size deck round-trips and still fits the device's persisted blob.
  {
    const full = new Array(DECK_MAX_CARD_COUNT).fill(null).map((_, i): DeckCard => ({
      ...BASIC[0]!, col: (i * 2) % 12, row: 0, page: Math.floor(i / 6),
    }));
    const bytes = encodeDeck(full, -1, ["Home"], 12, 7);
    assert(bytes.length <= DECK_MAX_DOCUMENT_BYTES, "max-card deck fits the blob");
    deepEqual(decodeDeck(bytes), full, "max-card deck round-trips");
  }
  // ...and a deck that overflows the blob is rejected at encode.
  {
    const fat: DeckCard = {
      ...BASIC[0]!, entity: "x".repeat(63), title: "y".repeat(63), entity2: "z".repeat(63),
      icon: "i".repeat(63), iconAlt: "a".repeat(63),
    };
    throws(() => encodeDeck(new Array(DECK_MAX_CARD_COUNT).fill(fat)), "oversize deck rejected");
  }

  // Golden fixture: the committed bytes both codecs are pinned to.
  const fixture = new Uint8Array(readFileSync("tests/firmware/fixtures/deck_basic.bin"));
  deepEqual(Array.from(encodeDeck(BASIC)), Array.from(fixture), "encode matches golden fixture");
  deepEqual(decodeDeck(fixture), BASIC, "decode of golden fixture matches BASIC");

  // --- v2 absolute placement ---
  {
    const card: DeckCard = {
      type: 4, entity: "light.desk", title: "Lamp", w: 2, h: 3,
      entity2: "", icon: "", iconAlt: "", activeColor: -1, inactiveColor: -1,
      hideLabel: false, detail: false, followColor: false, showHilo: false, transparent: false, showWeather: true, showClock: true, tightMargins: false, align: 5, page: 0, col: 4, row: 2,
    };
    const round = decodeDeck(encodeDeck([card]));
    assert(round[0]!.col === 4 && round[0]!.row === 2, "col/row round-trip");
  }
  // --- v3 deck accent round-trips ---
  {
    const doc = decodeDeckDocument(encodeDeck(BASIC, 0x3AA0FF));
    assert(doc.accent === 0x3AA0FF, "accent round-trip");
    deepEqual(doc.cards, BASIC, "cards intact alongside accent");
    assert(decodeDeckDocument(encodeDeck(BASIC)).accent === -1, "default accent is -1");
  }
  // --- v5 pages ---
  {
    const home: DeckCard = { ...BASIC[0]!, page: 0 };
    const sub: DeckCard = { ...BASIC[1]!, type: 21, entity: "light.x", entity2: "Lights", page: 1 };
    const doc = decodeDeckDocument(encodeDeck([home, sub], -1, ["Home", "Lights"]));
    assert(JSON.stringify(doc.pages) === JSON.stringify(["Home", "Lights"]), "pages round-trip");
    assert(doc.cards[1]!.page === 1 && doc.cards[1]!.type === 21, "page + Page type round-trip");
    assert(doc.cards[1]!.entity2 === "Lights", "target page name in entity2");
  }
  // --- v6 grid size ---
  {
    const doc = decodeDeckDocument(encodeDeck(BASIC, -1, ["Home"], 8, 4));
    assert(doc.gridCols === 8 && doc.gridRows === 4, "grid size round-trip");
  }
  // a v5 doc decodes with the 10x6 default grid
  {
    const v5 = new Uint8Array(readFileSync("tests/firmware/fixtures/deck_v5_pages.bin"));
    const doc = decodeDeckDocument(v5);
    assert(doc.gridCols === 10 && doc.gridRows === 6, "v5 defaults to 10x6");
    assert(doc.pages.length === 2, "v5 pages still decode");
  }
}

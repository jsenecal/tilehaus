import { createHistory, type DeckSnapshot } from "../../web/history";
import { BackupError, backupFileName, exportSpec, parseSpec } from "../../web/backup";
import { defaultCard } from "../../web/editor_state";

function assert(cond: boolean, message: string): void {
  if (!cond) throw new Error(message);
}

function deck(titles: string[], accent = -1): DeckSnapshot {
  return {
    cards: titles.map((title) => ({ ...defaultCard(2), title })),
    pages: ["Home"], pageBack: [false], accent, gridCols: 10, gridRows: 6,
  };
}

const titles = (d: DeckSnapshot | null): string => (d === null ? "null" : d.cards.map((c) => c.title).join(","));

export function runHistoryBackupTests(): void {
  // Nothing to undo at first; the loaded deck is the floor.
  {
    const h = createHistory({ now: () => 0 });
    h.checkpoint(deck(["a"]));
    assert(!h.canUndo() && h.undo() === null, "fresh history has no undo");
  }

  // Separate edits are separate steps; undo walks back, redo walks forward.
  {
    let t = 0;
    const h = createHistory({ now: () => t, coalesceMs: 800 });
    h.checkpoint(deck(["a"]));
    t = 1000; h.checkpoint(deck(["a", "b"]));
    t = 3000; h.checkpoint(deck(["a", "b", "c"]));
    t = 3000; h.checkpoint(deck(["a", "b", "c"]));  // unchanged: no step
    assert(titles(h.undo()) === "a,b", "undo one step");
    assert(titles(h.undo()) === "a", "undo to the loaded deck");
    assert(h.undo() === null, "nothing past the loaded deck");
    assert(titles(h.redo()) === "a,b", "redo");
    assert(titles(h.redo()) === "a,b,c", "redo again");
    assert(!h.canRedo(), "redo exhausted");
  }

  // Rapid changes (typing a title) are one step.
  {
    let t = 0;
    const h = createHistory({ now: () => t, coalesceMs: 800 });
    h.checkpoint(deck([""]));
    for (const [i, title] of ["K", "Ki", "Kit", "Kitchen"].entries()) {
      t = 2000 + i * 100;
      h.checkpoint(deck([title]));
    }
    assert(titles(h.undo()) === "", "a typed word undoes in one step");
    assert(!h.canUndo(), "only one step was recorded");
  }

  // An edit after an undo drops the redo branch, and starts its own step.
  {
    let t = 0;
    const h = createHistory({ now: () => t, coalesceMs: 800 });
    h.checkpoint(deck(["a"]));
    t = 1000; h.checkpoint(deck(["a", "b"]));
    const back = h.undo();
    assert(titles(back) === "a", "undone");
    h.checkpoint(back!);                     // the restored deck re-renders: no step
    t = 1100; h.checkpoint(deck(["a", "x"])); // within coalesceMs of the undo, still a new step
    assert(!h.canRedo(), "redo cleared by a new edit");
    assert(titles(h.undo()) === "a", "new edit undoes to the restored deck");
  }

  // The stack is capped.
  {
    let t = 0;
    const h = createHistory({ now: () => t, limit: 3 });
    h.checkpoint(deck(["0"]));
    for (let i = 1; i <= 5; i += 1) { t += 10000; h.checkpoint(deck([String(i)])); }
    let n = 0;
    while (h.undo() !== null) n += 1;
    assert(n === 3, `capped at 3 steps, got ${n}`);
  }

  // Export -> import round-trips, and is the deck_cli spec shape.
  {
    const d = deck(["Lamp", "Fan"], 0xb5e853);
    d.pages = ["Home", "Office"];
    d.pageBack = [false, true];
    d.cards[1]!.page = 1;
    const text = exportSpec(d);
    const spec = JSON.parse(text) as Record<string, unknown>;
    assert(Array.isArray(spec["cards"]) && Array.isArray(spec["pages"]) && spec["gridCols"] === 10, "cli spec shape");
    assert(text.endsWith("\n"), "trailing newline");
    const back = parseSpec(text);
    assert(JSON.stringify(back) === JSON.stringify(d), "round trip");
  }

  // Import fills missing card fields, drops unknown ones, accepts a bare array.
  {
    const back = parseSpec(JSON.stringify([{ type: 2, entity: "switch.fan", bogus: 1 }]));
    const card = back.cards[0]!;
    assert(card.entity === "switch.fan" && card.w === 2 && card.showClock === true, "defaults filled");
    assert(!("bogus" in card), "unknown key dropped");
    assert(back.pages.length === 1 && back.gridCols === 10, "bare array takes deck defaults");
    assert(back.pageBack.length === 1 && !back.pageBack[0], "no Back button by default");
    const short = parseSpec(JSON.stringify({ pages: ["Home", "A", "B"], pageBack: [false, true], cards: [] }));
    assert(JSON.stringify(short.pageBack) === JSON.stringify([false, true, false]), "short pageBack pads with off");
  }

  // Bad files fail with a reason a person can act on.
  {
    const rejects = (text: string, needle: string): void => {
      try {
        parseSpec(text);
      } catch (error) {
        assert(error instanceof BackupError, `BackupError for ${text}`);
        assert((error as Error).message.includes(needle), `"${(error as Error).message}" mentions "${needle}"`);
        return;
      }
      throw new Error(`accepted ${text}`);
    };
    rejects("{not json", "valid JSON");
    rejects(JSON.stringify({ pages: ["Home"] }), "cards");
    rejects(JSON.stringify({ cards: [{ entity: "x" }] }), "card type");
    rejects(JSON.stringify({ cards: [{ type: 2, page: 3 }] }), "page 3");
    rejects(JSON.stringify({ cards: [{ type: 2, title: "x".repeat(80) }] }), "reject");
    rejects(JSON.stringify({ cards: [], gridCols: "ten" }), "gridCols");
    rejects(JSON.stringify({ cards: [], pageBack: ["yes"] }), "pageBack");
    rejects(JSON.stringify({ cards: [], pageBack: [false, true] }), "pageBack");
  }

  // File names come from the panel's host.
  {
    const d = new Date(2026, 9, 3);
    assert(backupFileName("tilehaus-office.local", d) === "tilehaus-office-2026-10-03.json", "mdns host");
    assert(backupFileName("192.168.252.102", d) === "192-168-252-102-2026-10-03.json", "ip host");
    assert(backupFileName("", d) === "tilehaus-2026-10-03.json", "no host");
  }
}

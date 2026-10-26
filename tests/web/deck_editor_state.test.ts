import {
  addCard, addPage, autoArrange, canPlace, canSwap, cardAt, createEditorState, defaultCard, deletePage, moveCard,
  removeCard, renamePage, setPageBack, swapCards, updateCard,
} from "../../web/editor_state";
import { DECK_MAX_CARD_COUNT } from "../../web/model/deck";

function assert(cond: boolean, message: string): void {
  if (!cond) throw new Error(message);
}
function deepEqual(actual: unknown, expected: unknown, message: string): void {
  const a = JSON.stringify(actual);
  const e = JSON.stringify(expected);
  if (a !== e) throw new Error(`${message}: expected ${e}, received ${a}`);
}

export function runEditorStateTests(): void {
  // defaultCard
  deepEqual(defaultCard(4), {
    type: 4, entity: "", title: "", w: 2, h: 2, entity2: "",
    icon: "", iconAlt: "", activeColor: -1, inactiveColor: -1,
    hideLabel: false, detail: false, followColor: false,
    showHilo: false, transparent: false, showWeather: true, showClock: true,
    tightMargins: false, align: 5, page: 0, col: 0, row: 0,
  }, "defaultCard shape");

  // add selects the new card
  const s = createEditorState();
  addCard(s, 2);
  addCard(s, 4);
  assert(s.cards.length === 2, "two cards added");
  assert(s.selected === 1, "second card selected");
  assert(s.cards[1]!.type === 4, "second card type");

  // update patches fields
  updateCard(s, 1, { title: "Lamp", entity: "light.x", w: 4 });
  assert(s.cards[1]!.title === "Lamp" && s.cards[1]!.entity === "light.x" && s.cards[1]!.w === 4, "update patch");
  assert(s.cards[0]!.title === "", "update did not touch other card");

  // remove: fixes selection
  removeCard(s, 0);
  assert(s.cards.length === 1 && s.cards[0]!.type === 4, "removed first");
  assert(s.selected === 0, "selection clamped after remove");
  removeCard(s, 0);
  assert(s.cards.length === 0 && s.selected === null, "empty -> no selection");

  // card-count cap
  const capped = createEditorState();
  for (let i = 0; i < DECK_MAX_CARD_COUNT + 5; i += 1) addCard(capped, 0);
  assert(capped.cards.length === DECK_MAX_CARD_COUNT, "card count capped");

  // canPlace: within bounds, self-excluded, overlap rejected
  {
    const s = createEditorState();
    addCard(s, 4); // tile 0 default 2x2 auto-placed at 0,0
    assert(s.cards[0]!.col === 0 && s.cards[0]!.row === 0, "first tile at 0,0");
    addCard(s, 2); // tile 1 auto-placed at next free fitting cell (2,0)
    assert(s.cards[1]!.col === 2 && s.cards[1]!.row === 0, "second tile at 2,0");
    assert(canPlace(s.cards, 1, 2, 0, 2, 2, 0, 10), "self position is placeable");
    assert(!canPlace(s.cards, 1, 0, 0, 2, 2, 0, 10), "cannot overlap tile 0");
    assert(!canPlace(s.cards, 1, 9, 0, 2, 2, 0, 10), "cannot exceed columns");
  }
  // moveCard respects canPlace
  {
    const s = createEditorState();
    addCard(s, 4); addCard(s, 2);
    moveCard(s, 1, 5, 3);
    assert(s.cards[1]!.col === 5 && s.cards[1]!.row === 3, "moved to free cell");
    moveCard(s, 1, 0, 0); // would overlap tile 0 → rejected, stays
    assert(s.cards[1]!.col === 5 && s.cards[1]!.row === 3, "overlap move rejected");
  }
  // autoArrange repacks to top-left
  {
    const s = createEditorState();
    addCard(s, 4); addCard(s, 2);
    moveCard(s, 1, 6, 4);
    autoArrange(s);
    assert(s.cards[0]!.col === 0 && s.cards[0]!.row === 0, "arranged tile 0");
    assert(s.cards[1]!.col === 2 && s.cards[1]!.row === 0, "arranged tile 1");
  }
  // pages: add / select / rename (repoints Page targets) / delete
  {
    const s = createEditorState();
    assert(JSON.stringify(s.pages) === JSON.stringify(["Home"]) && s.currentPage === 0, "starts on Home");
    addPage(s, "Lights");
    assert(s.pages.length === 2 && s.currentPage === 1, "add selects new page");
    addCard(s, 4);
    assert(s.cards[0]!.page === 1, "new card lands on current page");
    s.currentPage = 0;
    addCard(s, 21);                    // a Page tile on Home
    s.cards[1]!.entity2 = "Lights";    // targeting the Lights page
    renamePage(s, 1, "Lamps");
    assert(s.pages[1] === "Lamps" && s.cards[1]!.entity2 === "Lamps", "rename repoints Page targets");
    deletePage(s, 1);
    assert(s.pages.length === 1 && s.cards.every((c) => c.page === 0), "delete drops page + reassigns");
    assert(s.cards[0]!.entity2 === "", "deleted target cleared");
  }
  // the per-page Back button: new pages have it, Home never does, delete keeps step
  {
    const s = createEditorState();
    deepEqual(s.pageBack, [false], "Home has no Back button");
    addPage(s, "A"); addPage(s, "B");
    deepEqual(s.pageBack, [false, true, true], "new pages get a Back button");
    setPageBack(s, 1, false);
    setPageBack(s, 0, true);
    deepEqual(s.pageBack, [false, false, true], "toggled per page; Home stays off");
    deletePage(s, 1);
    deepEqual(s.pageBack, [false, true], "deleting a page drops its setting");
  }
  // canPlace is per-page: same cell on different pages doesn't collide
  {
    const s = createEditorState();
    addCard(s, 4);                     // page 0 at (0,0)
    addPage(s, "P2"); addCard(s, 4);   // page 1 at (0,0) — allowed
    assert(s.cards[0]!.col === 0 && s.cards[0]!.row === 0, "home tile at 0,0");
    assert(s.cards[1]!.col === 0 && s.cards[1]!.row === 0, "page-2 tile also at 0,0 (no cross-page collision)");
    assert(!canPlace(s.cards, -1, 0, 0, 2, 2, 1, 10), "cannot overlap a same-page tile");
    assert(canPlace(s.cards, -1, 0, 0, 2, 2, 2, 10), "a fresh page 2 is free at 0,0");
  }
  // gridCols/gridRows on state; canPlace bound uses the passed-in column count
  {
    const s = createEditorState();
    assert(s.gridCols === 10 && s.gridRows === 6, "default grid 10x6");
    s.gridCols = 8;
    addCard(s, 4); // 2x2 at 0,0
    assert(!canPlace(s.cards, -1, 7, 0, 2, 2, 0, 8), "cannot exceed 8 columns");
    assert(canPlace(s.cards, -1, 6, 0, 2, 2, 0, 8), "fits at col 6 of 8");
  }
  // swap: the tiles trade slots, each taking the other's cell and size
  {
    const s = createEditorState();
    const put = (col: number, row: number, w: number, h: number, title: string, page = 0): void => {
      s.cards.push({ ...defaultCard(4), col, row, w, h, title, page });
    };
    put(0, 0, 2, 2, "A");     // 0
    put(4, 0, 2, 1, "B");     // 1
    put(4, 1, 2, 1, "C");     // 2
    put(0, 0, 2, 2, "P", 1);  // 3: same cell, other page

    assert(cardAt(s.cards, 0, 5, 0) === 1, "cardAt finds B by any covered cell");
    assert(cardAt(s.cards, 0, 1, 1) === 0, "cardAt finds A's lower-right cell");
    assert(cardAt(s.cards, 0, 9, 5) === -1, "cardAt: empty cell");
    assert(cardAt(s.cards, 1, 1, 1) === 3, "cardAt is per page");

    // Different sizes: A (2x2) and B (2x1) trade cells and sizes
    assert(swapCards(s, 0, 1), "A and B swap");
    const slot = (i: number): number[] => [s.cards[i]!.col, s.cards[i]!.row, s.cards[i]!.w, s.cards[i]!.h];
    deepEqual(slot(0), [4, 0, 2, 1], "A takes B's slot and size");
    deepEqual(slot(1), [0, 0, 2, 2], "B takes A's slot and size");
    assert(s.cards[0]!.title === "A" && s.cards[1]!.title === "B", "only the slots moved");
    deepEqual(slot(2), [4, 1, 2, 1], "other tiles untouched");
    assert(s.selected === 0, "the dragged tile stays selected");

    // Swapping back restores the layout
    assert(swapCards(s, 0, 1), "swap back");
    deepEqual([slot(0), slot(1)], [[0, 0, 2, 2], [4, 0, 2, 1]], "swap back restores");

    // Across pages and with itself: refused
    assert(!canSwap(s.cards, 0, 3), "no swap across pages");
    assert(!canSwap(s.cards, 0, 0), "no swap with itself");
    assert(!swapCards(s, 0, 3), "cross-page swap reports false");
  }
}

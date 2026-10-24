import { type DeckCard } from "./model/deck";
import { CARD_TYPES } from "./card_types";
import { addCard, addPage, autoArrange, deletePage, removeCard, renamePage, updateCard, type EditorState } from "./editor_state";
import { createDeckClient, type DeckClient, type SaveResult, type SessionInfo } from "./deck_client";
import { createGridPane } from "./grid_view";
import { createHaClient, type HaClient } from "./ha_client";
import { createIconPicker } from "./icon_picker";
import { iconGlyphForEntity } from "./icon_from_entity";
import { UI_ICON_PATHS } from "./icon_catalog";
import { iconSvg } from "./icon_svg";
import { createHistory, type DeckSnapshot } from "./history";
import { BackupError, backupFileName, exportSpec, parseSpec } from "./backup";
import { cardFieldDefs, cardFlagDefs, fieldValue, flagValue, matchEntities, patchForField, patchForFlag } from "./card_fields";
import { gridLimits } from "./model/grid_layout";
import { TIME_ZONES } from "./time_zones";
import { hex as hexColor, isBright } from "./tile_preview";

type StatusState = "idle" | "busy" | "ok" | "warn" | "err";

function colorInputValue(color: number): string {
  if (color < 0) return "#000000";
  return "#" + color.toString(16).padStart(6, "0");
}

export interface EditorView {
  render(): void;
  refreshSession(): Promise<void>;
}

// While this browser holds the session, editing counts as use: check in with
// the panel at most this often so a long edit doesn't hit the idle timeout
// before Save.
const SESSION_HEARTBEAT_MS = 5 * 60 * 1000;

export function createEditorView(
  root: HTMLElement,
  state: EditorState,
  client: DeckClient = createDeckClient(),
  ha: HaClient = createHaClient(),
): EditorView {
  let status = "Ready";
  let statusState: StatusState = "idle";
  let busy = false;
  let session: SessionInfo | null = null;
  let previewNight = false;               // the preview's page: day or night mode
  let previewSlot: HTMLElement | null = null;

  // Live states: re-read HA every 30s and redraw only the preview, so a field
  // being typed in keeps its focus.
  const HA_REFRESH_MS = 30 * 1000;
  // Numbers are fitted by measuring text; once the web fonts arrive, measure again.
  void document.fonts?.ready.then(() => { renderPreview(); });
  setInterval(() => {
    if (ha.status().state !== "connected") return;
    void ha.refresh().then(() => { renderPreview(); });
  }, HA_REFRESH_MS);

  // ---- Undo, and what's unsaved ----
  const history = createHistory();
  const snapshot = (): DeckSnapshot => ({
    cards: state.cards, pages: state.pages, accent: state.accent,
    gridCols: state.gridCols, gridRows: state.gridRows,
  });
  const serialized = (): string => JSON.stringify(snapshot());
  let savedDeck: string | null = null;  // the deck as the panel has it; null until first render
  let undoBtn: HTMLButtonElement | null = null;
  let redoBtn: HTMLButtonElement | null = null;

  function syncHistory(): void {
    history.checkpoint(snapshot());
    if (undoBtn !== null) undoBtn.disabled = busy || !history.canUndo();
    if (redoBtn !== null) redoBtn.disabled = busy || !history.canRedo();
  }

  function restore(deck: DeckSnapshot): void {
    state.cards = deck.cards;
    state.pages = deck.pages;
    state.accent = deck.accent;
    state.gridCols = deck.gridCols;
    state.gridRows = deck.gridRows;
    if (state.currentPage >= state.pages.length) state.currentPage = 0;
    if (state.selected !== null && state.selected >= state.cards.length) state.selected = null;
  }

  function onUndo(): void {
    const deck = history.undo();
    if (deck === null) return;
    restore(deck);
    render();
  }
  function onRedo(): void {
    const deck = history.redo();
    if (deck === null) return;
    restore(deck);
    render();
  }

  // Field edits update the state without re-rendering (to keep focus), so
  // record after every input event too. Root listeners run after the field's.
  root.addEventListener("input", syncHistory);
  root.addEventListener("change", syncHistory);

  // Text fields keep the browser's own undo; everywhere else Ctrl/Cmd+Z is ours.
  document.addEventListener("keydown", (e) => {
    if (!(e.ctrlKey || e.metaKey) || e.altKey) return;
    const t = e.target;
    if (t instanceof HTMLInputElement && t.type !== "checkbox" && t.type !== "color") return;
    if (t instanceof HTMLTextAreaElement || t instanceof HTMLSelectElement) return;
    const key = e.key.toLowerCase();
    if (key === "z" && !e.shiftKey) { e.preventDefault(); onUndo(); }
    else if ((key === "z" && e.shiftKey) || key === "y") { e.preventDefault(); onRedo(); }
  });

  window.addEventListener("beforeunload", (e) => {
    if (savedDeck !== null && serialized() !== savedDeck) e.preventDefault();
  });

  // ---- Backup ----
  function onExport(): void {
    const blob = new Blob([exportSpec(snapshot())], { type: "application/json" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = backupFileName(window.location.hostname, new Date());
    a.click();
    setTimeout(() => { URL.revokeObjectURL(a.href); }, 0);
    setStatus(`Exported ${a.download}`, "ok");
  }

  const importInput = document.createElement("input");
  importInput.type = "file";
  importInput.accept = ".json,application/json";
  importInput.onchange = () => {
    const file = importInput.files?.[0];
    importInput.value = "";  // choosing the same file again still fires
    if (file === undefined) return;
    void file.text().then((text) => {
      let deck: DeckSnapshot;
      try {
        deck = parseSpec(text);
      } catch (error) {
        setStatus(error instanceof BackupError ? `Not imported: ${error.message}` : `Not imported: ${String(error)}`, "err");
        return;
      }
      restore(deck);
      state.currentPage = 0;
      state.selected = null;
      setStatus(`Imported ${file.name} — press Save to send it to the panel (Undo reverts)`, "warn");
    });
  };
  let lastCheckIn = 0;
  let pollTimer: ReturnType<typeof setTimeout> | null = null;
  const BLOCKED_POLL_MS = 4000;

  // Ask the panel who may write. Failures leave the last known state: the
  // panel is often mid-reboot right after a save.
  async function refreshSession(): Promise<void> {
    lastCheckIn = Date.now();
    try {
      session = await client.session();
    } catch {
      schedulePoll();
      return;
    }
    render();
    schedulePoll();
  }

  // While Save is replaced by the lock notice, keep asking. A check without a
  // key is not "use", so this never holds a session open.
  function schedulePoll(): void {
    if (pollTimer !== null) clearTimeout(pollTimer);
    pollTimer = null;
    const s = session;
    const blocked = s === null || s.state === "locked" || (s.state === "claimed" && !s.mine);
    if (blocked) pollTimer = setTimeout(() => { void refreshSession(); }, BLOCKED_POLL_MS);
  }

  // Every edit re-renders, so render() is where editing is noticed.
  function keepSessionAlive(): void {
    if (session?.mine === true && !busy && Date.now() - lastCheckIn >= SESSION_HEARTBEAT_MS) {
      void refreshSession();
    }
  }

  function setStatus(text: string, next: StatusState): void {
    status = text;
    statusState = next;
    render();
  }

  async function runMutation(action: () => Promise<SaveResult>, verb: string): Promise<void> {
    busy = true;
    setStatus(`${verb}… panel rebooting to apply`, "busy");
    const result = await action();
    if (result.status === "applied") {
      state.cards = result.cards;
      state.accent = result.accent;
      state.pages = result.pages;
      state.gridCols = result.gridCols;
      state.gridRows = result.gridRows;
      state.etag = result.etag;
      if (state.selected !== null && state.selected >= state.cards.length) {
        state.selected = state.cards.length > 0 ? state.cards.length - 1 : null;
      }
      savedDeck = serialized();
      setStatus("Applied — panel updated", "ok");
    } else if (result.status === "conflict") {
      state.etag = result.etag;
      setStatus("Changed on the device — your edits are kept, press Save again", "warn");
    } else if (result.status === "locked") {
      setStatus("Locked — turn on the Configurator switch in Home Assistant, then Save again. Your edits are kept.", "warn");
    } else if (result.status === "taken") {
      setStatus("Another device is configuring this panel. Turn the Configurator switch off and on in Home Assistant to take over.", "err");
    } else {
      setStatus(`Error: ${result.message}`, "err");
    }
    busy = false;
    render();
    void refreshSession();
  }

  function onSave(): void {
    void runMutation(
      () => client.save(state.cards, state.etag, state.accent, state.pages, state.gridCols, state.gridRows),
      "Saving",
    );
  }

  // Push the deck accent into the page's CSS variables; -1 clears the override
  // back to the panel's built-in lime.
  function applyAccent(): void {
    const root = document.documentElement;
    if (state.accent < 0) {
      root.style.removeProperty("--accent");
      root.style.removeProperty("--accent-ink");
      return;
    }
    root.style.setProperty("--accent", hexColor(state.accent));
    // Text on accent-filled buttons follows the fill, as on the panel's tiles.
    root.style.setProperty("--accent-ink", isBright(state.accent) ? "#141414" : "#ffffff");
  }
  function onReset(): void { void runMutation(() => client.reset(state.etag), "Resetting"); }

  function el<K extends keyof HTMLElementTagNameMap>(
    tag: K, className?: string, text?: string,
  ): HTMLElementTagNameMap[K] {
    const node = document.createElement(tag);
    if (className !== undefined) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  function typeSelect(selected: number | null): HTMLSelectElement {
    const sel = el("select");
    // Present the types alphabetically; CARD_TYPES itself is the frozen wire
    // enum (ordered by value) and must not be reordered.
    const ordered = [...CARD_TYPES].sort((a, b) => a.label.localeCompare(b.label));
    for (const option of ordered) {
      const opt = el("option");
      opt.value = String(option.value);
      opt.textContent = option.label;
      // Types newer than the panel's firmware stay visible but can't be picked,
      // so the deck never gains a tile the panel would refuse.
      if (option.value > state.maxType) {
        opt.textContent = `${option.label} (needs a firmware update)`;
        opt.disabled = option.value !== selected;
      }
      if (option.value === selected) opt.selected = true;
      sel.append(opt);
    }
    return sel;
  }

  function fieldRow(labelText: string, control: HTMLElement, hint?: string): HTMLElement {
    const wrap = el("div", "field");
    wrap.append(el("span", "flabel", labelText));
    if (hint !== undefined) wrap.append(el("span", "fhint", hint));
    wrap.append(control);
    return wrap;
  }

  function renderForm(card: DeckCard, index: number): HTMLElement {
    const form = el("div", "form");

    const type = typeSelect(card.type);
    type.onchange = () => { updateCard(state, index, { type: Number(type.value) }); render(); };
    form.append(fieldRow("Type", type, "What this tile shows or controls."));

    const defs = cardFieldDefs(card.type);
    const titleIsLabel = defs.some((d) => d.key === "title" && !d.entity);

    // On committing the primary entity (picked or on blur), fill Title / Icon
    // from Home Assistant — only when blank, and only when Title is a display
    // label (not an entity, as on Header) so a subtitle entity is never clobbered.
    const autofillFromEntity = (entityId: string): void => {
      const meta = ha.meta(entityId.trim());
      if (meta === undefined) return;
      const cur = state.cards[index];
      if (cur === undefined) return;
      const patch: Partial<DeckCard> = {};
      if (titleIsLabel && cur.title.trim() === "" && meta.friendlyName !== "") patch.title = meta.friendlyName;
      if (cur.icon === "") {
        const glyph = iconGlyphForEntity(meta, entityId.trim());
        if (glyph !== "") patch.icon = glyph;
      }
      if (Object.keys(patch).length > 0) { updateCard(state, index, patch); render(); }
    };

    // Entity field with a datalist that lists ids STARTING WITH what's typed
    // (capped), so the DOM never holds thousands of <option>s at once.
    let listSeq = 0;
    const entityField = (
      labelText: string, value: string, hint: string,
      apply: (v: string) => Partial<DeckCard>, onCommit?: (v: string) => void,
    ): HTMLElement => {
      const input = el("input");
      input.type = "text";
      input.value = value;
      input.autocomplete = "off";
      const datalist = document.createElement("datalist");
      datalist.id = `ents-${index}-${listSeq++}`;
      input.setAttribute("list", datalist.id);
      const refresh = (): void => {
        datalist.textContent = "";
        for (const id of matchEntities(ha.entities(), input.value)) {
          const opt = document.createElement("option");
          opt.value = id;
          datalist.append(opt);
        }
      };
      input.oninput = () => { updateCard(state, index, apply(input.value)); refresh(); };
      if (onCommit !== undefined) input.onchange = () => { onCommit(input.value); };
      refresh();
      const holder = el("span", "typeahead");
      holder.append(input, datalist);
      return fieldRow(labelText, holder, hint);
    };

    const labelField = (
      labelText: string, value: string, hint: string, apply: (v: string) => Partial<DeckCard>,
    ): HTMLElement => {
      const input = el("input");
      input.type = "text";
      input.value = value;
      input.oninput = () => { updateCard(state, index, apply(input.value)); };
      return fieldRow(labelText, input, hint);
    };

    for (const def of defs) {
      // Clock: a city picker that stores the city's POSIX rule. Cities sharing a
      // rule (Toronto, New York) are told apart by the tile's title.
      if (card.type === 27 && def.key === "entity2") {
        const sel = el("select");
        const known = TIME_ZONES.some((z) => z.tz === card.entity2);
        const match = TIME_ZONES.findIndex((z) => z.tz === card.entity2 && z.city === card.title);
        const chosen = match >= 0 ? match : TIME_ZONES.findIndex((z) => z.tz === card.entity2);
        TIME_ZONES.forEach((z, i) => {
          const opt = el("option"); opt.value = String(i); opt.textContent = z.city;
          if (i === chosen) opt.selected = true;
          sel.append(opt);
        });
        if (!known) {  // a rule typed in by hand (or from a CLI spec) stays selectable
          const opt = el("option"); opt.value = "custom"; opt.textContent = `Custom: ${card.entity2}`;
          opt.selected = true;
          sel.append(opt);
        }
        sel.onchange = () => {
          if (sel.value === "custom") return;
          const z = TIME_ZONES[Number(sel.value)]!;
          const before = TIME_ZONES.find((t) => t.tz === card.entity2)?.city ?? "";
          // Name the tile after the city, unless it was given a name of its own.
          const retitle = card.title === "" || card.title === before;
          updateCard(state, index, { entity2: z.tz, ...(retitle ? { title: z.tz === "" ? "" : z.city } : {}) });
          render();
        };
        form.append(fieldRow(def.label, sel, def.hint));
        continue;
      }
      if (card.type === 21 && def.key === "entity2") {
        const sel = el("select");
        for (const name of state.pages) {
          const opt = el("option"); opt.value = name; opt.textContent = name || "Home";
          if (name === card.entity2) opt.selected = true;
          sel.append(opt);
        }
        sel.onchange = () => { updateCard(state, index, { entity2: sel.value }); };
        form.append(fieldRow(def.label, sel, def.hint));
        continue;
      }
      const value = fieldValue(card, def.key);
      const apply = (v: string): Partial<DeckCard> => patchForField(def.key, v);
      if (def.entity) {
        form.append(entityField(def.label, value, def.hint, apply,
          def.key === "entity" ? autofillFromEntity : undefined));
      } else {
        form.append(labelField(def.label, value, def.hint, apply));
      }
    }

    const numField = (
      labelText: string, value: number, hint: string, apply: (v: number) => Partial<DeckCard>,
    ): HTMLElement => {
      const input = el("input");
      input.type = "number";
      input.min = "1";
      input.max = "10";
      input.value = String(value);
      input.oninput = () => { const n = Number(input.value); if (n >= 1) updateCard(state, index, apply(n)); };
      return fieldRow(labelText, input, hint);
    };
    form.append(numField("Width", card.w,
      "Tile width in grid cells (1–10). Tip: drag the tile's corner in the layout above.",
      (v) => ({ w: v })));
    form.append(numField("Height", card.h,
      "Tile height in grid cells. The panel shows 6 rows; taller decks scroll.",
      (v) => ({ h: v })));

    const iconField = (
      labelText: string, glyph: string, hint: string, apply: (g: string) => Partial<DeckCard>,
    ): HTMLElement => {
      const picker = createIconPicker(glyph, (g) => { updateCard(state, index, apply(g)); });
      return fieldRow(labelText, picker.element, hint);
    };
    // Weather (7) draws a condition-driven glyph and repurposes iconAlt as the
    // forecast-low helper (a field above), so the icon pickers don't apply.
    if (card.type !== 7) {
      form.append(iconField("Icon", card.icon,
        "Search the icons the panel can show; only these render on the device. Blank = none.",
        (g) => ({ icon: g })));
      form.append(iconField("Icon (alt)", card.iconAlt,
        "Icon for the alternate state — light off, cover open, unlocked, etc. Blank = use the same icon.",
        (g) => ({ iconAlt: g })));
    }

    const colorField = (
      labelText: string, value: number, hint: string, apply: (v: number) => Partial<DeckCard>,
    ): HTMLElement => {
      const wrap = el("span", "color");
      const picker = el("input");
      picker.type = "color";
      picker.value = colorInputValue(value);
      picker.disabled = value < 0;
      const toggle = el("label");
      const useDefault = el("input");
      useDefault.type = "checkbox";
      useDefault.checked = value < 0;
      useDefault.onchange = () => {
        picker.disabled = useDefault.checked;
        updateCard(state, index, apply(useDefault.checked ? -1 : Number.parseInt(picker.value.slice(1), 16)));
      };
      picker.oninput = () => { updateCard(state, index, apply(Number.parseInt(picker.value.slice(1), 16))); };
      toggle.append(useDefault, document.createTextNode("card default"));
      wrap.append(picker, toggle);
      return fieldRow(labelText, wrap, hint);
    };
    form.append(colorField("Active colour", card.activeColor,
      "Tile tint when on / active (on, occupied, locked). “card default” keeps the built-in colour.",
      (v) => ({ activeColor: v })));
    form.append(colorField("Inactive colour", card.inactiveColor,
      "Tile tint when off / inactive (off, clear, unlocked).",
      (v) => ({ inactiveColor: v })));

    const flags = el("div", "flags");
    const flag = (
      labelText: string, value: boolean, hint: string, apply: (v: boolean) => Partial<DeckCard>,
    ): void => {
      const label = el("label", "flag");
      const input = el("input");
      input.type = "checkbox";
      input.checked = value;
      input.onchange = () => { updateCard(state, index, apply(input.checked)); };
      const textWrap = el("span", "flag-text");
      textWrap.append(el("span", "flag-name", labelText), el("span", "fhint", hint));
      label.append(input, textWrap);
      flags.append(label);
    };
    for (const def of cardFlagDefs(card.type)) {
      flag(def.label, flagValue(card, def.key), def.hint, (v) => patchForFlag(def.key, v));
    }
    form.append(flags);

    // Blank tiles: icon/text alignment within the tile.
    if (card.type === 0) {
      const alignSelect = (
        labelText: string, hint: string, current: number, opts: string[],
        apply: (v: number) => Partial<DeckCard>,
      ): HTMLElement => {
        const sel = el("select");
        opts.forEach((label, value) => {
          const opt = el("option");
          opt.value = String(value);
          opt.textContent = label;
          if (value === current) opt.selected = true;
          sel.append(opt);
        });
        sel.onchange = () => { updateCard(state, index, apply(Number(sel.value))); };
        return fieldRow(labelText, sel, hint);
      };
      const liveAlign = (): number => state.cards[index]?.align ?? 0;
      form.append(alignSelect("Horizontal align",
        "Where the icon + text sit across the tile.", card.align & 0x3,
        ["Left", "Centre", "Right"], (v) => ({ align: (liveAlign() & ~0x3) | (v & 0x3) })));
      form.append(alignSelect("Vertical align",
        "Where the icon + text sit down the tile.", (card.align >> 2) & 0x3,
        ["Top", "Centre", "Bottom"], (v) => ({ align: (liveAlign() & ~0xc) | ((v & 0x3) << 2) })));
    }

    return form;
  }

  // Pages, vertically; the current one can be renamed or deleted (not Home).
  function renderPages(): HTMLElement {
    const sec = el("section", "side-sec");
    sec.append(el("h2", "side-cap", "Pages"));
    const list = el("div", "page-list");
    state.pages.forEach((name, i) => {
      const count = state.cards.filter((c) => c.page === i).length;
      const chip = el("button", "page-item" + (i === state.currentPage ? " active" : ""));
      chip.append(el("span", "page-name", name || "Home"), el("span", "page-count", String(count)));
      chip.onclick = () => { state.currentPage = i; state.selected = null; render(); };
      list.append(chip);
    });
    const add = el("button", "page-item add", "+ New page");
    add.onclick = () => {
      addPage(state, `Page ${state.pages.length}`);
      addCard(state, 22);  // new pages start with a Back tile so they aren't dead ends
      render();
    };
    list.append(add);
    sec.append(list);
    if (state.currentPage > 0) {
      const rename = el("input", "page-rename");
      rename.type = "text";
      rename.value = state.pages[state.currentPage] ?? "";
      rename.setAttribute("aria-label", "Page name");
      rename.onchange = () => { renamePage(state, state.currentPage, rename.value.trim()); render(); };
      const del = el("button", "ghost danger", "Delete page");
      del.onclick = () => { deletePage(state, state.currentPage); render(); };
      const row = el("div", "page-edit");
      row.append(rename, del);
      sec.append(row);
    }
    return sec;
  }

  // Deck-wide settings: grid size and the accent colour.
  function renderDeck(): HTMLElement {
    const sec = el("section", "side-sec");
    sec.append(el("h2", "side-cap", "Deck"));
    const { maxCols, maxRows } = gridLimits(state.displayW, state.displayH);
    const gridWrap = el("label", "grid-picker");
    const mkNum = (val: number, min: number, max: number, apply: (n: number) => void, label: string): HTMLInputElement => {
      const i = el("input");
      i.type = "number"; i.min = String(min); i.max = String(max); i.value = String(val);
      i.setAttribute("aria-label", label);
      i.oninput = () => { const n = Number(i.value); if (n >= min && n <= max) { apply(n); render(); } };
      return i;
    };
    gridWrap.append(
      el("span", "flabel", "Grid"),
      mkNum(state.gridCols, 2, maxCols, (n) => { state.gridCols = n; }, "Columns"),
      el("span", "grid-x", "×"),
      mkNum(state.gridRows, 2, maxRows, (n) => { state.gridRows = n; }, "Visible rows"),
    );
    gridWrap.title = `Columns × visible rows (max ${maxCols}×${maxRows} for this display). Shrinking may push tiles off-grid — use Auto-arrange.`;
    const accentWrap = el("label", "accent-picker");
    accentWrap.title = "Accent colour for lit tiles, on the panel and in this preview";
    const accentInput = el("input");
    accentInput.type = "color";
    accentInput.value = state.accent < 0 ? "#b5e853" : "#" + (state.accent & 0xffffff).toString(16).padStart(6, "0");
    accentInput.oninput = () => {
      state.accent = Number.parseInt(accentInput.value.slice(1), 16);
      applyAccent();
      renderPreview();
    };
    const accentReset = el("button", "ghost small", "Reset");
    accentReset.title = "Back to the panel's lime";
    accentReset.onclick = () => { state.accent = -1; applyAccent(); render(); };
    accentWrap.append(el("span", "flabel", "Accent"), accentInput, accentReset);
    sec.append(gridWrap, accentWrap);
    return sec;
  }

  function renderHaBar(): HTMLElement {
    const bar = el("details", "ha-bar side-sec");
    const st = ha.status();
    bar.open = st.state !== "connected";

    const summary = document.createElement("summary");
    const dot = el("span", "ha-dot");
    dot.dataset["state"] = st.state;
    let text = "Home Assistant · not connected";
    if (st.state === "connecting") text = "Home Assistant · connecting…";
    else if (st.state === "connected") text = `Home Assistant · ${st.count} entities`;
    else if (st.state === "error") text = "Home Assistant · connection problem";
    summary.append(dot, el("span", undefined, text));
    bar.append(summary);

    const fields = el("div", "ha-fields");
    const creds = ha.creds();
    const url = el("input", "ha-url");
    url.type = "text";
    url.placeholder = "http://homeassistant.local:8123";
    url.value = creds.baseUrl;
    const token = el("input", "ha-token");
    token.type = "password";
    token.placeholder = "Long-lived access token";
    token.value = creds.token;
    const connect = el("button", "primary", "Connect");
    connect.onclick = () => { void ha.connect(url.value, token.value).then(() => { render(); }); };
    fields.append(url, token, connect);
    bar.append(fields);

    if (st.state === "error") {
      bar.append(el("div", "ha-error", st.message));
    }
    return bar;
  }

  // Save and Reset only exist while this browser may write. Locked, or held by
  // another device, the slot shows why instead; clicking it checks again.
  function renderSaveArea(): HTMLElement[] {
    const s = session;
    const blocked = s !== null && (s.state === "locked" || (s.state === "claimed" && !s.mine));
    if (blocked) {
      const locked = s.state === "locked";
      const notice = el("button", "lock-notice");
      notice.type = "button";
      notice.dataset["state"] = locked ? "locked" : "taken";
      notice.title = "Click to check again";
      notice.onclick = () => { void refreshSession(); };
      notice.append(
        iconSvg(UI_ICON_PATHS[locked ? "lock" : "account-lock"] ?? ""),
        el("span", "lock-text",
          locked ? "Locked · turn on Configurator in Home Assistant" : "In use from another device"),
      );
      return [notice];
    }

    const saveBtn = el("button", "primary", "Save");
    saveBtn.disabled = busy;
    saveBtn.onclick = onSave;
    if (s?.state === "open") saveBtn.title = "Saving claims this panel for this browser";
    else if (s?.mine === true && s.timeoutMinutes > 0)
      saveBtn.title = `Your session · locks after ${s.timeoutMinutes} min without use`;
    const resetBtn = el("button", "ghost", "Reset");
    resetBtn.disabled = busy;
    resetBtn.onclick = onReset;
    return [saveBtn, resetBtn];
  }

  // Brand and panel on the left; backup, status and Save on the right.
  function renderHeader(): HTMLElement {
    const bar = el("header", "app-head");
    const brand = el("div", "brand");
    brand.append(el("span", "mark"), el("span", "brand-name", "tilehaus"),
      el("span", "brand-host", window.location.hostname || "panel"));
    const pill = el("div", "status");
    pill.dataset["state"] = statusState;
    pill.append(el("span", "dot"), document.createTextNode(status));
    const exportBtn = el("button", "ghost", "Export");
    exportBtn.title = "Download this deck as a JSON backup (also pushable with deck_cli.js)";
    exportBtn.onclick = onExport;
    const importBtn = el("button", "ghost", "Import");
    importBtn.title = "Load a JSON backup into the editor; nothing reaches the panel until Save";
    importBtn.disabled = busy;
    importBtn.onclick = () => { importInput.click(); };
    const actions = el("div", "actions");
    actions.append(pill, exportBtn, importBtn, ...renderSaveArea());
    bar.append(brand, actions);
    return bar;
  }

  // Adding and arranging tiles, over the preview.
  function renderToolbar(): HTMLElement {
    const bar = el("div", "toolbar");
    const picker = typeSelect(null);
    picker.setAttribute("aria-label", "Tile type to add");
    const addBtn = el("button", "primary-ink", "+ Add tile");
    addBtn.onclick = () => { addCard(state, Number(picker.value)); render(); };
    undoBtn = el("button", "ghost", "Undo");
    undoBtn.title = "Undo (Ctrl+Z)";
    undoBtn.onclick = onUndo;
    redoBtn = el("button", "ghost", "Redo");
    redoBtn.title = "Redo (Ctrl+Shift+Z)";
    redoBtn.onclick = onRedo;
    const arrangeBtn = el("button", "ghost", "Auto-arrange");
    arrangeBtn.title = "Repack tiles to the top-left, clearing gaps";
    arrangeBtn.onclick = () => { autoArrange(state); render(); };
    const seg = el("div", "seg");
    seg.title = "Preview the page in day or night mode";
    for (const [label, night] of [["Day", false], ["Night", true]] as const) {
      const b = el("button", previewNight === night ? "sel" : "", label);
      b.onclick = () => { previewNight = night; render(); };
      seg.append(b);
    }
    bar.append(picker, addBtn, undoBtn, redoBtn, arrangeBtn, el("div", "spacer"), seg);
    return bar;
  }

  function previewCaption(): string {
    const page = state.pages[state.currentPage] || "Home";
    return `${page} · ${state.gridCols} × ${state.gridRows} · ${state.displayW} × ${state.displayH}`;
  }

  function renderPreview(): void {
    if (previewSlot === null) return;
    previewSlot.textContent = "";
    previewSlot.append(createGridPane(state, render, { ha, night: previewNight }));
  }

  function renderFormPane(): HTMLElement {
    const pane = el("aside", "inspector");
    const sel = state.selected !== null ? state.cards[state.selected] : undefined;
    pane.append(el("h2", "side-cap", sel ? `Tile · ${CARD_TYPES.find((t) => t.value === sel.type)?.label ?? ""}` : "Tile"));
    if (state.selected !== null) {
      const card = state.cards[state.selected];
      if (card !== undefined) {
        pane.append(renderForm(card, state.selected));
        const del = el("button", "ghost danger", "Delete tile");
        del.onclick = () => {
          if (state.selected === null) return;
          removeCard(state, state.selected);
          render();
        };
        pane.append(del);
        return pane;
      }
    }
    pane.append(el("p", "no-selection", "Select a tile in the preview to edit it, or add one from the toolbar."));
    return pane;
  }

  function render(): void {
    if (savedDeck === null) savedDeck = serialized();  // first render: the deck as loaded
    keepSessionAlive();
    applyAccent();
    root.textContent = "";
    const side = el("nav", "sidebar");
    side.append(renderPages(), renderDeck(), renderHaBar());
    const main = el("main", "stage");
    previewSlot = el("div", "preview-slot");
    main.append(renderToolbar(), el("div", "pv-cap", previewCaption()), previewSlot);
    const shell = el("div", "app");
    shell.append(renderHeader(), side, main, renderFormPane());
    root.append(shell);
    renderPreview();
    syncHistory();  // after the toolbar exists, so Undo/Redo get their state
  }

  return { render, refreshSession };
}

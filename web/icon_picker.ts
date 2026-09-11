import { ICON_CATALOG, glyphToEntry, type IconEntry } from "./icon_catalog";

export interface IconPicker {
  element: HTMLElement;
}

// A filter-as-you-type picker over the bundled MDI glyphs. Stores the glyph
// string (blank = none); previews via the @mdi/font CDN classes in index.html.
export function createIconPicker(
  currentGlyph: string,
  onSelect: (glyph: string) => void,
): IconPicker {
  let current = currentGlyph;

  const wrap = document.createElement("div");
  wrap.className = "icon-picker";

  const field = document.createElement("div");
  field.className = "icon-picker-field";

  const preview = document.createElement("span");
  preview.className = "icon-picker-preview mdi";

  const input = document.createElement("input");
  input.type = "text";
  input.className = "icon-picker-input";
  input.placeholder = "Search icons… (blank = none)";

  field.append(preview, input);

  const dropdown = document.createElement("div");
  dropdown.className = "icon-dropdown";

  wrap.append(field, dropdown);

  interface Row {
    el: HTMLElement;
    glyph: string;
    slug: string;
    haystack: string; // "" for the none row so it always matches
  }
  const rows: Row[] = [];
  let highlighted = -1;

  function makeRow(glyph: string, slug: string, label: string, haystack: string): Row {
    const el = document.createElement("div");
    el.className = "icon-option";
    const g = document.createElement("span");
    g.className = "icon-option-glyph mdi" + (slug === "" ? " icon-option-glyph--none" : " mdi-" + slug);
    if (slug === "") g.textContent = "∅";
    const name = document.createElement("span");
    name.className = "icon-option-label";
    name.textContent = label;
    el.append(g, name);
    const row: Row = { el, glyph, slug, haystack };
    el.addEventListener("mousedown", (e) => {
      e.preventDefault();
      choose(row);
    });
    return row;
  }

  rows.push(makeRow("", "", "— none —", ""));
  for (const entry of ICON_CATALOG as readonly IconEntry[]) {
    rows.push(makeRow(entry.glyph, entry.slug, entry.name, entry.slug + " " + entry.name));
  }

  const emptyEl = document.createElement("div");
  emptyEl.className = "icon-option icon-option--empty";
  emptyEl.textContent = "No matches";
  emptyEl.style.display = "none";

  const frag = document.createDocumentFragment();
  for (const row of rows) frag.append(row.el);
  frag.append(emptyEl);
  dropdown.append(frag);

  function setCurrent(glyph: string): void {
    current = glyph;
    const entry = glyph === "" ? undefined : glyphToEntry(glyph);
    preview.className = "icon-picker-preview mdi" + (entry ? " mdi-" + entry.slug : " icon-picker-preview--none");
    preview.textContent = entry ? "" : glyph === "" ? "∅" : "?";
    input.value = entry ? entry.name : glyph === "" ? "" : "(unknown glyph)";
    for (const row of rows) row.el.classList.toggle("is-active", row.glyph === glyph);
  }

  function filter(query: string): void {
    const q = query.trim().toLowerCase();
    highlighted = -1;
    let anyMatch = false;
    for (const row of rows) {
      const match = q === "" || row.haystack.includes(q);
      row.el.style.display = match ? "" : "none";
      row.el.classList.remove("is-highlighted");
      if (match) anyMatch = true;
    }
    emptyEl.style.display = anyMatch ? "none" : "";
  }

  function visibleRows(): Row[] {
    return rows.filter((r) => r.el.style.display !== "none");
  }

  function highlightAt(idx: number): void {
    const vis = visibleRows();
    if (vis.length === 0) return;
    for (const row of rows) row.el.classList.remove("is-highlighted");
    let next = idx;
    if (next < 0) next = vis.length - 1;
    if (next >= vis.length) next = 0;
    highlighted = next;
    const row = vis[next];
    if (row === undefined) return;
    row.el.classList.add("is-highlighted");
    row.el.scrollIntoView({ block: "nearest" });
  }

  function open(): void {
    input.value = "";
    filter("");
    wrap.classList.add("is-open");
  }

  function close(): void {
    wrap.classList.remove("is-open");
    setCurrent(current);
    highlighted = -1;
  }

  function choose(row: Row): void {
    setCurrent(row.glyph);
    wrap.classList.remove("is-open");
    highlighted = -1;
    onSelect(row.glyph);
  }

  input.addEventListener("focus", open);
  input.addEventListener("blur", close);
  input.addEventListener("input", () => {
    filter(input.value);
    if (visibleRows().length > 0) highlightAt(0);
  });
  input.addEventListener("keydown", (e) => {
    if (e.key === "ArrowDown") {
      e.preventDefault();
      if (!wrap.classList.contains("is-open")) { open(); return; }
      highlightAt(highlighted + 1);
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      highlightAt(highlighted - 1);
    } else if (e.key === "Enter") {
      e.preventDefault();
      const vis = visibleRows();
      const row = highlighted >= 0 ? vis[highlighted] : undefined;
      if (row !== undefined) choose(row);
    } else if (e.key === "Escape") {
      e.preventDefault();
      close();
      input.blur();
    }
  });

  setCurrent(current);
  return { element: wrap };
}

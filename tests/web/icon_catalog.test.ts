import { ICON_CATALOG, filterIcons, glyphToEntry } from "../../web/icon_catalog";

function assert(cond: boolean, message: string): void {
  if (!cond) throw new Error(message);
}

export function runIconCatalogTests(): void {
  // Catalog is populated and sorted by slug.
  assert(ICON_CATALOG.length > 300, "catalog has the bundled glyph subset");
  for (let i = 1; i < ICON_CATALOG.length; i += 1) {
    const prev = ICON_CATALOG[i - 1]!;
    const cur = ICON_CATALOG[i]!;
    assert(prev.slug <= cur.slug, "catalog sorted by slug");
  }

  // A known glyph round-trips: lightbulb = F0335.
  const bulb = ICON_CATALOG.find((e) => e.slug === "lightbulb");
  assert(bulb !== undefined, "lightbulb present");
  assert(bulb!.hex === "F0335", "lightbulb hex F0335");
  assert(bulb!.glyph === String.fromCodePoint(0xf0335), "lightbulb glyph matches hex");
  assert(glyphToEntry(bulb!.glyph)?.slug === "lightbulb", "glyphToEntry resolves lightbulb");
  assert(glyphToEntry("") === undefined, "empty glyph resolves to nothing");

  // filterIcons: empty query returns all; substring matches slug and name.
  assert(filterIcons("").length === ICON_CATALOG.length, "empty query returns all");
  const lit = filterIcons("lightbulb");
  assert(lit.some((e) => e.slug === "lightbulb"), "filter finds lightbulb by slug");
  assert(filterIcons("LIGHTBULB").length === lit.length, "filter is case-insensitive");
  assert(filterIcons("zzzznomatch").length === 0, "no matches for nonsense");
}

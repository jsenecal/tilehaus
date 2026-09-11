import { cardFieldDefs, patchForField, fieldValue, matchEntities } from "../../web/card_fields";
import { type DeckCard } from "../../web/model/deck";

function assert(cond: boolean, message: string): void {
  if (!cond) throw new Error(message);
}

export function runCardFieldsTests(): void {
  // Default card (e.g. Toggle=2): Entity + Title label, no Entity 2.
  {
    const defs = cardFieldDefs(2);
    assert(defs.length === 2, "default has two fields");
    assert(defs[0]!.key === "entity" && defs[0]!.entity, "default entity is an entity field");
    assert(defs[1]!.key === "title" && !defs[1]!.entity, "default title is a display label");
    assert(!defs.some((d) => d.key === "entity2"), "default hides entity2");
  }

  // Header (11): all three are entity ids, ordered greeting → subtitle → weather.
  {
    const defs = cardFieldDefs(11);
    assert(defs.map((d) => d.key).join(",") === "entity,title,entity2", "header field order");
    assert(defs.every((d) => d.entity), "header treats every field as an entity");
    const title = defs.find((d) => d.key === "title")!;
    assert(title.entity, "header title is an entity (subtitle), not a label");
  }

  // Weather (7): Entity + forecast-high (entity2) + forecast-low (iconAlt) + Title.
  {
    const defs = cardFieldDefs(7);
    assert(defs.map((d) => d.key).join(",") === "entity,entity2,iconAlt,title", "weather field order");
    assert(defs.find((d) => d.key === "entity2")!.entity, "weather entity2 (high) is an entity");
    assert(defs.find((d) => d.key === "iconAlt")!.entity, "weather iconAlt (low) is an entity");
    assert(!defs.find((d) => d.key === "title")!.entity, "weather title is a label");
  }

  // Page (21): tracked entity + a Target page field.
  {
    const defs = cardFieldDefs(21);
    assert(defs.find((d) => d.key === "entity")!.entity, "Page tracked entity is an entity field");
    assert(defs.some((d) => d.key === "entity2" && d.label === "Target page"), "Page has a Target page field");
  }

  // Back (22): just an optional title, no entity/target fields.
  {
    const defs = cardFieldDefs(22);
    assert(defs.length === 1 && defs[0]!.key === "title", "Back has only a Title field");
  }

  // patchForField / fieldValue map keys to the right DeckCard property.
  {
    assert(JSON.stringify(patchForField("entity", "x")) === JSON.stringify({ entity: "x" }), "patch entity");
    assert(JSON.stringify(patchForField("entity2", "y")) === JSON.stringify({ entity2: "y" }), "patch entity2");
    assert(JSON.stringify(patchForField("iconAlt", "w")) === JSON.stringify({ iconAlt: "w" }), "patch iconAlt");
    assert(JSON.stringify(patchForField("title", "z")) === JSON.stringify({ title: "z" }), "patch title");
    const card = { entity: "e", entity2: "e2", iconAlt: "ia", title: "t" } as DeckCard;
    assert(fieldValue(card, "entity") === "e" && fieldValue(card, "entity2") === "e2" && fieldValue(card, "iconAlt") === "ia" && fieldValue(card, "title") === "t", "fieldValue reads the right field");
  }

  // matchEntities: prefix match, case-insensitive, empty = none, capped.
  {
    const ids = ["light.a", "light.b", "lock.door", "switch.x"];
    assert(JSON.stringify(matchEntities(ids, "light.")) === JSON.stringify(["light.a", "light.b"]), "prefix match");
    assert(JSON.stringify(matchEntities(ids, "LIGHT")) === JSON.stringify(["light.a", "light.b"]), "case-insensitive");
    assert(matchEntities(ids, "door").length === 0, "not a substring match");
    assert(matchEntities(ids, "").length === 0, "empty query returns nothing");
    const many = Array.from({ length: 100 }, (_, i) => `sensor.s${i}`);
    assert(matchEntities(many, "sensor", 50).length === 50, "capped at limit");
  }
}

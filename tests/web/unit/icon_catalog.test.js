"use strict";

const { describe, test } = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

describe("icon catalog", () => {
  const { runIconCatalogTests } = loadTypescriptTest("tests/web/icon_catalog.test.ts");
  test("bundled glyph subset parses, sorts, and filters", () => {
    runIconCatalogTests();
  });
});

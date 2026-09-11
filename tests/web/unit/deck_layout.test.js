"use strict";

const { describe, test } = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

describe("deck layout", () => {
  const { runLayoutTests } = loadTypescriptTest("tests/web/deck_layout.test.ts");
  test("placeTiles mirrors place_tiles + reorderCard", () => {
    runLayoutTests();
  });
});

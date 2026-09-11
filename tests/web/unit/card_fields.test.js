"use strict";

const { describe, test } = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

describe("card fields", () => {
  const { runCardFieldsTests } = loadTypescriptTest("tests/web/card_fields.test.ts");
  test("type-aware field specs for default / Header / Weather", () => {
    runCardFieldsTests();
  });
});

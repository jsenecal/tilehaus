"use strict";

const { describe, test } = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

describe("deck configuration codec", () => {
  const { runDeckCodecTests } = loadTypescriptTest("tests/web/deck.test.ts");

  test("round-trips cards and rejects malformed documents", () => {
    runDeckCodecTests();
  });
});

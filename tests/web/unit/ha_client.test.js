"use strict";

const { describe, test } = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

describe("home assistant client", () => {
  const { runHaClientTests } = loadTypescriptTest("tests/web/ha_client.test.ts");
  test("connect fetches, caches, persists, and classifies errors", async () => {
    await runHaClientTests();
  });
});

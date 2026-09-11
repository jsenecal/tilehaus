"use strict";

const { describe, test } = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

describe("deck config client", () => {
  const { runDeckClientTests } = loadTypescriptTest("tests/web/deck_client.test.ts");
  test("load/save/reset with If-Match, ETag, and reboot-wait", async () => {
    await runDeckClientTests();
  });
});

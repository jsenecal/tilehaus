"use strict";

const { describe, test } = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

describe("configurator preview", () => {
  const { runTilePreviewTests } = loadTypescriptTest("tests/web/tile_preview.test.ts");
  test("tile colours, value split and the panel's grid arithmetic", () => {
    runTilePreviewTests();
  });
});

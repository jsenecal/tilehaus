"use strict";

const { describe, test } = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

describe("icon from entity", () => {
  const { runIconFromEntityTests } = loadTypescriptTest("tests/web/icon_from_entity.test.ts");
  test("resolves HA icon, domain defaults, and device_class refinements", () => {
    runIconFromEntityTests();
  });
});

"use strict";

const { describe, test } = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

describe("deck editor state", () => {
  const { runEditorStateTests } = loadTypescriptTest("tests/web/deck_editor_state.test.ts");
  test("mutations add/remove/move/update with selection + cap", () => {
    runEditorStateTests();
  });
});

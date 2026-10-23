"use strict";

const { describe, test } = require("node:test");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

describe("undo history and deck backups", () => {
  const { runHistoryBackupTests } = loadTypescriptTest("tests/web/history_backup.test.ts");
  test("history steps, coalescing, export/import validation", () => {
    runHistoryBackupTests();
  });
});

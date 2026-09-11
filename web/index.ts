import { createDeckClient } from "./deck_client";
import { createHaClient } from "./ha_client";
import { createEditorState } from "./editor_state";
import { createEditorView } from "./editor_view";

async function main(): Promise<void> {
  const root = document.getElementById("app");
  if (root === null) return;
  root.textContent = "Loading…";

  const client = createDeckClient();
  const ha = createHaClient();
  const state = createEditorState();
  const view = createEditorView(root, state, client, ha);

  try {
    const loaded = await client.load();
    state.cards = loaded.cards;
    state.accent = loaded.accent;
    state.pages = loaded.pages;
    state.gridCols = loaded.gridCols;
    state.gridRows = loaded.gridRows;
    state.displayW = loaded.displayW;
    state.displayH = loaded.displayH;
    state.etag = loaded.etag;
    state.selected = loaded.cards.length > 0 ? 0 : null;
  } catch (error) {
    root.textContent = `Failed to load config: ${String(error)}`;
    return;
  }

  const creds = ha.creds();
  if (creds.baseUrl !== "" && creds.token !== "") {
    void ha.connect(creds.baseUrl, creds.token).then(() => { view.render(); });
  }
  window.addEventListener("resize", () => { view.render(); });
  view.render();
}

void main();

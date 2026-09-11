---
name: docs
description: Build or live-preview the Tilehaus Zensical documentation site. Use when editing docs/ pages or checking how the docs render.
---

# Tilehaus docs (Zensical)

Docs content is Markdown under `docs/` (the `docs_dir`); the nav is in `mkdocs.yml`;
output is `site/` (git-ignored). Internal planning docs live in `planning/`, outside
`docs_dir`, and are not published.

Zensical runs from a Python venv (not pinned by mise). One-time:
```bash
python3 -m venv .venv && .venv/bin/pip install zensical
```

- **Build**: `.venv/bin/zensical build`  (or `npm run docs:build` with zensical on PATH) → writes `site/`.
- **Preview**: `.venv/bin/zensical serve`  (or `npm run docs:serve`) → live-reloading local server; open the URL it prints.

When you change firmware/web behaviour, update the matching page in `docs/` in the
same change. CI (`.github/workflows/docs.yml`) builds + deploys to GitHub Pages on
push to `main`.

const SVG_NS = "http://www.w3.org/2000/svg";

// An MDI icon as inline SVG: 1em square, filled with the current text colour,
// so it sizes and colours like the webfont glyph it replaces. Decorative —
// every caller labels the control in text.
export function iconSvg(path: string): SVGSVGElement {
  const svg = document.createElementNS(SVG_NS, "svg");
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("aria-hidden", "true");
  svg.setAttribute("class", "icon");
  const p = document.createElementNS(SVG_NS, "path");
  p.setAttribute("d", path);
  svg.append(p);
  return svg;
}

// Modèle de positionnement : ancrage LVGL + offset (dx, dy). Pur, sans DOM.
// parent = carré 360×360 ; le widget s'aligne par le même "point d'ancrage" que le parent.
export const ANCHORS = ['CENTER','TOP_MID','BOTTOM_MID','LEFT_MID','RIGHT_MID','TOP_LEFT','TOP_RIGHT','BOTTOM_LEFT','BOTTOM_RIGHT'];
export const SCREEN = 360;

const P = {
  CENTER:[180,180], TOP_MID:[180,0], BOTTOM_MID:[180,360], LEFT_MID:[0,180], RIGHT_MID:[360,180],
  TOP_LEFT:[0,0], TOP_RIGHT:[360,0], BOTTOM_LEFT:[0,360], BOTTOM_RIGHT:[360,360]
};

export function parentPoint(anchor) { return P[anchor]; }

export function widgetPoint(anchor, x, y, w, h) {
  const px = anchor.includes('LEFT') ? x : anchor.includes('RIGHT') ? x + w : x + w / 2;
  const py = anchor.startsWith('TOP') ? y : anchor.startsWith('BOTTOM') ? y + h : y + h / 2;
  return [px, py];
}

// anchor doit être un membre de ANCHORS (pas de garde : enum interne contrôlé).
export function offsetFor(anchor, x, y, w, h) {
  const [wx, wy] = widgetPoint(anchor, x, y, w, h);
  return [Math.round(wx - P[anchor][0]), Math.round(wy - P[anchor][1])];
}

export function nearestAnchor(x, y, w, h) {
  let best = null, bd = Infinity;
  // Égalité départagée par l'ordre de ANCHORS (CENTER d'abord) ; ties pixel-exacts rares sur 360.
  for (const a of ANCHORS) {
    const [dx, dy] = offsetFor(a, x, y, w, h);
    const d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = a; }
  }
  return best;
}

export function snapPlacement(x, y, w, h, snap = 16) {
  const anchor = nearestAnchor(x, y, w, h);
  let [dx, dy] = offsetFor(anchor, x, y, w, h);
  const snapped = Math.hypot(dx, dy) < snap;
  if (snapped) { dx = 0; dy = 0; }
  return { anchor, dx, dy, snapped };
}

export function placeAt(anchor, dx, dy, w, h) {
  const px = P[anchor][0] + dx, py = P[anchor][1] + dy;
  const x = px - (anchor.includes('LEFT') ? 0 : anchor.includes('RIGHT') ? w : w / 2);
  const y = py - (anchor.startsWith('TOP') ? 0 : anchor.startsWith('BOTTOM') ? h : h / 2);
  return { x: Math.round(x), y: Math.round(y) };
}

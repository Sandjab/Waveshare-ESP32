// Rendu best-effort des widgets — 2e implémentation du rendu firmware (src/dashboard.cpp + src/view.cpp).
// ⚠ Double-maintenance assumée : tout changement de rendu firmware doit être répliqué ici. Le device arbitre.
// La math (ci-dessous) est pure et testée ; les builders DOM (plus bas) sont vérifiés au navigateur.

// Valeurs d'aperçu mock par défaut. Plan C les rendra éditables via l'inspecteur ; ici elles sont fixes.
export const MOCKS = {
  readout: { value: 42 },
  bar:     { value: 60 },
  ring:    { value: 72, reset_in_s: 18000 }
};

// Police LVGL embarquée : 14/20/28 px (pick_font, view.cpp:19). Toute autre valeur retombe sur 14.
export function pickFontPx(font) {
  if (font >= 28) return 28;
  if (font >= 20) return 20;
  return 14;
}

// bar : fraction remplie (clampée). Miroir lv_bar : (value − min) / (max − min).
export function barFill(value, min = 0, max = 100) {
  if (max === min) return 0;
  return Math.max(0, Math.min(1, (value - min) / (max - min)));
}

// ring : couleur de seuil — 1er seuil dont value < limite, sinon couleur de base. Miroir threshold_color (color.cpp:13).
export function pickThresholdColor(thresholds, value, base) {
  for (const [limit, color] of thresholds || []) {
    if (value < limit) return color;
  }
  return base;
}

// readout : "<num> <unit>". Miroir format_value (format.cpp:19) : entier brut sinon 1 décimale.
export function formatValue(v, unit) {
  const num = Number.isInteger(v) ? String(v) : v.toFixed(1);
  return unit ? `${num} ${unit}` : num;
}

// ring countdown : reste formaté. Miroir format_remaining (format.cpp:5).
export function formatRemaining(s) {
  if (s >= 86400) return `${Math.floor(s / 86400)}j${Math.floor((s % 86400) / 3600)}h`;
  if (s >= 3600)  return `${Math.floor(s / 3600)}h${String(Math.floor((s % 3600) / 60)).padStart(2, '0')}`;
  if (s >= 60)    return `${Math.floor(s / 60)}m`;
  return `${s}s`;
}

// ring : balayage (deg) de l'indicateur = fraction × (360 − gap). L'ouverture (gap) reste en bas.
export function ringSweepDeg(value, min, max, gapDeg) {
  return barFill(value, min, max) * (360 - gapDeg);
}

// Point sur un cercle, convention écran (0°=droite, 90°=bas car y vers le bas) — identique à LVGL.
export function pointOnArc(cx, cy, r, deg) {
  const rad = deg * Math.PI / 180;
  return [cx + r * Math.cos(rad), cy + r * Math.sin(rad)];
}

// Chemin SVG d'un arc : centre (cx,cy), rayon r, de startDeg, balayé de sweepDeg dans le sens horaire écran.
export function arcPath(cx, cy, r, startDeg, sweepDeg) {
  const [x1, y1] = pointOnArc(cx, cy, r, startDeg);
  const [x2, y2] = pointOnArc(cx, cy, r, startDeg + sweepDeg);
  const large = sweepDeg > 180 ? 1 : 0;
  const f = n => n.toFixed(2);
  return `M ${f(x1)} ${f(y1)} A ${r} ${r} 0 ${large} 1 ${f(x2)} ${f(y2)}`;
}

// ring : chemins fond + indicateur (rayon de tracé au milieu de la bande). Centralise la géométrie
// d'arc partagée par buildRing (initial) et canvas.js paintRing (live resize). Miroir view.cpp:54.
export function ringPaths(r, th, gap, value, min, max) {
  const rr = r - th / 2;           // rayon au centre de la bande
  const start = 90 + gap / 2;      // lv_arc_set_bg_angles(arc, 90 + gap/2, 90 − gap/2)
  return {
    rr, start,
    track:     arcPath(r, r, rr, start, 360 - gap),
    indicator: arcPath(r, r, rr, start, ringSweepDeg(value, min, max, gap))
  };
}

// --- Builders DOM (non testés sous Node ; vérifiés au navigateur). Aucun ne s'exécute à l'import. ---

const FONT = px => `${px}px Montserrat, system-ui, sans-serif`;
const SVGNS = 'http://www.w3.org/2000/svg';

export function buildLabel(comp) {
  const n = document.createElement('div');
  n.className = 'w w-label';
  n.style.font = FONT(pickFontPx(comp.font ?? 20));
  n.style.color = comp.color || '#FFFFFF';
  n.textContent = comp.text || 'Label';
  return n;
}

export function buildReadout(comp, mock = MOCKS.readout) {
  const n = document.createElement('div');
  n.className = 'w w-readout';
  n.style.font = FONT(pickFontPx(comp.font ?? 20));
  n.style.color = comp.color || '#FFFFFF';
  const val = formatValue(mock.value, comp.unit || '');
  n.textContent = comp.label ? `${comp.label} ${val}` : val; // miroir view.cpp:201-209
  return n;
}

export function buildBar(comp, placement, mock = MOCKS.bar) {
  const wrap = document.createElement('div');
  wrap.className = 'w w-bar';
  if (comp.label) {                          // label au-dessus (view.cpp:137-144)
    const lbl = document.createElement('div');
    lbl.className = 'w-bar-label';
    lbl.textContent = comp.label;
    wrap.appendChild(lbl);
  }
  const track = document.createElement('div');
  track.className = 'w-bar-track';
  track.style.width  = (placement.width  || 200) + 'px'; // défauts firmware (view.cpp:132)
  track.style.height = (placement.height || 16)  + 'px';
  const fill = document.createElement('div');
  fill.className = 'w-bar-fill';
  fill.style.width = (barFill(mock.value, comp.min ?? 0, comp.max ?? 100) * 100) + '%';
  fill.style.background = comp.color || '#38BDF8';
  track.appendChild(fill);
  wrap.appendChild(track);
  return wrap;
}

export function buildRing(comp, placement, mock = MOCKS.ring) {
  const r   = placement.radius    || 80;
  const th  = placement.thickness || 16;
  const gap = placement.gap_deg ?? 70;
  const size = r * 2;
  const wrap = document.createElement('div');
  wrap.className = 'w w-ring';
  wrap.style.width = size + 'px';
  wrap.style.height = size + 'px';
  const svg = document.createElementNS(SVGNS, 'svg');
  svg.setAttribute('width', size);
  svg.setAttribute('height', size);
  svg.setAttribute('viewBox', `0 0 ${size} ${size}`);
  const { track, indicator } = ringPaths(r, th, gap, mock.value, comp.min ?? 0, comp.max ?? 100);
  const col = pickThresholdColor(comp.thresholds, mock.value, comp.color || '#38BDF8');
  const mk = (cls, d, stroke) => {
    const p = document.createElementNS(SVGNS, 'path');
    p.setAttribute('class', cls);
    p.setAttribute('d', d);
    p.setAttribute('fill', 'none');
    p.setAttribute('stroke', stroke);
    p.setAttribute('stroke-width', th);
    p.setAttribute('stroke-linecap', 'round');
    return p;
  };
  svg.appendChild(mk('ring-track', track, '#1F2937')); // fond firmware (view.cpp:58)
  svg.appendChild(mk('ring-ind', indicator, col));
  wrap.appendChild(svg);
  if (comp.pill) {                            // pastille % en haut de bande (view.cpp:66-74)
    const pill = document.createElement('div');
    pill.className = 'w-ring-pill';
    pill.textContent = `${Math.round(mock.value)}%`;
    pill.style.background = col;
    pill.style.top = th + 'px';
    wrap.appendChild(pill);
  }
  if (comp.countdown) {                       // légende dans l'ouverture du bas (view.cpp:43)
    const cap = document.createElement('div');
    cap.className = 'w-ring-cap';
    cap.textContent = formatRemaining(mock.reset_in_s);
    cap.style.color = comp.color || '#38BDF8';
    cap.style.bottom = th + 'px';
    wrap.appendChild(cap);
  }
  return wrap;
}

// led_ring / sound : physiques, pas de rendu écran → badge hors canvas (spec § rendu).
export function buildBadge(id, comp) {
  const n = document.createElement('span');
  n.className = 'badge badge-' + comp.type;
  n.textContent = (comp.type === 'led_ring' ? '◉ LED ring' : '♪ sound') + ' : ' + id;
  return n;
}

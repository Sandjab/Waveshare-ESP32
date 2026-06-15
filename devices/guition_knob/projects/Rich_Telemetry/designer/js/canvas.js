// Canvas WYSIWYG : construit la page active (pages[0]) depuis le modèle, gère sélection,
// drag + snap (commit-on-drop) et poignées de redimensionnement. Vérifié au navigateur.
import {
  snapPlacement, placeAt, resizeBox,
  ringRadiusAt, ringThicknessAt, gapDegAt, cornersOutsideCircle, SCREEN
} from './geometry.js';
import {
  buildLabel, buildReadout, buildBar, buildRing, buildBadge,
  ringPaths, pickThresholdColor, MOCKS
} from './render.js';

// Le firmware CENTRE le ring (lv_obj_center, view.cpp:51) : anchor/dx/dy ignorés.
// → un ring n'est pas déplaçable, seulement redimensionnable.
const DRAGGABLE = new Set(['label', 'readout', 'bar']);

export function createCanvas({ stage, badges }, model, { onSelect } = {}) {
  let selected = null; // index du placement sélectionné sur la page active

  const placements = () => model.state.pages?.[0]?.place ?? [];
  const comps = () => model.state.components || {};
  const nodeFor = i => stage.querySelector(`.w[data-pi="${i}"]`);

  function buildNode(pl, comp) {
    if (comp.type === 'bar')     return buildBar(comp, pl);
    if (comp.type === 'ring')    return buildRing(comp, pl);
    if (comp.type === 'readout') return buildReadout(comp);
    return buildLabel(comp); // label
  }

  function position(node, pl, comp) {
    if (comp.type === 'ring') {                 // centré, ignore anchor/dx/dy
      const r = pl.radius || 80;
      node.style.left = (SCREEN / 2 - r) + 'px';
      node.style.top  = (SCREEN / 2 - r) + 'px';
      return;
    }
    const rect = node.getBoundingClientRect();  // 1:1 → px = unités écran
    const { x, y } = placeAt(pl.anchor || 'CENTER', pl.dx || 0, pl.dy || 0, rect.width, rect.height);
    node.style.left = x + 'px';
    node.style.top  = y + 'px';
    node.classList.toggle('outside', cornersOutsideCircle(x, y, rect.width, rect.height));
  }

  function render() {
    stage.querySelectorAll('.w').forEach(n => n.remove());
    badges.replaceChildren();
    stage.style.background = model.state.background || '#000000';
    placements().forEach((pl, i) => {
      const comp = comps()[pl.ref];
      if (!comp) return;                         // ref inconnue : la validation le signale déjà
      if (comp.type === 'led_ring' || comp.type === 'sound') {
        badges.appendChild(buildBadge(pl.ref, comp));
        return;
      }
      const node = buildNode(pl, comp);
      node.dataset.pi = i;
      stage.appendChild(node);                   // append avant de mesurer
      position(node, pl, comp);
      node.addEventListener('pointerdown', e => onPointerDown(e, i, node, comp));
    });
    applySelection();
  }

  function applySelection() {
    stage.querySelectorAll('.w.selected').forEach(n => n.classList.remove('selected'));
    stage.querySelectorAll('.handle').forEach(n => n.remove());
    if (selected == null) return;
    const node = nodeFor(selected);
    if (!node) { selected = null; return; }
    node.classList.add('selected');
    const pl = placements()[selected];
    const comp = comps()[pl.ref];
    if (comp.type === 'bar')  addBarHandles(node, selected, pl);
    if (comp.type === 'ring') addRingHandles(node, selected, comp, pl);
  }

  function select(i) {
    selected = i;
    applySelection();
    onSelect && onSelect(i == null ? null : placements()[i]);
  }

  // --- Drag (label/readout/bar) : aperçu live, UN SEUL commit au drop (piège HANDOFF a) ---
  function onPointerDown(e, i, node, comp) {
    if (e.target.classList.contains('handle')) return; // laisser le resize gérer
    select(i);
    if (!DRAGGABLE.has(comp.type)) return;             // ring : centré, non déplaçable
    e.preventDefault();
    const sr = stage.getBoundingClientRect();
    const nr = node.getBoundingClientRect();
    const grabX = e.clientX - nr.left, grabY = e.clientY - nr.top;
    const w = nr.width, h = nr.height;
    node.setPointerCapture(e.pointerId);
    let live = null;
    const move = ev => {
      const x = ev.clientX - sr.left - grabX;
      const y = ev.clientY - sr.top  - grabY;
      live = snapPlacement(x, y, w, h, 16);
      const p = placeAt(live.anchor, live.dx, live.dy, w, h);
      node.style.left = p.x + 'px'; node.style.top = p.y + 'px';
      node.classList.toggle('snapped', live.snapped);
      node.classList.toggle('outside', cornersOutsideCircle(p.x, p.y, w, h));
    };
    const up = () => {
      node.releasePointerCapture(e.pointerId);
      node.removeEventListener('pointermove', move);
      node.removeEventListener('pointerup', up);
      node.classList.remove('snapped');
      if (live) model.commit(s => {                    // commit unique, pas par frame
        const q = s.pages[0].place[i];
        q.anchor = live.anchor; q.dx = live.dx; q.dy = live.dy;
      });
    };
    node.addEventListener('pointermove', move);
    node.addEventListener('pointerup', up);
  }

  // --- Resize bar : poignée bas-droite → width/height ---
  function addBarHandles(node, i, pl) {
    const h = document.createElement('div');
    h.className = 'handle handle-br';
    node.appendChild(h);
    h.addEventListener('pointerdown', e => {
      e.stopPropagation(); e.preventDefault();
      const startW = pl.width || 200, startH = pl.height || 16;
      const sx = e.clientX, sy = e.clientY;
      const track = node.querySelector('.w-bar-track');
      h.setPointerCapture(e.pointerId);
      let dim = null;
      const move = ev => {
        dim = resizeBox(startW, startH, ev.clientX - sx, ev.clientY - sy, 8);
        track.style.width = dim.width + 'px'; track.style.height = dim.height + 'px';
      };
      const up = () => {
        h.releasePointerCapture(e.pointerId);
        h.removeEventListener('pointermove', move); h.removeEventListener('pointerup', up);
        if (dim) model.commit(s => { const q = s.pages[0].place[i]; q.width = dim.width; q.height = dim.height; });
      };
      h.addEventListener('pointermove', move); h.addEventListener('pointerup', up);
    });
  }

  // --- Resize ring : 3 poignées (radius / thickness / gap_deg), aperçu live, commit au drop ---
  function positionRingHandles(node, g) {
    const c = g.r; // centre dans le repère du wrap (taille 2r)
    const set = (sel, x, y) => { const el = node.querySelector(sel); if (el) { el.style.left = x + 'px'; el.style.top = y + 'px'; } };
    set('.handle-radius', c, c - g.r);            // bord externe, haut
    set('.handle-thick',  c, c - g.r + g.th);     // bord interne, haut
    const a = (90 + g.gap / 2) * Math.PI / 180;   // extrémité de l'ouverture (bas)
    set('.handle-gap', c + g.r * Math.cos(a), c + g.r * Math.sin(a));
  }

  function paintRing(node, comp, g) {
    const size = g.r * 2;
    node.style.width = size + 'px'; node.style.height = size + 'px';
    node.style.left = (SCREEN / 2 - g.r) + 'px'; node.style.top = (SCREEN / 2 - g.r) + 'px';
    const svg = node.querySelector('svg');
    svg.setAttribute('width', size); svg.setAttribute('height', size);
    svg.setAttribute('viewBox', `0 0 ${size} ${size}`);
    const p = ringPaths(g.r, g.th, g.gap, MOCKS.ring.value, comp.min ?? 0, comp.max ?? 100);
    const col = pickThresholdColor(comp.thresholds, MOCKS.ring.value, comp.color || '#38BDF8');
    const t = svg.querySelector('.ring-track'), ind = svg.querySelector('.ring-ind');
    t.setAttribute('d', p.track); t.setAttribute('stroke-width', g.th);
    ind.setAttribute('d', p.indicator); ind.setAttribute('stroke-width', g.th); ind.setAttribute('stroke', col);
    const pill = node.querySelector('.w-ring-pill'); if (pill) pill.style.top = g.th + 'px';
    const cap = node.querySelector('.w-ring-cap');   if (cap)  cap.style.bottom = g.th + 'px';
    positionRingHandles(node, g);
  }

  function addRingHandles(node, i, comp, pl) {
    const geo = () => ({ r: pl.radius || 80, th: pl.thickness || 16, gap: pl.gap_deg ?? 70 });
    for (const kind of ['radius', 'thick', 'gap']) {
      const h = document.createElement('div');
      h.className = 'handle handle-' + kind;
      node.appendChild(h);
      h.addEventListener('pointerdown', e => {
        e.stopPropagation(); e.preventDefault();
        const sr = stage.getBoundingClientRect();
        h.setPointerCapture(e.pointerId);
        let g = geo();
        let moved = false;
        const move = ev => {
          moved = true;
          const px = ev.clientX - sr.left, py = ev.clientY - sr.top; // coords écran
          const base = geo();
          if (kind === 'radius')      g = { ...base, r:  ringRadiusAt(px, py) };
          else if (kind === 'thick')  g = { ...base, th: ringThicknessAt(px, py, base.r) };
          else                        g = { ...base, gap: gapDegAt(px, py) };
          paintRing(node, comp, g);
        };
        const up = () => {
          h.releasePointerCapture(e.pointerId);
          h.removeEventListener('pointermove', move); h.removeEventListener('pointerup', up);
          if (moved) model.commit(s => {
            const q = s.pages[0].place[i];
            q.radius = g.r; q.thickness = g.th; q.gap_deg = g.gap;
          });
        };
        h.addEventListener('pointermove', move); h.addEventListener('pointerup', up);
      });
    }
    positionRingHandles(node, geo());
  }

  // Clic dans le vide (fond ou liseré) → désélection.
  stage.addEventListener('pointerdown', e => {
    if (e.target === stage || e.target.classList.contains('screen-circle')) select(null);
  });

  model.subscribe(render);
  render();
  return { render, getSelected: () => selected };
}

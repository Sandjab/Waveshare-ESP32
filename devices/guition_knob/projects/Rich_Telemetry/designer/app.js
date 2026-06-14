// Rich_Telemetry Designer — SQUELETTE.
// But : éditer un layout.json conforme au contrat (../schema/layout.schema.json)
// et le pousser au device via le REST existant. NE touche pas au firmware.
//
// Ce fichier fait le strict minimum runnable : charger/pousser le layout,
// une validation légère alignée sur le parser embarqué (dashboard.cpp), et un
// aperçu de STRUCTURE. Le vrai éditeur WYSIWYG (palette, drag-and-drop, rendu
// fidèle des widgets) est la conception propre de la session C — voir les TODO.

const $ = (id) => document.getElementById(id);
const TYPES = ['label', 'readout', 'bar', 'ring', 'led_ring', 'sound'];
const PHYSICAL = new Set(['led_ring', 'sound']); // pas de géométrie / pas de rendu écran

let schema = null; // chargé pour info ; la validation complète façon JSON-Schema est un TODO

// --- Réseau -------------------------------------------------------------
function base() {
  return $('base').value.replace(/\/+$/, '');
}
function setStatus(msg, kind) {
  const el = $('status');
  el.textContent = msg;
  el.className = 'status' + (kind ? ' ' + kind : '');
}

async function loadFromDevice() {
  if (!base()) return setStatus('Renseigne l’URL du device', 'err');
  setStatus('Chargement…');
  try {
    const r = await fetch(base() + '/layout');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    $('json').value = JSON.stringify(await r.json(), null, 2);
    setStatus('Layout chargé', 'ok');
    renderPreview();
  } catch (e) {
    // CORS probable depuis file:// : voir README (servir le designer / activer CORS device).
    setStatus('Échec : ' + e.message + ' (CORS ? cf. README)', 'err');
  }
}

async function pushToDevice() {
  if (!base()) return setStatus('Renseigne l’URL du device', 'err');
  const errs = validate();
  if (errs.length) return setStatus('Layout invalide, voir erreurs', 'err');
  setStatus('Envoi…');
  try {
    const r = await fetch(base() + '/layout', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: $('json').value,
    });
    const body = await r.json().catch(() => ({}));
    if (!r.ok || body.ok === false) throw new Error(body.error || 'HTTP ' + r.status);
    setStatus('Layout poussé et persisté', 'ok');
  } catch (e) {
    setStatus('Échec : ' + e.message, 'err');
  }
}

// --- Validation légère (miroir des checks du firmware) ------------------
// Volontairement minimale. TODO (session C) : valider contre layout.schema.json
// (p. ex. bundler ajv) pour couvrir tout le contrat, pas juste ces invariants.
function validate() {
  const out = [];
  let doc;
  try {
    doc = JSON.parse($('json').value);
  } catch (e) {
    out.push('JSON invalide : ' + e.message);
    showErrors(out);
    return out;
  }
  if (!doc.components || typeof doc.components !== 'object') out.push('components manquant');
  const ids = new Set(Object.keys(doc.components || {}));
  for (const [id, c] of Object.entries(doc.components || {})) {
    if (!TYPES.includes(c.type)) out.push(`type inconnu pour '${id}' : ${c.type}`);
  }
  for (const [pi, p] of (doc.pages || []).entries()) {
    for (const pl of p.place || []) {
      if (!ids.has(pl.ref)) out.push(`page ${pi} : ref inconnue '${pl.ref}'`);
    }
  }
  showErrors(out);
  return out;
}
function showErrors(out) {
  $('errors').textContent = out.length ? out.join('\n') : '';
}

// --- Aperçu de structure -------------------------------------------------
// Marqueurs aux ancrages, pas un rendu fidèle. TODO (session C) : vrai canvas WYSIWYG.
const ANCHORS = {
  CENTER: [180, 180], TOP_MID: [180, 30], BOTTOM_MID: [180, 330],
  LEFT_MID: [30, 180], RIGHT_MID: [330, 180],
  TOP_LEFT: [60, 60], TOP_RIGHT: [300, 60], BOTTOM_LEFT: [60, 300], BOTTOM_RIGHT: [300, 300],
};

function populatePages(doc) {
  const sel = $('page');
  const prev = sel.value;
  sel.replaceChildren();
  (doc.pages || []).forEach((p, i) => {
    const o = document.createElement('option');
    o.value = i;
    o.textContent = p.name || `page ${i}`;
    sel.appendChild(o);
  });
  if (prev && prev < sel.options.length) sel.value = prev;
}

function renderPreview() {
  const cv = $('canvas');
  const g = cv.getContext('2d');
  g.clearRect(0, 0, 360, 360);
  let doc;
  try { doc = JSON.parse($('json').value); } catch { return; }
  populatePages(doc);

  const page = doc.pages?.[$('page').value | 0];
  if (!page) return;
  for (const pl of page.place || []) {
    const comp = doc.components?.[pl.ref];
    if (!comp || PHYSICAL.has(comp.type)) continue; // led_ring/sound : pas à l'écran
    const [ax, ay] = ANCHORS[pl.anchor || 'CENTER'] || ANCHORS.CENTER;
    const x = ax + (pl.dx || 0);
    const y = ay + (pl.dy || 0);
    g.fillStyle = comp.color || '#38bdf8';
    g.beginPath();
    g.arc(x, y, 6, 0, Math.PI * 2);
    g.fill();
    g.fillStyle = '#e6edf3';
    g.font = '11px monospace';
    g.fillText(`${pl.ref} (${comp.type})`, x + 10, y + 4);
  }
}

// --- Bootstrap -----------------------------------------------------------
async function init() {
  try {
    schema = await (await fetch('../schema/layout.schema.json')).json();
  } catch {
    // Pas bloquant : le schéma sert d'info/contrat ; la validation actuelle ne l'utilise pas encore.
  }
  $('load').onclick = loadFromDevice;
  $('push').onclick = pushToDevice;
  $('validate').onclick = validate;
  $('render').onclick = renderPreview;
  $('page').onchange = renderPreview;
}
init();

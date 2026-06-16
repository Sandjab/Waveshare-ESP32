// Inspecteur : édite le composant + le placement sélectionnés. Pilote les champs par des tables de
// descripteurs (DRY). Chaque édition committée = UN commit (sur 'change', pas par frappe → pas de
// flood undo). Le signalement ASCII est live (sur 'input'). S'abonne au modèle pour se rafraîchir.
import { setComponentProp, setPlacementProp, setThresholds, removePlacement } from './mutations.js';
import { ANCHORS } from './geometry.js';
import { getMock, setMock } from './mocks.js';

const FONTS = [14, 20, 28, 36, 48];
const nonAscii = v => /[^\x00-\x7F]/.test(v ?? '');

// Champs de composant par type : [clé, libellé, kind, enableWhen?]. kind: asciitext|text|num|color|bool|font.
// enableWhen(c) optionnel : false grise le champ (non pertinent dans l'état courant), sans le supprimer.
const COMP_FIELDS = {
  label:    [['text', 'Texte', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color']],
  readout:  [['label', 'Label', 'asciitext'], ['unit', 'Unité', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color']],
  bar:      [['label', 'Label', 'asciitext'], ['min', 'Min', 'num'], ['max', 'Max', 'num'], ['color', 'Couleur', 'color']],
  ring:     [['color', 'Couleur', 'color'],
             ['pill', 'Pastille %', 'bool', c => !c.center_pct],         // ignoré quand center_pct (prioritaire)
             ['center_pct', 'Centre %', 'bool'],
             ['font', 'Police centre', 'font', c => !!c.center_pct],     // dimensionne le chiffre central
             ['center_color', 'Couleur centre', 'color', c => !!c.center_pct],
             ['countdown', 'Countdown', 'bool'], ['min', 'Min', 'num'], ['max', 'Max', 'num']],
  led_ring: [['color', 'Couleur', 'color'], ['brightness', 'Luminosité (0-255)', 'num']],
  sound:    []
};

// Champs de géométrie de placement par type : [clé, libellé, kind].
const PLACE_FIELDS = {
  label:    [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num']],
  readout:  [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num']],
  bar:      [['anchor', 'Ancrage', 'anchor'], ['dx', 'dx', 'num'], ['dy', 'dy', 'num'], ['width', 'Largeur', 'num'], ['height', 'Hauteur', 'num']],
  ring:     [['radius', 'Rayon', 'num'], ['thickness', 'Épaisseur', 'num'], ['gap_deg', 'Ouverture°', 'num'], ['start_angle', 'Angle départ°', 'num']],
  led_ring: [],
  sound:    []
};

// Champs de valeur mock (aperçu) par type : [clé, libellé].
const MOCK_FIELDS = {
  readout: [['value', 'Valeur (aperçu)']],
  bar:     [['value', 'Valeur (aperçu)']],
  ring:    [['value', 'Valeur % (aperçu)'], ['reset_in_s', 'Countdown (s)']],
  label:   [], led_ring: [], sound: []
};

// Construit un <input>/<select> selon kind. onChange reçoit la valeur typée. Les éditeurs textuels
// committent sur 'change' (pas 'input') pour ne pas inonder l'undo.
function makeInput(kind, value, onChange) {
  let el;
  if (kind === 'bool') {
    el = document.createElement('input'); el.type = 'checkbox'; el.checked = !!value;
    el.addEventListener('change', () => onChange(el.checked));
  } else if (kind === 'color') {
    el = document.createElement('input'); el.type = 'color'; el.value = value || '#FFFFFF';
    el.addEventListener('change', () => onChange(el.value.toUpperCase()));
  } else if (kind === 'font') {
    el = document.createElement('select');
    for (const f of FONTS) { const o = document.createElement('option'); o.value = String(f); o.textContent = f + ' px'; if (f === (value ?? 20)) o.selected = true; el.appendChild(o); }
    el.addEventListener('change', () => onChange(Number(el.value)));
  } else if (kind === 'anchor') {
    el = document.createElement('select');
    for (const a of ANCHORS) { const o = document.createElement('option'); o.value = a; o.textContent = a; if (a === (value || 'CENTER')) o.selected = true; el.appendChild(o); }
    el.addEventListener('change', () => onChange(el.value));
  } else if (kind === 'num') {
    el = document.createElement('input'); el.type = 'number'; el.value = value ?? '';
    el.addEventListener('change', () => onChange(el.value === '' ? '' : Number(el.value)));
  } else { // text / asciitext
    el = document.createElement('input'); el.type = 'text'; el.value = value ?? '';
    el.addEventListener('change', () => onChange(el.value));
  }
  return el;
}

// Ligne libellé + champ (+ avertissement ASCII live pour les champs asciitext).
function fieldRow(label, input, { ascii } = {}) {
  const row = document.createElement('label');
  row.className = 'insp-row';
  const span = document.createElement('span'); span.className = 'insp-label'; span.textContent = label;
  row.appendChild(span); row.appendChild(input);
  if (ascii) {
    const warn = document.createElement('span'); warn.className = 'insp-warn'; warn.textContent = '⚠ ASCII';
    warn.style.display = nonAscii(input.value) ? '' : 'none';
    input.addEventListener('input', () => { warn.style.display = nonAscii(input.value) ? '' : 'none'; });
    row.appendChild(warn);
  }
  return row;
}

export function createInspector(root, model, { rerenderCanvas, clearSelection, getActivePage = () => 0 } = {}) {
  let sel = null; // { placeIndex, ref } ou null

  const comp = () => sel && model.state.components[sel.ref];
  const place = () => sel && model.state.pages?.[getActivePage()]?.place?.[sel.placeIndex];

  function select(s) { sel = s; render(); }

  // Sous-titre de section.
  function sub(body, text) { const h = document.createElement('div'); h.className = 'insp-sub'; h.textContent = text; body.appendChild(h); }
  function note(body, text) { const n = document.createElement('div'); n.className = 'insp-note'; n.textContent = text; body.appendChild(n); }

  function renderExtras(body, c) {
    const p = place();
    // --- Géométrie du placement ---
    const gf = PLACE_FIELDS[c.type] || [];
    if (gf.length) {
      sub(body, 'Placement');
      if (c.type === 'ring') note(body, 'Anneau centré : ancrage/dx/dy ignorés par le firmware.');
      for (const [key, label, kind] of gf) {
        const input = makeInput(kind, p[key], v => model.commit(s => setPlacementProp(s, getActivePage(), sel.placeIndex, key, v)));
        body.appendChild(fieldRow(label, input));
      }
    }

    // --- Seuils du ring (liste éditable de [limite, #couleur]) ---
    if (c.type === 'ring') {
      sub(body, 'Seuils (couleur si valeur < limite)');
      const ths = (c.thresholds || []).map(t => [t[0], t[1]]); // copie locale éditable
      const commitThs = () => model.commit(s => setThresholds(s, sel.ref, ths.filter(t => t[1])));
      ths.forEach((t, idx) => {
        const row = document.createElement('div'); row.className = 'insp-row';
        const lim = makeInput('num', t[0], v => { ths[idx][0] = v === '' ? 0 : v; commitThs(); });
        const col = makeInput('color', t[1], v => { ths[idx][1] = v; commitThs(); });
        const rm = document.createElement('button'); rm.className = 'insp-th-rm'; rm.textContent = '×';
        rm.addEventListener('click', () => { ths.splice(idx, 1); commitThs(); });
        row.appendChild(lim); row.appendChild(col); row.appendChild(rm);
        body.appendChild(row);
      });
      const add = document.createElement('button'); add.className = 'insp-th-add'; add.textContent = '+ seuil';
      add.addEventListener('click', () => { ths.push([0, '#FF0000']); commitThs(); });
      body.appendChild(add);
    }

    // --- Valeur d'aperçu (mock) : hors layout, re-rend le canvas sans toucher au modèle/undo ---
    const mf = MOCK_FIELDS[c.type] || [];
    if (mf.length) {
      sub(body, 'Aperçu (mock, non poussé au device)');
      const m = getMock(sel.ref, c.type);
      for (const [key, label] of mf) {
        const input = makeInput('num', m[key], v => {
          setMock(sel.ref, { [key]: v === '' ? 0 : v });
          rerenderCanvas && rerenderCanvas();
        });
        body.appendChild(fieldRow(label, input));
      }
    }
  }

  function render() {
    // garde focus : ne pas reconstruire pendant qu'un champ de l'inspecteur est en cours d'édition.
    if (root.contains(document.activeElement) && document.activeElement !== document.body) return;
    root.querySelectorAll('.insp-body').forEach(n => n.remove());
    const c = comp();
    const p = place();
    const body = document.createElement('div');
    body.className = 'insp-body';
    if (!c || !p) {                               // sélection absente ou devenue obsolète (page changée, undo…)
      const para = document.createElement('p'); para.className = 'todo'; para.textContent = 'Sélectionne un widget sur le canvas.';
      body.appendChild(para); root.appendChild(body); return;
    }
    const head = document.createElement('div'); head.className = 'insp-head';
    head.textContent = `${c.type} · ${sel.ref}`;
    body.appendChild(head);

    const rows = {};
    for (const [key, label, kind, enableWhen] of COMP_FIELDS[c.type] || []) {
      const input = makeInput(kind, c[key], v => model.commit(s => setComponentProp(s, sel.ref, key, v)));
      const row = fieldRow(label, input, { ascii: kind === 'asciitext' });
      rows[key] = { input, row, enableWhen };
      body.appendChild(row);
    }
    // Grise les champs non pertinents dans l'état courant (ex : couleur/police du centre si center_pct off).
    // En direct, sans rebuild : le garde-focus de render() bloquerait une reconstruction juste après le clic.
    const syncEnabled = () => {
      const cc = comp(); if (!cc) return;
      for (const { input, row, enableWhen } of Object.values(rows)) {
        if (!enableWhen) continue;
        const ok = enableWhen(cc);
        input.disabled = !ok;
        row.classList.toggle('disabled', !ok);
      }
    };
    syncEnabled();
    body.addEventListener('change', syncEnabled); // un toggle (ex: center_pct) re-évalue les dépendants

    renderExtras(body, c); // Task 6

    const del = document.createElement('button'); del.className = 'insp-del'; del.textContent = 'Supprimer de la page';
    del.addEventListener('click', () => {
      const i = sel.placeIndex;
      sel = null;
      clearSelection && clearSelection();                 // désélectionne AVANT le commit (évite le flash, note C1-c)
      model.commit(s => removePlacement(s, getActivePage(), i));
    });
    body.appendChild(del);
    root.appendChild(body);
  }

  model.subscribe(render);
  render();
  return { select };
}

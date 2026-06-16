// Inspecteur : édite le composant + le placement sélectionnés. Pilote les champs par des tables de
// descripteurs (DRY). Chaque édition committée = UN commit (sur 'change', pas par frappe → pas de
// flood undo). Le signalement ASCII est live (sur 'input'). S'abonne au modèle pour se rafraîchir.
import { setComponentProp, removePlacement } from './mutations.js';
import { ANCHORS } from './geometry.js';

const FONTS = [14, 20, 28];
const nonAscii = v => /[^\x00-\x7F]/.test(v ?? '');

// Champs de composant par type : [clé, libellé, kind]. kind: asciitext|text|num|color|bool|font.
const COMP_FIELDS = {
  label:    [['text', 'Texte', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color']],
  readout:  [['label', 'Label', 'asciitext'], ['unit', 'Unité', 'asciitext'], ['font', 'Police', 'font'], ['color', 'Couleur', 'color']],
  bar:      [['label', 'Label', 'asciitext'], ['min', 'Min', 'num'], ['max', 'Max', 'num'], ['color', 'Couleur', 'color']],
  ring:     [['color', 'Couleur', 'color'], ['pill', 'Pastille %', 'bool'], ['countdown', 'Countdown', 'bool'], ['min', 'Min', 'num'], ['max', 'Max', 'num']],
  led_ring: [['color', 'Couleur', 'color'], ['brightness', 'Luminosité (0-255)', 'num']],
  sound:    []
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

export function createInspector(root, model, { rerenderCanvas, clearSelection } = {}) {
  let sel = null; // { placeIndex, ref } ou null

  const comp = () => sel && model.state.components[sel.ref];
  const place = () => sel && model.state.pages[0].place[sel.placeIndex];

  function select(s) { sel = s; render(); }

  // hook d'extension rempli en Task 6 (géométrie + seuils + mock). No-op ici.
  function renderExtras(body, c) {}

  function render() {
    // garde focus : ne pas reconstruire pendant qu'un champ de l'inspecteur est en cours d'édition.
    if (root.contains(document.activeElement) && document.activeElement !== document.body) return;
    root.querySelectorAll('.insp-body').forEach(n => n.remove());
    const c = comp();
    const body = document.createElement('div');
    body.className = 'insp-body';
    if (!c) {
      const p = document.createElement('p'); p.className = 'todo'; p.textContent = 'Sélectionne un widget sur le canvas.';
      body.appendChild(p); root.appendChild(body); return;
    }
    const head = document.createElement('div'); head.className = 'insp-head';
    head.textContent = `${c.type} · ${sel.ref}`;
    body.appendChild(head);

    for (const [key, label, kind] of COMP_FIELDS[c.type] || []) {
      const input = makeInput(kind, c[key], v => model.commit(s => setComponentProp(s, sel.ref, key, v)));
      body.appendChild(fieldRow(label, input, { ascii: kind === 'asciitext' }));
    }

    renderExtras(body, c); // Task 6

    const del = document.createElement('button'); del.className = 'insp-del'; del.textContent = 'Supprimer de la page';
    del.addEventListener('click', () => {
      const i = sel.placeIndex;
      model.commit(s => removePlacement(s, 0, i));
      sel = null;
      clearSelection && clearSelection(); // désélectionne le canvas
    });
    body.appendChild(del);
    root.appendChild(body);
  }

  model.subscribe(render);
  render();
  return { select };
}

// Palette : (1) 6 créateurs de type — glisser sur le #stage crée un composant + un placement au point
// de dépôt ; (2) Bibliothèque des composants déjà définis — glisser un existant sur le #stage le PLACE
// (partage : même id/état, pas une copie) sur la page ACTIVE. Drop = UN commit, puis sélection du
// nouveau placement. Vérifié au navigateur. (Pages = pages.js ; valeurs d'aperçu = mocks.js.)
import { uniqueId, addComponent, addPlacement, DEFAULTS } from './mutations.js';
import { snapPlacement } from './geometry.js';

const TYPES = [
  ['label', 'Label'], ['readout', 'Lecture'], ['bar', 'Barre'],
  ['ring', 'Anneau'], ['led_ring', 'LED ring'], ['sound', 'Son']
];

// Placement initial selon le type. Ring centré ; led_ring/sound sans géométrie ; widgets écran :
// ancrage + offset déduits du point de dépôt (boîte ~0, affinable au drag).
function makePlacement(type, id, x, y) {
  if (type === 'ring') return { ref: id, radius: 80, thickness: 16, gap_deg: 70 };
  if (type === 'led_ring' || type === 'sound') return { ref: id };
  const { anchor, dx, dy } = snapPlacement(x, y, 0, 0, 16);
  return { ref: id, anchor, dx, dy };
}

export function createPalette(root, model, { stage, getActivePage, onCreated } = {}) {
  const page = () => (getActivePage ? getActivePage() : 0);

  // --- Section créateurs de type (statique) ---
  const list = document.createElement('div');
  list.className = 'palette-list';
  for (const [type, libelle] of TYPES) {
    const item = document.createElement('div');
    item.className = 'palette-item';
    item.draggable = true;
    item.dataset.type = type;
    item.textContent = libelle;
    item.addEventListener('dragstart', e => e.dataTransfer.setData('text/rt-type', type));
    list.appendChild(item);
  }
  root.appendChild(list);

  // --- Section bibliothèque (dynamique : reflète components) ---
  const libTitle = document.createElement('div');
  libTitle.className = 'lib-title';
  libTitle.textContent = 'Bibliothèque';
  root.appendChild(libTitle);
  const libList = document.createElement('div');
  libList.className = 'lib-list';
  root.appendChild(libList);

  function renderLibrary() {
    libList.replaceChildren();
    const comps = model.state.components || {};
    const ids = Object.keys(comps);
    if (!ids.length) {
      const empty = document.createElement('div');
      empty.className = 'lib-empty';
      empty.textContent = 'Aucun composant défini.';
      libList.appendChild(empty);
      return;
    }
    for (const id of ids) {
      const item = document.createElement('div');
      item.className = 'lib-item';
      item.draggable = true;
      const name = document.createElement('span'); name.textContent = id;
      const type = document.createElement('span'); type.className = 'lib-type'; type.textContent = comps[id].type;
      item.appendChild(name); item.appendChild(type);
      item.addEventListener('dragstart', e => e.dataTransfer.setData('text/rt-ref', id));
      libList.appendChild(item);
    }
  }
  model.subscribe(renderLibrary);
  renderLibrary();

  // --- Cible de drop : crée (type) ou place un existant (ref), sur la page active ---
  stage.addEventListener('dragover', e => {
    const t = e.dataTransfer.types;
    if (t.includes('text/rt-type') || t.includes('text/rt-ref')) e.preventDefault();
  });
  stage.addEventListener('drop', e => {
    const type = e.dataTransfer.getData('text/rt-type');
    const ref = e.dataTransfer.getData('text/rt-ref');
    if (!type && !ref) return;
    e.preventDefault();
    const r = stage.getBoundingClientRect();
    const x = e.clientX - r.left, y = e.clientY - r.top; // coords écran (1:1)
    const pi = page();
    let newIndex;
    model.commit(s => {
      if (type) {
        const id = uniqueId(s, type);
        addComponent(s, id, DEFAULTS[type]());
        addPlacement(s, pi, makePlacement(type, id, x, y));
      } else {
        const existing = s.components[ref];
        if (!existing) return;                            // ref disparue : rien à placer
        addPlacement(s, pi, makePlacement(existing.type, ref, x, y));
      }
      newIndex = s.pages[pi].place.length - 1;
    });
    if (newIndex != null) onCreated && onCreated(newIndex);
  });
}

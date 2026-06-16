// Palette : 6 créateurs de type. Glisser un item sur le #stage crée le composant + un placement
// au point de dépôt, en UN SEUL commit, puis sélectionne le nouveau widget. (Bibliothèque de
// composants partagés inter-pages = Plan C2.)
import { uniqueId, addComponent, addPlacement, DEFAULTS } from './mutations.js';
import { snapPlacement } from './geometry.js';

const TYPES = [
  ['label', 'Label'], ['readout', 'Lecture'], ['bar', 'Barre'],
  ['ring', 'Anneau'], ['led_ring', 'LED ring'], ['sound', 'Son']
];

// Placement initial selon le type. Ring centré (radius par défaut) ; led_ring/sound sans géométrie ;
// widgets écran : ancrage + offset déduits du point de dépôt (boîte de taille ~0, affinable au drag).
function makePlacement(type, id, x, y) {
  if (type === 'ring') return { ref: id, radius: 80, thickness: 16, gap_deg: 70 };
  if (type === 'led_ring' || type === 'sound') return { ref: id };
  const { anchor, dx, dy } = snapPlacement(x, y, 0, 0, 16);
  return { ref: id, anchor, dx, dy };
}

export function createPalette(root, model, { stage, onCreated } = {}) {
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

  stage.addEventListener('dragover', e => {
    if (e.dataTransfer.types.includes('text/rt-type')) e.preventDefault(); // autorise le drop
  });
  stage.addEventListener('drop', e => {
    const type = e.dataTransfer.getData('text/rt-type');
    if (!type) return;
    e.preventDefault();
    const r = stage.getBoundingClientRect();
    const x = e.clientX - r.left, y = e.clientY - r.top; // coords écran (1:1)
    let newIndex;
    model.commit(s => {
      const id = uniqueId(s, type);
      addComponent(s, id, DEFAULTS[type]());
      addPlacement(s, 0, makePlacement(type, id, x, y));
      newIndex = s.pages[0].place.length - 1;
    });
    onCreated && onCreated(newIndex);
  });
}

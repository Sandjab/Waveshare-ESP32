// Onglets de pages. Un onglet par page (clic = page active). Les contrôles agissent sur la page
// ACTIVE : + Page (ajoute en fin et l'active), Renommer (édition inline du nom, pas de prompt()),
// ◀/▶ (réordonne la page active), Supprimer (désactivé s'il ne reste qu'une page). La page active
// vit dans le canvas (source de vérité unique), lue via getActivePage et pilotée via setPage.
import { addPage, removePage, renamePage, reorderPages } from './mutations.js';

function mkBtn(text, onClick, cls) {
  const b = document.createElement('button');
  b.className = 'page-btn' + (cls ? ' ' + cls : '');
  b.textContent = text;
  b.addEventListener('click', onClick);
  return b;
}

export function createPages(root, model, { getActivePage, setPage } = {}) {
  let renaming = null; // index de la page en cours de renommage inline, ou null

  // Backstop : après removePage (ou undo/import), l'index actif peut dépasser la liste → on le ramène.
  function clampActive() {
    const n = model.state.pages?.length ?? 0;
    if (n && getActivePage() > n - 1) setPage(n - 1);
  }

  function render() {
    clampActive();
    root.replaceChildren();
    const pages = model.state.pages || [];
    const active = getActivePage();

    const tabs = document.createElement('div');
    tabs.className = 'page-tabs';
    pages.forEach((p, i) => {
      if (renaming === i) {
        const inp = document.createElement('input');
        inp.className = 'page-rename';
        inp.value = p.name || '';
        inp.addEventListener('change', () => {
          const name = inp.value.trim() || `Page ${i + 1}`;
          renaming = null;
          model.commit(s => renamePage(s, i, name));   // → subscribe → render()
        });
        inp.addEventListener('blur', () => { if (renaming === i) { renaming = null; render(); } });
        tabs.appendChild(inp);
        queueMicrotask(() => inp.focus());
      } else {
        const tab = document.createElement('button');
        tab.className = 'page-tab' + (i === active ? ' active' : '');
        tab.textContent = p.name || `Page ${i + 1}`;
        tab.addEventListener('click', () => { setPage(i); render(); });
        tabs.appendChild(tab);
      }
    });
    root.appendChild(tabs);

    const ctrls = document.createElement('div');
    ctrls.className = 'page-ctrls';

    ctrls.appendChild(mkBtn('+ Page', () => {
      model.commit(s => addPage(s, `Page ${s.pages.length + 1}`));
      setPage(model.state.pages.length - 1);
      render();
    }));

    ctrls.appendChild(mkBtn('Renommer', () => { renaming = active; render(); }));

    const left = mkBtn('◀', () => {
      if (active <= 0) return;
      model.commit(s => reorderPages(s, active, active - 1));
      setPage(active - 1);
      render();
    });
    left.disabled = active <= 0;
    ctrls.appendChild(left);

    const right = mkBtn('▶', () => {
      if (active >= pages.length - 1) return;
      model.commit(s => reorderPages(s, active, active + 1));
      setPage(active + 1);
      render();
    });
    right.disabled = active >= pages.length - 1;
    ctrls.appendChild(right);

    const del = mkBtn('Supprimer', () => {
      if (pages.length <= 1) return;                       // garder au moins une page
      model.commit(s => removePage(s, active));
      setPage(Math.min(active, model.state.pages.length - 1));
      render();
    }, 'page-del');
    del.disabled = pages.length <= 1;
    ctrls.appendChild(del);

    root.appendChild(ctrls);
  }

  model.subscribe(render);
  render();
  return { render };
}

// Layout de départ de l'éditeur (vide-utile). Valide vis-à-vis de layout.schema.json.
// Indépendant du layout par défaut du firmware.
export const DEFAULT_LAYOUT = {
  title: "Dashboard",
  background: "#000000",
  components: {
    hello: { type: "label", text: "Hello", font: 20, color: "#FFFFFF" }
  },
  pages: [
    { name: "Page 1", place: [ { ref: "hello", anchor: "CENTER" } ] }
  ]
};

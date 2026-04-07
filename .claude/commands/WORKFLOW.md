# WORKFLOW — Comment utiliser ces skills

Ce fichier explique comment penser et séquencer le travail avec ces skills.

---

## Le modèle mental

Ne pas penser : "Claude, fais-moi tout d'un coup."

Penser : "Claude, on avance en étapes."

L'ordre universel est presque toujours :

1. Comprendre le projet source
2. Décider ce qu'on porte vraiment
3. Dessiner le module cible
4. Générer les fichiers un par un
5. Relire / corriger
6. Tester natif
7. Valider sur hardware

C'est exactement pour ça que les skills sont découpés comme ils le sont.

---

## À quoi sert chaque type de fichier

### `CLAUDE.md`
Mémoire permanente du repo. Dit à Claude comment travailler, quelles conventions respecter, quels pièges éviter. Pas utilisé "à la main" — il est là pour que les autres skills s'appuient dessus.

### `commands/schwung/`
Skills de workflow. Pilotent une étape de travail : audit, design, implémentation, UI, packaging. À utiliser quand on veut faire avancer un projet.

### `commands/templates/`
Skills de génération de fichiers. Produisent directement du code ou du JSON : `module.json`, `dsp/*.c`, `ui.js`, `ui_chain.js`. À utiliser **après** l'audit et le design, pas avant.

---

## Quel skill utiliser et quand

| Situation | Skill |
|---|---|
| Démarrer une session, changer de projet | `repo-bootstrap` |
| Partir d'un repo externe, mesurer la faisabilité | `audit-open-source-midi-fx` |
| Savoir ce qu'on construit, définir avant le code | `design-module` |
| Figer les paramètres et capacités | `create-module-json` |
| Générer le moteur natif | `create-dsp-c` |
| Générer le wrapper host Schwung | `create-host-wrapper` |
| Construire l'interface Move | `build-move-ui-and-controls` |
| Vérifier la cohérence globale entre les fichiers | `convert-open-source-midi-fx` |
| Préparer une version installable | `build-and-install` |

---

## Le workflow universel pour porter un projet

### Pour porter un projet existant

```
/repo-bootstrap
/audit-open-source-midi-fx ...
/design-module ...
/create-module-json ...
/create-dsp-c ...
/create-host-wrapper ...
/build-move-ui-and-controls ...   (si UI custom justifiée)
/convert-open-source-midi-fx ...  (revue de cohérence)
/build-and-install
```

### Pour créer un module from scratch

```
/repo-bootstrap
/design-module ...
/create-module-json ...
/create-dsp-c ...
/create-host-wrapper ...
/build-and-install
```

### Pour améliorer un module déjà commencé

```
/repo-bootstrap
/implement-native-midi-fx
/build-move-ui-and-controls
/build-and-install
```

Avec une consigne ciblée :
> Review and improve the current implementation. Do not redesign everything. Focus on note safety, parameter coherence, and Move usability.

---

## La recette copier-coller universelle

Garder ça sous la main pour n'importe quel portage.

### Étape 1 — Bootstrap
```
Use this repo to evaluate a Schwung port of [PROJECT].
Focus on a small, reliable V1.
```

### Étape 2 — Audit
```
/audit-open-source-midi-fx [URL ou description]
Target a reduced V1.
Port only the musical core.
Do not port host-specific workflow.
```

### Étape 3 — Design
```
/design-module [description courte]
Prefer a native engine.
Keep controls compact.
Design for Ableton Move.
```

### Étape 4 — Manifest
```
/create-module-json [description]
Use a minimal V1 parameter set.
```

### Étape 5 — Moteur
```
/create-dsp-c [description comportementale]
Use safe note lifecycle handling.
Keep timing explicit and deterministic.
```

### Étape 6 — Wrapper
```
/create-host-wrapper [module et params]
```

### Étape 7 — UI (si justifiée)
```
/build-move-ui-and-controls
Only if a true custom Move UI is justified.
```

### Étape 8 — Revue de cohérence
```
/convert-open-source-midi-fx [projet]
Review coherence between manifest, engine, and UI.
Check for mismatches, stuck note risk, and Move UX clarity.
```

### Étape 9 — Deploy
```
/build-and-install
```

---

## Le brief court idéal

À ajouter après presque chaque commande :

```
Target a reduced V1.
Port only the musical core.
Do not port host-specific workflow.
Keep controls compact.
Design for Ableton Move.
Prefer a stable Schwung-native design over feature parity.
```

---

## La phrase magique

À réutiliser à chaque étape quand on dérive :

> Keep this as a reduced V1. Prefer a stable Schwung-native design over a faithful clone of the source project.

---

## Les erreurs classiques à éviter

**Erreur 1 — Demander tout d'un coup**
```
Port this full project with all features, full UI, presets, and optimization.
```
→ Toujours cadrer : "Target a reduced V1. Focus on the musical core."

**Erreur 2 — Générer du code avant le design**
L'ordre est : audit → design → code. Pas l'inverse.

**Erreur 3 — Trop de paramètres**
Sur Move, moins c'est souvent mieux. V1 = 2 à 6 contrôles. Pas 20.

**Erreur 4 — Forcer une full UI**
Beaucoup de modules n'ont pas besoin de `ui.js`. Un bon `module.json` + moteur natif suffit souvent. `OMIT_UI_JS` est un bon résultat.

**Erreur 5 — Chercher la fidélité totale**
Le vrai but est de faire un bon module Schwung, pas reproduire l'original mot pour mot.

**Erreur 6 — Valider un `module.json` trop tôt**
Ne pas valider si : des paramètres ne servent à rien, les noms sont flous, les contrôles sont trop nombreux, ça ne colle pas à Move.

**Erreur 7 — Ne pas relire la note lifecycle**
Toujours vérifier avant de passer à l'étape suivante : stuck notes, note-off manquants, reset sur transport stop.

**Erreur 8 — Supposer que `tick()` suffit pour le transport**
Sur Move, un `midi_fx` clock-driven peut recevoir `0xFA` et surtout `0xF8` dans `process_midi()`. Si le module démarre seulement après une note jouée, vérifier d'abord si le wrapper ignore les ticks MIDI clock.

**Erreur 9 — Confondre valeur demandée et valeur effective**
Si un paramètre dépend d'un autre, ne pas forcément refléter la valeur clampée dans l'UI. Exemple typique : `fills` demandé à `16`, `steps` à `8`. Le moteur peut jouer avec `8`, mais l'UI et `get_param` doivent souvent continuer à refléter `16`.

---

## Ce qu'on attend à chaque étape

| Étape | Ce qu'on doit pouvoir dire à la fin |
|---|---|
| Audit | "Je sais ce qui est portable et sous quelle forme" |
| Design | "Je sais exactement ce que je construis" |
| `module.json` | "Les paramètres et capacités sont figés" |
| `create-dsp-c` | "Le moteur est propre, les stuck notes sont impossibles" |
| `create-host-wrapper` | "Tous les paramètres sont exposés et testés" |
| `build-and-install` | "Ça tourne sur Move, le hardware test est passé" |

Ajout pratique pour les modules clock-driven :
- `0xFA` : premier pas immédiat ou explicitement armé
- `0xF8` : avancée du pattern si le wrapper choisit le scheduler MIDI clock
- `0xFC` : flush note-off garanti

---

## La méthode la plus simple à retenir

```
1. Faisable
2. Propre
3. Musical
4. Beau
5. Riche
```

Pas l'inverse.

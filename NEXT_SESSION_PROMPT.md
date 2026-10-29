# Prompt pour la prochaine session — DQIX Enhanced

Copie-colle le prompt ci-dessous dans une nouvelle conversation :

---

Tu reprends le travail sur le dépôt GitHub **`rakmahefa/dqix-enhanced`**, fork de `sackchief/dqix-enhanced`. Lis d'abord le fichier racine [`SESSION_HANDOFF.md`](SESSION_HANDOFF.md), puis `spec.md`, `appimage-audit.md`, `audit-files.md`, `upstream-sync.md` et les workflows CI/release. Le handoff est daté : vérifie les SHA, l'état de `main`, l'upstream et le dernier statut CI avant de modifier quoi que ce soit.

## Contexte connu

Le PR #4, « Sync upstream v32 and harden Linux AppImage plan », a été fusionné. Commit de merge connu à la date du handoff : `3a84fe776dfed59421987722073d03a438a3ab56`.

Dernière CI connue : [Actions run 40719500321](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40719500321).
- Le job `Source and recompiler tests (no ROM)` a réussi.
- Le job `Package and smoke-test builder AppImage` a échoué pendant `Build AppImage`.
- Message fatal exact : `desktop-file-validate command is missing, please install it`.
- Cause probable : le job n'installe pas `desktop-file-utils`. Confirme le workflow avant de corriger, puis installe ce paquet dans la bonne étape. Ne désactive pas la validation du fichier desktop et ne masque pas les erreurs.
- Le smoke test de l'AppImage, les checksums et l'upload d'artefact ont été sautés après l'échec du build. Il faut donc obtenir une nouvelle CI verte, et pas seulement corriger le premier message.

## Mission : poursuivre jusqu'à un résultat vérifié

### Étape 1 — Valider l'état courant

1. Vérifie `main`, le commit de merge, les workflows et les runs les plus récents.
2. Lis les fichiers de contexte et le workflow `.github/workflows/linux-ci-appimage.yml`.
3. Compare à l'upstream uniquement pour détecter une évolution pertinente depuis la synchronisation v32 ; préserve les modifications propres au fork.
4. Résume les faits actuels, surtout les erreurs CI vérifiées, avant de changer le code.

### Étape 2 — Corriger et rerun la CI AppImage (priorité immédiate)

1. Ajoute `desktop-file-utils` à l'installation des outils de packaging, en gardant les dépendances nécessaires déjà présentes (`libfuse2`, `rsync`, `squashfs-tools`).
2. Crée une branche dédiée à partir de `main`, commits propres, et ouvre une PR de correction.
3. Exécute les tests possibles ; déclenche ou suis la CI GitHub sur cette branche.
4. Lis les logs complets pour chaque échec. Corrige le prochain problème réel jusqu'à ce que la génération de l'AppImage et ses smoke tests passent. Ne t'arrête pas au premier correctif si le workflow expose ensuite un autre défaut.
5. Vérifie `--version`, `--help`, `--self-test`, l'exécution avec des répertoires HOME/XDG temporaires, les checksums et la création de l'artefact. Associe les résultats au SHA testé.
6. Le log annonçait `appimagetool, continuous build` alors que le workflow télécharge via une URL `1.9.0` et vérifie le SHA déclaré. La vérification hash a réussi ; n'en déduis pas d'emblée une compromission ou une incohérence. Vérifie ce point si la sortie de version reste pertinente après la correction principale.

### Étape 3 — Investiguer la lenteur de « préparation des fichiers du jeu »

C'est un objectif fonctionnel explicite, pas un simple axe futur. Audite le chemin réellement lancé par le constructeur AppImage, en particulier :
- `packaging/linux/gui.py`
- `packaging/linux/AppRun`
- `tools/build-game.sh`
- `recomp/build.py`, `recomp/arm7.py` et les autres outils appelés pendant la génération
- `build.mk` et tous les scripts de codegen/compilation appelés.

Trace le parcours de bout en bout : clic GUI → worker → validation/extraction ROM → génération du code C → compilation/link → assemblage des fichiers du jeu. Identifie les fonctions les plus coûteuses et instrumente des phases distinctes si les logs actuels ne suffisent pas.

**Vérifie explicitement le multithreading/le parallélisme dans le code livré par l'AppImage.** Ne conclus pas que des threads sont utilisés simplement parce qu'il y a une barre de progression, un worker ou plusieurs tâches de build. Repère précisément les threads, processus, appels de compilation parallèle et dépendances entre étapes. Vérifie aussi que la GUI reste réactive. Ne remplace pas un thread par un processus ni l'inverse sans preuve ou mesure utile.

Mesure avant d'optimiser :
- durée de chaque étape ;
- fichiers/volume lus et générés ;
- utilisation CPU et éventuels goulots d'étranglement I/O ;
- lancement avec cache froid puis chaud ;
- temps de résolution de toolchain (CI), empaquetage, lancement AppImage et vraie génération locale quand cela est possible.

Ne prétends pas mesurer un build complet avec une ROM si aucun essai local n'a été fait. En CI, utilise uniquement les tests sans ROM et des fixtures/sentinelles synthétiques.

### Étape 4 — Exécuter le reste du plan AppImage par priorité

Utilise les sections non cochées de `spec.md` comme backlog réel, et travaille par lots testables :

1. **Confidentialité et intégrité du payload** : liste positive des fichiers AppDir, exclusions de défense en profondeur, fixtures sentinelles qui simulent des données interdites, scan du payload et des assets. Vérifier au minimum ROM (`.nds`), save (`.sav`), `game/`, `build/`, `extract/`, `rom/`, `testbuild/`, Mods, musique, textures et le code/binaire généré depuis une ROM.
2. **Release sûre** : `dry_run=true` par défaut, préflight read-only, job publish séparé, permission `contents: write` uniquement sur le job de publication conditionnel, contrôle de provenance/commit/artifact/checksum et aucun écrasement de tag/release.
3. **Reproductibilité / supply chain** : lock Conda transitif pour Linux x86_64, métadonnées de provenance, analyse des dépendances ELF, rapport de taille et SBOM si réalisable.
4. **Performance** : baselines comparables, optimisation seulement si mesurée et sans dégrader la portabilité ni supprimer une dépendance nécessaire.

Actualise `spec.md`, `appimage-audit.md`, `audit-files.md` et/ou la doc utilisateur au fur et à mesure ; sépare les constats, les changements effectués et les points restant à tester. Ne coche comme accompli que ce qui a réellement été exécuté et validé.

## Invariants de sécurité et de release

- L'AppImage distribue un **constructeur**, pas le jeu précompilé.
- Aucune ROM privée, sauvegarde, source générée depuis la ROM, jeu compilé ni workspace utilisateur ne doit entrer dans le dépôt, les logs, les artefacts ou une release.
- La génération complète du jeu se fait localement avec la ROM du propriétaire.
- Aucune release ou tag n'est créé automatiquement ; ne pas publier.
- Ne jamais supprimer ou écraser le workspace, les données utilisateur ou les sauvegardes sur une erreur de build.
- Ne pas affirmer que le gameplay est validé sans véritable test local documenté.
- Créer une branche dédiée et une PR, ne pas pousser sur `main` et ne pas fusionner la PR.
- Si des outils GitHub sont disponibles, utilise-les réellement pour inspecter les commits, workflows et logs, et pour créer la branche/PR. N'invente aucun résultat d'exécution.

## Résultat attendu

À la fin de la session :
1. la cause CI observée est corrigée et la nouvelle CI a un résultat rapporté avec l'URL/run et le SHA ;
2. la lenteur de préparation est expliquée par des mesures ou, si la mesure locale dépend d'une ROM, par une analyse de code clairement qualifiée ;
3. l'utilisation réelle de threads/processus/parallélisme par la chaîne AppImage est documentée ;
4. les améliorations faites sont testées et isolées dans une PR ;
5. les documents de suivi reflètent honnêtement ce qui est terminé et ce qui reste.

Commence par vérifier l'état courant et avancer jusqu'au maximum vérifiable dans cette session. Ne demande pas de confirmation pour les étapes de diagnostic et de correction non destructives ; si une information manque, fais une analyse raisonnable, documente la limite, et continue sans action destructive.

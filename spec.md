# Spec — Synchronisation upstream et AppImage optimale

Date : 2026-10-28  
Projet : DQIX Enhanced — fork `rakmahefa/dqix-enhanced`  
Upstream de référence : `sackchief/dqix-enhanced`

## 1. Objectif

Maintenir le fork à jour avec le projet upstream tout en conservant son constructeur Linux/AppImage graphique et son workflow de publication. L'artefact est un **constructeur** : il embarque les sources publiques et une toolchain Linux, puis compile localement le jeu à partir d'une ROM fournie par l'utilisateur. Il ne doit jamais embarquer ou publier la ROM, le jeu compilé, les sauvegardes ou du code généré à partir de la ROM.

L'objectif d'optimisation est la meilleure combinaison de reproductibilité, sécurité de distribution, compatibilité Linux, démarrage rapide au second lancement et taille maîtrisée — sans supprimer des outils nécessaires au build local.

## 2. État initial vérifié

| Élément | État observé au 2026-10-28 |
|---|---|
| Fork avant synchronisation | `4320789d4c16880f08fa9852510e9489146ff789` |
| Upstream de référence | `b85da5d6f8a5580f737e5f96418f4b07a8083591` |
| Ancêtre commun | `bcba9c2821c2d540e26e617a20152ad7823307d0` |
| Écart | Fork : 30 commits propres ; upstream : 51 commits supplémentaires |
| AppImage | Constructeur x86_64, GUI Tk par défaut, outilchain Conda empaquetée et cache utilisateur vérifié par SHA-256 |
| Publication | Le fork possède `.github/workflows/release-appimage.yml`, absent de l'upstream |
| Limites principales | Pas de lock Conda transitif complet, pas d'audit ELF complet ni de SBOM, pas de mode dry-run pour la publication, couverture GUI limitée en CI |

La synchronisation doit intégrer les évolutions v32 : ROM européenne et choix de langue, adaptation des adresses de recompilation, améliorations du frontend/rendu et widescreen, HD text, Mods/musique, Discord et corrections du recompiler. Le constructeur AppImage et ses fichiers principaux sont déjà présents dans les deux arbres ; on conserve la version amont actuelle de ces fichiers, dont le pin d'appimagetool 1.9.0.

## 3. Invariants impératifs

1. **Aucun contenu de jeu dans l'artefact CI/release** : pas de `*.nds`, `*.sav`, dossier `game/`, ROM extraite, sources C générées depuis une ROM, binaires du jeu ni données de workspace utilisateur.
2. Le build complet s'exécute localement depuis la ROM choisie ; aucun workflow distant ne reçoit de ROM personnelle.
3. Les sauvegardes et fichiers générés dans le workspace sont préservés lors de la mise à jour des sources.
4. Toute écriture de publication GitHub est derrière une action explicite ; le préflight par défaut est en lecture seule.
5. Une validation sans ROM vérifie les sources et la toolchain, mais n'affirme pas que le jeu complet fonctionne. Un test avec ROM reste manuel et local.
6. Ne pas écraser les fonctionnalités et réglages v32 lors d'une prochaine synchronisation. Les changements upstream doivent rester séparables des extensions propres au fork.

## 4. Plan priorisé

### P0 — Synchronisation et séparation des changements

- [x] Comparer les têtes, identifier l'ancêtre commun et mesurer l'écart.
- [x] Synchroniser le code source depuis l'upstream sans perdre les fichiers AppImage propres au fork.
- [x] Conserver `.github/workflows/release-appimage.yml`, les audits du fork et cette spec.
- [x] Actualiser l'audit d'arborescence et l'audit AppImage.
- [ ] Faire passer la CI après synchronisation et vérifier la compatibilité de tous les changements v32 dans le constructeur Linux.
- [ ] Garder les prochains changements de performance/sécurité sur une branche dédiée et en PR.

### P0 — Protection du contenu utilisateur

- [ ] Faire de l'assemblage AppDir une liste explicite de contenu autorisé, avec une liste d'exclusion en défense en profondeur.
- [ ] Exclure explicitement les répertoires et données locales possibles : `testbuild/`, `music/`, `textures/`, `game/`, `build/`, `extract/`, `rom/`, `reports/`, objets, bibliothèques et fichiers ROM/sauvegarde. Réviser les motifs lorsque le projet ajoute de nouveaux types de médias.
- [ ] Vérifier les fichiers réellement placés sous `AppDir/usr/share/dqix-enhanced/source`, puis échouer si une ROM, une sauvegarde ou un binaire/code généré est détecté.
- [ ] Ajouter un test de politique de contenu qui utilise des fichiers sentinelles synthétiques et vérifie qu'ils ne sont pas inclus.
- [ ] Protéger la synchronisation du workspace : ne jamais supprimer les sorties de build, le dossier `game/`, les ROMs ni les sauvegardes ; supprimer seulement les fichiers source gérés devenus obsolètes à l'aide d'un manifeste appartenant au constructeur.

### P1 — Reproductibilité du build

- [ ] Remplacer l'environnement Conda seulement partiellement verrouillé par un lock de plateforme x86_64 Linux incluant les versions, builds et dépendances transitives, généré par une procédure documentée.
- [ ] Épingler les actions GitHub par SHA complet et conserver les sommes de contrôle des outils téléchargés. Le workflow actuel épingle déjà appimagetool 1.9.0 par SHA-256.
- [ ] Écrire les métadonnées de build : version du projet, SHA exact des sources, cible et runner, version/hash du lock, version et SHA d'appimagetool, hash de l'archive toolchain.
- [ ] Garder l'artefact de toolchain exact qui a réussi la CI ; ne pas résoudre une nouvelle fois les dépendances au moment de la publication.

### P1 — Hardening de la publication

- [ ] Ajouter un input `dry_run` à `release-appimage.yml`, par défaut `true`.
- [ ] Séparer un préflight strictement en lecture seule du job de publication ; `contents: write` uniquement dans le job conditionnel déclenché par `dry_run=false`.
- [ ] Contrôler le nom et le chemin du workflow source, l'événement, le dépôt source, la branche autorisée, le SHA exact, la conclusion réussie et la cohérence de l'artefact téléchargé.
- [ ] Valider les sommes, les noms exacts des assets, leur taille et l'absence de ROM/sauvegarde avant toute création de tag/release.
- [ ] Ne jamais écraser un tag ou une release existante.

### P1 — Audit ELF et chaîne de dépendances

- [ ] Extraire l'AppImage en CI et analyser chaque exécutable/bibliothèque ELF du payload.
- [ ] Échouer sur toute dépendance non résolue et toute dépendance hôte inattendue ; maintenir une allowlist courte et expliquée pour les composants qui doivent venir de l'hôte (glibc, serveur de fenêtres, audio, OpenGL/Vulkan et pilotes).
- [ ] Vérifier spécifiquement que les exécutables du toolchain et le programme de smoke test SDL se résolvent dans l'environnement empaqueté.
- [ ] Générer un rapport JSON d'audit ELF conservé comme artefact.
- [ ] Ne pas tenter d'embarquer les pilotes graphiques du noyau ni la pile graphique de la machine.

### P1 — Tests fiables sans ROM

- [ ] Conserver les tests sémantiques du recompiler, les contrôles shell/Python et le self-test AppImage.
- [ ] Tester `--version`, `--help` et `--self-test` sur l'AppImage extraite, avec un `HOME`, `XDG_CACHE_HOME` et `XDG_DATA_HOME` temporaires.
- [ ] Tester le premier démarrage (extraction de toolchain), le second démarrage (cache), un hash de toolchain invalide et l'absence d'affichage.
- [ ] Ajouter un test de lancement de GUI sous serveur X virtuel si fiable ; ne pas traiter l'ouverture réelle de sélecteurs Tk comme couverte par un test sans display.
- [ ] Tester les chemins de workspace (vide, existant et non géré) et s'assurer qu'une erreur de build laisse le journal consultable et ne supprime pas les données.
- [ ] Faire un test manuel final de la GUI et du build avec une ROM personnelle, uniquement sur la machine du testeur.

### P2 — Performance et taille, seulement après une baseline

- [ ] Mesurer durée et taille de résolution Conda, empaquetage de toolchain, construction AppImage, premier lancement et lancement avec cache chaud.
- [ ] Publier un rapport de taille (AppImage, archive de toolchain, AppDir et plus gros fichiers).
- [ ] Évaluer si PyYAML est nécessaire dans la toolchain packagée : le recompiler upstream sait lire certains fichiers sans ce module, mais le self-test actuel le demande explicitement. Ne le retirer qu'après adaptation du test et preuve que les deux régions de ROM passent.
- [ ] Comparer le temps de compression et la taille avant de changer le format/les niveaux de compression.
- [ ] Garder les caches micromamba, téléchargements de paquets, appimagetool et toolchain par SHA ; éviter les caches mutables non validés.
- [ ] Optimiser la copie des sources et les recompilations locales seulement après avoir défini les règles d'invalidation et mesuré un build de référence.

## 5. Critères de fin (Definition of Done)

- La branche de synchronisation contient les évolutions de l'upstream choisi et conserve les extensions du fork identifiées dans `upstream-sync.md`.
- Tous les tests de source passent, le workflow AppImage produit un artefact et `--version`, `--help` et `--self-test` réussissent.
- L'ELF audit ne signale aucune dépendance inattendue ; provenance, somme de contrôle, lock, rapport de taille et SBOM sont produits.
- Un scan du payload et des assets téléchargés confirme l'absence de ROM, de sauvegarde, de code généré depuis la ROM et du dossier `game/`.
- Le préflight de release par défaut n'a aucun droit d'écriture et ne crée ni tag ni release.
- La GUI a été essayée sur un bureau Linux et un test local du jeu a été validé sans partager la ROM ni le workspace.
- Les limitations et les versions exactes validées sont documentées. Aucune release n'est publiée automatiquement par cette spec.

## 6. Décision d'implémentation

Cette spec fixe le plan. Les éléments non cochés sont des travaux ultérieurs ; la synchronisation et la mise à jour des audits ne doivent pas être présentées comme une validation déjà passée du build v32 complet ou du gameplay Linux.

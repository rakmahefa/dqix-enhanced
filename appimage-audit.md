# Audit du packaging Linux/AppImage — DQIX Enhanced

Date : 2026-10-28  
Synchronisation de référence : `sackchief/dqix-enhanced@b85da5d6f8a5580f737e5f96418f4b07a8083591`  
Ancien état du fork : `rakmahefa/dqix-enhanced@4320789d4c16880f08fa9852510e9489146ff789`  
Spec AppImage : [spec.md](spec.md)

## Résumé

Le fork partait de v31 et avait ajouté un constructeur AppImage graphique ainsi qu'un workflow manuel de publication. L'upstream a depuis évolué vers v32. La synchronisation prend comme base le code upstream afin de récupérer les améliorations de jeu et de build, tout en conservant les extensions Linux spécifiques au fork. Elle ne publie aucune ROM ni aucun jeu compilé.

L'AppImage est un **constructeur x86_64**, pas une distribution du jeu déjà construit. L'utilisateur choisit sa propre ROM et le constructeur produit le jeu localement dans un workspace qu'il doit garder privé.

## Structure et responsabilités

| Chemin | Rôle | Points d'audit |
|---|---|---|
| `README.md`, `ROADMAP.md`, `release-notes/v32.md` | Documentation de la version upstream v32 et plan v33 | Base de référence désormais upstream ; ne pas restaurer l'ancien README/roadmap du fork. |
| `docs/LINUX-APPIMAGE.md` | Instructions Linux importées de l'upstream | Complété par l'audit du fork et sa spec ; noter que le fork conserve aussi son workflow de release. |
| `appimage-audit.md`, `audit-files.md`, `spec.md`, `upstream-sync.md` | Audit du fork, plan d'évolution et historique de synchronisation | À actualiser après toute modification du packaging ou du workflow. |
| `tools/build-game.sh` | Valide la ROM, extrait le code, appelle le recompiler ARM9/ARM7, compile le runtime et assemble `game/` | Copie la ROM dans le workspace local. Jamais à exécuter dans la CI sans une ROM synthétique explicitement conçue pour le test. |
| `tools/extract_rom.py`, `tools/region.py` | Extrait les régions CPU/overlays et adapte les adresses selon la région | v32 ajoute le support de la ROM européenne ; les deux régions doivent être testées par le propriétaire d'une ROM appropriée. |
| `recomp/build.py`, `recomp/arm7.py`, `recomp/insn.py` | Recompilation ARM vers le code C | Le recompiler v32 a une lecture YAML de repli ; mesurer avant de supprimer PyYAML de la toolchain AppImage. |
| `wrap.txt`, `hle.txt` et `config/usa/arm9/` | Descriptions de hooks, symboles, relocalisations et HLE | Respecter les changements amont : ne pas réintroduire des adresses USA fixes là où les wrappers/régions sont nécessaires. |
| `runtime/` | Runtime DS, SDL2, audio, entrée, rendu et réseau | v32 modifie considérablement le frontend, le rendu, les langues et les Mods ; pas de cherry-pick isolé de l'ancien runtime. |
| `runtime/text_hd.*`, `runtime/music.*`, `runtime/discord.*`, `runtime/gpu2d.c`, `runtime/gpu3d.c` | HD text, export/musique, Rich Presence et rendus | Ressources/outils runtime upstream ; vérifier les dépendances et notices, mais ne pas les exclure pour gagner de la taille sans preuve. |
| `build.mk`, `BUILD.bat`, `build-windows.sh`, `recomp.bat` | Construction Windows/Unix | Le build upstream utilise désormais `wrap.txt` et les nouveaux fichiers de génération. |
| `packaging/linux/AppRun` | Entrée AppImage | Vérifie le SHA-256 de l'archive toolchain, l'extrait dans le cache XDG, effectue `conda-unpack`, configure PATH/LD_LIBRARY_PATH et lance la GUI par défaut. |
| `packaging/linux/gui.py` | Interface de construction Tk | Choix ROM/workspace, build worker, journal et jalons de progression. Les données de build restent dans le workspace. |
| `packaging/linux/environment.yml` | Toolchain Conda | Versions directes contraintes, mais pas encore de lock transitif complet par builds de paquets. |
| `packaging/linux/dqix-enhanced-builder.desktop`, `*.svg` | Intégration bureau et icône neutre | Ne doit pas dépendre de l'icône tirée de la ROM. |
| `.github/workflows/linux-ci-appimage.yml` | Tests et fabrication de l'AppImage | Tests sans ROM, toolchain packée, checksum appimagetool, scan sommaire de contenu, AppImage et self-test. |
| `.github/workflows/release-appimage.yml` | Publication manuelle spécifique au fork | Absent de l'upstream ; à renforcer avec préflight read-only, `dry_run=true` par défaut et droits d'écriture limités au job publish. |
| `tests/semantics/run.py` | Tests sémantiques du recompiler | Tests sans ROM ; ils ne valident pas le build complet ou le rendu en jeu. |
| `.gitignore` | Exclusions des résultats locaux | Ne remplace pas les filtres d'exclusion du workflow AppImage. |

## Invariants de distribution

- Le dépôt public et l'artefact CI ne contiennent pas de ROM utilisateur.
- Le contenu du workspace après lancement est privé : la ROM copiée, `build/`, code généré, `game/`, saves et settings n'ont pas à être partagés.
- Aucun workflow ne doit recevoir ou uploader ces fichiers.
- Le script de packaging exclut actuellement plusieurs dossiers de build et extensions connues ; la spec demande une politique de contenu positive et un test de sentinelles, incluant `testbuild/`, dossiers de Mods et extensions audio supplémentaires.
- Le workflow Linux crée `DQIX-Enhanced-Builder-x86_64.AppImage`, `SHA256SUMS` et `build-info.txt`. Il ne produit pas encore de SBOM CycloneDX, de manifeste exhaustif de payload, d'audit ELF complet ni de rapport de taille.

## Validation à faire après synchronisation

- Vérifier que le workflow source/sémantique et le workflow AppImage passent sur la branche synchronisée.
- Exécuter `--version`, `--help` et `--self-test` avec des répertoires HOME/XDG temporaires.
- Vérifier le contenu de l'AppDir/AppImage extraite, ses bibliothèques ELF et les SHA-256.
- Tester la GUI sur un bureau Linux : sélection ROM et workspace, progression, journal, erreur et reprise après échec.
- Sur une machine possédant une ROM personnelle, réaliser un build local USA et/ou Europe puis vérifier jeu, audio, entrées, contrôles et sauvegardes. Ne pas transférer la ROM ni le workspace vers CI.

## Vérification CI et AppImage — 2026-10-28

- main était sur le merge #4, SHA 3a84fe776dfed59421987722073d03a438a3ab56. Le run main #19 a réussi les tests source mais échoué à Build AppImage car desktop-file-validate n'était pas installé.
- Le PR #6 ajoute desktop-file-utils tout en gardant libfuse2, rsync et squashfs-tools. Le run #21 a construit l'AppImage, passé le self-test, généré SHA256SUMS et uploadé l'artefact. Le run #22 (SHA ba5039298f2d95c17c0d2d54113d7a45c7ac6685) a aussi validé --version, --help et sha256sum --check.
- La sortie appimagetool reste « continuous build (git version 81ee1db) » ; le hash du fichier téléchargé correspond toutefois au SHA-256 épinglé dans le workflow. Ne pas traiter la seule chaîne de version comme preuve d'un artefact altéré.
- Le log appimagetool rapporte un filesystem squashfs de 230.29 MiB. Le ZIP d'artefact CI fait 242421192 octets. Il ne s'agit pas encore d'un rapport détaillé par composant, et aucun manifeste exhaustif de payload, SBOM ou audit ELF complet n'est démontré.

## Chemin de build et parallélisme

- La GUI crée un thread Python threading.Thread pour le worker. Celui-ci copie source vers le workspace, lit la sortie du script de build via un pipe, puis remonte les événements à Tk avec une file et root.after(100, ...).
- copytree recopie les sources dans le workspace à chaque construction ; le nouveau journal donne sa durée et le nombre/volume des fichiers copiés. C'est un candidat de coût, pas une conclusion sans résultats locaux.
- Extraction, détection de région, génération ARM9, génération ARM7, compilation et assemblage sont exécutés en séquence par le script shell. Le recompiler parcourt les fonctions séquentiellement ; le parallélisme explicite est make -j$(nproc) pendant la compilation C.
- L'assemblage inclut une copie de l'extraction vers game/data/code et une requête DQVC facultative avec un timeout de 30 secondes. Sa contribution reste à mesurer.
- Aucun build avec ROM personnelle, benchmark froid/chaud, mesure CPU/I/O locale ou test gameplay n'a été réalisé dans cette session.

## Filtrage positif du payload — vérifié par CI #29

Le workflow est modifié pour utiliser packaging/linux/appimage-filter.txt, une liste positive avec exclusions de défense en profondeur, et tests/packaging/test_appimage_payload.py exécute le filtre réel contre des sentinelles synthétiques. Un scan de contenu du payload et des assets refuse les extensions privées connues ainsi que les dossiers de build privés. La réussite de ce lot reste soumise à la CI du nouveau SHA.

La CI #29 (`40725797382`, SHA `23a7cee4605e35c8f58ca92fcb856e7d15172f6d`) a passé le test synthétique de la politique : 10 fixtures autorisées conservées, 38 sentinelles privées exclues. Le scan défensif du payload, la génération AppImage, `--version v32`, `--help`, `--self-test`, `sha256sum --check` et l'upload d'artefact ont tous réussi. Artefact ID `12832952308`, ZIP 242372040 octets, digest `sha256:dc09d1f3275db1264bd81a250ec9f965426a5ce4648ffba8de7cff1d82ec4ea8`. La validation s'applique au SHA de cette CI, avant les modifications du workflow release.

## Release sûre — changement en cours

Le workflow release est réorganisé en préflight read-only et job publish distinct. Le dry-run est vrai par défaut. Le préflight compare la provenance du run et de l'artefact puis vérifie le ZIP et les assets ; seul le job explicitement déclenché avec dry-run faux a la permission `contents: write`. Le tag neuf est créé sans force-update et la release utilise `--verify-tag`. La CI doit confirmer les contrôles statiques avant de traiter ce lot comme terminé.


## Bilan de cette reprise — 2026-10-28

La CI #30 (run `40727004320`, SHA `3ad7890cf5c6297a27ad2b709838146bd64efe9b`) passe avec les tests statiques du workflow de release et le build AppImage complet. La CI #29 (run `40725797382`, SHA `23a7cee4605e35c8f58ca92fcb856e7d15172f6d`) a validé le filtre positif, 10 fixtures conservées et 38 sentinelles exclues. Les contrôles release ont donc une validation statique ; le vrai préflight read-only en workflow_dispatch n'a pas été déclenché, et aucun tag/release n'a été créé.

Restent ouverts : lockfile Conda transitif versionnée et effectivement consommée par la construction, analyse des dépendances ELF, SBOM et rapport de taille par composants, et benchmarks locaux avec ROM appartenant à l'utilisateur. Ne pas interpréter les timings instrumentés comme des résultats de benchmark tant qu'aucun build ROM local n'a été chronométré.


## Conda lock explicite — validation CI en attente

Le PR #7 versionne `packaging/linux/conda-linux-64.lock`, une liste explicite Linux x86_64 issue du solve du runner (96 paquets, builds fixés, URLs `linux-64`/`noarch` avec fragments MD5, SHA-256 `5aeec2066f1d9fcfbf729de3ea6531559c8b9b0a31065a610059fb18793bae96`). Le workflow est modifié pour installer depuis cette liste, contrôler le checksum et comparer la liste installée à celle commise. Le résultat final doit confirmer cette CI avant de déclarer le lock effectif et reproductible.

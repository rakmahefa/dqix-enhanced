# Audit des fichiers — DQIX Enhanced

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
| `wrap.txt`, `hle.txt`, `config/usa/arm9/`, `config/eur/`* | Descriptions de hooks, symboles, relocalisations et HLE | Respecter les changements amont : ne pas réintroduire des adresses USA fixes là où les wrappers/régions sont nécessaires. |
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

`config/eur/`* représente les chemins de configuration régionale réellement présents dans la branche synchronisée ; vérifier l'arborescence si l'upstream change la disposition.

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

## État vérifié lors de la reprise — 2026-10-28

- SHA de main : 3a84fe776dfed59421987722073d03a438a3ab56. Upstream sackchief/dqix-enhanced toujours à b85da5d6f8a5580f737e5f96418f4b07a8083591 ; comparaison : 32 commits en avance, aucun en retard.
- CI #19 sur main : tests source réussis, empaquetage bloqué faute de desktop-file-validate. Correctif dans le PR #6 ; run #21 sur 519c14d7d2b7486de402ece21a226de67b6ba930 a passé entièrement, avec AppImage, self-test, checksums générés et artefact uploadé. Run #22 sur ba5039298f2d95c17c0d2d54113d7a45c7ac6685 a en plus vérifié --version, --help et les checksums.
- L'instrumentation de timing est ajoutée après le SHA ba503929 ; attendre le run du SHA final pour valider les changements Python et shell.

## Détails de la chaîne de build relevés dans le code

- packaging/linux/gui.py utilise un thread de travail et une file d'événements afin de garder la boucle Tk réactive. shutil.copytree recopie les sources dans le workspace à chaque build ; durée et octets sont instrumentés par le PR #6.
- tools/build-game.sh appelle successivement tools/extract_rom.py, tools/region.py, recomp/build.py ARM9, recomp/arm7.py, make -j$(nproc) puis l'assemblage game/. Le codegen Python est séquentiel ; le parallélisme explicite concerne la compilation C.
- Les mesures à venir sont des temps muraux et volumes de fichiers/octets. Ni CPU/I/O local, ni temps de génération complète avec ROM ne sont mesurés dans cette session. Le test sans ROM ne valide pas le gameplay.

## Politique de contenu AppImage

La branche du PR contient une liste positive packaging/linux/appimage-filter.txt. Elle autorise explicitement les sources de build/runtime, la configuration, les polices, skins et dépendances sources requises ; tout chemin non déclaré est refusé. Les dossiers de sortie et de données privées sont exclus avant les entrées positives, et les extensions ROM/save/binaires sont exclues à tous les niveaux. Le test synthétique tests/packaging/test_appimage_payload.py vérifie les inclusions positives et les sentinelles interdites ; attendre la CI du SHA final avant de considérer la règle entièrement validée.

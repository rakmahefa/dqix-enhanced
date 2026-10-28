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
- [x] Faire passer la CI après synchronisation et vérifier la compatibilité v32 du constructeur Linux (post-merge #35 et runs PR #7 #45 verts ; aucun gameplay ROM n'est revendiqué).
- [x] Garder les changements de performance/sécurité sur une branche dédiée et en PR (branche `build/conda-lock-elf-audit`, PR #7 ouverte, non fusionnée).

### P0 — Protection du contenu utilisateur

- [x] Faire de l'assemblage AppDir une liste positive de contenu autorisé, avec exclusions de défense en profondeur (validé en CI #29/#45).
- [x] Exclure explicitement les répertoires et données locales possibles : `testbuild/`, `music/`, `textures/`, `game/`, `build/`, `extract/`, `rom/`, `reports/`, objets, bibliothèques et fichiers ROM/sauvegarde (38 sentinelles synthétiques exclues dans le run #29).
- [x] Vérifier le payload AppDir et échouer sur les extensions/dossiers privés identifiés (CI #29/#45). Le commit en cours élargit encore l'inspection ELF aux fichiers directement sous AppDir.
- [x] Ajouter un test de politique de contenu à sentinelles synthétiques (CI #29/#45 : 10 fixtures autorisées et 38 sentinelles exclues).
- [ ] Protéger la synchronisation du workspace : ne jamais supprimer les sorties de build, le dossier `game/`, les ROMs ni les sauvegardes ; supprimer seulement les fichiers source gérés devenus obsolètes à l'aide d'un manifeste appartenant au constructeur.

### P1 — Reproductibilité du build

- [x] Remplacer l'environnement Conda seulement partiellement verrouillé par un lock explicite de plateforme Linux x86_64 incluant les URLs exactes des 96 paquets résolus et leurs builds. La lockfile est versionnée et le workflow l'utilise ; la validation de bout en bout est encore en attente de la CI de ce SHA.
- [ ] Épingler les actions GitHub par SHA complet et conserver les sommes de contrôle des outils téléchargés. Le workflow actuel épingle déjà appimagetool 1.9.0 par SHA-256.
- [x] Écrire les métadonnées de build : version du projet, SHA source, cible/runner, SHA du lock, version/SHA appimagetool et SHA archive toolchain (vérifié dans le build CI #45).
- [x] Garder l'artefact AppImage exact produit par la CI ; la publication vérifie la provenance de l'artefact du run sélectionné au lieu de résoudre à nouveau les dépendances.

### P1 — Hardening de la publication

- [x] Ajouter un input `dry_run` à `release-appimage.yml`, par défaut `true` (validation statique CI #45 ; aucun dispatch de publication exécuté).
- [x] Séparer le préflight en lecture seule du job de publication ; `contents: write` uniquement dans le job conditionnel à `dry_run=false` (validation statique CI #45).
- [x] Contrôler workflow source, événement, dépôt, branche, SHA, conclusion, provenance et digest de l'artefact (validation statique CI #45 ; aucun vrai préflight dispatché).
- [x] Valider les checksums, le manifeste exact des assets, le digest ZIP, le commit source et les smoke tests CLI avant publication (statique CI #45 ; aucun tag/release créé).
- [x] Refuser un tag/release existant et créer un nouveau tag sans force-update ; aucun tag/release n'a été publié.

### P1 — Audit ELF et chaîne de dépendances

- [ ] Extraire l'AppImage en CI et analyser chaque exécutable/bibliothèque ELF du payload. (Implémenté dans le PR #7 ; en attente du résultat CI.)
- [ ] Échouer sur toute dépendance non résolue et toute dépendance hôte inattendue ; maintenir une allowlist courte et expliquée pour les composants qui doivent venir de l'hôte (glibc, serveur de fenêtres, audio, OpenGL/Vulkan et pilotes). (Implémenté, résultat CI en attente.)
- [ ] Vérifier spécifiquement que les exécutables du toolchain et le programme de smoke test SDL se résolvent dans l'environnement empaqueté.
- [x] Générer un rapport JSON d'audit ELF conservé comme artefact de diagnostic CI (#45). (Implémenté, résultat CI en attente.)
- [ ] Ne pas tenter d'embarquer les pilotes graphiques du noyau ni la pile graphique de la machine.

### P1 — Tests fiables sans ROM

- [ ] Conserver les tests sémantiques du recompiler, les contrôles shell/Python et le self-test AppImage.
- [ ] Tester `--version`, `--help` et `--self-test` sur l'AppImage extraite, avec un `HOME`, `XDG_CACHE_HOME` et `XDG_DATA_HOME` temporaires.
- [ ] Tester le premier démarrage (extraction de toolchain), le second démarrage (cache), un hash de toolchain invalide et l'absence d'affichage.
- [ ] Ajouter un test de lancement de GUI sous serveur X virtuel si fiable ; ne pas traiter l'ouverture réelle de sélecteurs Tk comme couverte par un test sans display.
- [ ] Tester les chemins de workspace (vide, existant et non géré) et s'assurer qu'une erreur de build laisse le journal consultable et ne supprime pas les données.
- [ ] Faire un test manuel final de la GUI et du build avec une ROM personnelle, uniquement sur la machine du testeur.

### P2 — Performance et taille, seulement après une baseline

- [x] Mesurer en CI la durée de résolution/environnement verrouillé, empaquetage toolchain et construction AppImage. Mesures cold/warm de démarrage ajoutées dans le commit de suivi ; leur résultat est en attente.
- [x] Publier un rapport JSON de taille AppImage/toolchain/AppDir/fichiers les plus volumineux (artefact de diagnostic CI #45).
- [ ] Évaluer si PyYAML est nécessaire dans la toolchain packagée : le recompiler upstream sait lire certains fichiers sans ce module, mais le self-test actuel le demande explicitement. Ne le retirer qu'après adaptation du test et preuve que les deux régions de ROM passent.
- [ ] Comparer le temps de compression et la taille avant de changer le format/les niveaux de compression.
- [x] Utiliser un lock Conda explicite vérifié, un cache appimagetool par SHA-256 et un marqueur SHA-256 pour l'archive toolchain (CI #45).
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

## 7. Journal de vérification — 2026-10-28

### CI et AppImage

- Le SHA courant de main avant le PR de correction était 3a84fe776dfed59421987722073d03a438a3ab56. La comparaison avec l'upstream b85da5d6f8a5580f737e5f96418f4b07a8083591 donnait 32 commits en avance et 0 en retard ; aucun nouveau commit upstream n'était à intégrer.
- Le run main #19 (40720090797) a reproduit « desktop-file-validate command is missing, please install it ». Le PR #6 ajoute desktop-file-utils sans retirer les autres utilitaires.
- Le premier run complet vert après ce correctif est #21 (40721331417), sur le SHA 519c14d7d2b7486de402ece21a226de67b6ba930 : tests sans ROM, construction AppImage, --self-test, génération des checksums et upload de l'artefact. L'archive d'artefact faisait 242421192 octets et son digest ZIP était 3285b650453a92add0d2e676b1ff7ba7dc05051e8b760a65621bb9c4f9277677. Ce run ne vérifiait pas explicitement --version, --help ni les checksums par sha256sum --check.
- Le workflow du PR #6 ajoute ces contrôles explicites ; le run #22 (40721839065) a réussi sur le SHA ba5039298f2d95c17c0d2d54113d7a45c7ac6685. Le SHA du commit est important : toute instrumentation ajoutée après lui doit avoir sa propre CI avant d'être considérée validée.
- Le log appimagetool affiche encore « continuous build (git version 81ee1db) », mais le fichier téléchargé depuis l'URL épinglée 1.9.0 passe le SHA-256 attendu 46fdd785094c7f6e545b61afcfb0f3d98d8eab243f644b4b17698c01d06083d1. La seule bannière ne prouve pas une incohérence du binaire ni une compromission. L'avertissement appstreamcli manquant n'a pas bloqué la construction.

### Parcours du build et parallélisme (analyse du code, pas un benchmark ROM)

- packaging/linux/gui.py démarre _build_worker dans threading.Thread ; le worker envoie journal/progression dans queue.Queue et Tk interroge cette file par root.after(100, ...). La copie et le sous-processus de build sont donc hors du thread UI.
- La préparation du workspace copie les sources avec shutil.copytree à chaque demande de build. C'est un candidat de coût I/O, à confirmer par les temps, fichiers et octets consignés dans le journal.
- tools/build-game.sh exécute en séquence l'extraction, la détection de région, recomp/build.py (ARM9), recomp/arm7.py (ARM7), make puis l'assemblage de game/. tools/extract_rom.py lit la ROM entière en mémoire et décompresse les overlays dans une boucle séquentielle.
- Les boucles de traduction dans recomp/build.py et recomp/arm7.py sont séquentielles (for fn in queue) ; aucun pool de threads/processus de génération n'y a été constaté. En revanche, la compilation C utilise make -j"$(nproc)", donc les cibles d'objet indépendantes peuvent compiler en parallèle. -lpthread dans les options de link ne prouve pas un parallélisme de codegen.
- L'assemblage copie en série l'extraction vers game/data/code, la ROM et les assets. Le téléchargement facultatif des données DQVC peut attendre jusqu'à 30 secondes ; sa contribution au temps global reste à mesurer.
- packaging/linux/AppRun hache l'archive de toolchain à chaque lancement, puis ne l'extrait et ne lance conda-unpack que si le cache XDG ne passe pas la vérification. Les temps de lancement à cache froid/chaud, CPU et I/O ne sont pas mesurés ici.
- Une instrumentation est ajoutée dans ce PR : temps muraux, nombre de fichiers et octets pour la copie du workspace, extraction, codegen ARM9/ARM7, compilation/link et assemblage. Aucun build complet avec ROM personnelle, aucune mesure CPU/I/O locale et aucun test gameplay ne sont revendiqués. Comparer ensuite un build local froid et chaud sur la même machine, le même workspace et la même ROM.


## 8. Protection du payload AppImage — validé par CI

- Ajout d'une politique rsync à liste positive : seuls les chemins source explicitement autorisés sont copiés dans l'AppDir.
- Les exclusions de défense en profondeur refusent notamment ROM/saves, game/, build/, extract/, rom/, testbuild/, codegen temporaire, Mods, musique, textures et artefacts de compilation.
- Ajout d'un test de fixture synthétique qui applique la véritable politique rsync à un arbre temporaire, vérifie les fichiers légitimes autorisés et recherche les sentinelles interdites.
- Le scan final du payload et des assets est exécuté lors de l'assemblage. Ne marquer ce lot comme validé qu'après réussite de la CI sur le SHA contenant ces changements. Le test n'ouvre ni ne requiert de ROM réelle.

## 9. Release sûre — préflight et garde-fous

- Le workflow de publication est manuel uniquement et `dry_run=true` par défaut. Le préflight est en lecture seule et n'a pas de permission d'écriture de contenu.
- Le préflight vérifie run CI terminé avec succès sur main/stable par push, dépôt source identique, SHA source, artefact associé au même run/SHA, digest SHA-256 de l'archive, manifeste exact, checksums internes et smoke tests CLI. Toute erreur API autre qu'un 404 explicite bloque la voie.
- Le job publish n'existe en exécution que pour `dry_run=false`; lui seul reçoit `contents: write`. Avant publication, il revalide le run et l'artefact, puis crée un tag neuf via un POST de référence Git non forcé et crée la release avec `--verify-tag`. Aucune exécution de publication n'a été déclenchée dans cette session.
- Les protections du workflow doivent être testées statiquement par la CI du commit final. Cela ne signifie pas que la release a été publiée ou qu'une exécution publish réelle a été testée.


## 10. État des contrôles résiduels — 2026-10-28

- Le workflow release a passé les tests statiques de sécurité dans la CI #30 (run `40727004320`, SHA `3ad7890cf5c6297a27ad2b709838146bd64efe9b`) : déclenchement manuel seulement, `dry_run=true` par défaut, préflight en lecture seule, et permission `contents: write` limitée au job publish. Aucun vrai `workflow_dispatch` de préflight n'a été exécuté dans cette session et aucune release/tag n'a été créée.
- Le filtrage positif du payload a été exécuté avec des fixtures synthétiques et le smoke test AppImage a passé en CI #29 (run `40725797382`, SHA `23a7cee4605e35c8f58ca92fcb856e7d15172f6d`). Ces résultats valident la portée des chemins/extensions testés, pas une analyse complète sémantique de chaque octet embarqué.
- La lockfile Conda explicite est consommée et vérifiée par la CI #54 ; l'audit ELF, le rapport de taille et le SBOM CycloneDX ont ensuite passé ensemble dans le run #45. Le benchmark local de génération ROM, la validation de gameplay et l'essai GUI sous bureau Linux ne sont pas exécutés.
- Aucun build complet avec ROM, benchmark local cache froid/chaud, mesure CPU/I/O locale ou test gameplay n'a été exécuté. L'instrumentation ajoutée donne des durées murales et volumes de fichiers/octets dès qu'un propriétaire lance un build local.


## 12. Lock Conda explicite — validée par CI #54

- Lock: `packaging/linux/conda-linux-64.lock`, exporté du solve effectif du runner Linux x86_64 au run `40730463353`.
- Contenu contrôlé : 96 URLs de paquets, uniquement `linux-64`/`noarch`, chacune avec fragment MD5. SHA-256 attendu du fichier : `5aeec2066f1d9fcfbf729de3ea6531559c8b9b0a31065a610059fb18793bae96`.
- La CI est modifiée pour vérifier SHA-256, installer depuis la lockfile explicite, comparer l'ensemble des URLs installées à la liste versionnée et tester que `environment.yml` correspond au SHA annoncé par les métadonnées.
- Run #54 (`40732085626`, SHA `54afb520bc1a4beceb1aecefd77e5456e1d4350a`) a réussi les deux jobs : test statique du lock, checksum, installation depuis le lock, comparaison des URLs installées et construction/smoke-test AppImage. Le run #45 répète ces contrôles sur un commit ultérieur.


## 13. Rapport de dépendances, taille et SBOM — validé par CI #45

Le nouveau script `tools/packaging/audit_appimage.py` analyse les ELF x86_64 du runtime AppImage et de la toolchain relocalisée, exécute `ldd` avec le chemin de bibliothèques Conda, échoue si une dépendance est introuvable ou si une bibliothèque hôte ne figure pas dans l'allowlist documentée, puis écrit `elf-audit.json`. Il écrit aussi le rapport de taille (AppImage, archive toolchain, AppDir, toolchain relocalisée et fichiers les plus volumineux) et un SBOM CycloneDX 1.5 basé sur les 96 composants Conda de la lockfile.

Le run #45 (`40733305358`, SHA `81144d0ecf163fbdbf0106d8bd996cc258aa682c`) a réussi l'audit : 717 ELF, zéro dépendance introuvable, zéro dépendance runtime hôte non autorisée, 408 ELF runtime et 309 ELF de sysroot link-time uniquement. Le rapport de taille enregistre AppImage 242383352 octets, archive toolchain 249665432 octets, AppDir 263478781 octets et toolchain relocalisée 1251667787 octets ; le SBOM CycloneDX 1.5 contient 96 composants Conda. Rapports et checksums : artefact diagnostics ID `12835458535`, digest ZIP `sha256:7dbe02e267c69dde17c0b45d5d27d8765f5c5ff3f357cde3c4c26163c0399e03`. Un commit de suivi élargit le scan aux ELF directement sous AppDir et ajoute des tests cold/warm, archive corrompue et workspace non destructif ; ces extensions attendent leur propre résultat CI.


## Résultat de l'audit ELF initial — run #44

Le run `40732766439` sur le SHA `695c737e32e08662cd34e8d67212e248ea11f7b1` a construit l'AppImage et passé le self-test, puis a échoué à l'audit ELF. Le rapport de diagnostic a été conservé dans l'artefact `12834998141`. Il indique 717 ELF examinés, zéro dépendance introuvable, mais 15 faux positifs « unexpected host » dus à la résolution incorrecte des bibliothèques du sysroot GCC de link (ex. `libGB.so`, `libJIS.so`) comme s'il s'agissait de bibliothèques runtime hors bundle. `libcrypt-2.17.so` du sysroot a aussi fait remonter `libfreebl3.so` du runner. Le script a été corrigé puis validé dans le run #45 : 717 ELF, zéro dépendance runtime introuvable/inattendue. Les 309 ELF du sysroot sont inventoriés comme link-time-only ; les 408 ELF runtime restent soumis à l'analyse de résolution et à l'allowlist hôte.

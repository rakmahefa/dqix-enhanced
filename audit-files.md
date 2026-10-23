# Audit des fichiers — DQIX Enhanced

Date de l’audit : 2026-10-23  
Branche de référence : main  
Commit analysé : bcba9c2821c2d540e26e617a20152ad7823307d0

## Résumé

Le dépôt est une recompilation native de Dragon Quest IX pour PC. Le code C du runtime et le recompiler sont présents dans le dépôt, mais le code du jeu utilisé pour une compilation complète est extrait d’une ROM fournie séparément. La compilation Linux existe déjà dans tools/build-game.sh ; aucun workflow GitHub Actions Linux/AppImage n’était présent dans l’arborescence examinée.

L’AppImage ajoutée par cette branche est donc un **constructeur Linux portable** : elle embarque les sources publiques et une toolchain Linux versionnée, mais ni ROM, ni code du jeu généré, ni exécutable du jeu précompilé. Le build du jeu s’effectue sur la machine de l’utilisateur à partir de sa propre ROM.

## Arborescence et rôle des composants

| Chemin | Rôle | Considérations d’audit |
|---|---|---|
| README.md | Présentation, installation Windows, fonctionnement, contrôles et limites du projet | Décrit explicitement le modèle « ROM personnelle, build local » et avertit de ne pas partager game/ ou build/. |
| ROADMAP.md | État des fonctions et travaux futurs | Le support Linux/installateur est identifié comme du travail restant dans la version examinée. |
| build.mk | Compilation du code recompilé, runtime et bibliothèque dqixcore | Utilise make, un compilateur C, SDL2 ; compile aussi le code embarqué de third_party/libslirp. |
| tools/build-game.sh | Build principal partagé : extraction ROM, recompilation ARM9/ARM7, édition de liens et assemblage du dossier game | Exige un chemin vers un fichier .nds ; écrit dans build/ et game/ et copie la ROM dans game/data/dqix.nds. |
| tools/extract_rom.py | Extrait les parties de code de la ROM | Écrit les fichiers ARM9, ARM7 et overlays utilisés par le recompiler. |
| recomp/build.py, recomp/arm7.py, recomp/insn.py | Recompilation des instructions ARM en C | recomp/build.py importe PyYAML ; la sortie est dérivée du contenu de la ROM et ne doit pas être publiée comme source ordinaire. |
| runtime/ | Runtime de la console, frontend SDL, rendu, son, contrôles, réseau, sauvegarde et sessions | Le frontend SDL et le rendu OpenGL font partie de la cible Linux. |
| third_party/libslirp/ | Pile réseau en espace utilisateur intégrée au dépôt | Compilée à partir des sources fournies ; ne pas ajouter une dépendance système libslirp inutile. Respecter sa licence BSD. |
| config/usa/arm9/ | Symboles et relocations qui guident la recompilation | Données de référence utilisées par le build ; elles ne remplacent pas la ROM source. |
| skins/ | Images/coordonnées pour les habillages de console | Ressources optionnelles du projet, copiées dans le dossier de jeu par le script de build. |
| wfc/ et extras/ | Ressources optionnelles du serveur de téléchargement | Les fichiers de jeux téléchargés ne doivent pas être ajoutés par inadvertance aux artefacts publics. |
| tests/semantics/run.py | Tests sémantiques du recompiler par comparaison avec du code compilé natif | Test réalisable en CI sans ROM, mais nécessite Clang, lld, les outils LLVM et GCC. |
| tests/gpu3d/t3d.c | Petit banc de test du rendu 3D | Produit une image de test ; ce n’est pas un build complet du jeu. |
| tests/verify_decode.py | Comparaison du décodeur avec LLVM sur des modules extraits | Besoin de données extraites et de config appropriées ; ne peut pas être exécuté sur la ROM absente. |
| BUILD.bat, build-pc.bat, build-windows.sh | Chemin de build/installateur Windows ou MSYS2 | Ne sont pas le point d’entrée Linux. |
| .gitignore | Exclut les ROMs, objets, répertoires de build, fichiers générés et dossiers de jeu | Les workflows doivent conserver ces exclusions et ne pas ajouter leurs sorties au commit. |
| docs/TECHNICAL.md, docs/STATE.md | Architecture et état technique détaillés | Sources utiles pour déterminer les dépendances et le statut des fonctionnalités. |

## Flux de build constaté

1. tools/build-game.sh valide un argument ROM.
2. tools/extract_rom.py écrit les données extraites dans build/extract.
3. recomp/build.py et recomp/arm7.py génèrent le code C dans build/recomp et build/recomp7.
4. build.mk compile ce code, le runtime SDL et third_party/libslirp, puis lie game/dqix et game/dqixcore.so.
5. Le script compose game/data/code depuis l’extraction, copie la ROM vers game/data/dqix.nds et copie les habillages/ressources optionnels. game/play.sh démarre le jeu à partir de ce dossier.

Ce flux est cohérent pour un build local. Il n’est pas exécutable intégralement dans une CI publique sans une ROM fournie de façon privée. En outre, publier le dossier game/ ou un binaire dérivé de la ROM dans une Release publique pourrait redistribuer du contenu protégé. Les workflows ne font donc pas de build de jeu précompilé.

## Dépendances relevées

- Python 3 ; PyYAML est importé par recomp/build.py.
- GNU Make et un compilateur C compatible avec les options du Makefile.
- SDL2, ses en-têtes et sdl2-config pour compiler runtime/frontend_sdl.c.
- Outils de base POSIX/Linux : shell, cp, cmp, dd, tr, nproc et curl (le téléchargement du message DQVC est optionnel).
- Clang, lld/LLVM et GCC pour tests/semantics/run.py.
- Pilotes graphiques OpenGL de la machine hôte ; Enhanced demande OpenGL 3.3 selon la documentation technique.

Le réseau slirp est inclus dans third_party/libslirp. Le projet n’utilise pas CMake pour son chemin de build Linux documenté.

## Écarts et risques

1. Pas de CI Linux/AppImage présente à l’état initial.
2. Les dépendances de compilation Linux n’étaient pas encapsulées par un fichier d’environnement versionné.
3. La compilation réelle du jeu dépend de la ROM ; un job CI sans ROM peut vérifier les tests, le toolchain et le packaging, mais ne peut pas revendiquer avoir lancé le jeu.
4. Les sorties build/, game/, dossiers d’extraction et C généré depuis la ROM sont sensibles au regard de la redistribution et doivent rester hors de l’AppImage générique et des artefacts distribués.
5. OpenGL et les pilotes GPU ne peuvent pas être garantis par l’AppImage seul ; ils continuent à dépendre de la machine hôte.
6. L’AppImage doit être testée sur plusieurs distributions Linux réelles. La création réussie de l’image et un test du toolchain ne remplacent pas un test manuel de gameplay avec une ROM locale.

## Changements associés dans cette branche

- .github/workflows/linux-ci-appimage.yml : contrôles Linux sans ROM, création et test de l’AppImage constructeur, artefact téléchargeable et cache de dépendances.
- .github/workflows/release-appimage.yml : publication manuelle d’un artefact issu d’une exécution CI réussie et de confiance.
- packaging/linux/environment.yml : versions contraintes pour le compilateur, Python, PyYAML, Make, SDL2 et utilitaires.
- packaging/linux/AppRun : extraction locale de la toolchain, compilation à partir d’une ROM fournie par l’utilisateur et lancement du build local.
- packaging/linux/dqix-enhanced-builder.desktop et .svg : métadonnées et icône générique de l’AppImage.
- appimage-audit.md : décisions de packaging, dépendances et limites de compatibilité.

## Vérifications après fusion

- Vérifier que le workflow CI finit avec un artefact contenant l’AppImage et SHA256SUMS.
- Télécharger l’artefact sur une distribution Linux propre et lancer --self-test.
- Tester le premier build avec une ROM dumpée légalement, puis le lancement d’une seconde fois, sauvegardes et contrôles.
- Tester l’affichage et le son sur au moins une distribution basée sur Ubuntu et une distribution différente.
- Ne jamais ajouter une ROM, game/data/code, les fichiers générés par recomp ou un exécutable déjà compilé au dépôt ou à l’artefact générique.

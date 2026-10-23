# Audit AppImage Linux — DQIX Enhanced

Date : 2026-10-23  
Cible initiale : Linux x86_64

## Décision de packaging

Le projet ne contient pas de ROM redistribuable et tools/build-game.sh attend un fichier .nds local. La compilation produit du code généré à partir de cette ROM, copie celle-ci dans game/data/dqix.nds et écrit le jeu complet dans game/. L’AppImage publiée par ce dépôt est donc un **constructeur Linux portable**, pas un binaire précompilé du jeu.

Elle contient les sources du projet nécessaires au build et une toolchain packagée. Elle ne contient aucun fichier .nds, aucun dossier build/ ou game/, aucune extraction de ROM, aucun code C généré depuis le jeu ni aucun exécutable de jeu. Au premier lancement, l’utilisateur indique le chemin de sa propre ROM ; l’extraction, la recompilation et le build se déroulent localement dans son espace utilisateur.

Cette séparation empêche le workflow de publier involontairement la ROM ou un binaire de jeu dérivé. Le contenu construit localement reste sur la machine de l’utilisateur.

## Dépendances du build Linux

| Dépendance | Usage | Traitement de packaging |
|---|---|---|
| Python 3.11 | Scripts d’extraction et recompiler | Embarqué dans l’environnement Conda packagé. |
| PyYAML | Importé par recomp/build.py | Version contrainte et embarquée. |
| GNU Make | Orchestration de build.mk | Embarqué dans l’environnement packagé. |
| GCC Linux x86_64 | Compile le C généré et runtime | Wrapper de compilateur, runtime et dépendances d’outilchain embarqués. |
| sysroot Linux | En-têtes et bibliothèques de compilation ciblés | Fournis par l’environnement de build plutôt que par la version locale de GCC. |
| SDL2 + headers et sdl2-config | Fenêtre, entrées, son et frontend SDL | Version contrainte et bibliothèque embarquée. |
| pkg-config | Résolution des métadonnées des bibliothèques | Embarqué. |
| curl | Téléchargement optionnel du message DQVC pendant le build | Embarqué ; l’échec de ce téléchargement est déjà non fatal dans tools/build-game.sh. |
| third_party/libslirp | Pile réseau | Compilée depuis les sources déjà présentes dans le dépôt, pas liée à une libslirp système. |
| Clang, lld et LLVM | Tests de sémantique du recompiler | Installés dans le job CI ; non nécessaires au build normal de l’utilisateur et donc non ajoutés à l’AppImage runtime. |
| OpenGL / pilote GPU | Mode Enhanced et rendu matériel | Le pilote noyau et le pilote GPU de la machine restent ceux de l’hôte. L’AppImage ne peut pas garantir ce composant matériel. |

Le fichier packaging/linux/environment.yml contraint les versions directes. setup-micromamba met l’environnement en cache selon le fichier et les paramètres de création. Lors de la construction, conda-pack fige l’environnement effectivement résolu dans toolchain.tar.gz ; AppRun le décompresse une fois dans le cache utilisateur et applique conda-unpack pour relocaliser les chemins.

Le cache accélère les constructions suivantes sans partager la toolchain de l’utilisateur entre versions différentes de l’archive : son SHA-256 sert d’identifiant. Cette approche fige l’environnement livré dans chaque artefact, mais ne garantit pas à elle seule que deux résolutions Conda effectuées à des dates différentes produiront une archive bit-à-bit identique. Une reproduction exacte demanderait de versionner aussi un lockfile complet des dépendances transitives.

## Création de l’AppImage

1. La CI s’exécute sur Ubuntu 22.04 x86_64, pour garder une base de construction fixe.
2. Les tests du recompiler et les contrôles de syntaxe sont exécutés sans ROM.
3. La toolchain Conda est packagée dans une archive relocalisable.
4. Un AppDir est créé avec AppRun, un fichier desktop, une icône neutre, les sources de build et l’archive de toolchain.
5. appimagetool construit l’AppImage. La version de cet outil et son SHA-256 sont épinglés dans le workflow.
6. L’AppImage est lancée avec --self-test en mode extraction sans FUSE, puis SHA256SUMS et un fichier de provenance sont ajoutés à l’artefact.
7. Le deuxième workflow peut publier manuellement un artefact provenant d’une exécution CI réussie sur main ou stable.

## Utilisation

Premier build, depuis un terminal :

    chmod +x DQIX-Enhanced-Builder-x86_64.AppImage
    ./DQIX-Enhanced-Builder-x86_64.AppImage /chemin/absolu/vers/votre-ROM.nds

Le builder stocke son espace de travail sous ~/.local/share/dqix-enhanced-linux/workspace. Cet espace contient la ROM copiée, le code extrait, le binaire et les sauvegardes : ne pas le publier ni le partager.

Options prévues :

- --help : affiche l’aide.
- --version : affiche la version de source incluse.
- --self-test : contrôle la toolchain embarquée sans exiger de ROM.
- --build-only [ROM] : réalise le build mais ne lance pas le jeu.
- --rebuild [ROM] : reconstruit depuis la ROM indiquée, ou depuis la copie locale existante si aucun chemin n’est fourni.

Sans option, l’AppImage lance le jeu local déjà construit. Au premier lancement, elle demande le chemin de ROM si une session terminal est disponible.

## Compatibilité et limites

- Architecture livrée : x86_64 uniquement. AArch64 nécessite un job, une toolchain et un artefact propres à cette architecture.
- Base de construction : Ubuntu 22.04. Les versions de bibliothèques user-space du build sont embarquées, mais glibc, noyau, serveur de fenêtre et pilote GPU ne peuvent pas être rendus indépendants de toutes les distributions avec un AppImage classique. La compatibilité avec des distributions plus anciennes doit être testée explicitement.
- Enhanced requiert OpenGL 3.3 et un pilote compatible selon docs/TECHNICAL.md. Le rendu ou les pilotes ne doivent pas être « copiés en aveugle » dans l’AppImage.
- FUSE peut être absent sur la machine cible ; le format AppImage récent peut généralement être lancé via son mode d’extraction, sinon l’utilisateur peut extraire le contenu manuellement.
- Le premier build a besoin d’espace disque et peut prendre plusieurs minutes ; il doit disposer des bibliothèques d’exécution nécessaires au frontend SDL.
- Le job CI ne possède pas de ROM. Il valide les tests, le packaging AppImage et la toolchain ; il ne valide pas le lancement effectif du jeu. Le smoke-test gameplay doit être fait localement avec une ROM personnelle.
- L’AppImage constructeur ne copie pas la ROM dans le dépôt et ne publie pas game/ ni build/. Cette règle doit rester vraie pour toute modification ultérieure du workflow.

## Contrôles avant publication

- Vérifier le succès du job de test et du smoke-test --self-test.
- Vérifier SHA256SUMS après téléchargement.
- Lancer le premier build et le lancement du jeu sur une distribution Linux propre.
- Vérifier SDL2, les périphériques audio, contrôleurs, fenêtres, rendu 2D et mode Enhanced/OpenGL.
- Vérifier que l’archive AppImage ne contient aucun .nds, build/, game/, ROM extrait ni fichier C généré depuis une ROM.
- Publier uniquement l’artefact constructeur provenant d’une exécution de confiance ; ne jamais remplacer cet artefact par un dossier de jeu construit à partir d’une ROM privée.

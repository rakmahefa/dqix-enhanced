# Plan-spec — Linux v32 feature parity, launcher et menu in-game

**Statut :** analyse et spécification ; aucune implémentation fonctionnelle dans ce commit.  
**Base de travail :** branche `main`, commit `ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a` (après la fusion du correctif de liaison Linux et de l'interface AppImage en anglais).  
**Cible primaire :** Linux x86_64, build local et AppImage. Windows doit conserver sa barre de menus et ses comportements actuels.

## 1. Résumé exécutif

Le symptôme rapporté — F5 ne change pas le mode, les paramètres semblent revenir en arrière, et aucune interface de configuration n'apparaît sous Linux — s'explique en partie par des différences d'architecture vérifiées dans le code :

1. **Le menu de configuration du jeu est Windows-only.** Le contrat de `runtime/menu.h` décrit une barre de menu Windows ; `runtime/menu_win32.c` repose sur les API Win32. Le frontend SDL Linux n'a pas de menu visuel équivalent. Les nombreuses options exposées par ce menu sont donc présentes dans le runtime mais difficiles, voire impossibles, à atteindre depuis Linux.
2. **F5 ne fait rien lorsque le renderer OpenGL est indisponible.** `runtime/controls.c` associe bien F5 à `CA_MODE`, et `runtime/frontend_sdl.c` traite les événements clavier SDL. Toutefois, `hotkey(CA_MODE)` ne change de mode que si `g_gl` est vrai. Le frontend exige un contexte OpenGL 3.3 ; en cas d'échec, il retombe sur le renderer logiciel. `apply_mode()` force alors le mode Classic. La sauvegarde de fin de processus peut ensuite réécrire `mode = classic` dans `settings.ini`, même si l'utilisateur avait demandé Enhanced.
3. **Le diagnostic qui expliquerait F5 est masqué par le launcher actuel.** `packaging/linux/gui.py` lance `game/play.sh` en redirigeant stdout/stderr vers `DEVNULL`. Le message « OpenGL 3.3 not available » ou l'échec d'initialisation du renderer risque donc d'être invisible pour l'utilisateur.
4. **Le chemin de configuration est relatif au répertoire courant.** `runtime/frontend_sdl.c` ouvre `settings.ini` avec `fopen("settings.ini", ...)`. Le script généré `game/play.sh` fait un `cd` dans le dossier du jeu, mais un lancement direct depuis un autre répertoire peut lire ou écrire un autre fichier. De plus, une édition manuelle faite pendant que le jeu tourne sera écrasée à la fermeture par la sauvegarde de l'état conservé en mémoire.

Ces constatations rendent le comportement observé plausible, mais **elles ne prouvent pas encore que le GPU ou le pilote de la machine de test est la cause effective**. La première phase doit rendre le renderer, le chemin de configuration et les erreurs visibles, puis reproduire le problème sur l'environnement concerné.

## 2. Éléments du dépôt vérifiés

| Zone | Observation | Conséquence |
|---|---|---|
| `runtime/controls.c` | F5 est la touche par défaut de `CA_MODE` ; F1 est associé à HD text. | Le mapping de base existe ; il faut tester l'événement jusqu'à son effet, pas seulement vérifier le mapping. |
| `runtime/frontend_sdl.c` — `handle_events()` / `hotkey()` | Les événements clavier passent par `controls_key_action()`. F5 n'agit que si `g_gl` est vrai. | Si OpenGL échoue, l'absence de réaction à F5 est un comportement codé, actuellement silencieux. |
| `runtime/frontend_sdl.c` — `try_gl()` / `apply_mode()` | Le contexte demandé est OpenGL 3.3 core ; l'échec déclenche un fallback logiciel et force Classic. | L'interface doit annoncer les capacités effectives. Le mode demandé ne doit pas être effacé simplement parce que l'environnement ne peut pas l'appliquer. |
| `runtime/menu.h` / `runtime/menu_win32.c` | La barre native, les dialogues de contrôles et les entrées du menu reposent sur le frontend Windows. | Linux a besoin d'une véritable UI de remplacement, pas seulement d'une traduction de la barre Win32. |
| `runtime/frontend_sdl.c` — `settings_load()` / `settings_save()` | Le parser sait déjà lire de nombreuses clés v32, dont mode, render scale, outlines, soft shadows, HD text, texture packs, music packs, language, DLC et wireless. Le fichier est néanmoins ouvert relativement au cwd et est réécrit depuis l'état en mémoire. | Il faut auditer le cycle lecture → validation → application → sauvegarde et afficher le chemin exact ; ne pas réécrire tout le fichier sur une préférence impossible à activer. |
| `tools/build-game.sh` | `game/play.sh` démarre le binaire depuis le dossier game. | Le chemin actuel paraît cohérent lorsqu'on utilise ce script, mais il n'est pas garanti en lancement direct ou externe. |
| `packaging/linux/gui.py` | Fenêtre de build : ROM, workspace, progression, journal, bouton de lancement. La sortie du jeu lancé est supprimée. | Elle n'offre pas le launcher de configuration du jeu et masque un diagnostic runtime crucial. |
| `release-notes/v32.md` | v32 comprend les langues européennes, mods texture/musique, HD text, Basic/Dynamic Widescreen, battle HUD, options graphiques, sauvegardes, wireless et autres fonctions. | La validation doit couvrir toutes les catégories et distinguer les options indisponibles à cause du renderer des options seulement inaccessibles dans l'UI Linux. |

## 3. Lecture des fonctionnalités v32 côté Linux

Cette matrice distingue l'existence du code de la disponibilité réelle de l'option pour l'utilisateur Linux. Le fait qu'une clé soit chargée depuis settings.ini ne suffit pas : le renderer doit pouvoir l'appliquer et l'UI doit permettre de la modifier.

| Catégorie v32 | Options à vérifier | Dépendances / risque repéré | Résultat attendu |
|---|---|---|---|
| Mode et renderer | Classic / Enhanced, échelle 3D 1x–12x ou Auto | Enhanced et le changement F5 sont conditionnés à OpenGL 3.3. Le fallback force Classic. | État réel du renderer visible ; Enhanced activable et persistant si le contexte est disponible ; sinon message explicite, préférence non détruite. |
| Qualité graphique | textures lissées, outlines Thin/Medium/DS, soft shadows/glows, smooth shading | Principalement liées au renderer GL et au mode Enhanced ; actuellement accessibles via le menu Windows/settings.ini plutôt que depuis Linux. | Contrôle disponible, valeur courante affichée, effet vérifiable ou raison de désactivation indiquée. |
| HD text | HD text, ombres/contours du texte | F1 existe, mais l'action est conditionnée au renderer GL. | F1 et l'UI ont le même comportement ; statut clair si le renderer ne peut pas l'appliquer. |
| Présentation des écrans | stacked, side-by-side, corner, Basic Widescreen, Dynamic Widescreen, two windows, fenêtre séparée, battle HUD | F2 existe, mais les layouts larges sont conditionnés à GL. Un cycle de raccourci ne remplace pas les contrôles explicites. | Choix explicites et sélection visible dans le launcher et l'overlay ; état sauvegardé. |
| Petit écran et taille | position, taille, opacité, écran unique, swap, fenêtre 1x–8x, integer scaling, actual size, fullscreen | Plusieurs raccourcis existent ; certaines variantes sont inaccessibles sans menu ou conditionnées à GL. | Contrôles rangés dans Screen & Display, prévisualisation/feedback et valeurs persistantes. |
| Filtre DS Lite / console | filtre LCD, couleurs, grille de pixels, motion blur, backlight, coque/skin, espace entre écrans | La majorité de ces actions est dans le menu Windows ; le dessin de la coque dépend de GL. | Contrôles organisés dans Graphics/Display, capacités signalées et skins validés. |
| Mods texture | texture packs, xBR upscale, dump, reload, comparaison avec textures originales, dossier textures | Les actions principales sont des entrées du menu Mods ; texture packs/upscale requièrent le pipeline GL et un contenu compatible dans textures/. | Contrôles visibles, chemins d'assets explicites, compteur de textures chargées et messages si le dossier est vide. |
| Mods musique | OGG/FLAC/WAV, reload, noms de chansons, volume des packs, export WAV | Pas de raccourci général ; options principalement derrière le menu Mods. Les effets dépendent de la présence de fichiers dans music/. | Contrôles visibles, détection des assets, export lancé avec progression/erreur observable. |
| Langue et DLC | langue EUR English/French/German/Italian/Spanish, contenu téléchargeable | La langue n'a de sens que pour une ROM européenne ; la langue et le DLC sont appliqués au démarrage ou au chargement de sauvegarde. | Sélecteur de langue tenant compte de la région ; avertissement « prochain démarrage » là où nécessaire. |
| Contrôles | remappage clavier, deuxième touche, gamepad, contrôles DS | L'éditeur `Game > Controls` est Windows-only bien que la logique des mappings soit partagée. | Page Controls pour clavier et manette, capture d'une touche/bouton, conflit résolu par échange, Esc annule la capture. |
| Sauvegarde et session | export/import save, wireless/server, host/join, nom/port, DLC | La plupart des commandes se trouvent dans Game/Wireless/Save file ; dialogues Windows spécifiques pour plusieurs actions. | Page dédiée, dialogues Linux équivalents, validation de chemin et erreurs visibles. |
| Utilitaires | volume/mute, pause, fast-forward, screenshot, bug report, update, Discord presence | Certains raccourcis sont disponibles ; dialogues de fichier, reports et options globales sont majoritairement dans le menu Windows. | Entrées visibles au bon endroit ; aucun échec silencieux ; options indisponibles désactivées avec explication. |

**Règle de validation :** ne pas déclarer toutes les fonctions « cassées » avant d'avoir mesuré la capacité OpenGL. Pour chaque ligne, consigner séparément (1) valeur chargée, (2) capacité requise, (3) action demandée, (4) effet observé, (5) valeur sauvegardée.

## 4. Inspirer le launcher de Let's Go Recomp

La structure vérifiée de Let's Go Recomp est utile : launcher avec une barre latérale et des pages Game, Graphics, Image and sound, Controls et Mods, bouton Play, puis les mêmes réglages accessibles depuis le jeu (F12). Les choix sont centralisés dans le système de settings au lieu de dépendre exclusivement d'une barre de menus système.

Nous reprendrons **la structure, le modèle de navigation et le principe d'un même modèle de settings**, pas le backend graphique tel quel. Le launcher de référence utilise SDL3 + Dear ImGui + Vulkan, alors que DQIX utilise SDL2, un renderer OpenGL 3.3 et un fallback SDL_Renderer logiciel. Un port direct de ses fichiers UI ne serait donc pas compatible.

### Architecture proposée

- Ajouter un **modèle de configuration commun et typé** pour le runtime : chargement, validation, capacités applicables, valeur courante, sauvegarde, état modifié et indication « nécessite redémarrage ».
- Ajouter une UI native de jeu inspirée du launcher Let's Go, basée sur Dear ImGui avec les backends SDL2. Pour rester fonctionnelle dans les deux modes du renderer, prévoir OpenGL3 quand le contexte GL existe et SDLRenderer2 dans le fallback logiciel. Cette décision implique un petit pont C/C++, un linker C++ explicite et une dépendance ImGui épinglée/vendored avec sa licence.
- Réutiliser le même composant/pages UI dans l'écran de démarrage du jeu et le menu in-game. Ne pas dupliquer deux implémentations de chaque contrôle.
- Conserver pour l'instant la fenêtre Tk de l'AppImage pour choisir la ROM, sélectionner le workspace et construire le jeu. Le bouton Launch game démarre ensuite le nouveau launcher natif, qui donne accès aux réglages avant Play. Le remplacement éventuel de la fenêtre de build est hors du premier jalon.
- Garder les options de démarrage et les raccourcis existants ; ne pas supprimer la barre Win32. Le menu natif Windows reste un accès compatible à la même couche de réglages.
- Utiliser le fichier de configuration à côté du jeu comme emplacement historique de référence, mais le résoudre par le répertoire de l'exécutable (pas par le cwd). Prévoir une variable d'environnement de test/override explicite et une migration lisible depuis le chemin historique si nécessaire.
- Aucune option ne doit être « active » seulement parce qu'elle a une case cochée : l'UI doit distinguer **préférence demandée**, **valeur effective** et **raison de non-applicabilité**.

### Navigation visuelle

**Launcher avant Play**
- Sidebar : Game, Graphics, Screen & Display, Audio & Music, Controls, Mods, Wireless & Online, System.
- Page Game : ROM/région détectée, version runtime, sauvegarde détectée, renderer disponible, état du build. Le bouton Play lance le jeu ; si la ROM/build manque, afficher pourquoi et proposer de revenir au constructeur AppImage.
- Bouton Play fixe, statut du jeu et accès à l'emplacement des données utilisateur.

**Menu in-game**
- Touche **Escape** : ouvre/ferme le menu Settings, au-dessus du rendu de jeu.
- À l'ouverture, suspendre le tick de jeu et consommer les touches destinées à l'UI pour éviter qu'elles deviennent des commandes DS.
- Escape ferme le panneau ; Resume reprend explicitement le jeu ; Quit est une action séparée avec confirmation. En fullscreen, Escape ne doit pas seulement enlever le fullscreen et ne doit pas fermer le jeu sans distinction.
- Souris, clavier et navigation manette. Focus visible, raccourcis affichés à côté des commandes, retour clavier cohérent.
- Les options s'appliquent immédiatement lorsqu'elles sont sûres. La langue, le DLC ou les paramètres qui nécessitent une réinitialisation doivent afficher « applied next launch/restart » plutôt que donner l'impression d'une application immédiate.

## 5. Spécification fonctionnelle

### 5.1 Détection des capacités et diagnostic

Au démarrage, déterminer et conserver :
- renderer effectif : OpenGL ou logiciel SDL ;
- version/identifiant OpenGL quand disponible ;
- erreur d'initialisation du contexte ou du renderer, si elle existe ;
- mode demandé dans settings.ini et mode effectif ;
- chemin absolu de settings.ini et controls.ini ;
- résultat de lecture/sauvegarde du fichier.

L'information doit être visible dans le launcher et dans System/Troubleshooting, et écrite dans un log local. Le démarrage depuis le launcher ne doit plus jeter stdout/stderr sans remplacement par un log accessible.

**Ne pas forcer la préférence persistée à Classic si OpenGL échoue.** L'application peut exécuter Classic comme mode effectif, mais doit conserver la préférence Enhanced demandée, indiquer que la capacité requise manque et proposer comment diagnostiquer OpenGL. F5 dans cet état affiche une notification expliquant la limitation au lieu d'être silencieux.

### 5.2 Contrat des paramètres

- Résoudre le chemin depuis l'emplacement du binaire du jeu (SDL_GetBasePath ou wrapper commun), avec override explicite pour les tests. Le launcher doit afficher « Settings file: <path> » et proposer d'ouvrir le dossier.
- La lecture est faite une fois à l'initialisation et retourne un résultat exploitable : absent, chargé, partiellement invalide ou erreur d'accès. Les clés/valeurs hors plage sont rejetées avec diagnostic et fallback par défaut.
- La sauvegarde ne s'effectue qu'à partir du modèle validé. Écriture dans un fichier temporaire puis remplacement atomique, vérification des erreurs et aucune troncature silencieuse du fichier.
- Éviter la réécriture inutile au premier lancement uniquement pour créer la MAC Wi-Fi. Générer l'identité puis enregistrer sans perdre les valeurs reconnues ni modifier les valeurs non applicables.
- Ne pas lire/sauvegarder settings.ini dans deux cwd différents. Garder l'ancien emplacement compatible en migrant une fois seulement si le nouveau fichier n'existe pas ; jamais écraser un fichier utilisateur sans sauvegarde/diagnostic.
- Les contrôles remappés ont leur fichier dédié, actuellement controls.ini. Résoudre et diagnostiquer ce chemin de la même façon.
- Enregistrer la liste des options supportées par la version runtime pour que le test de parité détecte une option présente dans le menu Windows mais absente de l'UI Linux.

### 5.3 Sémantique de F5, Escape et actions

- F5 appelle l'action Classic/Enhanced depuis le même modèle de paramètres utilisé par le launcher et le menu in-game.
- Si OpenGL est actif : appliquer le nouveau mode, rafraîchir le statut/checkbox UI, sauvegarder et montrer une confirmation.
- Si OpenGL est indisponible : afficher une explication et garder intacte la préférence demandée ; aucun faux succès.
- Escape ouvre l'overlay depuis le jeu ; quand il est déjà ouvert, Escape ferme l'overlay. En capture de touche dans Controls, Escape annule la capture sans fermer tout le menu.
- Les contrôles de l'UI ne doivent pas atteindre le jeu pendant que l'overlay a le focus.
- Toutes les actions qui changent un réglage doivent avoir un résultat observable et une règle uniforme d'application/sauvegarde. Les actions longues (export de chansons, dump de textures, rapports) doivent signaler leur progression ou leur résultat.

## 6. Plan d'exécution par phases

### P0 — Reproduire et rendre le problème observable

1. Lancer l'AppImage et le binaire Linux dans un cwd temporaire, puis depuis le script play.sh ; consigner le renderer effectif et le chemin des fichiers ini.
2. Ajouter un diagnostic temporaire ou permanent qui expose le résultat de try_gl()/gl_render_init() sans dépendre d'un terminal.
3. Reproduire F5 sur un contexte OpenGL 3.3 et sur le fallback logiciel. Confirmer que le mode est bloqué uniquement dans le cas attendu.
4. Vérifier si le fichier édité par l'utilisateur est celui effectivement lu ; comparer le fichier avant démarrage, après lecture et après fermeture.
5. Capturer un rapport de test (renderer, versions SDL/OpenGL, chemin de settings, logs) sans joindre la ROM ni le fichier de sauvegarde.

**Sortie P0 :** causes observées sur une machine réelle, et non uniquement déduites du code.

### P1 — Fiabiliser configuration et renderer

1. Introduire une couche de paramètres claire et des tests round-trip couvrant toutes les clés actuellement sauvegardées.
2. Stabiliser le chemin des réglages indépendamment du cwd ; afficher les erreurs de permission, de parse et d'écriture.
3. Séparer le mode demandé du mode réellement applicable ; empêcher la sauvegarde de remplacer la préférence de l'utilisateur lorsque le renderer est limité.
4. Publier un statut de capacités cohérent pour OpenGL, Enhanced, widescreen, HD text et mods texture.
5. Garder un log runtime sous le dossier utilisateur du workspace, en plus des sorties de diagnostic accessibles depuis le launcher.

**Sortie P1 :** plus de reset silencieux ; F5 a un comportement documenté et testable dans les deux types de renderer.

### P2 — Construire le launcher natif

1. Ajouter Dear ImGui et les backends SDL2 nécessaires avec une révision épinglée et la licence incluse. Ajouter le compilateur C++ à la toolchain Conda verrouillée si nécessaire, puis régénérer le lockfile et son SHA-256.
2. Isoler le pont C/C++ et les appels d'initialisation/rendu/destruction afin que le runtime C garde la responsabilité du jeu.
3. Construire les pages et la navigation sidebar de l'écran pré-Play, avec statut de renderer/capacités.
4. Charger les réglages, laisser l'utilisateur configurer, puis appliquer les paramètres avant firmware_init/boot. Revoir l'ordre d'initialisation pour que langue, DLC, wireless et volume prennent effet au bon moment.
5. Maintenir la fenêtre de build Tk dans l'AppImage pour ROM/workspace/build ; après Launch game, afficher le launcher natif.

**Sortie P2 :** écran de démarrage visible et utilisable sous Linux, sans nécessiter un fichier ini édité à la main.

### P3 — Menu in-game Escape et parité des options

1. Réutiliser les pages de réglages de l'écran pré-Play dans l'overlay in-game.
2. Ajouter ouverture/fermeture par Escape, pause du jeu pendant l'édition, Resume et Quit distincts.
3. Réimplémenter les actions de la barre de menu Win32 dans les pages Linux. Chaque valeur doit être lisible, modifiable, reflétée dans le menu et persistée.
4. Ajouter les états activé/désactivé avec une explication de la dépendance : Enhanced/OpenGL, ROM EUR, assets présents, manette disponible, service online, etc.
5. Couvrir au minimum toutes les familles de la matrice de la section 3. Aucun item de v32 accessible seulement via un menu Win32 ne doit rester sans alternative Linux ou déclaration explicite de non-support.

**Sortie P3 :** toutes les options v32 sont découvrables, et les limites de capacité sont affichées au lieu de produire des no-op.

### P4 — Tests, documentation et AppImage

1. Ajouter des tests unitaires de lecture/écriture des paramètres, plage invalide, fichier absent, fichier non inscriptible, migration et changement de cwd.
2. Ajouter un test de dispatch clavier vérifiant F5 et Escape avec des événements SDL synthétiques ; vérifier le remapping Controls.
3. Construire un test UI sans ROM qui démarre SDL, dessine le launcher, change de page et prend une capture via une variable de test déterministe.
4. Sur CI Linux, ajouter Xvfb + Mesa/llvmpipe pour tester le chemin OpenGL 3.3 logiciel lorsque disponible ; vérifier séparément le fallback SDL_Renderer. Un test qui n'a pas créé de contexte GL ne doit pas être présenté comme une validation Enhanced.
5. Vérifier la compilation C et C++ avec la même toolchain verrouillée, l'édition de liens Linux du jeu et de dqixcore.so, le lancement AppImage, la résolution des dépendances dynamiques, le cache froid/chaud et l'absence de ROM/jeu compilé dans l'artefact.
6. Mettre à jour docs/LINUX-APPIMAGE.md et le README avec les touches, l'emplacement des fichiers ini/logs, les prérequis OpenGL 3.3 et le dépannage.
7. Tester manuellement avec ROM personnelle sur Linux : premier démarrage, options graphiques, langues EUR, packs, sauvegarde, wireless, fermeture et redémarrage avec paramètres persistants.

## 7. Critères d'acceptation

- [ ] F5 commute Classic/Enhanced sur Linux quand OpenGL 3.3 est initialisé et l'effet visuel est confirmé.
- [ ] Sans OpenGL 3.3, l'UI affiche la cause, le jeu reste utilisable en Classic et la préférence Enhanced n'est pas écrasée silencieusement.
- [ ] Escape ouvre puis ferme le menu in-game ; Resume reprend la partie ; Quit est séparé et confirmé.
- [ ] Les mêmes catégories essentielles existent dans le launcher et le menu in-game ; elles lisent/écrivent le même modèle.
- [ ] Les paramètres édités dans le fichier affiché par l'UI sont bien ceux qui sont chargés, quel que soit le cwd.
- [ ] settings.ini et controls.ini ne sont plus écrasés par des valeurs par défaut uniquement parce que le jeu a été lancé via un autre chemin ou que le renderer ne peut pas appliquer une préférence.
- [ ] Les options à redémarrage requis indiquent ce besoin, et les options appliquées à chaud affichent le résultat.
- [ ] Toutes les commandes du menu Windows sont mappées vers un contrôle Linux ou explicitement marquées non applicables avec raison.
- [ ] Les contrôles clavier/manette, capture de touche et navigation d'UI n'envoient pas accidentellement des boutons au jeu pendant le menu.
- [ ] CI vérifie le parser, les mappings, la compilation/link, le rendu UI sans ROM et les chemins OpenGL/fallback lorsqu'ils sont disponibles.
- [ ] Le build ne dépend pas d'une ROM ou d'une sauvegarde dans CI, et aucune donnée de jeu n'est ajoutée à l'AppImage.
- [ ] Le menu Win32 continue à fonctionner et le build Windows ne récupère pas de dépendance Linux.

## 8. Hors périmètre du premier cycle

- Réécrire le moteur graphique ou changer le comportement du gameplay pour contourner un manque de capacités OpenGL.
- Promettre que les options Enhanced fonctionnent sur une machine sans OpenGL 3.3.
- Ajouter une ROM, une sauvegarde ou des assets de jeu aux artefacts CI/AppImage.
- Remplacer le mécanisme de build ROM/workspace de l'AppImage avant que la couche de réglages et le lancement soient stabilisés.

## 9. Références de code

- Runtime SDL, chargement/sauvegarde des paramètres, détection OpenGL, hotkeys et dispatch des actions : runtime/frontend_sdl.c.
- Mapping clavier/manette, F5/F1 et actions personnalisables : runtime/controls.c et runtime/controls.h.
- Menu Windows-only et IDs de commandes réutilisables : runtime/menu_win32.c et runtime/menu.h.
- Script de lancement du jeu depuis son dossier : tools/build-game.sh.
- Builder AppImage et redirection silencieuse stdout/stderr du processus de jeu : packaging/linux/gui.py.
- Fonctionnalités annoncées de v32 : release-notes/v32.md.
- Modèle de launcher utilisé comme inspiration (pages et bouton Play, UI au lancement et menu in-game partagé) : https://github.com/rakmahefa/letsgo-recomp/blob/main/runtime/ui/launcher_lg.mm et https://github.com/rakmahefa/letsgo-recomp/blob/main/runtime/ui/settings_ui.mm.

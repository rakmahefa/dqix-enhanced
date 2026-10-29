# Plan-spec — Linux v32, launcher et menu in-game SDL3/Vulkan

**Statut :** spécification en cours ; un probe technique autonome SDL3/Vulkan/ImGui et un modèle de paramètres Linux isolé sont ajoutés dans `third_party/extra/linux/`. Leur validation CI, le launcher et l'intégration au runtime restent non confirmés.  
**Base de travail :** branche `main`, commit `ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a`.  
**Cible primaire :** Linux x86_64, build local et AppImage.  
**Décision d'architecture :** SDL3 + Dear ImGui + Vulkan pour les nouvelles interfaces Linux.  
**Branche de planification :** `plan/v32-linux-launcher-settings`.

## 0. Règle primordiale : préserver intégralement les sources Windows

**Aucun fichier source Windows existant ne doit être modifié.** La compatibilité avec l'upstream Windows est une contrainte obligatoire, pas une préférence.

- Ne pas modifier `runtime/menu_win32.c`, les ressources et dialogues Windows, les fichiers spécifiques à Windows, ni aucune source ou configuration de build utilisée par la cible Windows.
- Tout nouveau fichier source Linux (C, C++, headers, shaders, adaptateurs, glue code et tests spécifiques à cette UI) doit être placé sous **`third_party/extra/linux/`**. Garder une arborescence claire : `ui/`, `runtime/`, `bridge/`, `vulkan/`, `tests/` et `vendor/` si nécessaire.
- Les dépendances Linux ajoutées, leurs patches, licences, shaders générés et fichiers d'intégration propres à ces composants doivent également rester sous `third_party/extra/linux/` autant que possible. Ne pas modifier une copie partagée d'une dépendance pour résoudre un problème Linux.
- L'intégration doit se faire par des fichiers de build/packaging/CI Linux dédiés et par sélection de cible, sans altérer le build Windows. Ne pas changer la sémantique d'un fichier partagé pour satisfaire Linux si une adaptation Linux isolée est possible.
- Si l'accès aux fonctions du moteur nécessite une modification d'un fichier source utilisé par Windows, **ne pas effectuer cette modification** : rechercher une API existante, un point d'extension ou un adaptateur Linux isolé. Si aucun raccordement propre n'existe, documenter le blocage et proposer une séparation minimale avant de poursuivre.
- Toute PR doit inclure une vérification du diff prouvant que les fichiers sources/configurations Windows sont inchangés et que les nouveaux sources Linux sont confinés à `third_party/extra/linux/`.

**Critère de rejet :** une solution qui rend le port Linux plus simple en modifiant les sources Windows n'est pas acceptable, même si elle fonctionne localement.

## 1. Résumé exécutif

Les symptômes rapportés — F5 ne change pas le mode, les paramètres semblent revenir en arrière, et aucune interface de configuration n'apparaît sous Linux — correspondent à plusieurs différences d'architecture observées dans le dépôt :

1. **Le menu du jeu est Windows-only.** `runtime/menu.h` et `runtime/menu_win32.c` décrivent et implémentent un menu natif fondé sur Win32. Linux n'a pas d'équivalent visuel complet ; plusieurs options v32 sont donc inaccessibles sans éditer le fichier de paramètres.
2. **F5 peut ne rien faire si le renderer OpenGL est indisponible.** `runtime/controls.c` mappe F5 vers `CA_MODE`, mais le chemin existant n'effectue le changement que si `g_gl` est vrai. Le frontend demande OpenGL 3.3 ; en cas d'échec, il passe au rendu logiciel et `apply_mode()` force Classic. La sauvegarde de fin de processus peut alors écrire Classic même si Enhanced avait été demandé.
3. **Les diagnostics sont masqués par le launcher de build.** `packaging/linux/gui.py` démarre le jeu en redirigeant stdout/stderr vers `DEVNULL`, ce qui peut cacher la cause de l'échec OpenGL.
4. **Le fichier de configuration dépend du répertoire courant.** `runtime/frontend_sdl.c` ouvre `settings.ini` par chemin relatif. Une édition faite pendant l'exécution peut aussi être écrasée à la fermeture par l'état en mémoire.

Ces observations rendent le problème plausible, mais elles ne prouvent pas encore la cause sur la machine de test. La première phase doit rendre les capacités, les fichiers utilisés et les erreurs visibles.

Le choix SDL3 + Dear ImGui + Vulkan vise la **nouvelle interface Linux** : launcher de configuration et UI de réglages. **Il ne transforme pas automatiquement le renderer historique du jeu en renderer Vulkan.** Le code actuel de jeu contient un chemin SDL2/OpenGL et un fallback logiciel. Le plan doit donc commencer par vérifier comment une UI SDL3/Vulkan peut être raccordée au moteur existant sans toucher aux sources Windows. Aucun port global du moteur ne doit être présumé faisable sans audit et prototype distincts.

## 2. Éléments du dépôt vérifiés

| Zone | Observation | Conséquence |
|---|---|---|
| `runtime/controls.c` | F5 correspond à `CA_MODE` ; F1 est lié à HD text. | Le mapping existe ; vérifier le trajet événement → action → effet. |
| `runtime/frontend_sdl.c` — événements / hotkeys | Les événements clavier passent par le traitement d'actions ; F5 dépend de `g_gl`. | Le fallback logiciel peut rendre F5 inopérant et silencieux. |
| `runtime/frontend_sdl.c` — `try_gl()` / `apply_mode()` | Le contexte demandé est OpenGL 3.3 core ; le fallback logiciel force Classic comme mode effectif. | Distinguer préférence demandée et capacité réellement disponible. |
| `runtime/menu.h` / `runtime/menu_win32.c` | Le menu et plusieurs dialogues s'appuient sur les APIs Windows. | Ajouter une UI Linux sans réécrire ni modifier le menu Win32. |
| `runtime/frontend_sdl.c` — paramètres | Le parser reconnaît plusieurs clés v32, mais utilise un chemin relatif et réécrit l'état en mémoire. | Auditer lecture, validation, application et sauvegarde ; garder un seul modèle de settings. |
| `tools/build-game.sh` | Le script généré démarre le jeu depuis le dossier du jeu. | Le chemin actuel peut fonctionner via ce script mais n'est pas garanti en lancement externe. |
| `packaging/linux/gui.py` | Fenêtre Tk de build (ROM, workspace, progression, logs, launch) ; stdout/stderr du jeu sont supprimés. | Conserver la fenêtre de build dans la première phase, mais rendre les logs runtime consultables. |
| `release-notes/v32.md` | v32 annonce langues EUR, packs texture/musique, HD text, widescreen, options graphiques, sauvegardes, wireless et utilitaires. | Valider la présence, l'applicabilité et l'effet réel de chaque famille. |
| `runtime/frontend_sdl.c` et `runtime/menu_win32.c` | L'interface actuelle et le menu Windows sont liés aux mécanismes historiques du runtime. | Le raccordement du nouveau front-end Linux est un risque technique à traiter avant de bâtir toute l'UI. |

## 3. Matrice des fonctionnalités v32 sous Linux

Le fait qu'une clé soit présente dans `settings.ini` ne prouve ni que le moteur l'applique ni que l'utilisateur puisse la modifier. Pour chaque option, mesurer séparément valeur chargée, capacité requise, action, effet observé et valeur persistée.

| Catégorie v32 | Options à vérifier | Risque / dépendance | Résultat attendu |
|---|---|---|---|
| Mode et renderer | Classic / Enhanced, échelle 3D 1x–12x ou Auto | Le renderer historique conditionne Enhanced à OpenGL 3.3 ; fallback Classic. | État effectif visible ; aucune préférence écrasée silencieusement. |
| Qualité graphique | textures lissées, outlines Thin/Medium/DS, soft shadows/glows, smooth shading | Options possiblement limitées par le renderer historique ; pas de menu Linux complet. | Contrôle avec état et raison d'indisponibilité explicites. |
| HD text | HD text, ombres/contours du texte | F1 existe mais son action est liée au renderer existant. | Raccourci et UI cohérents ; aucune fausse confirmation. |
| Présentation des écrans | stacked, side-by-side, corner, Basic/Dynamic Widescreen, two windows, fenêtre séparée, battle HUD | Certaines options dépendent du renderer et/ou de la fenêtre SDL historique. | Choix explicites, états lisibles et limites documentées. |
| Petit écran / taille | position, taille, opacité, swap, fenêtre 1x–8x, integer scaling, actual size, fullscreen | Différents raccourcis et points d'entrée ; comportement lié aux fenêtres existantes. | Contrôles groupés et valeurs persistantes. |
| Filtre DS Lite / console | filtre LCD, couleurs, grille de pixels, motion blur, backlight, coque/skin, écart entre écrans | Certains effets peuvent exiger le chemin de rendu historique/OpenGL. | L'UI annonce le support réel ; aucun contrôle promettant un effet absent. |
| Mods texture | texture packs, xBR upscale, dump, reload, comparaison avec textures originales, dossier textures | Dépend des assets et du pipeline de rendu actuel ; Vulkan pour l'UI ne remplace pas ce pipeline. | Chemins explicites, détection des assets, résultat et erreurs visibles. |
| Mods musique | OGG/FLAC/WAV, reload, noms, volume, export WAV | Dépend des fichiers de `music/` et des services audio existants. | Contrôles visibles et opérations longues avec progression. |
| Langue et DLC | langues EUR English/French/German/Italian/Spanish, DLC | Dépend de la région ROM et du moment d'application. | Sélecteur sensible à la région ; indication « au prochain démarrage » si requise. |
| Contrôles | clavier, deuxième touche, gamepad, commandes DS | L'éditeur Windows ne peut pas être réutilisé tel quel sous Linux. | Page de remapping, capture de touche/bouton, résolution de conflit et annulation par Escape. |
| Sauvegarde et session | export/import save, wireless/server, host/join, nom/port, DLC | Dialogues natifs Windows pour certaines fonctions. | Parcours Linux dédiés, validation de chemin et erreurs visibles. |
| Utilitaires | volume/mute, pause, fast-forward, screenshot, bug report, update, présence Discord | Plusieurs commandes actuellement dans le menu Windows. | Actions accessibles ou explicitement indisponibles avec raison. |

## 4. Architecture cible : SDL3 + Dear ImGui + Vulkan

### Décisions obligatoires

- Les nouvelles interfaces Linux sont développées en **SDL3 + Dear ImGui + Vulkan**. Ne pas créer la nouvelle UI avec le backend SDL2/OpenGL3 proposé dans l'ancienne version du plan.
- Tous les nouveaux fichiers source dédiés au port Linux sont sous **`third_party/extra/linux/`**. Cela inclut la couche de présentation, les composants ImGui, les adaptateurs, les bindings clavier, le code Vulkan, les tests et tout glue C/C++.
- Épingler une révision de SDL3, Dear ImGui et les backends nécessaires ; conserver les licences et les informations de provenance. Les dépendances peuvent être vendored sous `third_party/extra/linux/vendor/` ou récupérées de manière reproductible par le build Linux. Pas de dépendance Linux ajoutée au build Windows.
- Garder une séparation stricte entre **UI**, **adaptateur du moteur**, **modèle de paramètres** et **renderer de jeu**. L'UI Vulkan ne doit pas prétendre pouvoir modifier un effet du moteur si l'adaptateur n'a pas obtenu de confirmation.
- Utiliser le même modèle de settings pour le launcher avant Play et le menu in-game ; ne pas maintenir deux implémentations indépendantes.
- La fenêtre Tk existante de `packaging/linux/gui.py` reste la fenêtre de construction ROM/workspace au premier jalon. Le bouton de lancement démarre ensuite le nouveau launcher Linux natif.
- Aucun remplacement du renderer de jeu par Vulkan ne doit être implicite. Le choix Vulkan concerne la nouvelle UI ; un port du renderer gameplay est une décision distincte, avec estimation et preuve de faisabilité séparées.

### Porte technique obligatoire : intégration in-game sans toucher aux sources Windows

Un launcher SDL3/Vulkan autonome est un processus et une fenêtre distincts. Il ne peut pas être supposé pouvoir dessiner un overlay dans une fenêtre SDL2/OpenGL existante. De même, recevoir Escape depuis une UI séparée ne constitue pas un hook clavier global portable, notamment sous Wayland.

Avant d'implémenter le menu in-game, réaliser un prototype Linux limité qui démontre toutes les conditions suivantes :

1. démarrer SDL3 et un device Vulkan sur la toolchain/AppImage cible ;
2. appeler un point d'extension existant du moteur, ou un adaptateur Linux isolé, sans modifier de source compilée par Windows ;
3. ouvrir/fermer l'UI avec Escape et distinguer ce raccourci du remapping des commandes du jeu ;
4. suspendre et reprendre le jeu, consommer les événements destinés à l'UI et afficher les réglages au-dessus du jeu ou dans un contexte de réglages clairement attaché au jeu ;
5. garder une seule source de vérité pour les paramètres et les capacités.

Le prototype doit tester l'environnement de fenêtre supporté (X11 et Wayland si les deux sont visés), la fermeture propre, le fullscreen et la capture clavier/manette. Si l'overlay intégré exige de modifier une source partagée avec Windows, ne pas contourner la règle : documenter l'API manquante et concevoir un remplacement strictement Linux pour le point d'entrée concerné avant d'avancer. Ne pas annoncer P3 comme acquis tant que ce test n'est pas réussi.

### Organisation proposée sous `third_party/extra/linux/`

La structure ci-dessous est indicative ; tous ces fichiers sont nouveaux et Linux-only.

- `ui/` : application SDL3, pages Dear ImGui, navigation sidebar, launcher et overlay/menu.
- `runtime/` : adaptation Linux des événements, lifecycle de l'UI et commandes de pause/reprise/quit.
- `bridge/` : API d'accès au moteur, capacités, actions et paramètres ; contrats explicites et tests.
- `vulkan/` : initialisation Vulkan, présentation, synchronisation et gestion de perte/échec de device.
- `vendor/` : sources upstream épinglées et licences de dépendances, si le projet choisit le vendoring.
- `tests/` : tests unitaires et tests d'intégration Linux sans ROM.

Les scripts de build, packaging et CI Linux peuvent rester dans leurs emplacements Linux existants (ex. `packaging/linux/` et workflows Linux dédiés) lorsqu'ils doivent être appelés par les outils du dépôt. Ils ne doivent pas faire basculer la cible Windows vers SDL3/Vulkan ou ajouter des dépendances Windows.

### Navigation visuelle

**Launcher avant Play**
- Sidebar : Game, Graphics, Screen & Display, Audio & Music, Controls, Mods, Wireless & Online, System.
- Page Game : ROM/région détectée, version runtime, sauvegarde détectée, statut UI Vulkan, capacités gameplay réellement disponibles, état du build.
- Bouton Play fixe, statut du jeu, chemin des données et raccourci vers les logs/configurations.
- En cas de ROM/build manquant, afficher la cause et proposer de revenir à la fenêtre de construction AppImage.

**Menu in-game**
- Escape ouvre/ferme Settings, uniquement après validation du prototype d'intégration.
- Suspendre le tick du jeu tant que le menu a le focus ; consommer les touches de l'UI afin qu'elles ne déclenchent pas des commandes DS.
- Resume reprend explicitement la partie ; Quit est distinct et demande confirmation. Escape en fullscreen ne doit pas fermer le jeu par accident.
- Souris, clavier et navigation manette ; focus visible et annulation cohérente de la capture de touches.
- Les réglages immédiats doivent être appliqués et vérifiés. Les changements exigeant une réinitialisation affichent « applied next launch/restart ».

## 5. Spécification fonctionnelle

### 5.1 Capacités et diagnostic

Au démarrage, exposer séparément :
- UI renderer : SDL3/Vulkan, device Vulkan et version de l'API quand disponibles ;
- renderer gameplay effectif : chemin réellement utilisé par le moteur existant (OpenGL, logiciel SDL ou futur backend), indépendamment de l'UI ;
- erreur d'initialisation de la UI Vulkan et erreur éventuelle du renderer gameplay ;
- mode demandé et mode gameplay effectivement appliqué ;
- chemins absolus de `settings.ini`, `controls.ini` et du log ;
- résultat de lecture, validation et sauvegarde des paramètres.

Le launcher et la page System/Troubleshooting doivent afficher ces informations et écrire un log local. Ne pas supprimer stdout/stderr sans redirection vers ce log. Une UI Vulkan qui démarre ne signifie pas que le mode Enhanced est supporté par le renderer du jeu.

**Ne pas écraser la préférence Enhanced par Classic uniquement parce que la capacité OpenGL du moteur historique manque.** Si le moteur tourne en Classic, conserver la préférence demandée, afficher le mode effectif et expliquer la limitation. F5 doit donner un retour observable, y compris en cas d'indisponibilité.

### 5.2 Contrat des paramètres

- Créer un modèle typé Linux sous `third_party/extra/linux/`, reflétant les mêmes clés de configuration reconnues par le runtime. Éviter une seconde source de vérité et les paramètres parallèles contradictoires.
- Résoudre le chemin de configuration par un contrat explicite indépendant du cwd (répertoire de données du jeu ou chemin utilisateur validé), fournir une migration depuis l'emplacement historique lorsqu'il le faut et afficher le chemin choisi. Un override explicite doit exister pour les tests.
- La lecture retourne un statut : absent, chargé, partiellement invalide ou erreur. Les valeurs hors plage sont rejetées avec diagnostic.
- La sauvegarde écrit d'abord dans un fichier temporaire puis remplace atomiquement la destination ; vérifier les erreurs et ne jamais tronquer silencieusement un fichier.
- Sauvegarder uniquement un état validé. Une préférence non applicable dans la session ne doit pas être remplacée par sa valeur effective temporaire.
- Résoudre et diagnostiquer `controls.ini` selon la même règle. Ne pas modifier la structure du fichier sans migration explicite.
- Maintenir une liste/test de parité des options du menu Windows afin de détecter les commandes sans alternative Linux.

Toute adaptation nécessaire au chargement ou à la sauvegarde doit être implémentée dans un nouvel adaptateur Linux sous `third_party/extra/linux/` et raccordée via une interface existante. Ne pas modifier le code de settings Windows pour faciliter ce raccordement.

### 5.3 F5, Escape et actions

- F5 envoie une demande de changement de mode à l'adaptateur Linux et au modèle de settings, plutôt que de modifier uniquement un état UI.
- Si le moteur peut appliquer le changement : vérifier l'effet, refléter l'état effectif, persister la préférence et donner un retour visible.
- Si la capacité requise manque : afficher la raison et garder intacte la préférence demandée ; ne pas afficher un succès fictif.
- Escape ouvre puis ferme le menu in-game seulement lorsque le bridge a prouvé qu'il peut le faire de façon fiable. En capture de touche dans Controls, Escape annule la capture sans fermer le menu.
- Pendant le menu, les événements UI ne doivent pas déclencher les commandes DS.
- Les actions longues (dump de textures, export audio, génération de rapports) présentent progression, succès ou erreur. Toute option désactivée explique pourquoi.

## 6. Plan d'exécution par phases

### CI de développement temporaire — diagnostics, essais et logs

Le développement a besoin d'un environnement reproductible pour exécuter de petits scripts, tester les prototypes, inspecter les dépendances et conserver les logs d'erreur. Pour éviter de reconstruire toute l'AppImage à chaque itération, une **CI temporaire dédiée au développement** sera ajoutée dans `.github/workflows/linux-dev-sandbox.yml`.

- Déclenchement automatique limité à la branche `plan/v32-linux-launcher-settings` et aux changements pertinents pour le développement Linux (workflow lui-même, `third_party/extra/linux/`, packaging Linux, scripts et tests). Elle sera aussi lançable manuellement via `workflow_dispatch`.
- Environnement initial : Ubuntu 26.04 (runner GitHub Actions pris en charge et paquet `libsdl3-dev` disponible), compilateur C/C++, CMake/Ninja, pkg-config, SDL3, Vulkan headers/loader, pilotes Vulkan Mesa et `vulkaninfo`. Le job journalise les versions, le système, les dépendances résolues et l'inventaire des fichiers modifiés.
- À chaque run, collecter les sorties de détection Vulkan, les sorties CMake/build/tests et les diagnostics. Publier les logs comme artefact même si un test échoue (`if: always()`), avec une rétention limitée, afin de pouvoir diagnostiquer une erreur sans avoir à la reproduire localement.
- Quand `third_party/extra/linux/CMakeLists.txt` existe, configurer, compiler et lancer CTest. Avant l'existence du prototype, le job reste un run de diagnostic et le signale explicitement ; l'option manuelle `require_linux_prototype` permettra de rendre l'absence du prototype bloquante dès que P0 doit être validé.
- Le résultat de `vulkaninfo` est enregistré. L'absence de device accessible devient un avertissement dans le profil normal, pas un succès de test Vulkan ; l'option `require_vulkan_device` permet de rendre un device obligatoire lorsque le run sert de validation Vulkan.
- Les tests généraux du dépôt peuvent être lancés explicitement avec `run_project_tests`. Par défaut, la CI de développement ne lance pas la construction AppImage complète et n'exécute pas toute la suite générale : elle privilégie un feedback ciblé et rapide.
- Cette CI est **temporaire** et dédiée aux essais ; réévaluer son utilité après P0/P4, puis supprimer ou fusionner ses contrôles utiles dans la CI permanente. Les fichiers sources Linux ajoutés restent sous `third_party/extra/linux/` ; le fichier workflow est de la configuration CI et fait exception à cette règle de placement.
- La CI actuelle `linux-ci-appimage.yml` reste inchangée. Elle cible actuellement `main`/ `stable` et la commande manuelle. Les pushes sur la branche de développement utilisent la CI temporaire sans déclencher l'AppImage complète. Une validation complète reste nécessaire avant une intégration finale lorsque les changements touchent le build ou le packaging ; ne pas utiliser une option de skip pour présenter comme validé un changement qui ne l'a pas été.

### P0 — Audit des frontières Windows/Linux et preuve technique

1. Auditer le build réel. `build.mk` sélectionne `runtime/frontend_$(FRONTEND).c` et, pour `FRONTEND=sdl`, ajoute également les unités de rendu/menu/contrôles historiques. `tools/build-game.sh` demande aujourd'hui `FRONTEND=sdl` pour les builds Windows comme Linux.
2. Cartographier les points d'entrée et le cycle runtime. `runtime/frontend.h` définit `frontend_init()`, `frontend_frame()` et `frontend_shutdown()`. `runtime/frontend_sdl.c` initialise SDL2, crée la fenêtre OpenGL 3.3 (ou le fallback SDL/software), traite Escape et les raccourcis, présente les écrans, cadence le jeu et exécute la boucle d'attente pour `g_paused`.
3. Compiler et lancer le probe autonome **SDL3 + Dear ImGui + Vulkan** sous `third_party/extra/linux/`. Le probe épingle Dear ImGui v1.93.0 à un commit immuable, compile l'exemple upstream `example_sdl3_vulkan` et fournit un smoke test Xvfb. Tant que les logs CI ne sont pas vérifiés, le résultat est « ajouté, non validé » ; l'absence d'un device Vulkan doit être enregistrée comme test ignoré/non exécuté, jamais comme rendu réussi.
4. Traiter séparément la faisabilité du menu in-game. Un processus SDL3/Vulkan distinct peut être un launcher, mais il ne partage ni la fenêtre SDL2/OpenGL, ni la file d'événements, ni l'état de pause/tick du runtime. X11/Wayland ne constituent pas, à eux seuls, une API du moteur. Ne pas déclarer un overlay ou Escape in-game réalisable sur la seule base du probe autonome.
5. Le build actuel n'offre pas de variable `FRONTEND_SRC` ou de point d'injection pour sélectionner directement un fichier sous `third_party/extra/linux/` : `build.mk` construit le chemin sous `runtime/`. Le modifier est hors des règles actuelles car il sert également au build Windows. Avant une intégration, rechercher un mécanisme d'extension Linux qui ne requiert ni changement de source Windows ni changement du build Windows. Ne pas créer un symlink/source shim dans `runtime/` pour contourner la règle de confinement.
6. Reproduire et instrumenter le problème F5 avec OpenGL 3.3 et fallback logiciel ; consigner séparément le renderer UI Vulkan et le renderer gameplay effectif. Garder la préférence demandée distincte de la capacité effective.
7. Auditer settings.ini/controls.ini, le cwd, la sauvegarde de fin de processus et les logs. Les tests unitaires et le probe UI ne doivent pas nécessiter de ROM. Si un point d'extension conforme n'existe pas, enregistrer le blocage et proposer une décision d'architecture avant P3.

**Sortie P0 :** (a) résultats build/smoke du probe SDL3/Vulkan/ImGui vérifiés ; (b) manifeste des frontières Windows/Linux ; (c) décision explicite sur l'intégration in-game. Le probe graphique ne valide pas l'intégration runtime. P3 reste bloqué tant que (c) n'est pas résolu.

### P1 — Fiabiliser les paramètres et rendre le renderer lisible

**État d'implémentation au 2026-10-29 :** une première couche `settings/settings_model.cpp` existe sous `third_party/extra/linux/`. Elle préserve les lignes/valeurs non reconnues, valide les clés connues, borne les valeurs selon une table de types et écrit avec remplacement atomique. Un test CTest compare les clés du schéma avec les clés lues par `runtime/frontend_sdl.c` et la fonction `settings_save()`, en plus des tests round-trip. Le test est ajouté mais son exécution CI reste à vérifier ; le launcher n'utilise pas encore ce modèle et le chemin relatif historique n'est pas encore résolu par un contrat de données.


1. Implémenter le modèle et l'adaptateur de configuration Linux dans `third_party/extra/linux/`.
2. Stabiliser les chemins et le cycle lecture/validation/sauvegarde sans écraser les préférences sur un fallback.
3. Séparer mode demandé, mode gameplay effectif et renderer UI Vulkan.
4. Exposer capacités et erreurs dans le log et le launcher ; laisser les sources Windows inchangées.
5. Ajouter des tests round-trip pour toutes les clés de paramètres et un test de migration sans perte.

**Sortie P1 :** paramètres traçables et persistants, capacité effective visible et aucun reset silencieux.

### P2 — Launcher Linux natif

1. Ajouter SDL3, Dear ImGui et Vulkan à la toolchain Linux/AppImage par un mécanisme épinglé et reproductible ; enregistrer les licences.
2. Implémenter sous `third_party/extra/linux/ui/` le shell de l'application, la sidebar et les pages du launcher.
3. Laisser l'utilisateur régler les options avant Play, puis appliquer uniquement les réglages que le bridge peut fournir au jeu.
4. Rendre les logs, les chemins de configuration et l'état du renderer accessibles.
5. Garder la fenêtre de build Tk existante ; le launcher natif vient après la phase de build. Le build Windows ne doit pas récupérer SDL3/Vulkan par cette intégration.

**Sortie P2 :** launcher Linux SDL3/Vulkan fonctionnel avant Play, testable sans ROM.

### P3 — Menu in-game Escape et parité v32

1. Commencer uniquement après réussite de la porte technique de P0.
2. Réutiliser les mêmes composants/pages ImGui et le même modèle de settings que le launcher.
3. Implémenter Escape, focus, pause/reprise, Resume et Quit via le bridge Linux.
4. Donner une alternative Linux aux actions de la barre de menu Win32 sans changer cette barre ni ses sources.
5. Parcourir la matrice de la section 3. Toute option sans support doit être désactivée avec sa raison, jamais présentée comme fonctionnelle par défaut.

**Sortie P3 :** menu in-game prouvé sur le runtime Linux ciblé et couverture mesurée des options v32.

### P4 — Tests, documentation, AppImage et contrôle upstream

1. Tests unitaires de parsing, plages invalides, fichier absent/non inscriptible, migration, cwd et état demandé/effectif.
2. Tests d'actions avec événements clavier synthétiques : F5, Escape, annulation de capture et remapping.
3. Test UI sans ROM : démarrage SDL3/Vulkan, navigation, changements de page, échec de création de device géré proprement.
4. Utiliser la CI temporaire de développement pour les essais ciblés et conserver ses logs en artefact. CI Linux : dépendances Vulkan/Mesa et environnement headless approprié ; si une machine CI n'offre pas de device Vulkan réel, marquer le test rendu comme non exécuté et ne pas le compter comme succès. Tester également les scénarios sans GPU via un résultat explicite.
5. Vérifier AppImage : dépendances dynamiques, lancement avec cache froid/chaud, logs et aucun asset de jeu dans l'artefact.
6. Maintenir une matrice de compilation prouvant que la cible Windows reste inchangée et compile toujours avec son outillage existant. Le diff ne doit inclure aucune source Windows ni modification de configuration partagée ayant un effet sur Windows.
7. Mettre à jour `docs/LINUX-APPIMAGE.md` et README avec touches, chemins, dépendances Vulkan, logs, limites connues et dépannage.
8. Test manuel sur Linux avec ROM personnelle : premier lancement, paramètres, options compatibles, langue EUR, packs, sauvegarde, wireless, fermeture et redémarrage.

## 7. Critères d'acceptation

- [ ] Aucun fichier source Windows existant ni configuration propre au build Windows n'est modifié.
- [ ] Tous les nouveaux sources Linux de l'interface, du bridge, de Vulkan et des tests spécifiques sont sous `third_party/extra/linux/`.
- [ ] Le launcher natif démarre avec SDL3 + Dear ImGui + Vulkan et affiche les erreurs de création d'instance/device/surface.
- [ ] Les diagnostics distinguent le renderer Vulkan de l'UI du renderer réel du jeu.
- [ ] Le probe P0 SDL3/Vulkan/ImGui compile et son smoke test est observé dans les logs CI ; un test ignoré faute de device n'est pas présenté comme rendu validé.
- [ ] La faisabilité d'Escape/in-game est prouvée par un raccordement réel au runtime, ou documentée comme blocage explicite. Le probe autonome ne satisfait pas ce critère ; P3 reste bloqué tant que l'intégration n'est pas résolue.
- [ ] F5 change réellement le mode gameplay quand le moteur le permet ; sinon une explication est affichée et la préférence demandée n'est pas écrasée.
- [ ] Escape ouvre/ferme le menu in-game ; Resume reprend le jeu ; Quit est séparé et confirmé.
- [ ] Launcher et menu in-game partagent les mêmes pages, le même modèle de settings et les mêmes résultats d'application.
- [ ] `settings.ini` et `controls.ini` sont résolus indépendamment du cwd, et le chemin utilisé est visible.
- [ ] Les changements à redémarrage requis le signalent ; les changements appliqués à chaud confirment leur résultat effectif.
- [ ] Chaque commande du menu Windows a un équivalent Linux ou est explicitement marquée non applicable avec une raison.
- [ ] La CI temporaire de développement produit des logs/diagnostics en artefact même sur échec, et peut rendre le prototype et un device Vulkan obligatoires lorsque nécessaire.\n- [ ] La CI vérifie settings, actions, compilation/link Linux, test UI et traitement des erreurs Vulkan sans nécessiter de ROM.
- [ ] L'AppImage ne contient aucune ROM, sauvegarde ni asset de jeu.
- [ ] La comparaison de branche montre que les sources et la configuration Windows sont inchangées ; le chemin upstream Windows reste propre.

## 8. Hors périmètre du premier cycle

- Réécrire le moteur de gameplay ou prétendre que l'adoption de Vulkan par l'UI remplace automatiquement son renderer OpenGL/SDL historique.
- Promettre Enhanced ou un effet graphique sur une machine sans la capacité gameplay requise.
- Modifier les sources Windows pour faciliter l'adaptation Linux.
- Ajouter ROM, sauvegarde ou assets de jeu dans CI ou AppImage.
- Remplacer la fenêtre de build ROM/workspace avant que le launcher et le démarrage soient stabilisés.
- Faire un port complet du renderer gameplay vers Vulkan sans une spécification, une preuve de faisabilité et un plan de migration indépendants.

## 9. Références de code

- Runtime SDL, paramètres, détection OpenGL, hotkeys et actions : `runtime/frontend_sdl.c`.
- Mapping clavier/manette, F5/F1 et actions configurables : `runtime/controls.c` et `runtime/controls.h`.
- Menu Windows-only et identifiants de commandes : `runtime/menu_win32.c` et `runtime/menu.h` — à préserver intégralement.
- Script de lancement depuis le dossier du jeu : `tools/build-game.sh`.
- Builder AppImage et redirection stdout/stderr : `packaging/linux/gui.py`.
- Fonctionnalités annoncées en v32 : `release-notes/v32.md`.
- Référence d'inspiration (architecture du launcher et de la UI de paramètres) : https://github.com/rakmahefa/letsgo-recomp/blob/main/runtime/ui/launcher_lg.mm et https://github.com/rakmahefa/letsgo-recomp/blob/main/runtime/ui/settings_ui.mm.

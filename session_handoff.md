# Session handoff — DQIX Enhanced Linux launcher (SDL3 / ImGui / Vulkan)

**Dernière mise à jour :** 2026-10-29  
**Dépôt :** https://github.com/rakmahefa/dqix-enhanced  
**Branche de travail :** `plan/v32-linux-launcher-settings`  
**Branche de base :** `main`  
**Commit de base connu :** `ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a`  
**Dernier commit confirmé avant cette mise à jour de la note :** `ad5251d3452d3e9ef5ab0f9863d2f9ea896211ca`  
**État fonctionnel :** plan, CI temporaire et probe autonome SDL3/Vulkan/Dear ImGui ajoutés ; compilation et smoke test CI non confirmés depuis cette session. Le launcher DQIX, le bridge runtime et le menu in-game ne sont pas implémentés.

## 1. Règles impératives

1. **Ne modifier aucun fichier source Windows existant ni sa configuration de build.** La compatibilité avec l'upstream Windows doit rester possible. Le menu Win32 et ses ressources doivent rester intacts.
2. Tout **nouveau fichier source natif Linux** (C/C++, headers, shaders, adaptateurs, glue code, tests natifs) doit être sous `third_party/extra/linux/`. Ne pas disperser les nouveaux sources dans `runtime/` ou ailleurs.
3. La nouvelle UI Linux utilise **SDL3 + Dear ImGui + Vulkan**.
4. **UI Vulkan ≠ renderer gameplay Vulkan.** Le jeu utilise actuellement un chemin SDL2/OpenGL avec fallback logiciel. Ne pas annoncer Enhanced/Vulkan gameplay simplement parce que le launcher Vulkan se lance.
5. Si l'intégration in-game nécessiterait de modifier un source partagé avec Windows, arrêter et chercher un point d'extension ou un adaptateur Linux isolé. Documenter le blocage plutôt que contourner la règle.

## 2. Documents et CI disponibles

- Plan de référence : [plan-spec.md](https://github.com/rakmahefa/dqix-enhanced/blob/plan/v32-linux-launcher-settings/plan-spec.md)
- CI temporaire : [linux-dev-sandbox.yml](https://github.com/rakmahefa/dqix-enhanced/blob/plan/v32-linux-launcher-settings/.github/workflows/linux-dev-sandbox.yml)
- Ce document : [session_handoff.md](https://github.com/rakmahefa/dqix-enhanced/blob/plan/v32-linux-launcher-settings/session_handoff.md)

La CI temporaire est destinée aux itérations de développement. Elle est limitée à la branche de travail et peut également être lancée manuellement. Elle :
- installe les dépendances de diagnostic SDL3/Vulkan sur Ubuntu 26.04 (dont rsync pour le test de politique AppImage facultatif) ;
- enregistre les versions des outils et le résultat de `vulkaninfo` ;
- vérifie la frontière de sources Windows et le placement des nouveaux sources natifs ;
- configure/compile/teste `third_party/extra/linux/` s'il contient un `CMakeLists.txt` ;
- conserve les logs comme artefact `linux-dev-diagnostics`, avec rétention de 14 jours, même quand un job échoue ;
- expose les options manuelles `require_linux_prototype`, `require_vulkan_device` et `run_project_tests`.

Valeurs par défaut du lancement manuel : les exigences strictes et les tests généraux sont désactivés, pour permettre un run de diagnostic au tout début. Quand le prototype existe, activer `require_linux_prototype`; pour une validation d'exécution Vulkan, activer aussi `require_vulkan_device`. Si l'hôte CI n'expose pas de device, un avertissement/diagnostic n'est pas une preuve de rendu Vulkan.

Constats P0 ajoutés le 2026-10-29 :

- `runtime/frontend.h` expose le contrat `frontend_init()` / `frontend_frame()` / `frontend_shutdown()`. `runtime/frontend_sdl.c` est actuellement le frontend SDL2/OpenGL du runtime ; son initialisation crée la fenêtre, son dispatch consomme les événements clavier/souris/manette, il présente les frames, cadence le jeu et sa boucle `g_paused` maintient la fenêtre vivante tout en suspendant l'avancement.
- `build.mk` choisit les fichiers via `runtime/frontend_$(FRONTEND).c`. En mode `sdl`, il ajoute aussi les unités de rendu/menu/contrôles historiques. `tools/build-game.sh` utilise `FRONTEND=sdl` pour Windows et Linux. Il n'existe pas actuellement de point d'injection pour pointer directement sur un frontend isolé sous `third_party/extra/linux/`.
- Un processus SDL3/Vulkan distinct prouve uniquement que la stack de l'UI démarre ; il ne partage pas la fenêtre, les événements ou l'état de pause/tick du jeu. **Ne pas présenter le menu in-game Escape comme faisable** tant qu'un raccordement réel n'est pas démontré.
- Le respect simultané de la règle « aucune source/configuration de build Windows modifiée » et le besoin d'un hook runtime in-game sont donc un point d'architecture bloquant. Il faut examiner une entrée de build Linux entièrement isolée ou décider explicitement d'une séparation de frontend Linux avant P3. Ne pas utiliser de source shim/symlink sous `runtime/` pour contourner le confinement.
- Le statut CI n'est pas encore vérifié : les outils GitHub consultés dans cette session ne renvoient pas les runs déclenchés par push pour le SHA courant. Une liste vide de statuts ou de runs retournés ne veut pas dire que le workflow a réussi. Lire le run dans GitHub Actions et ses artefacts avant de marquer le probe vert.

Le probe P0 est défini dans `third_party/extra/linux/CMakeLists.txt`. Il compile l'exemple officiel `example_sdl3_vulkan` de Dear ImGui v1.93.0 épinglé au commit `c1aaac65e896a45f81f47a4d68f9959e0f86c859`, sans copier/modifier de source upstream dans le dépôt. `tests/smoke-ui.sh` le lance sous Xvfb pendant 6 secondes ; si aucun device Vulkan n'est disponible, le test est marqué comme ignoré par défaut (et non réussi). Ce probe ne teste pas le runtime DQIX.

La CI existante [linux-ci-appimage.yml](https://github.com/rakmahefa/dqix-enhanced/blob/main/.github/workflows/linux-ci-appimage.yml) n'a pas été modifiée. Elle vise `main`/`stable` et le lancement manuel ; elle construit l'AppImage et exécute les tests généraux. Ne pas imposer cette construction lourde à chaque petite expérience. Avant une PR finale qui affecte le build, les dépendances ou le packaging, exécuter la validation complète pertinente et ne pas déclarer un test passé s'il n'a pas été exécuté.

## 3. Constats techniques de l'audit précédent

- `runtime/controls.c` mappe F5 vers `CA_MODE`, mais l'action existante dépend de `g_gl`.
- `runtime/frontend_sdl.c` demande OpenGL 3.3. Si le contexte échoue, le runtime peut utiliser un renderer logiciel et forcer Classic comme état effectif ; la sauvegarde de fin de processus peut écraser la préférence Enhanced. La cause exacte sur la machine de test reste à confirmer par des logs.
- Le menu et plusieurs dialogues sous `runtime/menu_win32.c` sont Windows-only ; Linux n'a pas d'UI équivalente pour l'ensemble des options v32.
- `settings.ini` est ouvert par chemin relatif au répertoire courant et la sauvegarde peut réécrire l'état conservé en mémoire.
- `packaging/linux/gui.py` masque actuellement stdout/stderr du jeu en les redirigeant vers `DEVNULL`. Ajouter des diagnostics accessibles sans exposer les données privées de jeu.
- Let's Go Recomp sert d'inspiration pour les pages, la navigation et le modèle partagé des paramètres ; ne pas supposer que son code UI peut être copié directement.

## 4. Prochaine tâche recommandée

1. **Vérifier le run CI réel** de `Linux development diagnostics (temporary)` sur cette branche. La consultation effectuée depuis le connecteur GitHub ne donnait pas la liste des runs déclenchés par push pour le SHA courant. Si aucun run n'est visible, le lancer manuellement dans Actions avec `require_linux_prototype=true`, puis consulter l'artefact `linux-dev-diagnostics`.
2. Lire `platform-and-dependencies.txt`, `vulkaninfo-summary.txt`, `changed-files.txt`, `source-boundary-check.txt`, `prototype-build.txt`, `cmake-build.log` et `ctest.log`. Noter explicitement si CTest passe, échoue ou ignore le smoke test faute de device Vulkan.
3. Compléter le manifeste de build à partir de `build.mk`, de `tools/build-game.sh` et des objets réellement générés ; la CI actuelle contrôle le diff de frontières mais ne prouve pas encore la matrice complète de compilation Windows/Linux.
4. **Garder P3 bloqué.** Définir un chemin d'intégration qui ne change ni les sources ni la configuration Windows. Avant de coder un bridge, comparer ses coûts avec deux choix honnêtes : launcher SDL3/Vulkan pré-Play seulement, ou build/frontend Linux entièrement isolé. Ne pas modifier `build.mk` partagé et ne pas ajouter de shim source dans `runtime/` sous le couvert d'une intégration Linux.
5. Poursuivre la partie pré-Play indépendante : modèle de settings typé, validation/round-trip/tests sans ROM, journalisation et page System/Troubleshooting. L'application effective des réglages au moteur devra réutiliser les clés réellement reconnues par le runtime, et signaler toute limite.
6. Seulement après la décision d'architecture, planifier la UI launcher finale. La fenêtre Tk de génération ROM/workspace reste en place ; ne pas la remplacer prématurément et ne pas construire l'AppImage complète à chaque itération.

## 5. Critères de validation à ne pas oublier

- Diff contre `main` propre vis-à-vis des sources Windows.
- Tous les nouveaux sources natifs Linux sous `third_party/extra/linux/`.
- Logs conservés même en cas d'échec, avec version des outils, résultat Vulkan et sorties de build/test.
- Distinguer le renderer de l'UI du renderer gameplay ; ne pas confondre « Vulkan disponible » et « Enhanced actif ».
- Distinguer préférence demandée et capacité effective ; aucune modification silencieuse de préférence parce qu'une capacité manque.
- Pas de ROM, de sauvegarde ni d'assets protégés dans CI ou les artefacts.
- Ne déclarer comme réussis que les tests réellement exécutés ; toute limite d'environnement doit apparaître dans les notes de run.

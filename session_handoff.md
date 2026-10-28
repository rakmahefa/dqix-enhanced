# Session handoff — DQIX Enhanced Linux launcher (SDL3 / ImGui / Vulkan)

**Date de rédaction :** 2026-10-28  
**Dépôt :** https://github.com/rakmahefa/dqix-enhanced  
**Branche de travail :** `plan/v32-linux-launcher-settings`  
**Branche de base :** `main`  
**Commit de base connu :** `ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a`  
**Dernier commit confirmé avant cette mise à jour de la note :** `b715b28fd37d8e3b99718c2c9a357cdd79a1c8ab`  
**État fonctionnel :** plan, CI temporaire et probe autonome SDL3/Vulkan/Dear ImGui ajoutés ; compilation et test de fumée restent à vérifier. Le vrai launcher, le bridge runtime et le menu in-game ne sont pas implémentés.

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
- installe les dépendances de diagnostic SDL3/Vulkan sur Ubuntu 24.04 (dont rsync pour le test de politique AppImage facultatif) ;
- enregistre les versions des outils et le résultat de `vulkaninfo` ;
- vérifie la frontière de sources Windows et le placement des nouveaux sources natifs ;
- configure/compile/teste `third_party/extra/linux/` s'il contient un `CMakeLists.txt` ;
- conserve les logs comme artefact `linux-dev-diagnostics`, avec rétention de 14 jours, même quand un job échoue ;
- expose les options manuelles `require_linux_prototype`, `require_vulkan_device` et `run_project_tests`.

Valeurs par défaut du lancement manuel : les exigences strictes et les tests généraux sont désactivés, pour permettre un run de diagnostic au tout début. Quand le prototype existe, activer `require_linux_prototype`; pour une validation d'exécution Vulkan, activer aussi `require_vulkan_device`. Si l'hôte CI n'expose pas de device, un avertissement/diagnostic n'est pas une preuve de rendu Vulkan.

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

Continuer **P0 — prouver les frontières d'intégration** avant de construire les pages du launcher :

1. Après la mise à jour de la branche de travail, vérifier que `Linux development diagnostics (temporary)` a réellement tourné. L'API GitHub consultée dans cette session ne renvoie pas de statut/check-run pour le commit de handoff ; ne pas en déduire que la CI est verte. Si le push ne déclenche rien, lancer le workflow manuellement depuis GitHub Actions.
2. Examiner l'artefact `linux-dev-diagnostics` : `platform-and-dependencies.txt`, `vulkaninfo-summary.txt`, `changed-files.txt`, `source-boundary-check.txt`, `prototype-build.txt`, `cmake-build.log` et `ctest.log`. Le build et le test restent **non vérifiés** jusqu'à l'observation de ces logs.
3. Cartographier l'ordre de vie du runtime : fenêtre/contexte SDL2, dispatch clavier, tick/pause, settings et fermeture. `runtime/frontend_sdl.c` détient actuellement la boucle SDL2, les fenêtres et le rendu ; le code de commandes appelle les actions du même processus. Une UI SDL3 indépendante ne suffit donc pas, en elle-même, à fournir un overlay intégré ni à suspendre le jeu.
4. Prouver un point d'extension Linux isolé. Escape, capture d'événements, pause/reprise et fullscreen ne doivent pas être annoncés comme faisables avant qu'une liaison vers le runtime soit démontrée. Ne pas modifier les sources Windows partagées pour rendre ce raccordement plus simple.
5. Si aucun point d'extension conforme n'existe, consigner le blocage et proposer une séparation du frontend/runtime Linux dédiée avant P1/P2/P3. Ensuite seulement, poursuivre la persistance des settings et logs, le launcher pré-Play, puis la parité v32.

## 5. Critères de validation à ne pas oublier

- Diff contre `main` propre vis-à-vis des sources Windows.
- Tous les nouveaux sources natifs Linux sous `third_party/extra/linux/`.
- Logs conservés même en cas d'échec, avec version des outils, résultat Vulkan et sorties de build/test.
- Distinguer le renderer de l'UI du renderer gameplay ; ne pas confondre « Vulkan disponible » et « Enhanced actif ».
- Distinguer préférence demandée et capacité effective ; aucune modification silencieuse de préférence parce qu'une capacité manque.
- Pas de ROM, de sauvegarde ni d'assets protégés dans CI ou les artefacts.
- Ne déclarer comme réussis que les tests réellement exécutés ; toute limite d'environnement doit apparaître dans les notes de run.
